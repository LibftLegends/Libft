#include "cancellation.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"

int32_t ft_cancellation_state::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

int32_t ft_cancellation_source::set_error(int32_t error_code) noexcept
{
    _last_error = error_code;
    return (error_code);
}

int32_t ft_cancellation_state::lock_internal(ft_bool *lock_acquired) const
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_state::unlock_internal(ft_bool lock_acquired) const
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

ft_cancellation_state::ft_cancellation_state() noexcept
    : _cancelled(FT_FALSE), _callbacks(), _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_cancellation_state::~ft_cancellation_state() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_cancellation_state::initialize() noexcept
{
    int32_t callbacks_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_cancellation_state::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_cancelled.store(FT_FALSE);
    callbacks_initialize_error = this->_callbacks.initialize();
    if (callbacks_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (callbacks_initialize_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_state::destroy() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t disable_error;
    int32_t callbacks_destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    callbacks_destroy_error = FT_ERR_SUCCESS;
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error == FT_ERR_SUCCESS)
    {
        callbacks_destroy_error = this->_callbacks.destroy();
        this->_cancelled.store(FT_FALSE);
    }
    (void)this->unlock_internal(lock_acquired);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    return (callbacks_destroy_error);
}

int32_t ft_cancellation_state::move(ft_cancellation_state &other) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "ft_cancellation_state::move",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    ft_size_t index;
    int32_t callback_error;

    this->_cancelled = other._cancelled.load();
    this->_callbacks.clear();
    index = 0;
    while (index < other._callbacks.size())
    {
        callback_error = this->_callbacks.push_back(other._callbacks[index]);
        if (callback_error != FT_ERR_SUCCESS)
        {
            (void)this->unlock_internal(lock_acquired);
            (void)this->destroy();
            return (callback_error);
        }
        index += 1;
    }
    (void)this->unlock_internal(lock_acquired);
    other._cancelled.store(FT_FALSE);
    other._callbacks.clear();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_state::register_callback(const ft_function<void()> &callback) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t callback_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_state::register_callback");
    if (!callback)
        return (FT_ERR_INVALID_ARGUMENT);
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_cancelled.load() == FT_TRUE)
    {
        (void)this->unlock_internal(lock_acquired);
        callback();
        return (FT_ERR_SUCCESS);
    }
    callback_error = this->_callbacks.push_back(callback);
    (void)this->unlock_internal(lock_acquired);
    return (callback_error);
}

int32_t ft_cancellation_state::request_cancel() noexcept
{
    ft_bool lock_acquired;
    ft_size_t index;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_state::request_cancel");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (set_error(lock_error));
    if (this->_cancelled.load() == FT_TRUE)
    {
        (void)this->unlock_internal(lock_acquired);
        return (set_error(FT_ERR_SUCCESS));
    }
    this->_cancelled.store(FT_TRUE);
    const ft_size_t count = this->_callbacks.size();
    index = 0;
    if (count != 0U)
    {
        ft_function<void()> *callbacks = this->_callbacks.begin();
        while (index < count)
            callbacks[index++]();
        this->_callbacks.clear();
    }
    (void)this->unlock_internal(lock_acquired);
    return (set_error(FT_ERR_SUCCESS));
}

ft_bool ft_cancellation_state::is_cancelled() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_state::is_cancelled");
    return (this->_cancelled.load());
}

int32_t ft_cancellation_state::enable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_state::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_ALREADY_INITIALISED);
    this->_mutex = new (std::nothrow) pt_recursive_mutex();
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_mutex->initialize() != FT_ERR_SUCCESS)
    {
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        return (FT_ERR_NO_MEMORY);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_state::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_error;

    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    return (destroy_error);
}

int32_t ft_cancellation_state::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_cancellation_state::get_error");
    return (_last_error);
}

const char *ft_cancellation_state::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_cancellation_state::get_error_str");
    return (ft_strerror(_last_error));
}

ft_cancellation_token::ft_cancellation_token(
    ft_cancellation_state *state_pointer) noexcept
    : _state(state_pointer)
{
    return ;
}

ft_cancellation_token::ft_cancellation_token() noexcept : _state(ft_nullptr)
{
    return ;
}

ft_cancellation_token::ft_cancellation_token(
    const ft_cancellation_token &other) noexcept : _state(other._state)
{
    return ;
}

ft_cancellation_token::ft_cancellation_token(
    ft_cancellation_token &&other) noexcept : _state(other._state)
{
    other._state = ft_nullptr;
    return ;
}

ft_cancellation_token::~ft_cancellation_token() noexcept
{
    return ;
}

ft_bool ft_cancellation_token::is_valid() const noexcept
{
    return (this->_state != ft_nullptr);
}

ft_bool ft_cancellation_token::is_cancellation_requested() const noexcept
{
    if (this->_state == ft_nullptr)
        return (FT_FALSE);
    return (this->_state->is_cancelled());
}

int32_t ft_cancellation_token::register_callback(const ft_function<void()> &callback) const noexcept
{
    if (this->_state == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (this->_state->register_callback(callback));
}

ft_cancellation_source::ft_cancellation_source() noexcept
    : _state(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_cancellation_source::~ft_cancellation_source() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_cancellation_source::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_cancellation_source::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_state = new (std::nothrow) ft_cancellation_state();
    if (this->_state == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    if (this->_state->initialize() != FT_ERR_SUCCESS)
    {
        delete this->_state;
        this->_state = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_STATE);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_source::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->_state != ft_nullptr)
    {
        (void)this->_state->destroy();
        delete this->_state;
        this->_state = ft_nullptr;
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cancellation_source::move(ft_cancellation_source &other) noexcept
{
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(
            other._initialised_state,
            "ft_cancellation_source::move",
            "called with source object that is not initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    this->_state = other._state;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._state = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ft_cancellation_token ft_cancellation_source::get_token() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_source::get_token");
    return (ft_cancellation_token(this->_state));
}

int32_t ft_cancellation_source::request_cancel() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_source::request_cancel");
    if (this->_state == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (this->_state->request_cancel());
}

ft_bool ft_cancellation_source::is_cancellation_requested() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_cancellation_source::is_cancellation_requested");
    if (this->_state == ft_nullptr)
        return (FT_FALSE);
    return (this->_state->is_cancelled());
}

int32_t ft_cancellation_source::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_cancellation_source::get_error");
    return (_last_error);
}

const char *ft_cancellation_source::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "ft_cancellation_source::get_error_str");
    return (ft_strerror(_last_error));
}

thread_local int32_t ft_cancellation_state::_last_error = FT_ERR_SUCCESS;
thread_local int32_t ft_cancellation_source::_last_error = FT_ERR_SUCCESS;
