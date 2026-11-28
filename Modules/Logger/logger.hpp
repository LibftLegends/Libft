#ifndef LOGGER_HPP
# define LOGGER_HPP

#include <cstddef>
#include <cstdarg>
#include <atomic>
#include "../Time/time.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/pthread.hpp"

class pt_recursive_mutex;

enum t_log_level {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_NONE
};

typedef int32_t (*t_log_sink)(const char *message, void *user_data);
int32_t     ft_log_add_sink(t_log_sink sink, void *user_data);
void    ft_log_remove_sink(t_log_sink sink, void *user_data);

struct s_log_remote_health
{
    const char  *host;
    uint16_t port;
    ft_bool use_tcp;
    ft_bool reachable;
    t_time last_check;
    int32_t  last_error;
};

struct s_log_field
{
    pt_mutex   *mutex;
    ft_bool        thread_safe_enabled;
    const char *key;
    const char *value;

    s_log_field()
        : mutex(ft_nullptr), thread_safe_enabled(FT_FALSE), key(ft_nullptr), value(ft_nullptr)
    {
        return ;
    }
};

struct s_log_async_metrics
{
    pt_mutex *mutex;
    ft_bool      thread_safe_enabled;
    ft_size_t pending_messages;
    ft_size_t peak_pending_messages;
    ft_size_t dropped_messages;

    s_log_async_metrics()
        : mutex(ft_nullptr), thread_safe_enabled(FT_FALSE), pending_messages(0),
          peak_pending_messages(0), dropped_messages(0)
    {
        return ;
    }
};

int32_t     log_field_prepare_thread_safety(s_log_field *field);
void    log_field_teardown_thread_safety(s_log_field *field);
int32_t     log_field_lock(const s_log_field *field, ft_bool *lock_acquired);
void    log_field_unlock(const s_log_field *field, ft_bool lock_acquired);

int32_t     log_async_metrics_prepare_thread_safety(s_log_async_metrics *metrics);
void    log_async_metrics_teardown_thread_safety(s_log_async_metrics *metrics);
int32_t     log_async_metrics_lock(s_log_async_metrics *metrics, ft_bool *lock_acquired);
void    log_async_metrics_unlock(s_log_async_metrics *metrics, ft_bool lock_acquired);

struct s_log_lock_contention_sample
{
    const void        *mutex_pointer;
    pt_thread_id_type owner_thread;
    pt_thread_id_type waiting_thread;
    int64_t              wait_duration_ms;
    ft_bool              priority_inversion;

    s_log_lock_contention_sample()
        : mutex_pointer(ft_nullptr), owner_thread(0), waiting_thread(0),
          wait_duration_ms(0), priority_inversion(FT_FALSE)
    {
        return ;
    }
};

struct s_log_lock_contention_statistics
{
    ft_size_t total_samples;
    ft_size_t priority_inversions;
    ft_size_t skipped_samples;
    int64_t longest_wait_ms;
    double average_wait_ms;

    s_log_lock_contention_statistics()
        : total_samples(0), priority_inversions(0), skipped_samples(0),
          longest_wait_ms(0), average_wait_ms(0.0)
    {
        return ;
    }
};

void    ft_log_set_level(t_log_level level);
int32_t     ft_log_set_file(const char *path, ft_size_t max_size);
int32_t     ft_log_set_rotation(ft_size_t max_size, ft_size_t retention_count,
                            uint32_t max_age_seconds);
int32_t     ft_log_get_rotation(ft_size_t *max_size, ft_size_t *retention_count,
                            uint32_t *max_age_seconds);
void    ft_log_close();
void    ft_log_set_alloc_logging(ft_bool enable);
ft_bool    ft_log_get_alloc_logging();
void    ft_log_set_api_logging(ft_bool enable);
ft_bool    ft_log_get_api_logging();
void    ft_log_set_color(ft_bool enable);
ft_bool    ft_log_get_color();
int32_t ft_json_sink(const char *message, void *user_data);
int32_t ft_syslog_sink(const char *message, void *user_data);
int32_t     ft_log_set_syslog(const char *identifier);
int32_t     ft_log_set_remote_sink(const char *host, uint16_t port,
                               ft_bool use_tcp);
int32_t     ft_log_add_redaction(const char *pattern);
int32_t     ft_log_add_redaction_with_replacement(const char *pattern,
                               const char *replacement);
void    ft_log_clear_redactions();
void    ft_log_enable_remote_health(ft_bool enable);
void    ft_log_set_remote_health_interval(uint32_t interval_seconds);
int32_t     ft_log_probe_remote_health();
int32_t     ft_log_get_remote_health(s_log_remote_health *statuses,
            ft_size_t capacity, ft_size_t *entry_count);
int32_t     ft_log_context_push(const s_log_field *fields, ft_size_t field_count);
int32_t     ft_log_context_push_field(const char *key, const char *value);
void    ft_log_context_pop(ft_size_t field_count);
void    ft_log_context_clear();

struct s_log_context_group;

class ft_log_context_guard
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        s_log_context_group *_group;
        mutable int32_t _error_code;

        void set_error(int32_t error_code_value) const;

    public:
        ft_log_context_guard() noexcept;
        ft_log_context_guard(const ft_log_context_guard &other) noexcept = delete;
        ft_log_context_guard(ft_log_context_guard &&other) noexcept = delete;
        ~ft_log_context_guard() noexcept;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_log_context_guard &other) noexcept;
        int32_t initialize(ft_log_context_guard &&other) noexcept;
        int32_t initialize(const s_log_field *fields, ft_size_t field_count) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(ft_log_context_guard &other) noexcept;

        ft_log_context_guard &operator=(const ft_log_context_guard &) = delete;
        ft_log_context_guard &operator=(ft_log_context_guard &&other) noexcept = delete;

        void release() noexcept;
        ft_bool is_active() const noexcept;
        int32_t get_error() const;
        const char *get_error_str() const;
};

void ft_log_debug(const char *format_string, ...);
void ft_log_info(const char *format_string, ...);
void ft_log_warn(const char *format_string, ...);
void ft_log_error(const char *format_string, ...);
void ft_log_enable_async(ft_bool enable);
void ft_log_enqueue(t_log_level level, const char *format_string, va_list argument_list);
void ft_log_structured(t_log_level level, const char *message,
                       const s_log_field *fields, ft_size_t field_count);
void ft_log_set_async_queue_limit(ft_size_t limit);
ft_size_t ft_log_get_async_queue_limit();
int32_t  ft_log_get_async_metrics(s_log_async_metrics *metrics);
void ft_log_reset_async_metrics();
void ft_log_debug_structured(const char *message, const s_log_field *fields,
                             ft_size_t field_count);
void ft_log_info_structured(const char *message, const s_log_field *fields,
                            ft_size_t field_count);
void ft_log_warn_structured(const char *message, const s_log_field *fields,
                            ft_size_t field_count);
void ft_log_error_structured(const char *message, const s_log_field *fields,
                             ft_size_t field_count);
void ft_log_enable_lock_contention_sampling(ft_bool enable);
void ft_log_set_lock_contention_sampling_interval(uint32_t interval_ms);
void ft_log_set_lock_contention_priority_threshold(int64_t threshold_ms);
int32_t  ft_log_sample_lock_contention(s_log_lock_contention_sample *samples, ft_size_t capacity, ft_size_t *entry_count);
int32_t  ft_log_lock_contention_get_statistics(s_log_lock_contention_statistics *statistics);
void ft_log_lock_contention_reset_statistics();

class ft_logger
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        uint8_t _initialised_state;
        pt_recursive_mutex *_mutex;
        ft_bool _thread_safe_enabled;
        std::atomic<ft_bool> _alloc_logging;
        std::atomic<ft_bool> _api_logging;
        mutable std::atomic<int32_t> _error_code;

        void set_error(int32_t error_code_value) const noexcept;
        void emit_variadic(t_log_level level, const char *format_string,
            va_list arguments) noexcept;
        int32_t release_instance(ft_bool close_runtime) noexcept;
        friend void logger_report_sink_error(int32_t error);

        int32_t lock(ft_bool *lock_acquired) const noexcept;
        void unlock(ft_bool lock_acquired) const noexcept;

    public:
        ft_logger() noexcept;
        ft_logger(const ft_logger &other) noexcept = delete;
        ft_logger(ft_logger &&other) noexcept = delete;
        ~ft_logger() noexcept;

        ft_logger &operator=(const ft_logger&) = delete;
        ft_logger &operator=(ft_logger&&) = delete;

        int32_t initialize() noexcept;
        int32_t initialize(const ft_logger &other) noexcept;
        int32_t initialize(ft_logger &&other) noexcept;
        int32_t initialize(const char *path, ft_size_t max_size,
                t_log_level level) noexcept;
        int32_t destroy() noexcept;
        uint32_t move(ft_logger &other) noexcept;

        void set_global() noexcept;
        void set_level(t_log_level level) noexcept;
        int32_t  set_file(const char *path, ft_size_t max_size) noexcept;
        int32_t  set_rotation(ft_size_t max_size, ft_size_t retention_count,
                          uint32_t max_age_seconds) noexcept;
        int32_t  get_rotation(ft_size_t *max_size, ft_size_t *retention_count,
                          uint32_t *max_age_seconds) noexcept;
        int32_t  add_sink(t_log_sink sink, void *user_data) noexcept;
        void remove_sink(t_log_sink sink, void *user_data) noexcept;
        void set_alloc_logging(ft_bool enable) noexcept;
        ft_bool get_alloc_logging() const noexcept;
        void set_api_logging(ft_bool enable) noexcept;
        ft_bool get_api_logging() const noexcept;
        void set_color(ft_bool enable) noexcept;
        ft_bool get_color() const noexcept;
        void close() noexcept;
        int32_t  set_syslog(const char *identifier) noexcept;
        int32_t  set_remote_sink(const char *host, uint16_t port,
                             ft_bool use_tcp) noexcept;
        int32_t  enable_thread_safety() noexcept;
        int32_t  disable_thread_safety() noexcept;
        ft_bool is_thread_safe() const noexcept;
        void set_async_queue_limit(ft_size_t limit) noexcept;
        ft_size_t get_async_queue_limit() const noexcept;
        int32_t  get_async_metrics(s_log_async_metrics *metrics) noexcept;
        void reset_async_metrics() noexcept;
        void enable_remote_health(ft_bool enable) noexcept;
        void set_remote_health_interval(uint32_t interval_seconds) noexcept;
        int32_t  probe_remote_health() noexcept;
        int32_t  get_remote_health(s_log_remote_health *statuses,
                               ft_size_t capacity, ft_size_t *entry_count) noexcept;
        int32_t  push_context(const s_log_field *fields, ft_size_t field_count) noexcept;
        void pop_context(ft_size_t field_count) noexcept;
        ft_log_context_guard *make_context_guard(const s_log_field *fields,
                ft_size_t field_count) noexcept;
        int32_t  get_error() const noexcept;
        const char *get_error_str() const noexcept;

        void debug(const char *format_string, ...) noexcept;
        void info(const char *format_string, ...) noexcept;
        void warn(const char *format_string, ...) noexcept;
        void error(const char *format_string, ...) noexcept;
        void structured(t_log_level level, const char *message,
                        const s_log_field *fields,
                        ft_size_t field_count) noexcept;
        void structured_debug(const char *message, const s_log_field *fields,
                              ft_size_t field_count) noexcept;
        void structured_info(const char *message, const s_log_field *fields,
                             ft_size_t field_count) noexcept;
        void structured_warn(const char *message, const s_log_field *fields,
                             ft_size_t field_count) noexcept;
        void structured_error(const char *message, const s_log_field *fields,
                              ft_size_t field_count) noexcept;
};

#endif
