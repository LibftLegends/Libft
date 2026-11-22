#include "api_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Template/move.hpp"
#include "../PThread/pthread_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

api_connection_pool_handle::api_connection_pool_handle()
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _mutex(ft_nullptr), key(), socket(),
      security_mode(api_connection_security_mode::PLAIN),
      has_socket(FT_FALSE), from_pool(FT_FALSE), should_store(FT_FALSE),
      negotiated_http2(FT_FALSE), plain_socket_timed_out(FT_FALSE),
      plain_socket_validated(FT_FALSE)
{
#if NETWORKING_HAS_OPENSSL
    this->tls_session = ft_nullptr;
    this->tls_context = ft_nullptr;
#endif
    return ;
}

api_connection_pool_handle::~api_connection_pool_handle()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

void api_connection_pool_handle::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

void api_connection_pool_handle::abort_if_not_initialised(const char *method_name) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, method_name);
    return ;
}

int32_t api_connection_pool_handle::enable_thread_safety() noexcept
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    this->abort_if_not_initialised("api_connection_pool_handle::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (initialize_result);
    }
    this->_mutex = new_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t api_connection_pool_handle::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    this->abort_if_not_initialised("api_connection_pool_handle::disable_thread_safety");
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (destroy_result);
    return (FT_ERR_SUCCESS);
}

ft_bool api_connection_pool_handle::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

void api_connection_pool_handle::reset_connection_state() noexcept
{
#if NETWORKING_HAS_OPENSSL
    this->tls_session = ft_nullptr;
    this->tls_context = ft_nullptr;
#endif
    this->security_mode = api_connection_security_mode::PLAIN;
    this->has_socket = FT_FALSE;
    this->from_pool = FT_FALSE;
    this->should_store = FT_FALSE;
    this->negotiated_http2 = FT_FALSE;
    this->plain_socket_timed_out = FT_FALSE;
    this->plain_socket_validated = FT_FALSE;
    return ;
}

int32_t api_connection_pool_handle::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->abort_lifecycle_error("api_connection_pool_handle::initialize",
            "initialize called on initialised instance");
    const int32_t key_error = this->key.initialize();
    if (key_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (key_error);
    }
    this->reset_connection_state();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t api_connection_pool_handle::initialize(
    const api_connection_pool_handle &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        other.abort_lifecycle_error("api_connection_pool_handle::initialize(copy)",
            "source is uninitialised");
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);

    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = other.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    int32_t key_error = this->key.initialize();
    const ft_size_t key_length = other.key.size();
    if (key_error == FT_ERR_SUCCESS && key_length != 0)
        key_error = this->key.assign(other.key.c_str(), key_length);
    if (key_error == FT_ERR_SUCCESS)
    {
        this->security_mode = other.security_mode;
        this->should_store = other.should_store;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    else
        (void)this->key.destroy();
    other.unlock(lock_acquired);
    return (key_error);
}

int32_t api_connection_pool_handle::initialize(
    api_connection_pool_handle &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        other.abort_lifecycle_error("api_connection_pool_handle::initialize(move)",
            "source is uninitialised");
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);

    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = other.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);

    int32_t move_error = this->key.move(other.key);
    if (move_error == FT_ERR_SUCCESS && other.has_socket)
    {
        move_error = this->socket.move(other.socket);
        if (move_error != FT_ERR_SUCCESS)
            (void)other.key.move(this->key);
    }
    if (move_error != FT_ERR_SUCCESS)
    {
        other.unlock(lock_acquired);
        (void)this->key.destroy();
        (void)this->socket.destroy();
        return (move_error);
    }

#if NETWORKING_HAS_OPENSSL
    this->tls_session = other.tls_session;
    this->tls_context = other.tls_context;
#endif
    this->security_mode = other.security_mode;
    this->has_socket = other.has_socket;
    this->from_pool = other.from_pool;
    this->should_store = other.should_store;
    this->negotiated_http2 = other.negotiated_http2;
    this->plain_socket_timed_out = other.plain_socket_timed_out;
    this->plain_socket_validated = other.plain_socket_validated;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other.reset_connection_state();
    other.unlock(lock_acquired);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t api_connection_pool_handle::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    int32_t first_error = this->disable_thread_safety();
#if NETWORKING_HAS_OPENSSL
    if (this->tls_session && api_connection_pool_untrack_tls_session(this->tls_session))
    {
        SSL_free(this->tls_session);
        SSL_CTX_free(this->tls_context);
    }
#endif
    const int32_t socket_error = this->socket.destroy();
    if (first_error == FT_ERR_SUCCESS)
        first_error = socket_error;
    const int32_t key_error = this->key.destroy();
    if (first_error == FT_ERR_SUCCESS)
        first_error = key_error;
    this->reset_connection_state();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

uint32_t api_connection_pool_handle::move(
    api_connection_pool_handle &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(ft_move(other))));
}

int32_t api_connection_pool_handle::lock(ft_bool *lock_acquired) const
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    this->abort_if_not_initialised("api_connection_pool_handle::lock");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void api_connection_pool_handle::unlock(ft_bool lock_acquired) const
{
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

#ifdef LIBFT_TEST_BUILD
pt_recursive_mutex *api_connection_pool_handle::get_mutex_for_validation() const noexcept
{
    this->abort_if_not_initialised("api_connection_pool_handle::get_mutex_for_validation");
    return (this->_mutex);
}
#endif
