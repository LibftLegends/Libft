#include "kv_store.hpp"

#include <cstddef>
#include <charconv>
#include <string_view>
#include <type_traits>
#include <cerrno>
#include <climits>
#include <cstdio>
#include <new>

#include "../Compatebility/sqlite_support.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/unordered_map.hpp"
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
#include "../Threading/concurrency.hpp"
#include "../File/file_internal.hpp"
#include "../JSon/json_internal.hpp"

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

int32_t kv_store::get_active_encryption_algorithm_name(ft_string &algorithm_name) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store::get_active_encryption_algorithm_name");
    const int32_t destroy_error = algorithm_name.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (this->_encryption_algorithm_name.empty() == FT_TRUE)
        return (algorithm_name.initialize("aes-128-ecb-base64"));
    return (algorithm_name.initialize(this->_encryption_algorithm_name));
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
    if (this->_replication_operation_count.load(std::memory_order_acquire) == 0)
        return (FT_ERR_SUCCESS);
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
        if (sink_entry._operations_callback != ft_nullptr)
        {
            const int32_t error = listeners_copy.push_back(sink_entry);
            if (error != FT_ERR_SUCCESS)
            {
                kv_store_finalize_lock(this->_replication_mutex, lock_acquired);
                return (error);
            }
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
    if (operations_callback != ft_nullptr)
        this->_replication_operation_count.fetch_add(1, std::memory_order_release);
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
            if (operations_callback != ft_nullptr)
                this->_replication_operation_count.fetch_sub(1, std::memory_order_release);
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

static int32_t storage_kv_take_json_string(ft_string &destination, char *&source)
{
    if (source == ft_nullptr)
        return (destination.initialize());
    const int32_t error_code = ft_string_take_owned_buffer(destination, source,
        ft_strlen_size_t(source));
    if (error_code == FT_ERR_SUCCESS)
        source = ft_nullptr;
    return (error_code);
}

int32_t kv_store::parse_json_groups(json_group *group_head, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::parse_json_groups");
    if (group_head == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    json_group *group = json_find_group(group_head, "kv_store");
    if (group == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t ttl_prefix_length = ft_strlen_size_t(g_kv_store_ttl_prefix);
    ft_size_t value_count = 0;
    ft_size_t ttl_count = 0;
    json_item *item = group->items;
    while (item != ft_nullptr)
    {
        if (ttl_prefix_length != 0
            && ft_strncmp(item->key, g_kv_store_ttl_prefix, ttl_prefix_length) == 0)
            ++ttl_count;
        else if (ft_strcmp(item->key, "__encryption__") != 0)
            ++value_count;
        item = item->next;
    }
    out_entries.reserve(value_count);
    int32_t error_code = out_entries.get_error();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_unordered_map<std::string_view, int64_t> ttl_metadata(ttl_count * 2);
    if (ttl_count != 0)
    {
        error_code = ttl_metadata.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    item = group->items;
    while (item != ft_nullptr)
    {
        if (ft_strcmp(item->key, "__encryption__") == 0)
        {
            ft_string active_algorithm;
            if (this->_encryption_enabled == FT_FALSE)
                return (FT_ERR_INVALID_ARGUMENT);
            error_code = this->get_active_encryption_algorithm_name(active_algorithm);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            if (ft_strcmp(item->value, active_algorithm.c_str()) != 0)
                return (FT_ERR_INVALID_ARGUMENT);
        }
        else if (ttl_prefix_length != 0
            && ft_strncmp(item->key, g_kv_store_ttl_prefix, ttl_prefix_length) == 0)
        {
            int64_t timestamp;
            if (this->parse_expiration_timestamp(item->value, timestamp) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_ARGUMENT);
            ttl_metadata.insert(std::string_view(item->key + ttl_prefix_length), timestamp);
            error_code = ttl_metadata.get_error();
        }
        else
        {
            kv_store_snapshot_entry entry;
            entry.has_expiration = FT_FALSE;
            entry.expiration_timestamp = 0;
            error_code = storage_kv_take_json_string(entry.key, item->key);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            if (this->_encryption_enabled == FT_TRUE)
            {
                ft_string encoded;
                error_code = storage_kv_take_json_string(encoded, item->value);
                if (error_code == FT_ERR_SUCCESS)
                    error_code = entry.value.initialize();
                if (error_code != FT_ERR_SUCCESS)
                    return (error_code);
                if (this->decrypt_value(encoded, entry.value) != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_ARGUMENT);
            }
            else
                error_code = storage_kv_take_json_string(entry.value, item->value);
            if (error_code == FT_ERR_SUCCESS)
                error_code = out_entries.push_back(ft_move(entry));
        }
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        item = item->next;
    }

    if (ttl_metadata.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        auto *entry = out_entries.begin();
        const auto *end = out_entries.end();
        const auto ttl_end = ttl_metadata.end();
        while (entry != end)
        {
            const std::string_view key(entry->key.c_str(), entry->key.size());
            const auto ttl = ttl_metadata.find(key);
            if (ttl != ttl_end)
            {
                entry->has_expiration = FT_TRUE;
                entry->expiration_timestamp = ttl->second;
            }
            ++entry;
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

static int32_t storage_kv_append_json_item(json_item **&tail, const char *key, const char *value)
{
    json_item *item = json_create_item(key, value);
    if (item == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    *tail = item;
    tail = &item->next;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::build_json_entries(const ft_vector<kv_store_snapshot_entry> &entries,
    json_group **output) const
{
    *output = ft_nullptr;
    json_group *group = json_create_json_group("kv_store");
    if (group == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    // The group is private until complete, so appending needs neither a traversal nor a lock.
    json_item **tail = &group->items;
    int32_t error_code = FT_ERR_SUCCESS;
    if (this->_encryption_enabled == FT_TRUE)
    {
        ft_string algorithm;
        error_code = this->get_active_encryption_algorithm_name(algorithm);
        if (error_code == FT_ERR_SUCCESS)
            error_code = storage_kv_append_json_item(tail, "__encryption__", algorithm.c_str());
    }

    const auto *cursor = entries.begin();
    const auto *end = entries.end();
    while (error_code == FT_ERR_SUCCESS && cursor != end)
    {
        ft_string encrypted;
        const char *value = cursor->value.c_str();
        if (this->_encryption_enabled == FT_TRUE)
        {
            error_code = encrypted.initialize();
            if (error_code == FT_ERR_SUCCESS)
                error_code = this->encrypt_value(cursor->value, encrypted);
            if (error_code == FT_ERR_SUCCESS)
                value = encrypted.c_str();
        }
        if (error_code == FT_ERR_SUCCESS)
            error_code = storage_kv_append_json_item(tail, cursor->key.c_str(), value);
        if (error_code == FT_ERR_SUCCESS && cursor->has_expiration == FT_TRUE)
        {
            ft_string ttl_key;
            error_code = ttl_key.initialize(g_kv_store_ttl_prefix);
            if (error_code == FT_ERR_SUCCESS)
                error_code = ttl_key.append(cursor->key);
            if (error_code == FT_ERR_SUCCESS)
            {
                char timestamp[32];
                auto result = std::to_chars(timestamp, timestamp + sizeof(timestamp) - 1,
                    cursor->expiration_timestamp);
                if (result.ec != std::errc{})
                    error_code = FT_ERR_INVALID_ARGUMENT;
                else
                {
                    *result.ptr = '\0';
                    error_code = storage_kv_append_json_item(tail, ttl_key.c_str(), timestamp);
                }
            }
        }
        ++cursor;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        json_free_groups(group);
        return (error_code);
    }
    *output = group;
    return (FT_ERR_SUCCESS);
}

struct storage_kv_lines_output
{
    int32_t descriptor;
    ft_size_t used;
    char buffer[16384];
};

static int32_t storage_kv_flush_line_buffer(storage_kv_lines_output &output)
{
    const int32_t error_code = file_write_all_to_descriptor(output.descriptor,
        output.buffer, output.used);
    output.used = 0;
    return (error_code);
}

static ft_size_t storage_kv_buffer_line_bytes(void *context, const char *data, ft_size_t size)
{
    auto &output = *static_cast<storage_kv_lines_output *>(context);
    if (size > sizeof(output.buffer) - output.used
        && storage_kv_flush_line_buffer(output) != FT_ERR_SUCCESS)
        return (0);
    if (size > sizeof(output.buffer))
    {
        if (file_write_all_to_descriptor(output.descriptor, data, size) != FT_ERR_SUCCESS)
            return (0);
    }
    else if (size != 0)
    {
        ft_memcpy(output.buffer + output.used, data, size);
        output.used += size;
    }
    return (size);
}

static int32_t storage_kv_json_event(json_stream_writer &writer, json_stream_event_type type,
    const char *text = ft_nullptr)
{
    json_stream_event event{};
    event.type = type;
    event.value.data = text;
    if (text != ft_nullptr)
        event.value.length = ft_strlen_size_t(text);
    if (json_stream_writer_process(&writer, &event) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

struct storage_kv_json_field
{
    const char *name;
    const char *value;
};

static int32_t storage_kv_write_line(storage_kv_lines_output &output, const char *group,
    const storage_kv_json_field *fields, ft_size_t count)
{
    json_stream_writer writer;
    int32_t error_code = json_stream_writer_init(&writer, storage_kv_buffer_line_bytes, &output);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_BEGIN_DOCUMENT);
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_BEGIN_OBJECT);
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_KEY, group);
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_BEGIN_OBJECT);
    ft_size_t index = 0;
    while (error_code == FT_ERR_SUCCESS && index < count)
    {
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_KEY, fields[index].name);
        if (error_code == FT_ERR_SUCCESS)
            error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_STRING, fields[index].value);
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_END_OBJECT);
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_END_OBJECT);
    if (error_code == FT_ERR_SUCCESS)
        error_code = storage_kv_json_event(writer, JSON_STREAM_EVENT_END_DOCUMENT);
    if (error_code == FT_ERR_SUCCESS && storage_kv_buffer_line_bytes(&output, "\n", 1) != 1)
        error_code = FT_ERR_IO;
    json_stream_writer_destroy(&writer);
    return (error_code);
}

int32_t kv_store::write_json_lines_descriptor(int32_t descriptor,
    const ft_vector<kv_store_snapshot_entry> &entries) const
{
    storage_kv_lines_output output;
    output.descriptor = descriptor;
    output.used = 0;
    if (this->_encryption_enabled == FT_TRUE)
    {
        ft_string algorithm;
        const int32_t error_code = this->get_active_encryption_algorithm_name(algorithm);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        const storage_kv_json_field metadata{"encryption", algorithm.c_str()};
        if (storage_kv_write_line(output, "metadata", &metadata, 1) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
    }

    const auto *cursor = entries.begin();
    const auto *end = entries.end();
    ft_string encrypted;
    if (this->_encryption_enabled == FT_TRUE && encrypted.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    while (cursor != end)
    {
        const char *value = cursor->value.c_str();
        if (this->_encryption_enabled == FT_TRUE)
        {
            const int32_t error_code = this->encrypt_value(cursor->value, encrypted);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            value = encrypted.c_str();
        }
        char timestamp[32] = {};
        const char *flag = "FT_FALSE";
        if (cursor->has_expiration == FT_TRUE)
        {
            const auto result = std::to_chars(timestamp, timestamp + sizeof(timestamp) - 1,
                cursor->expiration_timestamp);
            if (result.ec != std::errc{})
                return (FT_ERR_OUT_OF_RANGE);
            *result.ptr = '\0';
            flag = "FT_TRUE";
        }
        const storage_kv_json_field fields[] = {
            {"key", cursor->key.c_str()}, {"value", value},
            {"has_expiration", flag}, {"expiration", timestamp}
        };
        const int32_t error_code = storage_kv_write_line(output, "entry",
            fields, sizeof(fields) / sizeof(fields[0]));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        ++cursor;
    }
    return (storage_kv_flush_line_buffer(output));
}

int32_t kv_store::flush_json_lines_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_json_lines_entries");
    struct writer_context
    {
        const kv_store *store;
        const ft_vector<kv_store_snapshot_entry> *entries;
    };
    writer_context context{this, &entries};
    const int32_t error_code = file_replace_stream(this->_file_path.c_str(), FT_FALSE,
        [](int32_t descriptor, void *opaque) -> int32_t
        {
            const auto &state = *static_cast<writer_context *>(opaque);
            return (state.store->write_json_lines_descriptor(descriptor, *state.entries));
        }, &context);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::parse_json_line_record(const char *record,
    ft_vector<kv_store_snapshot_entry> &out_entries) const
{
    json_group *groups = json_read_from_string(record);
    if (groups == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    json_group *metadata = json_find_group(groups, "metadata");
    int32_t result = FT_ERR_SUCCESS;
    if (metadata != ft_nullptr)
    {
        const json_item *item = json_find_item(metadata, "encryption");
        if (item != ft_nullptr)
        {
            ft_string active_algorithm;
            result = this->get_active_encryption_algorithm_name(active_algorithm);
            if (result == FT_ERR_SUCCESS && (this->_encryption_enabled == FT_FALSE
                    || ft_strcmp(item->value, active_algorithm.c_str()) != 0))
                result = FT_ERR_INVALID_ARGUMENT;
        }
    }
    else
    {
        json_group *entry_group = json_find_group(groups, "entry");
        json_item *key = json_find_item(entry_group, "key");
        json_item *value = json_find_item(entry_group, "value");
        if (key == ft_nullptr || value == ft_nullptr)
            result = FT_ERR_INVALID_ARGUMENT;
        else
        {
            kv_store_snapshot_entry entry;
            const json_item *flag = json_find_item(entry_group, "has_expiration");
            entry.has_expiration = flag != ft_nullptr && ft_strcmp(flag->value, "FT_TRUE") == 0;
            entry.expiration_timestamp = 0;
            if (entry.has_expiration == FT_TRUE)
            {
                const json_item *timestamp = json_find_item(entry_group, "expiration");
                if (timestamp == ft_nullptr)
                    result = FT_ERR_INVALID_ARGUMENT;
                else
                    result = this->parse_expiration_timestamp(timestamp->value, entry.expiration_timestamp);
            }
            if (result == FT_ERR_SUCCESS)
                result = storage_kv_take_json_string(entry.key, key->value);
            if (result == FT_ERR_SUCCESS && this->_encryption_enabled == FT_TRUE)
            {
                ft_string encoded;
                result = storage_kv_take_json_string(encoded, value->value);
                if (result == FT_ERR_SUCCESS)
                    result = entry.value.initialize();
                if (result == FT_ERR_SUCCESS)
                    result = this->decrypt_value(encoded, entry.value);
            }
            else if (result == FT_ERR_SUCCESS)
                result = storage_kv_take_json_string(entry.value, value->value);
            if (result == FT_ERR_SUCCESS)
                result = out_entries.push_back(ft_move(entry));
        }
    }
    json_free_groups(groups);
    return (result);
}

struct storage_kv_record_scan
{
    ft_size_t depth = 0;
    ft_bool in_string = FT_FALSE;
    ft_bool escaped = FT_FALSE;
};

static void storage_kv_scan_record_byte(storage_kv_record_scan &scan, char byte_value)
{
    if (scan.in_string == FT_TRUE)
    {
        if (scan.escaped == FT_TRUE)
            scan.escaped = FT_FALSE;
        else if (byte_value == '\\')
            scan.escaped = FT_TRUE;
        else if (byte_value == '"')
            scan.in_string = FT_FALSE;
    }
    else if (byte_value == '"')
        scan.in_string = FT_TRUE;
    else if (byte_value == '{')
        ++scan.depth;
    else if (byte_value == '}')
        --scan.depth;
    return ;
}

int32_t kv_store::load_json_lines_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_json_lines_entries");
    FILE *file = ft_fopen(location, "rb");
    if (file == ft_nullptr)
        return (FT_ERR_FILE_OPEN_FAILED);
    ft_string record;
    int32_t result = record.initialize();
    storage_kv_record_scan scan;
    char block[16384];
    ft_size_t size = 0;
    // Balanced root objects also support snapshots written by the old pretty writer.
    while (result == FT_ERR_SUCCESS && (size = std::fread(block, 1, sizeof(block), file)) != 0)
    {
        ft_size_t start = 0;
        ft_size_t offset = 0;
        while (result == FT_ERR_SUCCESS && offset < size)
        {
            const char byte_value = block[offset];
            if (scan.depth == 0)
            {
                if (byte_value == ' ' || byte_value == '\t' || byte_value == '\r' || byte_value == '\n')
                {
                    ++offset;
                    continue ;
                }
                if (byte_value != '{')
                {
                    result = FT_ERR_INVALID_ARGUMENT;
                    break ;
                }
                start = offset;
            }
            storage_kv_scan_record_byte(scan, byte_value);
            ++offset;
            if (scan.depth == 0)
            {
                result = record.append(block + start, offset - start);
                if (result == FT_ERR_SUCCESS)
                    result = this->parse_json_line_record(record.c_str(), out_entries);
                if (result == FT_ERR_SUCCESS)
                    result = record.clear();
            }
        }
        if (result == FT_ERR_SUCCESS && scan.depth != 0)
            result = record.append(block + start, size - start);
    }
    if (result == FT_ERR_SUCCESS && (scan.depth != 0 || std::ferror(file) != 0))
        result = FT_ERR_INVALID_OPERATION;
    const int32_t close_error = ft_fclose(file);
    if (result == FT_ERR_SUCCESS)
        result = close_error;
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

#if SQLITE3_AVAILABLE
static constexpr char STORAGE_KV_SQLITE_SCHEMA[] =
    "CREATE TABLE IF NOT EXISTS kv_store_entries (key TEXT PRIMARY KEY, value TEXT NOT NULL, "
    "has_expiration INTEGER NOT NULL, expiration INTEGER NOT NULL);"
    "CREATE TABLE IF NOT EXISTS kv_store_metadata (id INTEGER PRIMARY KEY CHECK(id=1), "
    "encryption INTEGER NOT NULL);";

int32_t kv_store::flush_sqlite_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_sqlite_entries");
    const ft_bool encrypted = this->_encryption_enabled;
    ft_string encoded_value;
    if (encrypted && encoded_value.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    sqlite3 *database_handle = ft_nullptr;
    sqlite3_stmt *statement = ft_nullptr;
    ft_bool success = sqlite3_open(this->_file_path.c_str(), &database_handle) == SQLITE_OK;
    if (success)
        success = sqlite3_exec(database_handle, "BEGIN IMMEDIATE;", ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK
            && sqlite3_exec(database_handle, STORAGE_KV_SQLITE_SCHEMA, ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK
            && sqlite3_exec(database_handle, "DELETE FROM kv_store_entries;", ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK
            && sqlite3_prepare_v2(database_handle,
                "INSERT INTO kv_store_metadata (id, encryption) VALUES (1, ?) "
                "ON CONFLICT(id) DO UPDATE SET encryption=excluded.encryption;",
                -1, &statement, ft_nullptr) == SQLITE_OK
            && sqlite3_bind_int(statement, 1, encrypted != FT_FALSE) == SQLITE_OK
            && sqlite3_step(statement) == SQLITE_DONE;
    sqlite3_finalize(statement);
    statement = ft_nullptr;
    if (success)
        success = sqlite3_prepare_v2(database_handle, "INSERT INTO kv_store_entries "
            "(key, value, has_expiration, expiration) VALUES (?, ?, ?, ?);",
            -1, &statement, ft_nullptr) == SQLITE_OK;
    const kv_store_snapshot_entry *entry_cursor = entries.begin();
    const kv_store_snapshot_entry *entry_end = entries.end();
    while (success && entry_cursor != entry_end)
    {
        const char *value = entry_cursor->value.c_str();
        if (encrypted)
        {
            if (this->encrypt_value(entry_cursor->value, encoded_value) != FT_ERR_SUCCESS)
            {
                success = FT_FALSE;
                break ;
            }
            value = encoded_value.c_str();
        }
        success = sqlite3_bind_text(statement, 1, entry_cursor->key.c_str(), -1, SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_text(statement, 2, value, -1, SQLITE_STATIC) == SQLITE_OK
            && sqlite3_bind_int(statement, 3, entry_cursor->has_expiration != FT_FALSE) == SQLITE_OK
            && sqlite3_bind_int64(statement, 4, entry_cursor->expiration_timestamp) == SQLITE_OK
            && sqlite3_step(statement) == SQLITE_DONE
            && sqlite3_reset(statement) == SQLITE_OK;
        // Drop the borrowed ciphertext before reusing its buffer for the next row.
        if (success && encrypted)
            success = sqlite3_clear_bindings(statement) == SQLITE_OK;
        ++entry_cursor;
    }
    sqlite3_finalize(statement);
    if (success)
        success = sqlite3_exec(database_handle, "COMMIT;", ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK;
    // Closing rolls back any transaction that did not commit.
    if (database_handle != ft_nullptr)
        sqlite3_close(database_handle);
    if (success == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::load_sqlite_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_sqlite_entries");
    sqlite3 *database_handle = ft_nullptr;
    sqlite3_stmt *statement = ft_nullptr;
    const ft_bool encrypted = this->_encryption_enabled;
    ft_string encoded_value;
    if (encrypted && encoded_value.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    ft_bool success = sqlite3_open(location, &database_handle) == SQLITE_OK;
    if (success)
        success = sqlite3_exec(database_handle, STORAGE_KV_SQLITE_SCHEMA, ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK
            && sqlite3_exec(database_handle, "BEGIN;", ft_nullptr, ft_nullptr, ft_nullptr) == SQLITE_OK
            && sqlite3_prepare_v2(database_handle, "SELECT encryption FROM kv_store_metadata WHERE id = 1;",
                -1, &statement, ft_nullptr) == SQLITE_OK;
    if (success)
    {
        const int32_t step_result = sqlite3_step(statement);
        success = step_result == SQLITE_DONE || (step_result == SQLITE_ROW
            && (sqlite3_column_int(statement, 0) != 0) == (encrypted != FT_FALSE));
    }
    sqlite3_finalize(statement);
    statement = ft_nullptr;
    if (success)
        success = sqlite3_prepare_v2(database_handle, "SELECT key, value, has_expiration, expiration FROM kv_store_entries;",
            -1, &statement, ft_nullptr) == SQLITE_OK;
    out_entries.clear();
    while (success)
    {
        const int32_t step_result = sqlite3_step(statement);
        if (step_result == SQLITE_DONE)
            break ;
        if (step_result != SQLITE_ROW)
        {
            success = FT_FALSE;
            break ;
        }
        const char *key = reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));
        const char *value = reinterpret_cast<const char *>(sqlite3_column_text(statement, 1));
        kv_store_snapshot_entry entry;
        success = key != ft_nullptr && value != ft_nullptr && entry.key.initialize(key) == FT_ERR_SUCCESS;
        if (success && encrypted)
            success = encoded_value.assign(value, ft_strlen_size_t(value)) == FT_ERR_SUCCESS
                && entry.value.initialize() == FT_ERR_SUCCESS
                && this->decrypt_value(encoded_value, entry.value) == FT_ERR_SUCCESS;
        else if (success)
            success = entry.value.initialize(value) == FT_ERR_SUCCESS;
        if (success)
        {
            entry.has_expiration = sqlite3_column_int(statement, 2) != 0;
            entry.expiration_timestamp = sqlite3_column_int64(statement, 3);
            success = out_entries.push_back(ft_move(entry)) == FT_ERR_SUCCESS;
        }
    }
    sqlite3_finalize(statement);
    if (database_handle != ft_nullptr)
        sqlite3_close(database_handle);
    if (success == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}
#endif

int32_t kv_store::flush_memory_mapped_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_memory_mapped_entries");
    json_group *group;
    if (this->build_json_entries(entries, &group) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    char *serialized = ft_nullptr;
    ft_size_t length = 0;
    const int32_t build_error = json_write_buffer(group, serialized, length);
    json_free_groups(group);
    if (build_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);

    struct mapped_snapshot
    {
        const char *data;
        ft_size_t size;
    };
    mapped_snapshot snapshot{serialized, length};
    const int32_t result = file_replace_stream(this->_file_path.c_str(), FT_TRUE,
        [](int32_t descriptor, void *opaque) -> int32_t
        {
            const auto &data = *static_cast<mapped_snapshot *>(opaque);
            return (cmp_storage_write_memory_mapped_descriptor(descriptor, data.data, data.size));
        }, &snapshot);
    cma_free(serialized);
    return (result);
}

struct storage_kv_mapped_input
{
    const char *cursor;
    ft_size_t remaining;
};

static ft_size_t storage_kv_read_mapped_bytes(void *context, char *output, ft_size_t capacity)
{
    auto &input = *static_cast<storage_kv_mapped_input *>(context);
    ft_size_t count = input.remaining;
    if (count > capacity)
        count = capacity;
    if (count != 0)
    {
        ft_memcpy(output, input.cursor, count);
        input.cursor += count;
        input.remaining -= count;
    }
    return (count);
}

int32_t kv_store::load_memory_mapped_entries(const char *location, ft_vector<kv_store_snapshot_entry> &out_entries)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::load_memory_mapped_entries");
    struct loader_context
    {
        kv_store *store;
        ft_vector<kv_store_snapshot_entry> *entries;
    };
    loader_context context{this, &out_entries};
    return (cmp_storage_read_memory_mapped_file(location,
        [](const char *bytes, ft_size_t length, void *opaque) -> int32_t
        {
            auto &state = *static_cast<loader_context *>(opaque);
            storage_kv_mapped_input input{bytes, length};
            json_group *groups = json_read_from_stream(storage_kv_read_mapped_bytes, &input, 16384);
            if (groups == ft_nullptr)
                return (FT_ERR_INVALID_OPERATION);
            const int32_t result = state.store->parse_json_groups(groups, *state.entries);
            json_free_groups(groups);
            return (result);
        }, &context));
}

int32_t kv_store::flush_backend_entries(const ft_vector<kv_store_snapshot_entry> &entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::flush_backend_entries");
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

static ft_bool storage_kv_backend_is_supported(kv_store_backend_type type) noexcept
{
#if SQLITE3_AVAILABLE
    if (type == KV_STORE_BACKEND_SQLITE)
        return (FT_TRUE);
#endif
    return (type == KV_STORE_BACKEND_JSON || type == KV_STORE_BACKEND_JSON_LINES
        || type == KV_STORE_BACKEND_MEMORY_MAPPED);
}

int32_t kv_store::set_backend(kv_store_backend_type backend_type, const char *location)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::set_backend");
    if (location == ft_nullptr || location[0] == '\0'
        || storage_kv_backend_is_supported(backend_type) == FT_FALSE)
        return (FT_ERR_INVALID_OPERATION);

    ft_string destination;
    if (destination.initialize(location) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    ft_vector<kv_store_snapshot_entry> loaded_entries;
    if (loaded_entries.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_store(&lock_acquired);
    file_type destination_type = FILE_TYPE_UNKNOWN;
    if (result == FT_ERR_SUCCESS)
        result = cmp_file_get_type(destination.c_str(), &destination_type, ft_nullptr);
    if (result == FT_ERR_SUCCESS && destination_type != FILE_TYPE_MISSING)
        result = this->load_backend_entries(backend_type, destination.c_str(), loaded_entries);

    ft_string previous_path;
    ft_bool path_staged = FT_FALSE;
    if (result == FT_ERR_SUCCESS)
    {
        result = previous_path.move(this->_file_path);
        if (result == FT_ERR_SUCCESS)
        {
            path_staged = FT_TRUE;
            result = this->_file_path.move(destination);
        }
    }
    if (result == FT_ERR_SUCCESS && destination_type != FILE_TYPE_MISSING)
        result = this->apply_loaded_snapshot_locked(loaded_entries);
    if (result == FT_ERR_SUCCESS)
        this->_backend_type = backend_type;
    else if (path_staged == FT_TRUE)
        (void)this->_file_path.move(previous_path);
    this->unlock_store_guard(lock_acquired, result);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
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
    ft_vector<kv_store_snapshot_entry> entries;
    json_group *group = ft_nullptr;
    ft_bool lock_acquired = FT_FALSE;
    int32_t error_code = this->lock_store(&lock_acquired);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->export_snapshot_locked(entries);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->build_json_entries(entries, &group);
    this->unlock_store_guard(lock_acquired, error_code);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    const int32_t result = json_write_to_backend(sink, group);
    json_free_groups(group);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::read_snapshot(ft_document_source &source)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::read_snapshot");
    ft_string content;
    if (content.initialize() != FT_ERR_SUCCESS || source.read_all(content) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    json_group *group = json_read_from_string(content.c_str());
    if (group == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    ft_vector<kv_store_snapshot_entry> entries;
    int32_t result = entries.initialize();
    ft_bool lock_acquired = FT_FALSE;
    if (result == FT_ERR_SUCCESS)
        result = this->lock_store(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->parse_json_groups(group, entries);
    json_free_groups(group);
    if (result == FT_ERR_SUCCESS)
        result = this->apply_loaded_snapshot_locked(entries);
    this->unlock_store_guard(lock_acquired, result);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_init_set_operation(kv_store_operation &operation, const char *key_string,
    const char *value_string, int64_t ttl_seconds)
{
    if (key_string == ft_nullptr || value_string == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    if (operation._key.initialize(key_string) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (operation._value.initialize(value_string) != FT_ERR_SUCCESS)
    {
        (void)operation._key.destroy();
        return (FT_ERR_INVALID_OPERATION);
    }
    operation._type = KV_STORE_OPERATION_TYPE_SET;
    operation._has_value = FT_TRUE;
    operation._has_ttl = ttl_seconds >= 0;
    operation._ttl_seconds = -1;
    if (operation._has_ttl == FT_TRUE)
        operation._ttl_seconds = ttl_seconds;
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

int32_t kv_store::start_background_thread_locked(uint32_t interval_milliseconds) noexcept
{
    if (this->_background_wake != ft_nullptr)
    {
        this->_background_interval_milliseconds.store(interval_milliseconds, std::memory_order_relaxed);
        return (FT_ERR_SUCCESS);
    }

    ft_countdown_latch *wake = new (std::nothrow) ft_countdown_latch();
    if (wake == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t result = wake->initialize(1);
    if (result == FT_ERR_SUCCESS)
    {
        this->_background_interval_milliseconds.store(interval_milliseconds, std::memory_order_relaxed);
        try
        {
            ft_thread worker(&kv_store::background_compaction_worker, this, wake);
            if (worker.joinable())
            {
                this->_background_thread = ft_move(worker);
                this->_background_wake = wake;
                return (FT_ERR_SUCCESS);
            }
            result = FT_ERR_INVALID_OPERATION;
        }
        catch (const std::bad_alloc &)
        {
            result = FT_ERR_NO_MEMORY;
        }
    }
    delete wake;
    return (result);
}

void kv_store::background_compaction_worker(kv_store *store, ft_countdown_latch *wake) noexcept
{
    while (wake->wait_for(store->_background_interval_milliseconds.load(std::memory_order_relaxed))
            == FT_ERR_TIMEOUT)
    {
        (void)store->prune_expired();
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

static constexpr int64_t storage_kv_earlier_expiration(int64_t pending, int64_t timestamp) noexcept
{
    // Negative timestamps in imported snapshots are already due.
    if (timestamp < 0)
        timestamp = 0;
    if (pending < 0 || timestamp < pending)
        return (timestamp);
    return (pending);
}

static void storage_kv_relocate_entry(Pair<ft_string, kv_store_entry> &destination,
    Pair<ft_string, kv_store_entry> &source) noexcept
{
    // Store-owned keys and entries have no independent locks or allocation on move.
    (void)destination.key.move(source.key);
    (void)destination.value.move(source.value);
    return ;
}

static int32_t storage_kv_erase_entry(ft_map<ft_string, kv_store_entry> &data,
    const Pair<ft_string, kv_store_entry> *entry)
{
    return (data.erase_if(
        [entry](const Pair<ft_string, kv_store_entry> &candidate, ft_size_t) noexcept -> ft_bool
        {
            return (&candidate == entry);
        }, storage_kv_relocate_entry));
}

int32_t kv_store::prune_expired_locked(ft_size_t *lookup_index)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::prune_expired_locked");
    const ft_size_t size = this->_data.size();
    if (this->_data.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (size == 0)
    {
        this->_next_expiration = -1;
        return (FT_ERR_SUCCESS);
    }
    if (this->_next_expiration < 0)
    {
        this->record_prune_metrics(0, 0);
        return (FT_ERR_SUCCESS);
    }
    const int64_t current_time = this->current_time_seconds();
    if (current_time < 0)
        return (FT_ERR_INVALID_OPERATION);
    if (current_time < this->_next_expiration)
    {
        this->record_prune_metrics(0, 0);
        return (FT_ERR_SUCCESS);
    }
    const t_monotonic_time_point start_time = time_monotonic_point_now();
    int64_t removed_entries = 0;
    int64_t next_expiration = -1;
    ft_size_t original_lookup_index = ft_string::npos;
    if (lookup_index != ft_nullptr)
        original_lookup_index = *lookup_index;
    const int32_t result = this->_data.erase_if(
        [&](const Pair<ft_string, kv_store_entry> &entry, ft_size_t index) noexcept -> ft_bool
        {
            const ft_bool expired = entry.value._has_expiration && entry.value._expiration_timestamp <= current_time;
            if (lookup_index != ft_nullptr && index == original_lookup_index)
            {
                if (expired)
                    *lookup_index = ft_string::npos;
                else
                    *lookup_index = index - static_cast<ft_size_t>(removed_entries);
            }
            if (expired)
                ++removed_entries;
            else if (entry.value._has_expiration)
                next_expiration = storage_kv_earlier_expiration(next_expiration, entry.value._expiration_timestamp);
            return (expired);
        }, storage_kv_relocate_entry);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    this->_next_expiration = next_expiration;
    this->record_prune_metrics(removed_entries,
        time_monotonic_point_diff_ms(start_time, time_monotonic_point_now()));
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
    operations.push_back(ft_move(operation));
    operations_error = operations.get_error();
    if (operations_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (this->kv_apply(operations));
}

const char *kv_store::kv_get(const char *key_string) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_get");
    if (key_string == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t key_length = ft_strlen_size_t(key_string);
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_store(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    kv_store *mutable_this = const_cast<kv_store *>(this);
    const ft_size_t size = this->_data.size();
    const auto *end = this->_data.end();
    const auto *begin = end;
    if (size != 0)
        begin = end - size;
    const auto *cursor = begin;
    ft_size_t lookup_index = ft_string::npos;
    while (cursor != end)
    {
        if (cursor->key.size() == key_length
            && ft_memcmp(cursor->key.c_str(), key_string, key_length) == 0)
        {
            lookup_index = static_cast<ft_size_t>(cursor - begin);
            break ;
        }
        ++cursor;
    }
    // The lookup key may borrow storage that pruning is about to remove.
    if (mutable_this->prune_expired_locked(&lookup_index) != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (ft_nullptr);
    }
    if (lookup_index == ft_string::npos)
    {
        mutable_this->record_get_miss();
        this->unlock_store_guard(lock_acquired, FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    cursor = begin + lookup_index;
    const char *value_pointer = ft_nullptr;
    if (cursor->value.get_value_pointer(&value_pointer) != FT_ERR_SUCCESS)
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
    ft_vector<kv_store_snapshot_entry> entries;
    ft_bool lock_acquired = FT_FALSE;
    int32_t error_code = this->lock_store(&lock_acquired);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->export_snapshot_locked(entries);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, error_code);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_backend_type != KV_STORE_BACKEND_JSON)
    {
        error_code = this->flush_backend_entries(entries);
        this->unlock_store_guard(lock_acquired, error_code);
        return (error_code);
    }

    ft_string path;
    json_group *group = ft_nullptr;
    error_code = path.initialize(this->_file_path);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->build_json_entries(entries, &group);
    this->unlock_store_guard(lock_acquired, error_code);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    error_code = file_replace_stream(path.c_str(), FT_FALSE,
        [](int32_t descriptor, void *context) -> int32_t
        {
            return (json_write_to_descriptor(descriptor, static_cast<const json_group *>(context)));
        }, group);
    json_free_groups(group);
    if (error_code != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
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

int32_t kv_store::export_snapshot_locked(ft_vector<kv_store_snapshot_entry> &out_entries) const
{
    int32_t error_code = FT_ERR_SUCCESS;
    if (out_entries.is_initialised() != FT_CLASS_STATE_INITIALISED)
        error_code = out_entries.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = const_cast<kv_store *>(this)->prune_expired_locked();
    const Pair<ft_string, kv_store_entry> *cursor = ft_nullptr;
    const Pair<ft_string, kv_store_entry> *end = ft_nullptr;
    if (error_code == FT_ERR_SUCCESS)
    {
        const ft_size_t count = this->_data.size();
        error_code = this->_data.get_error();
        if (error_code == FT_ERR_SUCCESS)
        {
            out_entries.reserve(count);
            error_code = out_entries.get_error();
        }
        if (error_code == FT_ERR_SUCCESS)
        {
            out_entries.clear();
            error_code = out_entries.get_error();
        }
        if (error_code == FT_ERR_SUCCESS && count != 0)
        {
            end = this->_data.end();
            error_code = this->_data.get_error();
            if (error_code == FT_ERR_SUCCESS)
                cursor = end - count;
        }
    }
    while (error_code == FT_ERR_SUCCESS && cursor != end)
    {
        kv_store_snapshot_entry entry;
        error_code = entry.key.initialize(cursor->key);
        if (error_code == FT_ERR_SUCCESS)
            error_code = cursor->value.copy_value(entry.value);
        if (error_code == FT_ERR_SUCCESS)
            error_code = cursor->value.has_expiration(entry.has_expiration);
        entry.expiration_timestamp = 0;
        if (error_code == FT_ERR_SUCCESS && entry.has_expiration == FT_TRUE)
            error_code = cursor->value.get_expiration(entry.expiration_timestamp);
        if (error_code == FT_ERR_SUCCESS)
            error_code = out_entries.push_back(ft_move(entry));
        ++cursor;
    }
    return (error_code);
}

int32_t kv_store::export_snapshot(ft_vector<kv_store_snapshot_entry> &out_entries) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::export_snapshot");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_store(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->export_snapshot_locked(out_entries);
    this->unlock_store_guard(lock_acquired, result);
    if (result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
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

int32_t kv_store::apply_set_operation_locked(ft_map<ft_string, kv_store_entry> &data,
    const kv_store_operation &operation,
    kv_store_entry *existing_entry)
{
    if (operation._has_value == FT_FALSE || operation._value.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool has_expiration = FT_FALSE;
    int64_t expiration_timestamp = 0;
    if (operation._has_ttl == FT_TRUE
        && this->compute_expiration(operation._ttl_seconds, has_expiration, expiration_timestamp) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string replacement;
    int32_t error_code = replacement.initialize(operation._value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (existing_entry != ft_nullptr)
        error_code = existing_entry->commit_value(replacement, operation._has_ttl,
            has_expiration, expiration_timestamp);
    else
    {
        kv_store_entry entry;
        error_code = entry.initialize();
        if (error_code == FT_ERR_SUCCESS)
            error_code = entry.commit_value(replacement, FT_TRUE, has_expiration, expiration_timestamp);
        if (error_code == FT_ERR_SUCCESS)
        {
            data.insert(operation._key, ft_move(entry));
            error_code = data.get_error();
        }
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        if (has_expiration)
            this->_next_expiration = storage_kv_earlier_expiration(this->_next_expiration, expiration_timestamp);
        this->record_set_operation();
    }
    return (error_code);
}

struct storage_kv_staged_update
{
    kv_store_entry *entry;
    ft_bool update_expiration;
    ft_bool has_expiration;
    int64_t expiration_timestamp;
    ft_string value;

    int32_t initialize(storage_kv_staged_update &&other) noexcept;
};

int32_t storage_kv_staged_update::initialize(storage_kv_staged_update &&other) noexcept
{
    this->entry = other.entry;
    this->update_expiration = other.update_expiration;
    this->has_expiration = other.has_expiration;
    this->expiration_timestamp = other.expiration_timestamp;
    return (this->value.initialize(ft_move(other.value)));
}

int32_t kv_store::apply_existing_updates_locked(const ft_vector<kv_store_operation> &operations,
    ft_size_t operation_count)
{
    ft_vector<storage_kv_staged_update> updates;
    int32_t result = updates.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    auto *const end = this->_data.end();
    ft_size_t index = 0;
    while (index < operation_count)
    {
        const kv_store_operation &operation = operations[index];
        if (operation._type != KV_STORE_OPERATION_TYPE_SET)
            return (FT_ERR_UNSUPPORTED_TYPE);
        if (operation._key.is_initialised() == FT_FALSE || operation._key.empty()
            || operation._has_value == FT_FALSE || operation._value.is_initialised() == FT_FALSE)
            return (FT_ERR_INVALID_ARGUMENT);
        auto *entry = this->_data.find(operation._key);
        if (entry == end)
            return (FT_ERR_UNSUPPORTED_TYPE);
        if (index == 0)
        {
            updates.reserve(operation_count);
            if (updates.get_error() != FT_ERR_SUCCESS)
                return (updates.get_error());
        }
        storage_kv_staged_update update{&entry->value, operation._has_ttl, FT_FALSE, 0, {}};
        if (update.update_expiration)
            result = this->compute_expiration(operation._ttl_seconds, update.has_expiration, update.expiration_timestamp);
        if (result == FT_ERR_SUCCESS)
            result = update.value.initialize(operation._value);
        if (result == FT_ERR_SUCCESS)
            result = updates.push_back(ft_move(update));
        if (result != FT_ERR_SUCCESS)
            return (result);
        ++index;
    }

    // These entries are exclusively owned by the store. Commit only transfers buffers.
    auto *update = updates.begin();
    auto *const updates_end = updates.end();
    while (update != updates_end)
    {
        (void)update->entry->commit_value(update->value, update->update_expiration,
            update->has_expiration, update->expiration_timestamp);
        if (update->has_expiration)
            this->_next_expiration = storage_kv_earlier_expiration(this->_next_expiration, update->expiration_timestamp);
        this->record_set_operation();
        ++update;
    }
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::kv_apply(const ft_vector<kv_store_operation> &operations)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_apply");
    const ft_size_t operation_count = operations.size();
    ft_bool lock_acquired = FT_FALSE;
    int32_t error_code = this->lock_store(&lock_acquired);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->prune_expired_locked();
    if (error_code != FT_ERR_SUCCESS || operation_count == 0)
    {
        this->unlock_store_guard(lock_acquired, error_code);
        if (error_code != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        return (FT_ERR_SUCCESS);
    }
    if (operation_count == 1 && operations[0]._type == KV_STORE_OPERATION_TYPE_SET
        && operations[0]._key.is_initialised() == FT_TRUE && !operations[0]._key.empty())
    {
        const kv_store_operation &operation = operations[0];
        auto *end = this->_data.end();
        auto *entry = this->_data.find(operation._key);
        // Inserting into spare capacity leaves existing entries untouched on failure.
        if (entry != end || this->_data.size() < this->_data.capacity())
        {
            kv_store_entry *existing = ft_nullptr;
            if (entry != end)
                existing = &entry->value;
            error_code = this->apply_set_operation_locked(this->_data, operation, existing);
            this->unlock_store_guard(lock_acquired, error_code);
            if (error_code != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            return (this->notify_replication_listeners(operations));
        }
    }
    if (operation_count == 1 && operations[0]._type == KV_STORE_OPERATION_TYPE_DELETE
        && operations[0]._key.is_initialised() == FT_TRUE && !operations[0]._key.empty())
    {
        auto *const end = this->_data.end();
        auto *entry = this->_data.find(operations[0]._key);
        if (entry == end)
            error_code = FT_ERR_NOT_FOUND;
        else
        {
            error_code = storage_kv_erase_entry(this->_data, entry);
            if (error_code == FT_ERR_SUCCESS)
                this->record_delete_operation();
        }
        this->unlock_store_guard(lock_acquired, error_code);
        if (error_code != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        return (this->notify_replication_listeners(operations));
    }
    if (operation_count > 1)
    {
        error_code = this->apply_existing_updates_locked(operations, operation_count);
        if (error_code != FT_ERR_UNSUPPORTED_TYPE)
        {
            this->unlock_store_guard(lock_acquired, error_code);
            if (error_code != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            return (this->notify_replication_listeners(operations));
        }
    }

    ft_map<ft_string, kv_store_entry> replacement;
    error_code = replacement.copy_from(this->_data);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, error_code);
        return (FT_ERR_INVALID_OPERATION);
    }
    const int64_t set_operations = this->_metrics_set_operations;
    const int64_t delete_operations = this->_metrics_delete_operations;
    const int64_t next_expiration = this->_next_expiration;
    ft_size_t operation_index = 0;
    while (operation_index < operation_count)
    {
        const kv_store_operation &operation = operations[operation_index];
        if (operation._key.is_initialised() == FT_FALSE || operation._key.empty())
        {
            error_code = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        auto *end = replacement.end();
        auto *entry = replacement.find(operation._key);
        if (operation._type == KV_STORE_OPERATION_TYPE_DELETE)
        {
            if (entry == end)
            {
                error_code = FT_ERR_NOT_FOUND;
                break ;
            }
            error_code = storage_kv_erase_entry(replacement, entry);
            if (error_code == FT_ERR_SUCCESS)
                this->record_delete_operation();
        }
        else
        {
            kv_store_entry *existing = ft_nullptr;
            if (entry != end)
                existing = &entry->value;
            error_code = this->apply_set_operation_locked(replacement, operation, existing);
        }
        if (error_code != FT_ERR_SUCCESS)
            break ;
        ++operation_index;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_data.move_from(replacement);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_metrics_set_operations = set_operations;
        this->_metrics_delete_operations = delete_operations;
        this->_next_expiration = next_expiration;
        this->unlock_store_guard(lock_acquired, error_code);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (this->notify_replication_listeners(operations));
}

int32_t kv_store::kv_compare_and_swap(const char *key_string, const char *expected_value, const char *new_value, int64_t ttl_seconds)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::kv_compare_and_swap");
    ft_string key_storage;
    Pair<ft_string, kv_store_entry> *existing_pair;
    Pair<ft_string, kv_store_entry> *map_end;
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error;

    if (key_string == ft_nullptr || expected_value == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    if (key_storage.initialize(key_string) != FT_ERR_SUCCESS)
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
    existing_pair = this->_data.find(key_storage);
    map_end = this->_data.end();
    if (existing_pair == map_end)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_NOT_FOUND);
        return (FT_ERR_INVALID_OPERATION);
    }
    const char *current_value = ft_nullptr;
    if (existing_pair->value.get_value_pointer(&current_value) != FT_ERR_SUCCESS)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (ft_strcmp(current_value, expected_value) != 0)
    {
        this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (new_value == ft_nullptr)
    {
        const int32_t result = storage_kv_erase_entry(this->_data, existing_pair);
        if (result != FT_ERR_SUCCESS)
        {
            this->unlock_store_guard(lock_acquired, result);
            return (FT_ERR_INVALID_OPERATION);
        }
        this->record_delete_operation();
    }
    else
    {
        ft_bool has_expiration = FT_FALSE;
        int64_t expiration_timestamp = 0;
        ft_string replacement;
        int32_t result = this->compute_expiration(ttl_seconds, has_expiration, expiration_timestamp);
        if (result == FT_ERR_SUCCESS)
            result = replacement.initialize(new_value);
        if (result == FT_ERR_SUCCESS)
            result = existing_pair->value.commit_value(replacement, FT_TRUE, has_expiration, expiration_timestamp);
        if (result != FT_ERR_SUCCESS)
        {
            this->unlock_store_guard(lock_acquired, FT_ERR_INVALID_OPERATION);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (has_expiration)
            this->_next_expiration = storage_kv_earlier_expiration(this->_next_expiration, expiration_timestamp);
        this->record_set_operation();
    }
    this->unlock_store_guard(lock_acquired, FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::apply_snapshot_locked(const ft_vector<kv_store_snapshot_entry> &entries)
{
    const ft_size_t count = entries.size();
    if (entries.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (this->apply_snapshot_entries_locked(entries.begin(), count));
}

int32_t kv_store::apply_loaded_snapshot_locked(ft_vector<kv_store_snapshot_entry> &entries)
{
    const ft_size_t count = entries.size();
    if (entries.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    return (this->apply_snapshot_entries_locked(entries.begin(), count));
}

template <typename Entry>
int32_t kv_store::apply_snapshot_entries_locked(Entry *cursor, ft_size_t count)
{
    if (count != 0 && cursor == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    ft_map<ft_string, kv_store_entry> replacement(count);
    if (replacement.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    ft_size_t remaining = count;
    int64_t next_expiration = -1;
    while (remaining != 0)
    {
        if (cursor->key.is_initialised() == FT_FALSE || cursor->value.is_initialised() == FT_FALSE)
            return (FT_ERR_INVALID_OPERATION);
        kv_store_entry value;
        if (value.initialize() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        int64_t expiration_timestamp = 0;
        if (cursor->has_expiration == FT_TRUE)
        {
            expiration_timestamp = cursor->expiration_timestamp;
            next_expiration = storage_kv_earlier_expiration(next_expiration, expiration_timestamp);
        }
        if constexpr (std::is_const<Entry>::value)
        {
            if (value.set_value(cursor->value) != FT_ERR_SUCCESS
                || value.configure_expiration(cursor->has_expiration, expiration_timestamp) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
        }
        else if (value.commit_value(cursor->value, FT_TRUE, cursor->has_expiration,
                     expiration_timestamp) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        replacement.insert(cursor->key, ft_move(value));
        if (replacement.get_error() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        ++cursor;
        --remaining;
    }
    if (this->_data.move_from(replacement) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    this->_next_expiration = next_expiration;
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
    if (interval_seconds <= 0 || this->_mutex == ft_nullptr || this->_background_mutex == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    constexpr uint32_t MILLISECONDS_PER_SECOND = 1000U;
    uint32_t interval_milliseconds = UINT32_MAX;
    if (interval_seconds <= UINT32_MAX / MILLISECONDS_PER_SECOND)
        interval_milliseconds = static_cast<uint32_t>(interval_seconds) * MILLISECONDS_PER_SECOND;
    int32_t result = pt_recursive_mutex_lock_if_not_null(this->_background_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->start_background_thread_locked(interval_milliseconds);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
    return (result);
}

int32_t kv_store::stop_background_compaction()
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::stop_background_compaction");
    int32_t result = pt_recursive_mutex_lock_if_not_null(this->_background_mutex);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (this->_background_wake != ft_nullptr)
    {
        result = this->_background_wake->count_down();
        if (result == FT_ERR_SUCCESS)
        {
            this->_background_thread.join();
            if (this->_background_thread.joinable())
                result = FT_ERR_INVALID_OPERATION;
            else
            {
                delete this->_background_wake;
                this->_background_wake = ft_nullptr;
            }
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_background_mutex);
    return (result);
}
