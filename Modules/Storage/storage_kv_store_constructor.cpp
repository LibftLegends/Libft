#include "kv_store.hpp"

#include <cstddef>
#include <cstring>
#include <new>

#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"

kv_store::kv_store() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _data()
    , _file_path()
    , _encryption_key()
    , _encryption_algorithm_name()
    , _encryption_enabled(FT_FALSE)
    , _backend_type(KV_STORE_BACKEND_JSON)
    , _next_expiration(-1)
    , _background_interval_milliseconds(0)
    , _background_wake(ft_nullptr)
    , _background_thread()
    , _background_mutex(ft_nullptr)
    , _mutex(ft_nullptr)
    , _metrics_set_operations(0)
    , _metrics_delete_operations(0)
    , _metrics_get_hits(0)
    , _metrics_get_misses(0)
    , _metrics_prune_operations(0)
    , _metrics_pruned_entries(0)
    , _metrics_total_prune_duration_ms(0)
    , _metrics_last_prune_duration_ms(0)
    , _replication_sinks()
    , _replication_operation_count(0)
    , _replication_mutex(ft_nullptr)
{
    return ;
}

int32_t kv_store::initialize_members() noexcept
{
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->_backend_type = KV_STORE_BACKEND_JSON;
    int32_t result = this->_file_path.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_key.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_algorithm_name.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_replication_sinks.initialize();
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (result);
}

int32_t kv_store::copy_configuration_locked(const kv_store &other) noexcept
{
    int32_t result = this->_file_path.append(other._file_path);
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_key.append(other._encryption_key);
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_algorithm_name.append(other._encryption_algorithm_name);
    if (result == FT_ERR_SUCCESS)
    {
        this->_encryption_enabled = other._encryption_enabled;
        this->_backend_type = other._backend_type;
    }
    return (result);
}

int32_t kv_store::initialize(const kv_store &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "kv_store::initialize(const kv_store &)", "source is uninitialised");
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    result = this->initialize_members();
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_bool lock_acquired = FT_FALSE;
    result = other.lock_store(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->copy_configuration_locked(other);
    if (result == FT_ERR_SUCCESS)
    {
        const ft_size_t count = other._data.size();
        ft_map<ft_string, kv_store_entry> replacement(count);
        result = replacement.initialize();
        const Pair<ft_string, kv_store_entry> *const end = other._data.end();
        const Pair<ft_string, kv_store_entry> *entry = end;
        if (count != 0)
            entry -= count;
        while (result == FT_ERR_SUCCESS && entry != end)
        {
            replacement.insert(entry->key, entry->value);
            result = replacement.get_error();
            ++entry;
        }
        if (result == FT_ERR_SUCCESS)
            result = this->_data.move_from(replacement);
        if (result == FT_ERR_SUCCESS)
            this->_next_expiration = other._next_expiration;
    }
    other.unlock_store_guard(lock_acquired, result);
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (result);
}

int32_t kv_store::initialize(kv_store &&other) noexcept
{
    return (this->move(other));
}

int32_t kv_store::move(kv_store &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "kv_store::move",
            "source is uninitialised");
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    result = this->initialize_members();
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_bool lock_acquired = FT_FALSE;
    result = other.lock_store(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->copy_configuration_locked(other);
    other.unlock_store_guard(lock_acquired, result);
    if (result == FT_ERR_SUCCESS)
        result = other.stop_background_compaction();
    if (result == FT_ERR_SUCCESS)
    {
        result = other.lock_store(&lock_acquired);
        if (result == FT_ERR_SUCCESS)
            result = this->_data.move_from(other._data);
        if (result == FT_ERR_SUCCESS)
            this->_next_expiration = other._next_expiration;
        other.unlock_store_guard(lock_acquired, result);
    }
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (result);
}

int32_t kv_store::enable_thread_safety() noexcept
{
    pt_recursive_mutex *store_mutex;
    pt_recursive_mutex *background_mutex;
    pt_recursive_mutex *replication_mutex;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::enable_thread_safety");
    if (this->_mutex != ft_nullptr && this->_background_mutex != ft_nullptr
        && this->_replication_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    store_mutex = new (std::nothrow) pt_recursive_mutex();
    if (store_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = store_mutex->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete store_mutex;
        return (mutex_error);
    }
    background_mutex = new (std::nothrow) pt_recursive_mutex();
    if (background_mutex == ft_nullptr)
    {
        (void)store_mutex->destroy();
        delete store_mutex;
        return (FT_ERR_NO_MEMORY);
    }
    mutex_error = background_mutex->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete background_mutex;
        (void)store_mutex->destroy();
        delete store_mutex;
        return (mutex_error);
    }
    replication_mutex = new (std::nothrow) pt_recursive_mutex();
    if (replication_mutex == ft_nullptr)
    {
        (void)background_mutex->destroy();
        delete background_mutex;
        (void)store_mutex->destroy();
        delete store_mutex;
        return (FT_ERR_NO_MEMORY);
    }
    mutex_error = replication_mutex->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete replication_mutex;
        (void)background_mutex->destroy();
        delete background_mutex;
        (void)store_mutex->destroy();
        delete store_mutex;
        return (mutex_error);
    }
    this->_mutex = store_mutex;
    this->_background_mutex = background_mutex;
    this->_replication_mutex = replication_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store::disable_thread_safety() noexcept
{
    int32_t error_code;
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::disable_thread_safety");
    error_code = FT_ERR_SUCCESS;
    if (this->_background_wake != ft_nullptr)
    {
        error_code = this->stop_background_compaction();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (this->_replication_mutex != ft_nullptr)
    {
        destroy_error = this->_replication_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        delete this->_replication_mutex;
        this->_replication_mutex = ft_nullptr;
    }
    if (this->_background_mutex != ft_nullptr)
    {
        destroy_error = this->_background_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        delete this->_background_mutex;
        this->_background_mutex = ft_nullptr;
    }
    if (this->_mutex != ft_nullptr)
    {
        destroy_error = this->_mutex->destroy();
        if (destroy_error != FT_ERR_SUCCESS && error_code == FT_ERR_SUCCESS)
            error_code = destroy_error;
        delete this->_mutex;
        this->_mutex = ft_nullptr;
    }
    return (error_code);
}

ft_bool kv_store::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t kv_store::initialize(const char *file_path, const char *encryption_key,
    ft_bool enable_encryption, const char *encryption_algorithm_name)
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "kv_store::initialize", "initialize called on initialised instance");
    if (file_path == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    int32_t result = this->initialize_members();
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = this->_file_path.assign(file_path, ft_strlen_size_t(file_path));
    if (result == FT_ERR_SUCCESS)
        result = this->configure_encryption(encryption_key, enable_encryption, encryption_algorithm_name);

    json_group *group = ft_nullptr;
    if (result == FT_ERR_SUCCESS)
    {
        group = json_read_from_file(file_path);
        if (group == ft_nullptr)
            result = FT_ERR_IO;
    }
    ft_vector<kv_store_snapshot_entry> entries;
    if (result == FT_ERR_SUCCESS)
        result = entries.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->parse_json_groups(group, entries);
    json_free_groups(group);
    if (result == FT_ERR_SUCCESS)
        result = this->apply_loaded_snapshot_locked(entries);
    if (result == FT_ERR_SUCCESS)
        result = this->prune_expired();
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (result);
}

int32_t kv_store::destroy()
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const int32_t disable_error = this->disable_thread_safety();
    if (this->_background_wake != ft_nullptr)
        return (disable_error);
    const int32_t errors[] = {
        disable_error,
        this->_data.destroy(),
        this->_replication_sinks.destroy(),
        this->_file_path.destroy(),
        this->_encryption_key.destroy(),
        this->_encryption_algorithm_name.destroy()
    };
    int32_t first_error = FT_ERR_SUCCESS;
    ft_size_t index = 0;
    while (first_error == FT_ERR_SUCCESS && index < sizeof(errors) / sizeof(errors[0]))
    {
        first_error = errors[index];
        ++index;
    }
    this->_encryption_enabled = FT_FALSE;
    this->_next_expiration = -1;
    this->_replication_operation_count.store(0, std::memory_order_relaxed);
    this->_metrics_set_operations = 0;
    this->_metrics_delete_operations = 0;
    this->_metrics_get_hits = 0;
    this->_metrics_get_misses = 0;
    this->_metrics_prune_operations = 0;
    this->_metrics_pruned_entries = 0;
    this->_metrics_total_prune_duration_ms = 0;
    this->_metrics_last_prune_duration_ms = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

kv_store::~kv_store() noexcept
{
    (void)this->destroy();
    return ;
}
