#include <cstdlib>

#include "logger_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Template/move.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

static std::atomic<ft_logger *> g_logger{nullptr};
static pthread_mutex_t g_logger_mutex = PTHREAD_MUTEX_INITIALIZER;

void logger_set_global_logging(ft_bool allocation, ft_bool enable)
{
    if (g_logger.load(std::memory_order_acquire) == ft_nullptr
        || pthread_mutex_lock(&g_logger_mutex) != 0)
        return ;
    ft_logger *logger = g_logger.load(std::memory_order_relaxed);
    if (logger != ft_nullptr)
    {
        if (allocation != FT_FALSE)
            logger->set_alloc_logging(enable);
        else
            logger->set_api_logging(enable);
    }
    (void)pthread_mutex_unlock(&g_logger_mutex);
    return ;
}

ft_bool logger_get_global_logging(ft_bool allocation)
{
    if (g_logger.load(std::memory_order_acquire) == ft_nullptr
        || pthread_mutex_lock(&g_logger_mutex) != 0)
        return (FT_FALSE);
    ft_logger *logger = g_logger.load(std::memory_order_relaxed);
    ft_bool enabled = FT_FALSE;
    if (logger != ft_nullptr)
    {
        if (allocation != FT_FALSE)
            enabled = logger->get_alloc_logging();
        else
            enabled = logger->get_api_logging();
    }
    (void)pthread_mutex_unlock(&g_logger_mutex);
    return (enabled);
}

void logger_report_sink_error(int32_t error)
{
    if (g_logger.load(std::memory_order_acquire) == ft_nullptr
        || pthread_mutex_lock(&g_logger_mutex) != 0)
        return ;
    ft_logger *logger = g_logger.load(std::memory_order_relaxed);
    if (logger != ft_nullptr)
        logger->set_error(error);
    (void)pthread_mutex_unlock(&g_logger_mutex);
    return ;
}

ft_logger::ft_logger() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr),
      _thread_safe_enabled(FT_FALSE), _alloc_logging(FT_FALSE),
      _api_logging(FT_FALSE), _error_code(FT_ERR_SUCCESS)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_logger::~ft_logger() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_logger::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_logger::initialize",
            "already initialised");
    this->_mutex = ft_nullptr;
    this->_thread_safe_enabled = FT_FALSE;
    this->_alloc_logging = FT_FALSE;
    this->_api_logging = FT_FALSE;
    this->set_error(FT_ERR_SUCCESS);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_logger::initialize(const char *path, ft_size_t max_size,
        t_log_level level) noexcept
{
    int32_t operation_result;

    operation_result = this->initialize();
    if (operation_result != FT_ERR_SUCCESS)
        return (operation_result);
    if (path != ft_nullptr)
    {
        operation_result = this->set_file(path, max_size);
        if (operation_result != FT_ERR_SUCCESS)
        {
            const int32_t file_error = this->get_error();
            (void)this->release_instance(FT_FALSE);
            this->set_error(file_error);
            return (operation_result);
        }
    }
    ft_log_set_level(level);
    return (FT_ERR_SUCCESS);
}

int32_t ft_logger::initialize(const ft_logger &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "ft_logger::initialize(copy)",
            "source object is uninitialised");
    const int32_t release_error = this->release_instance(FT_FALSE);
    if (release_error != FT_ERR_SUCCESS)
        return (release_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    int32_t error = this->initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (other._thread_safe_enabled == FT_TRUE)
    {
        error = this->enable_thread_safety();
        if (error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            return (error);
        }
    }
    this->_alloc_logging.store(other._alloc_logging.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    this->_api_logging.store(other._api_logging.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    this->set_error(other._error_code.load(std::memory_order_relaxed));
    return (FT_ERR_SUCCESS);
}

int32_t ft_logger::initialize(ft_logger &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "ft_logger::initialize(move)",
            "source object is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t error = this->release_instance(FT_FALSE);
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    if (pthread_mutex_lock(&g_logger_mutex) != 0)
        return (FT_ERR_INTERNAL);
    this->_initialised_state = other._initialised_state;
    this->_mutex = other._mutex;
    this->_thread_safe_enabled = other._thread_safe_enabled;
    this->_alloc_logging.store(other._alloc_logging.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    this->_api_logging.store(other._api_logging.load(std::memory_order_relaxed),
        std::memory_order_relaxed);
    this->set_error(other._error_code.load(std::memory_order_relaxed));
    if (g_logger.load(std::memory_order_relaxed) == &other)
        g_logger.store(this, std::memory_order_release);
    other._mutex = ft_nullptr;
    other._thread_safe_enabled = FT_FALSE;
    other._alloc_logging = FT_FALSE;
    other._api_logging = FT_FALSE;
    other.set_error(FT_ERR_SUCCESS);
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)pthread_mutex_unlock(&g_logger_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_logger::destroy() noexcept
{
    return (this->release_instance(FT_TRUE));
}

int32_t ft_logger::release_instance(ft_bool close_runtime) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    ft_bool closing = FT_FALSE;
    uint64_t last_sink_identity = 0U;
    int32_t error = FT_ERR_SUCCESS;
    if (g_logger.load(std::memory_order_acquire) == this)
    {
        if (pthread_mutex_lock(&g_logger_mutex) != 0)
        {
            this->set_error(FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        if (g_logger.load(std::memory_order_relaxed) == this)
        {
            if (close_runtime != FT_FALSE)
            {
                error = logger_begin_runtime_close(last_sink_identity);
                closing = error == FT_ERR_SUCCESS;
            }
            g_logger.store(nullptr, std::memory_order_release);
        }
        (void)pthread_mutex_unlock(&g_logger_mutex);
    }
    if (this->_mutex != ft_nullptr)
    {
        const int32_t mutex_error = this->disable_thread_safety();
        if (error == FT_ERR_SUCCESS)
            error = mutex_error;
    }
    if (closing != FT_FALSE)
        logger_finish_runtime_close(last_sink_identity);
    this->_alloc_logging = FT_FALSE;
    this->_api_logging = FT_FALSE;
    this->set_error(error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (error);
}

uint32_t ft_logger::move(ft_logger &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(ft_move(other))));
}

void ft_logger::set_global() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "ft_logger::set_global");
    if (this->_initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return ;
    }
    if (pthread_mutex_lock(&g_logger_mutex) != 0)
    {
        this->set_error(FT_ERR_INTERNAL);
        return ;
    }
    g_logger.store(this, std::memory_order_release);
    (void)pthread_mutex_unlock(&g_logger_mutex);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void ft_logger::set_level(t_log_level level) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_set_level(level);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

int32_t ft_logger::set_file(const char *path, ft_size_t max_size) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_configure_file(path, max_size);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::set_rotation(ft_size_t max_size, ft_size_t retention_count,
                            uint32_t max_age_seconds) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_configure_rotation(max_size, retention_count, max_age_seconds);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::get_rotation(ft_size_t *max_size, ft_size_t *retention_count,
                            uint32_t *max_age_seconds) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_read_rotation(max_size, retention_count, max_age_seconds);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::add_sink(t_log_sink sink, void *user_data) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = ft_log_add_sink(sink, user_data);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

void ft_logger::remove_sink(t_log_sink sink, void *user_data) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_remove_sink(sink, user_data);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

void ft_logger::set_alloc_logging(ft_bool enable) noexcept
{
    this->_alloc_logging.store(enable, std::memory_order_relaxed);
    cma_set_alloc_logging(enable);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bool ft_logger::get_alloc_logging() const noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (cma_get_alloc_logging());
}

void ft_logger::set_api_logging(ft_bool enable) noexcept
{
    this->_api_logging.store(enable, std::memory_order_relaxed);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bool ft_logger::get_api_logging() const noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_api_logging.load(std::memory_order_relaxed));
}

void ft_logger::set_color(ft_bool enable) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_set_color(enable);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

ft_bool ft_logger::get_color() const noexcept
{
    ft_bool color_enabled;
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (FT_FALSE);

    color_enabled = ft_log_get_color();
    const_cast<ft_logger *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return (color_enabled);
}

void ft_logger::close() noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_close();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

int32_t ft_logger::set_syslog(const char *identifier) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_configure_syslog(identifier);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::set_remote_sink(const char *host, uint16_t port,
                               ft_bool use_tcp) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_configure_remote(host, port, use_tcp);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::enable_thread_safety() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_NOT_INITIALISED);
        return (FT_ERR_NOT_INITIALISED);
    }
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_result;

    if (this->_thread_safe_enabled && this->_mutex)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new(std::nothrow) pt_recursive_mutex();
    if (!mutex_pointer)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_result = mutex_pointer->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(initialize_result);
        return (initialize_result);
    }
    this->_mutex = mutex_pointer;
    this->_thread_safe_enabled = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t ft_logger::disable_thread_safety() noexcept
{
    if (!this->_mutex)
    {
        this->_thread_safe_enabled = FT_FALSE;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    int32_t destroy_result = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->_thread_safe_enabled = FT_FALSE;
    if (destroy_result != FT_ERR_SUCCESS && destroy_result != FT_ERR_INVALID_STATE)
    {
        this->set_error(destroy_result);
        return (destroy_result);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_logger::is_thread_safe() const noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

void ft_logger::set_async_queue_limit(ft_size_t limit) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_set_async_queue_limit(limit);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

ft_size_t ft_logger::get_async_queue_limit() const noexcept
{
    ft_size_t limit;
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return (0);

    limit = ft_log_get_async_queue_limit();
    const_cast<ft_logger *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return (limit);
}

int32_t ft_logger::get_async_metrics(s_log_async_metrics *metrics) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (metrics == ft_nullptr)
        error = FT_ERR_INVALID_ARGUMENT;
    else
        error = ft_log_get_async_metrics(metrics);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

void ft_logger::reset_async_metrics() noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_reset_async_metrics();
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

void ft_logger::enable_remote_health(ft_bool enable) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_enable_remote_health(enable);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

void ft_logger::set_remote_health_interval(uint32_t interval_seconds) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_set_remote_health_interval(interval_seconds);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

int32_t ft_logger::probe_remote_health() noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_probe_remote_health();
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::get_remote_health(s_log_remote_health *statuses, ft_size_t capacity, ft_size_t *entry_count) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_read_remote_health(statuses, capacity, entry_count);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

int32_t ft_logger::push_context(const s_log_field *fields, ft_size_t field_count) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t error = this->lock(&lock_acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = logger_context_push(fields, field_count, ft_nullptr);
    this->set_error(error);
    this->unlock(lock_acquired);
    return (error);
}

void ft_logger::pop_context(ft_size_t field_count) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    ft_log_context_pop(field_count);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock(lock_acquired);
    return ;
}

ft_log_context_guard *ft_logger::make_context_guard(const s_log_field *fields,
        ft_size_t field_count) noexcept
{
    auto *context_guard = new (std::nothrow) ft_log_context_guard();
    if (context_guard == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    int32_t error = context_guard->initialize();
    if (error == FT_ERR_SUCCESS)
        error = context_guard->initialize(fields, field_count);
    this->set_error(error);
    if (error != FT_ERR_SUCCESS)
    {
        delete context_guard;
        return (ft_nullptr);
    }
    return (context_guard);
}

int32_t ft_logger::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "ft_logger::get_error");
    return (this->_error_code.load(std::memory_order_relaxed));
}

const char *ft_logger::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised(this->_initialised_state, "ft_logger::get_error_str");
    return (ft_strerror(this->_error_code.load(std::memory_order_relaxed)));
}

void ft_logger::emit_variadic(t_log_level level, const char *format_string,
    va_list arguments) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->set_error(FT_ERR_SUCCESS);
    int32_t error;
    if (g_async_running.load(std::memory_order_relaxed) != FT_FALSE)
        error = logger_enqueue_running(level, format_string, arguments);
    else
        error = ft_log_vwrite(level, format_string, arguments);
    if (error != FT_ERR_SUCCESS)
        this->set_error(error);
    this->unlock(lock_acquired);
    return ;
}

void ft_logger::debug(const char *format_string, ...) noexcept
{
    va_list arguments;
    va_start(arguments, format_string);
    this->emit_variadic(LOG_LEVEL_DEBUG, format_string, arguments);
    va_end(arguments);
    return ;
}

void ft_logger::info(const char *format_string, ...) noexcept
{
    va_list arguments;
    va_start(arguments, format_string);
    this->emit_variadic(LOG_LEVEL_INFO, format_string, arguments);
    va_end(arguments);
    return ;
}

void ft_logger::warn(const char *format_string, ...) noexcept
{
    va_list arguments;
    va_start(arguments, format_string);
    this->emit_variadic(LOG_LEVEL_WARN, format_string, arguments);
    va_end(arguments);
    return ;
}

void ft_logger::error(const char *format_string, ...) noexcept
{
    va_list arguments;
    va_start(arguments, format_string);
    this->emit_variadic(LOG_LEVEL_ERROR, format_string, arguments);
    va_end(arguments);
    return ;
}

void ft_logger::structured(t_log_level level, const char *message,
                           const s_log_field *fields,
                           ft_size_t field_count) noexcept
{
    ft_bool lock_acquired;

    lock_acquired = FT_FALSE;
    if (this->lock(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    this->set_error(FT_ERR_SUCCESS);
    const int32_t error = logger_emit_structured(level, message, fields, field_count);
    if (error != FT_ERR_SUCCESS)
        this->set_error(error);
    this->unlock(lock_acquired);
    return ;
}

void ft_logger::structured_debug(const char *message,
                                 const s_log_field *fields,
                                 ft_size_t field_count) noexcept
{
    this->structured(LOG_LEVEL_DEBUG, message, fields, field_count);
    return ;
}

void ft_logger::structured_info(const char *message,
                                const s_log_field *fields,
                                ft_size_t field_count) noexcept
{
    this->structured(LOG_LEVEL_INFO, message, fields, field_count);
    return ;
}

void ft_logger::structured_warn(const char *message,
                                const s_log_field *fields,
                                ft_size_t field_count) noexcept
{
    this->structured(LOG_LEVEL_WARN, message, fields, field_count);
    return ;
}

void ft_logger::structured_error(const char *message,
                                 const s_log_field *fields,
                                 ft_size_t field_count) noexcept
{
    this->structured(LOG_LEVEL_ERROR, message, fields, field_count);
    return ;
}

void ft_logger::set_error(int32_t error_code_value) const noexcept
{
    this->_error_code.store(error_code_value, std::memory_order_relaxed);
    return ;
}

int32_t ft_logger::lock(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_status;

    if (lock_acquired)
        *lock_acquired = FT_FALSE;
    lock_status = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_status != FT_ERR_SUCCESS)
    {
        this->set_error(lock_status);
        return (lock_status);
    }
    if (lock_acquired)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void ft_logger::unlock(ft_bool lock_acquired) const noexcept
{
    if (!lock_acquired)
    {
        return ;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}
