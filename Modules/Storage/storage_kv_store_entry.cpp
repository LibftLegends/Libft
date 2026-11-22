#include "kv_store.hpp"

#include "../Errno/errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Template/move.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

int32_t s_kv_store_snapshot_entry::initialize(
    const s_kv_store_snapshot_entry &other) noexcept
{
    int32_t error_code;

    error_code = this->key.initialize(other.key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->value.initialize(other.value);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->key.destroy();
        return (error_code);
    }
    this->has_expiration = other.has_expiration;
    this->expiration_timestamp = other.expiration_timestamp;
    return (FT_ERR_SUCCESS);
}

int32_t s_kv_store_snapshot_entry::initialize(
    s_kv_store_snapshot_entry &&other) noexcept
{
    int32_t error_code;

    error_code = this->key.initialize(ft_move(other.key));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->value.initialize(ft_move(other.value));
    if (error_code != FT_ERR_SUCCESS)
    {
        this->key.destroy();
        return (error_code);
    }
    this->has_expiration = other.has_expiration;
    this->expiration_timestamp = other.expiration_timestamp;
    other.has_expiration = FT_FALSE;
    other.expiration_timestamp = 0;
    return (FT_ERR_SUCCESS);
}

int32_t s_kv_store_snapshot_entry::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->key.destroy();
    error_code = this->value.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    this->has_expiration = FT_FALSE;
    this->expiration_timestamp = 0;
    return (first_error);
}

int32_t s_kv_store_operation::initialize(
    const s_kv_store_operation &other) noexcept
{
    int32_t error_code;

    error_code = this->_key.initialize(other._key);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_value.initialize(other._value);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_key.destroy();
        return (error_code);
    }
    this->_type = other._type;
    this->_has_value = other._has_value;
    this->_has_ttl = other._has_ttl;
    this->_ttl_seconds = other._ttl_seconds;
    return (FT_ERR_SUCCESS);
}

int32_t s_kv_store_operation::initialize(
    s_kv_store_operation &&other) noexcept
{
    int32_t error_code;

    error_code = this->_key.initialize(ft_move(other._key));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->_value.initialize(ft_move(other._value));
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_key.destroy();
        return (error_code);
    }
    this->_type = other._type;
    this->_has_value = other._has_value;
    this->_has_ttl = other._has_ttl;
    this->_ttl_seconds = other._ttl_seconds;
    other._has_value = FT_FALSE;
    other._has_ttl = FT_FALSE;
    other._ttl_seconds = -1;
    return (FT_ERR_SUCCESS);
}

int32_t s_kv_store_operation::destroy() noexcept
{
    int32_t first_error;
    int32_t error_code;

    first_error = this->_key.destroy();
    error_code = this->_value.destroy();
    if (first_error == FT_ERR_SUCCESS && error_code != FT_ERR_SUCCESS)
        first_error = error_code;
    this->_type = KV_STORE_OPERATION_TYPE_SET;
    this->_has_value = FT_FALSE;
    this->_has_ttl = FT_FALSE;
    this->_ttl_seconds = -1;
    return (first_error);
}

kv_store_entry::kv_store_entry() noexcept
    : _value()
    , _has_expiration(FT_FALSE)
    , _expiration_timestamp(0)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
    , _mutex(ft_nullptr)
{
    return ;
}

kv_store_entry::~kv_store_entry() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t kv_store_entry::move(kv_store_entry &other) noexcept
{
    return (this->initialize(static_cast<kv_store_entry &&>(other)));
}

int32_t kv_store_entry::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool kv_store_entry::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t kv_store_entry::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state,
            "kv_store_entry::initialize", "initialize called on initialised instance");
    const int32_t error_code = this->_value.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (error_code);
    }
    this->_has_expiration = FT_FALSE;
    this->_expiration_timestamp = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::initialize(const kv_store_entry &other) noexcept
{
    int32_t init_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "kv_store_entry::initialize(const kv_store_entry &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        init_error = this->destroy();
        if (init_error != FT_ERR_SUCCESS)
            return (init_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    init_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (init_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (init_error);
    }
    init_error = this->_value.initialize(other._value);
    if (init_error == FT_ERR_SUCCESS)
    {
        this->_has_expiration = other._has_expiration;
        this->_expiration_timestamp = other._expiration_timestamp;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    else
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    if (init_error != FT_ERR_SUCCESS)
        return (init_error);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::initialize(kv_store_entry &&other) noexcept
{
    int32_t init_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "kv_store_entry::initialize(kv_store_entry &&)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        init_error = this->destroy();
        if (init_error != FT_ERR_SUCCESS)
            return (init_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    init_error = this->_value.initialize(ft_move(other._value));
    if (init_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (init_error);
    }
    this->_has_expiration = other._has_expiration;
    this->_expiration_timestamp = other._expiration_timestamp;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    (void)other._value.destroy();
    other._has_expiration = FT_FALSE;
    other._expiration_timestamp = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::destroy() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    destroy_error = this->disable_thread_safety();
    const int32_t value_error = this->_value.destroy();
    this->_has_expiration = FT_FALSE;
    this->_expiration_timestamp = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    return (value_error);
}

int32_t kv_store_entry::commit_value(ft_string &value, ft_bool update_expiration,
    ft_bool has_expiration, int64_t expiration_timestamp) noexcept
{
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t error_code = this->_value.move(value);
    if (error_code == FT_ERR_SUCCESS && update_expiration == FT_TRUE)
    {
        this->_has_expiration = has_expiration;
        this->_expiration_timestamp = expiration_timestamp;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (error_code);
}

int32_t kv_store_entry::set_value(const ft_string &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::set_value(const ft_string &)");
    ft_string replacement;
    const int32_t error_code = replacement.initialize(value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->commit_value(replacement, FT_FALSE, FT_FALSE, 0));
}

int32_t kv_store_entry::set_value(const char *value_string) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::set_value(const char *)");
    if (value_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string replacement;
    const int32_t error_code = replacement.initialize(value_string);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (this->commit_value(replacement, FT_FALSE, FT_FALSE, 0));
}

int32_t kv_store_entry::copy_value(ft_string &destination) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::copy_value");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    destination = this->_value;
    const int32_t error_code = destination.get_error();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (error_code);
}

int32_t kv_store_entry::get_value_pointer(const char **value_pointer) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::get_value_pointer");
    if (value_pointer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    *value_pointer = this->_value.c_str();
    if (*value_pointer == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::configure_expiration(ft_bool has_expiration,
    int64_t expiration_timestamp) noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::configure_expiration");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_has_expiration = has_expiration;
    if (has_expiration)
        this->_expiration_timestamp = expiration_timestamp;
    else
        this->_expiration_timestamp = 0;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::has_expiration(ft_bool &has_expiration) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::has_expiration");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    has_expiration = this->_has_expiration;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::get_expiration(int64_t &expiration_timestamp) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::get_expiration");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    expiration_timestamp = this->_expiration_timestamp;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t kv_store_entry::is_expired(int64_t current_time, ft_bool &expired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "kv_store_entry::is_expired");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_has_expiration == FT_FALSE)
    {
        expired = FT_FALSE;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_SUCCESS);
    }
    if (current_time < 0)
    {
        expired = FT_FALSE;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    expired = (this->_expiration_timestamp <= current_time);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}
