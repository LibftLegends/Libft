#include "kv_store.hpp"

#include <cstddef>
#include <cerrno>
#include <climits>
#include <fcntl.h>

#include "../Compatebility/sqlite_support.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"
#if SQLITE3_AVAILABLE
# include <sqlite3.h>
#endif

#include "../Basic/basic.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/move.hpp"
#include "../Time/time.hpp"
#include "../GetNextLine/get_next_line.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Printf/printf.hpp"

const char *g_kv_store_ttl_prefix = "__ttl__";

static void kv_store_finalize_lock(pt_recursive_mutex *mutex_pointer, ft_bool lock_acquired) noexcept
{
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    return ;
}

static int32_t storage_kv_move_string_error(ft_string &value) noexcept
{
    if (value.c_str() == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

static int64_t storage_kv_parse_long_long(const char *value_string, char **end_pointer, int32_t numeric_base) noexcept
{
    int64_t parsed_value = ft_strtol(value_string, end_pointer, numeric_base);
    return (parsed_value);
}

static int32_t storage_kv_write_newline(su_file *stream) noexcept
{
    char newline_character = '\n';
    ft_size_t newline_written = su_fwrite(&newline_character, 1, 1, stream);
    if (newline_written != 1)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::get_active_encryption_algorithm_name(ft_string &algorithm_name) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store::get_active_encryption_algorithm_name");
    (void)algorithm_name.destroy();
    if (this->_encryption_algorithm_name.size() == 0)
        algorithm_name = "aes-128-ecb-base64";
    else
        algorithm_name = this->_encryption_algorithm_name;
    if (algorithm_name.c_str() == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::lock_replication(ft_bool *lock_acquired) const noexcept
{
    ft_bool has_mutex;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::lock_replication");
    if (lock_acquired == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    has_mutex = (this->_replication_mutex != ft_nullptr);
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_replication_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    *lock_acquired = has_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::dispatch_snapshot_to_sink(kv_store_replication_snapshot_callback snapshot_callback, void *user_data) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::dispatch_snapshot_to_sink");
    ft_vector<kv_store_snapshot_entry> snapshot_entries;
    int32_t initialize_error;
    int32_t callback_error;

    if (snapshot_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    initialize_error = snapshot_entries.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    snapshot_entries.clear();
    if (snapshot_entries.get_error() != FT_ERR_SUCCESS)
        return (snapshot_entries.get_error());
    if (const_cast<kv_store *>(this)->export_snapshot(snapshot_entries) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    callback_error = snapshot_callback(snapshot_entries, user_data);
    if (callback_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::notify_replication_listeners(const ft_vector<kv_store_operation> &operations) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::notify_replication_listeners");
    kv_store *mutable_this;
    ft_bool lock_acquired = FT_FALSE;
    ft_vector<kv_store_replication_sink> listeners_copy;
    ft_size_t listener_count;
    ft_size_t listener_index;
    int32_t lock_error;

    mutable_this = const_cast<kv_store *>(this);
    lock_error = listeners_copy.initialize();
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    listener_count = operations.size();
    if (operations.get_error() != FT_ERR_SUCCESS)
        return (operations.get_error());
    if (listener_count == 0)
        return (FT_ERR_SUCCESS);
    lock_error = mutable_this->lock_replication(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (lock_error);
    }
    listeners_copy.clear();
    if (listeners_copy.get_error() != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (listeners_copy.get_error());
    }
    listener_count = mutable_this->_replication_sinks.size();
    if (mutable_this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (mutable_this->_replication_sinks.get_error());
    }
    listener_index = 0;
    while (listener_index < listener_count)
    {
        kv_store_replication_sink sink_entry;

        sink_entry = mutable_this->_replication_sinks[listener_index];
        if (mutable_this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
        {
            kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
            return (mutable_this->_replication_sinks.get_error());
        }
        listeners_copy.push_back(sink_entry);
        if (listeners_copy.get_error() != FT_ERR_SUCCESS)
        {
            kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
            return (listeners_copy.get_error());
        }
        listener_index++;
    }
    kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
    listener_count = listeners_copy.size();
    if (listeners_copy.get_error() != FT_ERR_SUCCESS)
        return (listeners_copy.get_error());
    listener_index = 0;
    while (listener_index < listener_count)
    {
        const kv_store_replication_sink &sink = listeners_copy[listener_index];
        int32_t callback_error;

        if (listeners_copy.get_error() != FT_ERR_SUCCESS)
            return (listeners_copy.get_error());
        if (sink._operations_callback != ft_nullptr)
        {
            callback_error = sink._operations_callback(operations, sink._user_data);
            if (callback_error != 0)
                return (FT_ERR_INVALID_OPERATION);
        }
        listener_index++;
    }
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::register_replication_sink(kv_store_replication_operations_callback operations_callback,
        kv_store_replication_snapshot_callback snapshot_callback, void *user_data,
        ft_bool ship_initial_snapshot)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::register_replication_sink");
    ft_bool lock_acquired = FT_FALSE;
    kv_store_replication_sink sink_entry;
    ft_size_t listener_count;
    ft_size_t listener_index;
    int32_t lock_error;

    if (operations_callback == ft_nullptr && snapshot_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = this->lock_replication(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (lock_error);
    }
    listener_count = this->_replication_sinks.size();
    if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (this->_replication_sinks.get_error());
    }
    listener_index = 0;
    while (listener_index < listener_count)
    {
        const kv_store_replication_sink &existing = this->_replication_sinks[listener_index];

        if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
        {
            kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
            return (this->_replication_sinks.get_error());
        }
        if (existing._operations_callback == operations_callback
            && existing._snapshot_callback == snapshot_callback
            && existing._user_data == user_data)
        {
            kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
            return (FT_ERR_INVALID_OPERATION);
        }
        listener_index++;
    }
    sink_entry._operations_callback = operations_callback;
    sink_entry._snapshot_callback = snapshot_callback;
    sink_entry._user_data = user_data;
    this->_replication_sinks.push_back(sink_entry);
    if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (this->_replication_sinks.get_error());
    }
    kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
    if (ship_initial_snapshot && snapshot_callback != ft_nullptr)
        return (this->dispatch_snapshot_to_sink(snapshot_callback, user_data));
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::unregister_replication_sink(kv_store_replication_operations_callback operations_callback,
        kv_store_replication_snapshot_callback snapshot_callback, void *user_data)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::unregister_replication_sink");
    ft_bool lock_acquired = FT_FALSE;
    ft_size_t listener_count;
    ft_size_t listener_index;
    ft_bool removed;
    int32_t lock_error;

    lock_error = this->lock_replication(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (lock_error);
    }
    listener_count = this->_replication_sinks.size();
    if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
        return (this->_replication_sinks.get_error());
    }
    listener_index = 0;
    removed = FT_FALSE;
    while (listener_index < listener_count)
    {
        const kv_store_replication_sink &existing = this->_replication_sinks[listener_index];

        if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
        {
            kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
            return (this->_replication_sinks.get_error());
        }
        if (existing._operations_callback == operations_callback
            && existing._snapshot_callback == snapshot_callback
            && existing._user_data == user_data)
        {
            this->_replication_sinks.erase(this->_replication_sinks.begin() + static_cast<ptrdiff_t>(listener_index));
            if (this->_replication_sinks.get_error() != FT_ERR_SUCCESS)
            {
                kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
                return (this->_replication_sinks.get_error());
            }
            removed = FT_TRUE;
            break ;
        }
        listener_index++;
    }
    kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
    if (removed == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::ship_replication_snapshot(kv_store_replication_snapshot_callback snapshot_callback, void *user_data) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::ship_replication_snapshot");
    return (this->dispatch_snapshot_to_sink(snapshot_callback, user_data));
}

int32_t kv_store::assign_backend_location(const char *location)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::assign_backend_location");
    if (location == ft_nullptr)
    {
        this->_file_path.clear();
        return (FT_ERR_SUCCESS);
    }
    this->_file_path = location;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::parse_json_groups(json_group *group_head, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::parse_json_groups");
    json_group *store_group;
    json_item *item_pointer;
    ft_map<ft_string, int64_t> ttl_metadata;
    ft_size_t ttl_prefix_length;
    int32_t ttl_initialize_error;

    if (group_head == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    ttl_initialize_error = ttl_metadata.initialize();
    if (ttl_initialize_error != FT_ERR_SUCCESS)
        return (ttl_initialize_error);
    store_group = json_find_group(group_head, "kv_store");
    if (store_group == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    ttl_metadata.clear();
    if (ttl_metadata.get_error() != FT_ERR_SUCCESS)
        return (ttl_metadata.get_error());
    ttl_prefix_length = ft_strlen(g_kv_store_ttl_prefix);
    item_pointer = store_group->items;
    while (item_pointer != ft_nullptr)
    {
        if (ft_strcmp(item_pointer->key, "__encryption__") == 0)
        {
            ft_string active_algorithm_name;

            if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
            {
                return (FT_ERR_INVALID_OPERATION);
            }
            if (ft_strcmp(item_pointer->value, active_algorithm_name.c_str()) == 0)
            {
                if (this->_encryption_enabled == FT_FALSE)
                {
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            else
            {
                return (FT_ERR_INVALID_OPERATION);
            }
            item_pointer = item_pointer->next;
            continue ;
        }
        if (ttl_prefix_length > 0
            && ft_strncmp(item_pointer->key, g_kv_store_ttl_prefix, ttl_prefix_length) == 0)
        {
            ft_string ttl_key;
            ft_string ttl_suffix;
            if (ttl_key.initialize() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            if (ttl_suffix.initialize(item_pointer->key + ttl_prefix_length) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            ttl_key = ttl_suffix;
            int64_t expiration_timestamp;

            int32_t ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                return (FT_ERR_INVALID_OPERATION);
            }
            if (this->parse_expiration_timestamp(item_pointer->value, expiration_timestamp) != 0)
                return (FT_ERR_INVALID_OPERATION);
            ttl_metadata.insert(ttl_key, expiration_timestamp);
            if (ttl_metadata.get_error() != FT_ERR_SUCCESS)
                return (ttl_metadata.get_error());
            item_pointer = item_pointer->next;
            continue ;
        }
        kv_store_snapshot_entry snapshot_entry;
        if (snapshot_entry.key.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        if (snapshot_entry.value.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);

        snapshot_entry.has_expiration = FT_FALSE;
        snapshot_entry.expiration_timestamp = 0;
        snapshot_entry.key = item_pointer->key;
        int32_t snapshot_key_error = storage_kv_move_string_error(snapshot_entry.key);
        if (snapshot_key_error != FT_ERR_SUCCESS)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize(item_pointer->value) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            ft_string decrypted_value;
            if (decrypted_value.initialize() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);

            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                return (FT_ERR_INVALID_OPERATION);
            }
            if (this->decrypt_value(encoded_value, decrypted_value) != 0)
                return (FT_ERR_INVALID_OPERATION);
            snapshot_entry.value = decrypted_value;
        }
        else
            snapshot_entry.value = item_pointer->value;
        int32_t snapshot_value_error = storage_kv_move_string_error(snapshot_entry.value);
        if (snapshot_value_error != FT_ERR_SUCCESS)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        out_entries.push_back(ft_move(snapshot_entry));
        if (out_entries.get_error() != FT_ERR_SUCCESS)
            return (out_entries.get_error());
        item_pointer = item_pointer->next;
    }
    ft_size_t ttl_size;

    ttl_size = ttl_metadata.size();
    if (ttl_metadata.get_error() != FT_ERR_SUCCESS)
        return (ttl_metadata.get_error());
    if (ttl_size > 0)
    {
        const Pair<ft_string, int64_t> *ttl_end;
        const Pair<ft_string, int64_t> *ttl_begin;
        ft_size_t ttl_index;

        ttl_end = ttl_metadata.end();
        if (ttl_metadata.get_error() != FT_ERR_SUCCESS)
            return (ttl_metadata.get_error());
        ttl_begin = ttl_end - static_cast<ptrdiff_t>(ttl_size);
        ttl_index = 0;
        while (ttl_index < ttl_size)
        {
            const Pair<ft_string, int64_t> &ttl_entry = ttl_begin[ttl_index];
            kv_store_snapshot_entry *entry_begin;
            kv_store_snapshot_entry *entry_end;
            kv_store_snapshot_entry *entry_cursor;

            entry_begin = out_entries.begin();
            if (out_entries.get_error() != FT_ERR_SUCCESS)
                return (out_entries.get_error());
            entry_end = out_entries.end();
            if (out_entries.get_error() != FT_ERR_SUCCESS)
                return (out_entries.get_error());
            entry_cursor = entry_begin;
            while (entry_cursor != entry_end)
            {
                if (entry_cursor->key == ttl_entry.key)
                {
                    entry_cursor->has_expiration = FT_TRUE;
                    entry_cursor->expiration_timestamp = ttl_entry.value;
                    break ;
                }
                entry_cursor++;
            }
            ttl_index++;
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::load_json_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_json_entries");
    json_group *group_head;

    if (location == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    out_entries.clear();
    if (out_entries.get_error() != FT_ERR_SUCCESS)
        return (out_entries.get_error());
    group_head = json_read_from_file(location);
    if (group_head == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->parse_json_groups(group_head, out_entries) != 0)
    {
        json_free_groups(group_head);
        return (FT_ERR_INVALID_OPERATION);
    }
    json_free_groups(group_head);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::flush_json_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_json_entries");
    json_group *store_group;
    json_group *head_group;
    kv_store *mutable_this;
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;

    mutable_this = const_cast<kv_store *>(this);
    store_group = json_create_json_group("kv_store");
    if (store_group == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_encryption_enabled)
    {
        json_item *metadata_item;
        ft_string active_algorithm_name;

        if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        metadata_item = json_create_item("__encryption__", active_algorithm_name.c_str());
        if (metadata_item == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, metadata_item);
    }
    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        json_item *value_item;
        ft_string stored_value;
        const char *value_pointer;
        if (stored_value.initialize() != FT_ERR_SUCCESS)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }

        value_pointer = entry_cursor->value.c_str();
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize() != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (mutable_this->encrypt_value(entry_cursor->value, encoded_value) != 0)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            stored_value = encoded_value;
            int32_t stored_value_error = storage_kv_move_string_error(stored_value);
            if (stored_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_pointer = stored_value.c_str();
        }
        value_item = json_create_item(entry_cursor->key.c_str(), value_pointer);
        if (value_item == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, value_item);
        if (entry_cursor->has_expiration)
        {
            ft_string ttl_key;
            char expiration_buffer[32];
            int32_t written_length;
            int32_t ttl_key_error;
            if (ttl_key.initialize() != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }

            ttl_key = g_kv_store_ttl_prefix;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            ttl_key += entry_cursor->key;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            written_length = pf_snprintf(expiration_buffer, sizeof(expiration_buffer), "%ld", static_cast<long>(entry_cursor->expiration_timestamp));
            if (written_length < 0 || static_cast<ft_size_t>(written_length) >= sizeof(expiration_buffer))
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_item = json_create_item(ttl_key.c_str(), expiration_buffer);
            if (value_item == ft_nullptr)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            json_add_item_to_group(store_group, value_item);
        }
        entry_cursor++;
    }
    head_group = ft_nullptr;
    json_append_group(&head_group, store_group);
    if (json_write_to_file(this->_file_path.c_str(), head_group) != 0)
    {
        json_free_groups(head_group);
        return (FT_ERR_INVALID_OPERATION);
    }
    json_free_groups(head_group);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::flush_json_lines_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_json_lines_entries");
    su_file *file_handle;
    kv_store *mutable_this;
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;

    mutable_this = const_cast<kv_store *>(this);
    file_handle = su_fopen(this->_file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file_handle)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_encryption_enabled)
    {
        json_group *metadata_group;
        json_group *head_group;
        json_item *encryption_item;
        char *serialized_line;
        ft_size_t written_length;
        ft_string active_algorithm_name;

        metadata_group = json_create_json_group("metadata");
        if (metadata_group == ft_nullptr)
        {
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
        {
            json_free_groups(metadata_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        encryption_item = json_create_item("encryption", active_algorithm_name.c_str());
        if (encryption_item == ft_nullptr)
        {
            json_free_groups(metadata_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(metadata_group, encryption_item);
        head_group = ft_nullptr;
        json_append_group(&head_group, metadata_group);
        serialized_line = json_write_to_string(head_group);
        if (!serialized_line)
        {
            json_free_groups(head_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        ft_size_t serialized_length = ft_strlen(serialized_line);
        written_length = su_fwrite(serialized_line, 1, serialized_length, file_handle);
        if (written_length != serialized_length)
        {
            json_free_groups(head_group);
            cma_free(serialized_line);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        int32_t newline_error = storage_kv_write_newline(file_handle);
        if (newline_error != FT_ERR_SUCCESS)
        {
            json_free_groups(head_group);
            cma_free(serialized_line);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_free_groups(head_group);
        cma_free(serialized_line);
    }
    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        json_group *entry_group;
        json_group *head_group;
        json_item *item_pointer;
        ft_string stored_value;
        const char *value_pointer;
        char ttl_buffer[32];
        char *serialized_line;
        ft_size_t line_length;
        if (stored_value.initialize() != FT_ERR_SUCCESS)
        {
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }

        entry_group = json_create_json_group("entry");
        if (entry_group == ft_nullptr)
        {
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        value_pointer = entry_cursor->value.c_str();
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize() != FT_ERR_SUCCESS)
            {
                json_free_groups(entry_group);
                su_fclose(file_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(entry_group);
                su_fclose(file_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (mutable_this->encrypt_value(entry_cursor->value, encoded_value) != 0)
            {
                json_free_groups(entry_group);
                su_fclose(file_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            stored_value = encoded_value;
            int32_t stored_value_error = storage_kv_move_string_error(stored_value);
            if (stored_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(entry_group);
                su_fclose(file_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_pointer = stored_value.c_str();
        }
        item_pointer = json_create_item("key", entry_cursor->key.c_str());
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(entry_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(entry_group, item_pointer);
        item_pointer = json_create_item("value", value_pointer);
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(entry_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(entry_group, item_pointer);
        const char *has_expiration_value;

        if (entry_cursor->has_expiration)
            has_expiration_value = "FT_TRUE";
        else
            has_expiration_value = "FT_FALSE";
        item_pointer = json_create_item("has_expiration", has_expiration_value);
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(entry_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(entry_group, item_pointer);
        if (entry_cursor->has_expiration)
        {
            int32_t written_length;

            written_length = pf_snprintf(ttl_buffer, sizeof(ttl_buffer), "%ld", static_cast<long>(entry_cursor->expiration_timestamp));
            if (written_length < 0 || static_cast<ft_size_t>(written_length) >= sizeof(ttl_buffer))
            {
                json_free_groups(entry_group);
                su_fclose(file_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
        }
        else
            ttl_buffer[0] = '\0';
        item_pointer = json_create_item("expiration", ttl_buffer);
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(entry_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(entry_group, item_pointer);
        head_group = ft_nullptr;
        json_append_group(&head_group, entry_group);
        serialized_line = json_write_to_string(head_group);
        if (!serialized_line)
        {
            json_free_groups(head_group);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        line_length = ft_strlen(serialized_line);
        ft_size_t entry_written_length = su_fwrite(serialized_line, 1, line_length, file_handle);
        if (entry_written_length != line_length)
        {
            json_free_groups(head_group);
            cma_free(serialized_line);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        int32_t newline_error = storage_kv_write_newline(file_handle);
        if (newline_error != FT_ERR_SUCCESS)
        {
            json_free_groups(head_group);
            cma_free(serialized_line);
            su_fclose(file_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_free_groups(head_group);
        cma_free(serialized_line);
        entry_cursor++;
    }
    su_fclose(file_handle);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::load_json_lines_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_json_lines_entries");
    ft_vector<ft_string> lines;
    ft_size_t line_count;
    ft_size_t line_index;
    int32_t read_result;

    if (location == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    lines.clear();
    read_result = ft_open_and_read_file(location, lines, 8192);
    if (read_result != 0)
    {
        return (FT_ERR_NOT_FOUND);
    }
    out_entries.clear();
    line_count = lines.size();
    line_index = 0;
    while (line_index < line_count)
    {
        const ft_string &line_value = lines[line_index];
        json_group *line_groups;
        json_group *metadata_group;
        json_group *entry_group;

        line_groups = json_read_from_string(line_value.c_str());
        if (!line_groups)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        metadata_group = json_find_group(line_groups, "metadata");
        if (metadata_group != ft_nullptr)
        {
            json_item *encryption_item;
            ft_string active_algorithm_name;

            encryption_item = metadata_group->items;
            if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
            {
                json_free_groups(line_groups);
                return (FT_ERR_INVALID_OPERATION);
            }
            while (encryption_item != ft_nullptr)
            {
                if (ft_strcmp(encryption_item->key, "encryption") == 0)
                {
                    if (ft_strcmp(encryption_item->value, active_algorithm_name.c_str()) != 0)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }
                    if (this->_encryption_enabled == FT_FALSE)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }
                }
                encryption_item = encryption_item->next;
            }
            json_free_groups(line_groups);
            line_index++;
            continue ;
        }
        entry_group = json_find_group(line_groups, "entry");
        if (entry_group == ft_nullptr)
        {
            json_free_groups(line_groups);
            return (FT_ERR_INVALID_OPERATION);
        }
        kv_store_snapshot_entry snapshot_entry;
        json_item *item_cursor;
        ft_bool has_expiration_flag;
        int64_t expiration_value;
        if (snapshot_entry.key.initialize() != FT_ERR_SUCCESS)
        {
            json_free_groups(line_groups);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (snapshot_entry.value.initialize() != FT_ERR_SUCCESS)
        {
            json_free_groups(line_groups);
            return (FT_ERR_INVALID_OPERATION);
        }

        snapshot_entry.has_expiration = FT_FALSE;
        snapshot_entry.expiration_timestamp = 0;
        has_expiration_flag = FT_FALSE;
        expiration_value = 0;
        item_cursor = entry_group->items;
        while (item_cursor != ft_nullptr)
        {
            if (ft_strcmp(item_cursor->key, "key") == 0)
            {
                snapshot_entry.key = item_cursor->value;
                int32_t snapshot_key_error = storage_kv_move_string_error(snapshot_entry.key);
                if (snapshot_key_error != FT_ERR_SUCCESS)
                {
                    json_free_groups(line_groups);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            else if (ft_strcmp(item_cursor->key, "value") == 0)
            {
                if (this->_encryption_enabled)
                {
                    ft_string encoded_value;
                    if (encoded_value.initialize(item_cursor->value) != FT_ERR_SUCCESS)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }
                    ft_string decrypted_value;
                    if (decrypted_value.initialize() != FT_ERR_SUCCESS)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }

                    int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
                    if (encoded_value_error != FT_ERR_SUCCESS)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }
                    if (this->decrypt_value(encoded_value, decrypted_value) != 0)
                    {
                        json_free_groups(line_groups);
                        return (FT_ERR_INVALID_OPERATION);
                    }
                    snapshot_entry.value = decrypted_value;
                }
                else
                    snapshot_entry.value = item_cursor->value;
                int32_t snapshot_value_error = storage_kv_move_string_error(snapshot_entry.value);
                if (snapshot_value_error != FT_ERR_SUCCESS)
                {
                    json_free_groups(line_groups);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            else if (ft_strcmp(item_cursor->key, "has_expiration") == 0)
            {
                if (ft_strcmp(item_cursor->value, "FT_TRUE") == 0)
                    has_expiration_flag = FT_TRUE;
                else
                    has_expiration_flag = FT_FALSE;
            }
            else if (ft_strcmp(item_cursor->key, "expiration") == 0)
            {
                if (ft_strlen(item_cursor->value) > 0)
                    expiration_value = storage_kv_parse_long_long(item_cursor->value, ft_nullptr, 10);
                else
                    expiration_value = 0;
            }
            item_cursor = item_cursor->next;
        }
        if (snapshot_entry.key.c_str() == ft_nullptr)
        {
            json_free_groups(line_groups);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (has_expiration_flag != 0)
            snapshot_entry.has_expiration = FT_TRUE;
        else
            snapshot_entry.has_expiration = FT_FALSE;
        snapshot_entry.expiration_timestamp = expiration_value;
        out_entries.push_back(ft_move(snapshot_entry));
        json_free_groups(line_groups);
        line_index++;
    }
    return (FT_ERR_SUCCESS);
}

#if SQLITE3_AVAILABLE
int32_t kv_store::flush_sqlite_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_sqlite_entries");
    sqlite3 *database_handle;
    kv_store *mutable_this;
    char *error_message;
    sqlite3_stmt *insert_statement;
    int32_t statement_result;
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;

    mutable_this = const_cast<kv_store *>(this);
    database_handle = ft_nullptr;
    error_message = ft_nullptr;
    insert_statement = ft_nullptr;
    if (sqlite3_open(this->_file_path.c_str(), &database_handle) != SQLITE_OK)
    {
        if (database_handle)
            sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "BEGIN IMMEDIATE TRANSACTION;", ft_nullptr, ft_nullptr, &error_message) != SQLITE_OK)
    {
        if (error_message)
            sqlite3_free(error_message);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "CREATE TABLE IF NOT EXISTS kv_store_entries (key TEXT PRIMARY KEY, value TEXT NOT NULL, has_expiration INTEGER NOT NULL, expiration INTEGER NOT NULL);", ft_nullptr, ft_nullptr, &error_message) != SQLITE_OK)
    {
        if (error_message)
            sqlite3_free(error_message);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "CREATE TABLE IF NOT EXISTS kv_store_metadata (id INTEGER PRIMARY KEY CHECK(id=1), encryption INTEGER NOT NULL);", ft_nullptr, ft_nullptr, &error_message) != SQLITE_OK)
    {
        if (error_message)
            sqlite3_free(error_message);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "DELETE FROM kv_store_entries;", ft_nullptr, ft_nullptr, &error_message) != SQLITE_OK)
    {
        if (error_message)
            sqlite3_free(error_message);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    sqlite3_stmt *metadata_statement;
    metadata_statement = ft_nullptr;
    if (sqlite3_prepare_v2(database_handle, "INSERT INTO kv_store_metadata (id, encryption) VALUES (1, ?) ON CONFLICT(id) DO UPDATE SET encryption=excluded.encryption;", -1, &metadata_statement, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    int32_t encryption_flag;

    if (this->_encryption_enabled)
        encryption_flag = 1;
    else
        encryption_flag = 0;
    if (sqlite3_bind_int(metadata_statement, 1, encryption_flag) != SQLITE_OK)
    {
        sqlite3_finalize(metadata_statement);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_step(metadata_statement) != SQLITE_DONE)
    {
        sqlite3_finalize(metadata_statement);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    sqlite3_finalize(metadata_statement);
    metadata_statement = ft_nullptr;
    if (sqlite3_prepare_v2(database_handle, "INSERT INTO kv_store_entries (key, value, has_expiration, expiration) VALUES (?, ?, ?, ?);", -1, &insert_statement, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        ft_string stored_value;
        const char *value_pointer;
        if (stored_value.initialize() != FT_ERR_SUCCESS)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }

        value_pointer = entry_cursor->value.c_str();
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize() != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(insert_statement);
                sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(insert_statement);
                sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (mutable_this->encrypt_value(entry_cursor->value, encoded_value) != 0)
            {
                sqlite3_finalize(insert_statement);
                sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            stored_value = encoded_value;
            int32_t stored_value_error = storage_kv_move_string_error(stored_value);
            if (stored_value_error != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(insert_statement);
                sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_pointer = stored_value.c_str();
        }
        sqlite3_reset(insert_statement);
        sqlite3_clear_bindings(insert_statement);
        if (sqlite3_bind_text(insert_statement, 1, entry_cursor->key.c_str(), -1, SQLITE_STATIC) != SQLITE_OK)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (sqlite3_bind_text(insert_statement, 2, value_pointer, -1, SQLITE_STATIC) != SQLITE_OK)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        int32_t entry_has_expiration;

        if (entry_cursor->has_expiration)
            entry_has_expiration = 1;
        else
            entry_has_expiration = 0;
        if (sqlite3_bind_int(insert_statement, 3, entry_has_expiration) != SQLITE_OK)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (sqlite3_bind_int64(insert_statement, 4, static_cast<sqlite3_int64>(entry_cursor->expiration_timestamp)) != SQLITE_OK)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        statement_result = sqlite3_step(insert_statement);
        if (statement_result != SQLITE_DONE)
        {
            sqlite3_finalize(insert_statement);
            sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        entry_cursor++;
    }
    sqlite3_finalize(insert_statement);
    insert_statement = ft_nullptr;
    if (sqlite3_exec(database_handle, "COMMIT;", ft_nullptr, ft_nullptr, &error_message) != SQLITE_OK)
    {
        if (error_message)
            sqlite3_free(error_message);
        sqlite3_exec(database_handle, "ROLLBACK;", ft_nullptr, ft_nullptr, ft_nullptr);
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    sqlite3_close(database_handle);
    return (FT_ERR_SUCCESS);
}

#endif

#if SQLITE3_AVAILABLE
int32_t kv_store::load_sqlite_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_sqlite_entries");
    sqlite3 *database_handle;
    sqlite3_stmt *metadata_statement;
    sqlite3_stmt *select_statement;
    int32_t prepare_result;

    database_handle = ft_nullptr;
    metadata_statement = ft_nullptr;
    select_statement = ft_nullptr;
    if (sqlite3_open(location, &database_handle) != SQLITE_OK)
    {
        if (database_handle)
            sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "CREATE TABLE IF NOT EXISTS kv_store_entries (key TEXT PRIMARY KEY, value TEXT NOT NULL, has_expiration INTEGER NOT NULL, expiration INTEGER NOT NULL);", ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (sqlite3_exec(database_handle, "CREATE TABLE IF NOT EXISTS kv_store_metadata (id INTEGER PRIMARY KEY CHECK(id=1), encryption INTEGER NOT NULL);", ft_nullptr, ft_nullptr, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    prepare_result = sqlite3_prepare_v2(database_handle, "SELECT encryption FROM kv_store_metadata WHERE id = 1;", -1, &metadata_statement, ft_nullptr);
    if (prepare_result == SQLITE_OK)
    {
        if (sqlite3_step(metadata_statement) == SQLITE_ROW)
        {
            int32_t encryption_flag;

            encryption_flag = sqlite3_column_int(metadata_statement, 0);
            if ((encryption_flag != 0 && this->_encryption_enabled == FT_FALSE)
                || (encryption_flag == 0 && this->_encryption_enabled == FT_TRUE))
            {
                sqlite3_finalize(metadata_statement);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
        }
        sqlite3_finalize(metadata_statement);
        metadata_statement = ft_nullptr;
    }
    out_entries.clear();
    if (sqlite3_prepare_v2(database_handle, "SELECT key, value, has_expiration, expiration FROM kv_store_entries;", -1, &select_statement, ft_nullptr) != SQLITE_OK)
    {
        sqlite3_close(database_handle);
        return (FT_ERR_INVALID_OPERATION);
    }
    while (FT_TRUE)
    {
        int32_t step_result;

        step_result = sqlite3_step(select_statement);
        if (step_result == SQLITE_DONE)
            break ;
        if (step_result != SQLITE_ROW)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        const unsigned char *key_text;
        const unsigned char *value_text;
        int32_t has_expiration_flag;
        sqlite3_int64 expiration_value;
        kv_store_snapshot_entry snapshot_entry;
        if (snapshot_entry.key.initialize() != FT_ERR_SUCCESS)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (snapshot_entry.value.initialize() != FT_ERR_SUCCESS)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }

        key_text = sqlite3_column_text(select_statement, 0);
        value_text = sqlite3_column_text(select_statement, 1);
        has_expiration_flag = sqlite3_column_int(select_statement, 2);
        expiration_value = sqlite3_column_int64(select_statement, 3);
        if (!key_text || !value_text)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        snapshot_entry.key = reinterpret_cast<const char *>(key_text);
        int32_t snapshot_key_error = storage_kv_move_string_error(snapshot_entry.key);
        if (snapshot_key_error != FT_ERR_SUCCESS)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize(reinterpret_cast<const char *>(value_text)) != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(select_statement);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            ft_string decrypted_value;
            if (decrypted_value.initialize() != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(select_statement);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }

            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                sqlite3_finalize(select_statement);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (this->decrypt_value(encoded_value, decrypted_value) != 0)
            {
                sqlite3_finalize(select_statement);
                sqlite3_close(database_handle);
                return (FT_ERR_INVALID_OPERATION);
            }
            snapshot_entry.value = decrypted_value;
        }
        else
            snapshot_entry.value = reinterpret_cast<const char *>(value_text);
        int32_t snapshot_value_error = storage_kv_move_string_error(snapshot_entry.value);
        if (snapshot_value_error != FT_ERR_SUCCESS)
        {
            sqlite3_finalize(select_statement);
            sqlite3_close(database_handle);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (has_expiration_flag != 0)
            snapshot_entry.has_expiration = FT_TRUE;
        else
            snapshot_entry.has_expiration = FT_FALSE;
        snapshot_entry.expiration_timestamp = static_cast<int64_t>(expiration_value);
        out_entries.push_back(ft_move(snapshot_entry));
    }
    sqlite3_finalize(select_statement);
    sqlite3_close(database_handle);
    return (FT_ERR_SUCCESS);
}

#endif

int32_t kv_store::flush_memory_mapped_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_memory_mapped_entries");
    kv_store *mutable_this;
    json_group *store_group;
    json_group *head_group;
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;
    char *serialized_buffer;
    ft_size_t serialized_length;

    mutable_this = const_cast<kv_store *>(this);
    store_group = json_create_json_group("kv_store");
    if (store_group == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_encryption_enabled)
    {
        json_item *metadata_item;
        ft_string active_algorithm_name;

        if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        metadata_item = json_create_item("__encryption__", active_algorithm_name.c_str());
        if (metadata_item == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, metadata_item);
    }
    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        json_item *item_pointer;
        ft_string stored_value;
        const char *value_pointer;
        char ttl_buffer[32];
        if (stored_value.initialize() != FT_ERR_SUCCESS)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }

        value_pointer = entry_cursor->value.c_str();
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;
            if (encoded_value.initialize() != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (mutable_this->encrypt_value(entry_cursor->value, encoded_value) != 0)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            stored_value = encoded_value;
            int32_t stored_value_error = storage_kv_move_string_error(stored_value);
            if (stored_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_pointer = stored_value.c_str();
        }
        item_pointer = json_create_item(entry_cursor->key.c_str(), value_pointer);
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, item_pointer);
        if (entry_cursor->has_expiration)
        {
            int32_t written_length;
            ft_string ttl_key;
            int32_t ttl_key_error;
            if (ttl_key.initialize() != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }

            written_length = pf_snprintf(ttl_buffer, sizeof(ttl_buffer), "%ld", static_cast<long>(entry_cursor->expiration_timestamp));
            if (written_length < 0 || static_cast<ft_size_t>(written_length) >= sizeof(ttl_buffer))
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            ttl_key = g_kv_store_ttl_prefix;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            ttl_key += entry_cursor->key;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            item_pointer = json_create_item(ttl_key.c_str(), ttl_buffer);
            if (item_pointer == ft_nullptr)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            json_add_item_to_group(store_group, item_pointer);
        }
        entry_cursor++;
    }
    head_group = ft_nullptr;
    json_append_group(&head_group, store_group);
    serialized_buffer = json_write_to_string(head_group);
    if (!serialized_buffer)
    {
        json_free_groups(head_group);
        return (FT_ERR_INVALID_OPERATION);
    }
    serialized_length = ft_strlen(serialized_buffer);
    int32_t write_result = cmp_storage_write_memory_mapped_file(
        this->_file_path.c_str(), serialized_buffer, serialized_length);
    json_free_groups(head_group);
    cma_free(serialized_buffer);
    return (write_result);
}

int32_t kv_store::load_memory_mapped_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_memory_mapped_entries");
    ft_size_t buffer_length;
    char *buffer_pointer;
    json_group *group_head;
    int32_t read_error;

    read_error = cmp_storage_read_memory_mapped_file(location, &buffer_pointer,
        &buffer_length);
    (void)buffer_length;
    if (read_error != FT_ERR_SUCCESS)
        return (read_error);
    group_head = json_read_from_string(buffer_pointer);
    cma_free(buffer_pointer);
    if (!group_head)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->parse_json_groups(group_head, out_entries) != 0)
    {
        json_free_groups(group_head);
        return (FT_ERR_INVALID_OPERATION);
    }
    json_free_groups(group_head);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::flush_backend_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_backend_entries");
    if (this->_backend_type == KV_STORE_BACKEND_JSON)
        return (this->flush_json_entries(entries));
    if (this->_backend_type == KV_STORE_BACKEND_JSON_LINES)
        return (this->flush_json_lines_entries(entries));
#if SQLITE3_AVAILABLE
    if (this->_backend_type == KV_STORE_BACKEND_SQLITE)
        return (this->flush_sqlite_entries(entries));
#endif
    if (this->_backend_type == KV_STORE_BACKEND_MEMORY_MAPPED)
        return (this->flush_memory_mapped_entries(entries));
    return (FT_ERR_INVALID_OPERATION);
}

int32_t kv_store::load_backend_entries(kv_store_backend_type backend_type, const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_backend_entries");
    if (backend_type == KV_STORE_BACKEND_JSON)
        return (this->load_json_entries(location, out_entries));
    if (backend_type == KV_STORE_BACKEND_JSON_LINES)
        return (this->load_json_lines_entries(location, out_entries));
#if SQLITE3_AVAILABLE
    if (backend_type == KV_STORE_BACKEND_SQLITE)
        return (this->load_sqlite_entries(location, out_entries));
#endif
    if (backend_type == KV_STORE_BACKEND_MEMORY_MAPPED)
        return (this->load_memory_mapped_entries(location, out_entries));
    return (FT_ERR_INVALID_OPERATION);
}

int32_t kv_store::set_backend(kv_store_backend_type backend_type, const char *location)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::set_backend");
    ft_vector<kv_store_snapshot_entry> loaded_entries;
    ft_string location_copy;
    int32_t load_result;
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;

    if (location == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    location_copy = location;
    int32_t location_copy_error = storage_kv_move_string_error(location_copy);
    if (location_copy_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    loaded_entries.clear();
    load_result = this->load_backend_entries(backend_type, location, loaded_entries);
    if (load_result < 0)
        return (FT_ERR_INVALID_OPERATION);
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    ft_string previous_path;
    int32_t previous_path_error = previous_path.initialize(this->_file_path);
    if (previous_path_error != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_mutex, lock_acquired);
        return (FT_ERR_INVALID_OPERATION);
    }
    previous_path_error = storage_kv_move_string_error(previous_path);
    if (previous_path_error != FT_ERR_SUCCESS)
    {
        kv_store_finalize_lock(this->_mutex, lock_acquired);
        return (FT_ERR_INVALID_OPERATION);
    }
    kv_store_backend_type previous_backend;

    previous_backend = this->_backend_type;
    this->_file_path = location_copy;
    int32_t path_error = storage_kv_move_string_error(this->_file_path);
    if (path_error != FT_ERR_SUCCESS)
    {
        this->_file_path = previous_path;
        int32_t revert_error = storage_kv_move_string_error(this->_file_path);
        if (revert_error != FT_ERR_SUCCESS)
            this->_file_path.clear();
        this->_backend_type = previous_backend;
        kv_store_finalize_lock(this->_mutex, lock_acquired);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_backend_type = backend_type;
    if (load_result == 0)
    {
        if (this->apply_snapshot_locked(loaded_entries) != 0)
        {
            this->_file_path = previous_path;
            int32_t revert_error = storage_kv_move_string_error(this->_file_path);
            if (revert_error != FT_ERR_SUCCESS)
                this->_file_path.clear();
            this->_backend_type = previous_backend;
            kv_store_finalize_lock(this->_mutex, lock_acquired);
            return (FT_ERR_INVALID_OPERATION);
        }
    }
    kv_store_finalize_lock(this->_mutex, lock_acquired);
    return (FT_ERR_SUCCESS);
}

kv_store_backend_type kv_store::get_backend() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::get_backend");
    return (this->_backend_type);
}

int32_t kv_store::write_snapshot(ft_document_sink &sink) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::write_snapshot");
    kv_store *mutable_this;
    ft_vector<kv_store_snapshot_entry> entries;
    json_group *store_group;
    json_group *head_group;
    char *serialized_buffer;
    int32_t write_result;

    mutable_this = const_cast<kv_store *>(this);
    entries.clear();
    if (mutable_this->export_snapshot(entries) != 0)
        return (FT_ERR_INVALID_OPERATION);
    store_group = json_create_json_group("kv_store");
    if (store_group == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_encryption_enabled)
    {
        json_item *metadata_item;
        ft_string active_algorithm_name;

        if (this->get_active_encryption_algorithm_name(active_algorithm_name) != FT_ERR_SUCCESS)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        metadata_item = json_create_item("__encryption__", active_algorithm_name.c_str());
        if (metadata_item == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, metadata_item);
    }
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;

    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        json_item *item_pointer;
        ft_string stored_value;
        const char *value_pointer;

        value_pointer = entry_cursor->value.c_str();
        if (this->_encryption_enabled)
        {
            ft_string encoded_value;

            encoded_value = ft_string();
            int32_t encoded_value_error = storage_kv_move_string_error(encoded_value);
            if (encoded_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (mutable_this->encrypt_value(entry_cursor->value, encoded_value) != 0)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            stored_value = encoded_value;
            int32_t stored_value_error = storage_kv_move_string_error(stored_value);
            if (stored_value_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            value_pointer = stored_value.c_str();
        }
        item_pointer = json_create_item(entry_cursor->key.c_str(), value_pointer);
        if (item_pointer == ft_nullptr)
        {
            json_free_groups(store_group);
            return (FT_ERR_INVALID_OPERATION);
        }
        json_add_item_to_group(store_group, item_pointer);
        if (entry_cursor->has_expiration)
        {
            ft_string ttl_key;
            char expiration_buffer[32];
            int32_t written_length;
            int32_t ttl_key_error;

            ttl_key = g_kv_store_ttl_prefix;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            ttl_key += entry_cursor->key;
            ttl_key_error = storage_kv_move_string_error(ttl_key);
            if (ttl_key_error != FT_ERR_SUCCESS)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            written_length = pf_snprintf(expiration_buffer, sizeof(expiration_buffer), "%ld", static_cast<long>(entry_cursor->expiration_timestamp));
            if (written_length < 0 || static_cast<ft_size_t>(written_length) >= sizeof(expiration_buffer))
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            item_pointer = json_create_item(ttl_key.c_str(), expiration_buffer);
            if (item_pointer == ft_nullptr)
            {
                json_free_groups(store_group);
                return (FT_ERR_INVALID_OPERATION);
            }
            json_add_item_to_group(store_group, item_pointer);
        }
        entry_cursor++;
    }
    head_group = ft_nullptr;
    json_append_group(&head_group, store_group);
    serialized_buffer = json_write_to_string(head_group);
    if (!serialized_buffer)
    {
        json_free_groups(head_group);
        return (FT_ERR_INVALID_OPERATION);
    }
    write_result = sink.write_all(serialized_buffer, ft_strlen(serialized_buffer));
    if (write_result != 0)
    {
        json_free_groups(head_group);
        cma_free(serialized_buffer);
        return (FT_ERR_INVALID_OPERATION);
    }
    json_free_groups(head_group);
    cma_free(serialized_buffer);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::read_snapshot(ft_document_source &source)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::read_snapshot");
    ft_string content;
    int32_t read_result;
    json_group *group_head;
    ft_vector<kv_store_snapshot_entry> entries;
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;

    content = ft_string();
    int32_t content_error = storage_kv_move_string_error(content);
    if (content_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    read_result = source.read_all(content);
    if (read_result != 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    group_head = json_read_from_string(content.c_str());
    if (!group_head)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    entries.clear();
    if (this->parse_json_groups(group_head, entries) != 0)
    {
        json_free_groups(group_head);
        return (FT_ERR_INVALID_OPERATION);
    }
    json_free_groups(group_head);
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->apply_snapshot_locked(entries) != 0)
        return (FT_ERR_INVALID_OPERATION);
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_init_set_operation(kv_store_operation &operation, const char *key_string, const char *value_string, int64_t ttl_seconds)
{
    int32_t key_initialize_error;
    int32_t value_initialize_error;

    if (key_string == ft_nullptr || value_string == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    key_initialize_error = operation._key.initialize();
    if (key_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    value_initialize_error = operation._value.initialize();
    if (value_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    operation._type = KV_STORE_OPERATION_TYPE_SET;
    operation._key = key_string;
    if (storage_kv_move_string_error(operation._key) != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operation._value = value_string;
    if (storage_kv_move_string_error(operation._value) != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operation._has_value = FT_TRUE;
    if (ttl_seconds >= 0)
    {
        operation._has_ttl = FT_TRUE;
        operation._ttl_seconds = ttl_seconds;
    }
    else
    {
        operation._has_ttl = FT_FALSE;
        operation._ttl_seconds = -1;
    }
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_init_delete_operation(kv_store_operation &operation, const char *key_string)
{
    int32_t key_initialize_error;
    int32_t value_initialize_error;

    if (key_string == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    key_initialize_error = operation._key.initialize();
    if (key_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    value_initialize_error = operation._value.initialize();
    if (value_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    operation._type = KV_STORE_OPERATION_TYPE_DELETE;
    operation._key = key_string;
    if (storage_kv_move_string_error(operation._key) != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operation._value.clear();
    operation._has_value = FT_FALSE;
    operation._has_ttl = FT_FALSE;
    operation._ttl_seconds = -1;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::lock_store(ft_bool *lock_acquired) const noexcept
{
    ft_bool has_mutex;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::lock_store");
    if (lock_acquired == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    has_mutex = (this->_mutex != ft_nullptr);
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    *lock_acquired = has_mutex;
    return (FT_ERR_SUCCESS);
}

void kv_store::unlock_store_guard(ft_bool lock_acquired, int32_t) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::unlock_store_guard");
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int64_t kv_store::current_time_seconds() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::current_time_seconds");
    t_time current_time;

    current_time = time_now();
    return (current_time);
}

int32_t kv_store::compute_expiration(int64_t ttl_seconds, ft_bool &has_expiration, int64_t &expiration_timestamp) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::compute_expiration");
    int64_t current_time;

    has_expiration = FT_FALSE;
    expiration_timestamp = 0;
    if (ttl_seconds < 0)
        return (FT_ERR_SUCCESS);
    current_time = this->current_time_seconds();
    if (current_time < 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    has_expiration = FT_TRUE;
    if (ttl_seconds > LLONG_MAX - current_time)
        expiration_timestamp = LLONG_MAX;
    else
        expiration_timestamp = current_time + ttl_seconds;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::parse_expiration_timestamp(const char *value_string, int64_t &expiration_timestamp) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::parse_expiration_timestamp");
    char *end_pointer;
    int64_t parsed_value;

    if (value_string == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    errno = 0;
    parsed_value = storage_kv_parse_long_long(value_string, &end_pointer, 10);
    if (errno != 0 || end_pointer == value_string || *end_pointer != '\0')
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    expiration_timestamp = parsed_value;
    return (FT_ERR_SUCCESS);
}

void kv_store::record_set_operation() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::record_set_operation");
    this->_metrics_set_operations = this->_metrics_set_operations + 1;
    return ;
}

void kv_store::record_delete_operation() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::record_delete_operation");
    this->_metrics_delete_operations = this->_metrics_delete_operations + 1;
    return ;
}

void kv_store::record_get_hit() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::record_get_hit");
    this->_metrics_get_hits = this->_metrics_get_hits + 1;
    return ;
}

void kv_store::record_get_miss() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::record_get_miss");
    this->_metrics_get_misses = this->_metrics_get_misses + 1;
    return ;
}

void kv_store::record_prune_metrics(int64_t removed_entries, int64_t duration_ms) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::record_prune_metrics");
    int64_t normalized_duration;

    normalized_duration = duration_ms;
    if (normalized_duration < 0)
        normalized_duration = 0;
    this->_metrics_prune_operations = this->_metrics_prune_operations + 1;
    this->_metrics_pruned_entries = this->_metrics_pruned_entries + removed_entries;
    this->_metrics_last_prune_duration_ms = normalized_duration;
    this->_metrics_total_prune_duration_ms = this->_metrics_total_prune_duration_ms + normalized_duration;
    return ;
}

int32_t kv_store::lock_background(ft_bool *lock_acquired) const noexcept
{
    ft_bool has_mutex;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::lock_background");
    if (lock_acquired == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *lock_acquired = FT_FALSE;
    has_mutex = (this->_background_mutex != ft_nullptr);
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_background_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    *lock_acquired = has_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::start_background_thread_locked(int64_t interval_seconds) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::start_background_thread_locked");
    ft_thread background_instance;

    if (interval_seconds <= 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_background_thread_active)
    {
        this->_background_interval_seconds = interval_seconds;
        return (FT_ERR_SUCCESS);
    }
    this->_background_stop_requested = FT_FALSE;
    this->_background_interval_seconds = interval_seconds;
    background_instance = ft_thread(&kv_store::background_compaction_worker, this);
    this->_background_thread = ft_move(background_instance);
    this->_background_thread_active = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void kv_store::stop_background_thread_locked(ft_thread &thread_holder) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::stop_background_thread_locked");
    if (this->_background_thread_active == FT_FALSE)
    {
        return ;
    }
    this->_background_stop_requested = FT_TRUE;
    this->_background_interval_seconds = 0;
    thread_holder = ft_move(this->_background_thread);
    this->_background_thread_active = FT_FALSE;
    return ;
}

void kv_store::background_compaction_worker(kv_store *store) noexcept
{
    if (store == ft_nullptr)
        return ;
    while (FT_TRUE)
    {
        ft_bool lock_acquired = FT_FALSE;
        ft_bool stop_requested;
        int64_t interval_seconds;
        int64_t sleep_milliseconds;

        if (store->lock_background(&lock_acquired) != FT_ERR_SUCCESS)
            return ;
        stop_requested = store->_background_stop_requested;
        interval_seconds = store->_background_interval_seconds;
        if (lock_acquired)
            (void)pt_recursive_mutex_unlock_if_not_null(store->_background_mutex);
        if (stop_requested)
            break ;
        if (interval_seconds <= 0)
            interval_seconds = 1;
        if (interval_seconds > FT_LLONG_MAX / 1000)
            interval_seconds = FT_LLONG_MAX / 1000;
        sleep_milliseconds = interval_seconds * 1000;
        if (sleep_milliseconds <= 0)
            sleep_milliseconds = 1000;
        if (sleep_milliseconds > static_cast<int64_t>(UINT_MAX))
            sleep_milliseconds = static_cast<int64_t>(UINT_MAX);
        pt_thread_sleep(static_cast<uint32_t>(sleep_milliseconds));
        store->prune_expired();
    }
    return ;
}

int32_t kv_store::prune_expired()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::prune_expired");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;
    int32_t prune_result;

    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    prune_result = this->prune_expired_locked();
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (prune_result == 0)
        return (prune_result);
    return (FT_ERR_INVALID_OPERATION);
}

int32_t kv_store::prune_expired_locked()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::prune_expired_locked");
    ft_size_t map_size;
    int64_t current_time;
    ft_size_t map_index;
    t_monotonic_time_point start_time;
    int64_t removed_entries;
    int32_t map_error;

    start_time = time_monotonic_point_now();
    removed_entries = 0;
    map_size = this->_data.size();
    map_error = FT_ERR_SUCCESS;
    if (map_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (map_size == 0)
    {
        return (FT_ERR_SUCCESS);
    }
    current_time = this->current_time_seconds();
    if (current_time < 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    map_index = 0;
    while (map_index < map_size)
    {
        Pair<ft_string, kv_store_entry> *map_end;
        Pair<ft_string, kv_store_entry> *map_begin;
        Pair<ft_string, kv_store_entry> *entry_pointer;
        ft_bool has_expiration;

        map_end = this->_data.end();
        map_error = FT_ERR_SUCCESS;
        if (map_error != FT_ERR_SUCCESS)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        map_begin = map_end - static_cast<ptrdiff_t>(map_size);
        entry_pointer = map_begin + map_index;
        if (entry_pointer == ft_nullptr)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        has_expiration = FT_FALSE;
        if (entry_pointer->value.has_expiration(has_expiration) != 0)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        if (has_expiration)
        {
            int64_t expiration_timestamp;

            if (entry_pointer->value.get_expiration(expiration_timestamp) != 0)
            {
                return (FT_ERR_INVALID_OPERATION);
            }
            if (expiration_timestamp <= current_time)
            {
                this->_data.remove(entry_pointer->key);
                map_error = FT_ERR_SUCCESS;
                if (map_error != FT_ERR_SUCCESS)
                {
                    return (FT_ERR_INVALID_OPERATION);
                }
                map_size = this->_data.size();
                map_error = FT_ERR_SUCCESS;
                if (map_error != FT_ERR_SUCCESS)
                {
                    return (FT_ERR_INVALID_OPERATION);
                }
                removed_entries = removed_entries + 1;
                continue ;
            }
        }
        map_index++;
    }
    t_monotonic_time_point finish_time;
    int64_t duration_ms;

    finish_time = time_monotonic_point_now();
    duration_ms = time_monotonic_point_diff_ms(start_time, finish_time);
    this->record_prune_metrics(removed_entries, duration_ms);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::kv_set(const char *key_string, const char *value_string, int64_t ttl_seconds)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_set");
    ft_vector<kv_store_operation> operations;
    kv_store_operation operation;
    int32_t operations_initialize_error;
    int32_t operations_error;

    if (key_string == ft_nullptr || value_string == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operations_initialize_error = operations.initialize();
    if (operations_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    operations.clear();
    if (kv_store_init_set_operation(operation, key_string, value_string, ttl_seconds) != 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operations.push_back(operation);
    operations_error = operations.get_error();
    if (operations_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (this->kv_apply(operations));
}

const char *kv_store::kv_get(const char *key_string) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_get");
    ft_string key_storage;
    const Pair<ft_string, kv_store_entry> *map_pair;
    const Pair<ft_string, kv_store_entry> *map_end;
    ft_bool lock_acquired = FT_FALSE;
    kv_store *mutable_this;
    int32_t lock_error;
    int32_t map_error;
    const char *value_pointer;

    if (key_string == ft_nullptr)
    {
        return (ft_nullptr);
    }
    if (key_storage.initialize() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    key_storage = key_string;
    int32_t key_storage_error = storage_kv_move_string_error(key_storage);
    if (key_storage_error != FT_ERR_SUCCESS)
    {
        return (ft_nullptr);
    }
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        mutable_this = const_cast<kv_store *>(this);
        return (ft_nullptr);
    }
    mutable_this = const_cast<kv_store *>(this);
    if (mutable_this->prune_expired_locked() != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (ft_nullptr);
    }
    map_pair = this->_data.find(key_storage);
    map_error = FT_ERR_SUCCESS;
    if (map_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, map_error);
        return (ft_nullptr);
    }
    map_end = this->_data.end();
    map_error = FT_ERR_SUCCESS;
    if (map_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, map_error);
        return (ft_nullptr);
    }
    if (map_pair == map_end)
    {
        mutable_this->record_get_miss();
        this->unlock_store_guard(lock_acquired, FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    value_pointer = ft_nullptr;
    if (map_pair->value.get_value_pointer(&value_pointer) != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (ft_nullptr);
    }
    mutable_this->record_get_hit();
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (value_pointer);
}

ft_bool kv_store::kv_contains(const char *key_string) const
{
    if (key_string == ft_nullptr)
        return (FT_FALSE);
    if (this->kv_get(key_string) == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}

int32_t kv_store::kv_delete(const char *key_string)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_delete");
    ft_vector<kv_store_operation> operations;
    kv_store_operation operation;
    ft_string key_value;
    ft_bool lock_acquired;
    Pair<ft_string, kv_store_entry> *existing_pair;
    Pair<ft_string, kv_store_entry> *map_end;
    int32_t operations_initialize_error;
    int32_t operations_error;
    int32_t lock_error;

    if (key_string == ft_nullptr)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operations_error = key_value.initialize(key_string);
    if (operations_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, lock_error);
        return (FT_ERR_INVALID_OPERATION);
    }
    existing_pair = this->_data.find(key_value);
    map_end = this->_data.end();
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    if (existing_pair == map_end)
        return (FT_ERR_SUCCESS);
    operations_initialize_error = operations.initialize();
    if (operations_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    operations.clear();
    if (kv_store_init_delete_operation(operation, key_string) != 0)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    operations.push_back(operation);
    operations_error = operations.get_error();
    if (operations_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (this->kv_apply(operations));
}

int32_t kv_store::kv_flush() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_flush");
    kv_store *mutable_this;
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;
    int32_t flush_result;

    mutable_this = const_cast<kv_store *>(this);
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    if (mutable_this->prune_expired_locked() != 0)
        return (FT_ERR_INVALID_OPERATION);
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    flush_result = 0;
    {
        ft_vector<kv_store_snapshot_entry> entries;
        int32_t entries_initialize_error;
        int32_t entries_error;

        entries_initialize_error = entries.initialize();
        if (entries_initialize_error != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        entries.clear();
        entries_error = entries.get_error();
        if (entries_error != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        if (mutable_this->export_snapshot(entries) != 0)
            return (FT_ERR_INVALID_OPERATION);
        flush_result = this->flush_backend_entries(entries);
    }
    if (flush_result != 0)
        return (flush_result);
    return (flush_result);
}

int32_t kv_store::get_metrics(kv_store_metrics &out_metrics) const
{
    kv_store *mutable_this;
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::get_metrics");
    mutable_this = const_cast<kv_store *>(this);
    lock_acquired = FT_FALSE;
    lock_error = mutable_this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    out_metrics.set_operations = this->_metrics_set_operations;
    out_metrics.delete_operations = this->_metrics_delete_operations;
    out_metrics.get_hits = this->_metrics_get_hits;
    out_metrics.get_misses = this->_metrics_get_misses;
    out_metrics.prune_operations = this->_metrics_prune_operations;
    out_metrics.pruned_entries = this->_metrics_pruned_entries;
    out_metrics.total_prune_duration_ms = this->_metrics_total_prune_duration_ms;
    out_metrics.last_prune_duration_ms = this->_metrics_last_prune_duration_ms;
    mutable_this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::export_snapshot(ft_vector<kv_store_snapshot_entry> &out_entries) const
{
    kv_store *mutable_this;
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t entry_count;
    ft_size_t entry_index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::export_snapshot");
    mutable_this = const_cast<kv_store *>(this);
    lock_acquired = FT_FALSE;
    lock_error = mutable_this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (mutable_this->prune_expired_locked() != 0)
    {
        mutable_this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    out_entries.clear();
    entry_count = this->_data.size();
    entry_index = 0;
    while (entry_index < entry_count)
    {
        const Pair<ft_string, kv_store_entry> *map_end;
        const Pair<ft_string, kv_store_entry> *map_begin;
        const Pair<ft_string, kv_store_entry> *map_entry;
        kv_store_snapshot_entry snapshot_entry;

        map_end = this->_data.end();
        map_begin = map_end - static_cast<ptrdiff_t>(entry_count);
        map_entry = map_begin + entry_index;
        snapshot_entry.key = map_entry->key;
        if (snapshot_entry.key.c_str() == ft_nullptr)
        {
            mutable_this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (map_entry->value.copy_value(snapshot_entry.value) != 0)
        {
            mutable_this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (map_entry->value.has_expiration(snapshot_entry.has_expiration) != 0)
        {
            mutable_this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        snapshot_entry.expiration_timestamp = 0;
        if (snapshot_entry.has_expiration)
        {
            if (map_entry->value.get_expiration(snapshot_entry.expiration_timestamp) != 0)
            {
                mutable_this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
        }
        out_entries.push_back(snapshot_entry);
        entry_index++;
    }
    mutable_this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::export_snapshot_to_file(const char *file_path) const
{
    ft_file_document_sink file_sink;
    int32_t initialize_error;
    int32_t write_error;
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::export_snapshot_to_file");
    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    initialize_error = file_sink.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    file_sink.set_path(file_path);
    write_error = this->write_snapshot(file_sink);
    destroy_error = file_sink.destroy();
    if (write_error != FT_ERR_SUCCESS)
        return (write_error);
    return (destroy_error);
}

int32_t kv_store::kv_apply(const ft_vector<kv_store_operation> &operations)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_apply");
    ft_bool lock_acquired = FT_FALSE;
    ft_size_t operation_count;
    ft_size_t operation_index;
    int32_t lock_error;
    ft_map<ft_string, kv_store_entry> data_snapshot;
    int32_t snapshot_initialize_error;
    int64_t metrics_set_operations_snapshot;
    int64_t metrics_delete_operations_snapshot;

    snapshot_initialize_error = data_snapshot.initialize();
    if (snapshot_initialize_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, lock_error);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->prune_expired_locked() != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    data_snapshot = this->_data;
    if (data_snapshot.get_error() != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    metrics_set_operations_snapshot = this->_metrics_set_operations;
    metrics_delete_operations_snapshot = this->_metrics_delete_operations;
    operation_count = operations.size();
    operation_index = 0;
    while (operation_index < operation_count)
    {
        const kv_store_operation &operation = operations[operation_index];
        Pair<ft_string, kv_store_entry> *existing_pair;
        Pair<ft_string, kv_store_entry> *map_end;

        if (operation._key.c_str() == ft_nullptr || operation._key.size() == 0)
        {
            this->_data = ft_move(data_snapshot);
            this->_metrics_set_operations = metrics_set_operations_snapshot;
            this->_metrics_delete_operations = metrics_delete_operations_snapshot;
            this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_OPERATION);
        }
        existing_pair = this->_data.find(operation._key);
        map_end = this->_data.end();
        if (operation._type == KV_STORE_OPERATION_TYPE_DELETE)
        {
            if (existing_pair == map_end)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_NOT_FOUND);
                return (FT_ERR_INVALID_OPERATION);
            }
            this->_data.remove(operation._key);
            this->record_delete_operation();
            operation_index++;
            continue ;
        }
        if (operation._has_value == FT_FALSE)
        {
            this->_data = ft_move(data_snapshot);
            this->_metrics_set_operations = metrics_set_operations_snapshot;
            this->_metrics_delete_operations = metrics_delete_operations_snapshot;
            this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (existing_pair != map_end)
        {
            if (existing_pair->value.set_value(operation._value) != 0)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (operation._has_ttl)
            {
                ft_bool has_expiration;
                int64_t expiration_timestamp;

                if (this->compute_expiration(operation._ttl_seconds, has_expiration, expiration_timestamp) != 0)
                {
                    this->_data = ft_move(data_snapshot);
                    this->_metrics_set_operations = metrics_set_operations_snapshot;
                    this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                    this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_ARGUMENT);
                    return (FT_ERR_INVALID_OPERATION);
                }
                if (existing_pair->value.configure_expiration(has_expiration, expiration_timestamp) != 0)
                {
                    this->_data = ft_move(data_snapshot);
                    this->_metrics_set_operations = metrics_set_operations_snapshot;
                    this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                    this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
        }
        else
        {
            kv_store_entry new_entry;
            ft_bool has_expiration;
            int64_t expiration_timestamp;

            if (new_entry.initialize() != FT_ERR_SUCCESS)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (new_entry.set_value(operation._value) != 0)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
            has_expiration = FT_FALSE;
            expiration_timestamp = 0;
            if (operation._has_ttl)
            {
                if (this->compute_expiration(operation._ttl_seconds, has_expiration, expiration_timestamp) != 0)
                {
                    this->_data = ft_move(data_snapshot);
                    this->_metrics_set_operations = metrics_set_operations_snapshot;
                    this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                    this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_ARGUMENT);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            if (new_entry.configure_expiration(has_expiration, expiration_timestamp) != 0)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
            this->_data.insert(operation._key, new_entry);
            if (this->_data.get_error() != FT_ERR_SUCCESS)
            {
                this->_data = ft_move(data_snapshot);
                this->_metrics_set_operations = metrics_set_operations_snapshot;
                this->_metrics_delete_operations = metrics_delete_operations_snapshot;
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
                return (FT_ERR_INVALID_OPERATION);
            }
        }
        this->record_set_operation();
        operation_index++;
    }
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    if (operation_count > 0)
        return (this->notify_replication_listeners(operations));
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::kv_compare_and_swap(const char *key_string, const char *expected_value, const char *new_value, int64_t ttl_seconds)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_compare_and_swap");
    ft_string key_storage;
    Pair<ft_string, kv_store_entry> *existing_pair;
    Pair<ft_string, kv_store_entry> *map_end;
    ft_bool lock_acquired = FT_FALSE;
    ft_string current_value;
    int32_t lock_error;

    if (key_string == ft_nullptr || expected_value == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    key_storage = key_string;
    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, lock_error);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->prune_expired_locked() != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    existing_pair = this->_data.find(key_storage);
    map_end = this->_data.end();
    if (existing_pair == map_end)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (existing_pair->value.copy_value(current_value) != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (ft_strcmp(current_value.c_str(), expected_value) != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (new_value == ft_nullptr)
    {
        this->_data.remove(key_storage);
        this->record_delete_operation();
    }
    else
    {
        ft_bool has_expiration;
        int64_t expiration_timestamp;

        if (existing_pair->value.set_value(new_value) != 0)
        {
            this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        has_expiration = FT_FALSE;
        expiration_timestamp = 0;
        if (ttl_seconds >= 0)
        {
            if (this->compute_expiration(ttl_seconds, has_expiration, expiration_timestamp) != 0)
            {
                this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_ARGUMENT);
                return (FT_ERR_INVALID_OPERATION);
            }
        }
        if (existing_pair->value.configure_expiration(has_expiration, expiration_timestamp) != 0)
        {
            this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        this->record_set_operation();
    }
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::apply_snapshot_locked(const ft_vector<kv_store_snapshot_entry> &entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::apply_snapshot_locked");
    const kv_store_snapshot_entry *entry_begin;
    const kv_store_snapshot_entry *entry_end;
    const kv_store_snapshot_entry *entry_cursor;

    this->_data.clear();
    entry_begin = entries.begin();
    entry_end = entries.end();
    entry_cursor = entry_begin;
    while (entry_cursor != entry_end)
    {
        kv_store_entry entry_value;

        if (entry_cursor->key.c_str() == ft_nullptr)
            return (FT_ERR_INVALID_OPERATION);
        if (entry_value.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        if (entry_value.set_value(entry_cursor->value) != 0)
            return (FT_ERR_INVALID_OPERATION);
        if (entry_value.configure_expiration(entry_cursor->has_expiration, entry_cursor->expiration_timestamp) != 0)
            return (FT_ERR_INVALID_OPERATION);
        this->_data.insert(entry_cursor->key, entry_value);
        entry_cursor++;
    }
    this->_metrics_set_operations = 0;
    this->_metrics_delete_operations = 0;
    this->_metrics_get_hits = 0;
    this->_metrics_get_misses = 0;
    this->_metrics_prune_operations = 0;
    this->_metrics_pruned_entries = 0;
    this->_metrics_total_prune_duration_ms = 0;
    this->_metrics_last_prune_duration_ms = 0;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::import_snapshot(const ft_vector<kv_store_snapshot_entry> &entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::import_snapshot");
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;
    int32_t operation_error;

    lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, lock_error);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->apply_snapshot_locked(entries) != 0)
    {
        operation_error = FT_ERR_SUCCESS;
        this->unlock_store_guard(lock_acquired, operation_error);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::start_background_compaction(int64_t interval_seconds)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::start_background_compaction");
    ft_bool lock_acquired = FT_FALSE;
    kv_store *mutable_this;
    int32_t lock_error;
    int32_t start_result;

    mutable_this = this;
    lock_error = this->lock_background(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
            (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    start_result = mutable_this->start_background_thread_locked(interval_seconds);
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
    if (start_result != 0)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::stop_background_compaction()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::stop_background_compaction");
    ft_bool lock_acquired = FT_FALSE;
    kv_store *mutable_this;
    ft_thread thread_holder;
    int32_t lock_error;

    mutable_this = this;
    lock_error = this->lock_background(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
            (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    mutable_this->stop_background_thread_locked(thread_holder);
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
    if (thread_holder.joinable())
    {
        thread_holder.join();
    }
    lock_error = this->lock_background(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        if (lock_acquired)
            (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_background_stop_requested = FT_FALSE;
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
    return (FT_ERR_SUCCESS);
}
