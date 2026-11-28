#ifndef LOGGER_INTERNAL_HPP
#define LOGGER_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include <atomic>
#include <cstdarg>
#include <pthread.h>
#include "../Template/vector.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "logger.hpp"

void logger_set_global_logging(ft_bool allocation, ft_bool enable);
ft_bool logger_get_global_logging(ft_bool allocation);
void logger_report_sink_error(int32_t error);
extern std::atomic<t_log_level> g_level;
extern std::atomic<ft_bool> g_async_running;
extern std::atomic<ft_bool> g_use_color;

typedef int32_t (*t_log_sink)(const char *message, void *user_data);

typedef ssize_t (*t_network_send_function)(int32_t socket_fd, const void *buffer, ft_size_t length, int32_t flags);

enum t_log_sink_registration_state : uint8_t
{
    LOG_SINK_READY,
    LOG_SINK_RUNNING,
    LOG_SINK_RETIRED_RUNNING,
    LOG_SINK_RETIRED
};

struct s_log_sink_registration
{
    std::atomic<ft_size_t> references{1U};
    std::atomic<t_log_sink_registration_state> state{LOG_SINK_READY};
    ft_bool close_output = FT_FALSE;
    uint64_t identity = 0U;
};

struct s_log_sink
{
    pt_mutex   *mutex;
    ft_bool        thread_safe_enabled;
    t_log_sink  function;
    void       *user_data;
    s_log_sink_registration *registration;

    s_log_sink()
        : mutex(ft_nullptr), thread_safe_enabled(FT_FALSE), function(ft_nullptr), user_data(ft_nullptr),
          registration(ft_nullptr)
    {
        return ;
    }
};

struct s_file_sink
{
    pt_mutex   *mutex;
    ft_bool        thread_safe_enabled;
    int32_t       file_descriptor;
    ft_string path;
    ft_size_t    max_size;
    ft_size_t    retention_count;
    uint32_t max_age_seconds;

    s_file_sink()
        : mutex(ft_nullptr), thread_safe_enabled(FT_FALSE), file_descriptor(-1), path(),
          max_size(0), retention_count(1), max_age_seconds(0)
    {
        return ;
    }
};

struct s_network_sink
{
    pt_mutex                   *mutex;
    ft_bool                        thread_safe_enabled;
    int32_t                         socket_fd;
    t_network_send_function     send_function;
    ft_string                   host;
    uint16_t              port;
    ft_bool                        use_tcp;

    s_network_sink()
        : mutex(ft_nullptr), thread_safe_enabled(FT_FALSE), socket_fd(-1),
          send_function(ft_nullptr), host(), port(0), use_tcp(FT_FALSE)
    {
        return ;
    }
};

struct s_redaction_rule
{
    ft_string   pattern;
    ft_string   replacement;

    s_redaction_rule()
        : pattern(), replacement()
    {
        return ;
    }

    int32_t initialize(const s_redaction_rule &other) noexcept;
    int32_t destroy() noexcept;
};

struct s_log_context_entry
{
    ft_string   key;
    ft_string   value;
    ft_bool        has_value;
    s_log_context_group *group;

    s_log_context_entry()
        : key(), value(), has_value(FT_FALSE), group(ft_nullptr)
    {
        return ;
    }

    int32_t initialize(const s_log_context_entry &other) noexcept;
    int32_t initialize(s_log_context_entry &&other) noexcept;
    int32_t destroy() noexcept;
    ~s_log_context_entry() noexcept;
};

struct s_log_context_group
{
    std::atomic<ft_size_t> references{1U};
    std::atomic<ft_size_t> owners{1U};
    std::atomic<ft_size_t> entries{0U};
    std::atomic<ft_bool> released{FT_FALSE};
};

extern ft_vector<s_log_sink> g_sinks;
extern pthread_mutex_t g_sinks_mutex;

int32_t logger_lock_sinks();
int32_t logger_unlock_sinks();
int32_t logger_prepare_redacted_context(ft_string &text, ft_string &context);
struct s_log_redaction_snapshot;
int32_t logger_acquire_redaction_snapshot(s_log_redaction_snapshot *&snapshot);
void logger_release_redaction_snapshot(s_log_redaction_snapshot *snapshot);
int32_t logger_redact_log_value(const s_log_redaction_snapshot *snapshot,
    const char *&value, ft_string &storage);
int32_t logger_apply_redactions(ft_string &text,
        const ft_vector<s_redaction_rule> &rules);
int32_t logger_context_push(const s_log_field *fields, ft_size_t field_count,
        s_log_context_group *group);
void logger_context_retain_group(s_log_context_group *group);
void logger_context_drop_group(s_log_context_group *group, ft_bool keep_fields);
void logger_context_pop(ft_size_t entry_count);
int32_t logger_context_append_json(ft_string &output, ft_bool &first_field,
    const s_log_redaction_snapshot *snapshot = nullptr);
int32_t logger_append_json_field(ft_string &output, const char *key,
    const char *value, ft_bool &first_field, const s_log_redaction_snapshot *snapshot);
void logger_context_clear();
int32_t logger_context_format_flat(ft_string &output,
    const s_log_redaction_snapshot *snapshot = nullptr);
int32_t logger_append_quoted_value(ft_string &buffer, const char *value,
    ft_bool use_json);
int32_t logger_build_standard_message(t_log_level level, const ft_string &message_text,
        const ft_string &context_fragment, ft_string &formatted_message);

void ft_log_rotate(s_file_sink *sink);
int32_t logger_write_file_unlocked(s_file_sink &sink, const char *message);
int32_t logger_write_rotating_file(s_file_sink *sink, const char *message);
int32_t ft_file_sink(const char *message, void *user_data);
int32_t ft_network_sink(const char *message, void *user_data);
const char *ft_level_to_str(t_log_level level);
int32_t ft_log_level_to_severity(t_log_level level);
int32_t logger_prepare_message(t_log_level level, ft_string &text,
    ft_string &formatted_message);
int32_t logger_write_text(t_log_level level, ft_string &text);
int32_t logger_write_prepared(t_log_level level, const ft_string &message, ft_bool structured);
int32_t logger_deliver_to_sinks(const ft_string &message, ft_bool stop_on_error,
    ft_bool &has_sinks, ft_bool structured);
int32_t logger_write_json_sink(const char *message, void *user_data, ft_bool structured);
void logger_release_sink(s_log_sink &entry);
void logger_retire_sink(s_log_sink &entry);
void logger_unlist_sink(s_log_sink &entry);
void logger_wait_for_sink_callback(s_log_sink_registration &registration);
void logger_close_sink_output(const s_log_sink &entry);
int32_t logger_begin_runtime_close(uint64_t &last_sink_identity);
void logger_finish_runtime_close(uint64_t last_sink_identity);
ft_bool logger_runtime_closing();
ft_bool logger_in_delivery_callback();
ft_bool logger_in_health_worker();
int32_t logger_add_sink_locked(t_log_sink sink, void *user_data,
    s_log_sink_registration **registration);
void logger_close_syslog_output(uint64_t identity);
int32_t logger_configure_file(const char *path, ft_size_t max_size);
int32_t logger_configure_rotation(ft_size_t max_size, ft_size_t retention_count,
    uint32_t max_age_seconds);
int32_t logger_read_rotation(ft_size_t *max_size, ft_size_t *retention_count,
    uint32_t *max_age_seconds);
int32_t logger_configure_syslog(const char *identifier);
int32_t logger_configure_remote(const char *host, uint16_t port, ft_bool use_tcp);
int32_t logger_probe_remote_health();
int32_t logger_read_remote_health(s_log_remote_health *statuses, ft_size_t capacity,
    ft_size_t *entry_count);
int32_t logger_enqueue_text(t_log_level level, ft_string &text, ft_bool require_worker);
int32_t logger_enqueue_prepared(t_log_level level, ft_string &message, ft_bool require_worker,
    ft_bool structured);
int32_t logger_enqueue_running(t_log_level level, const char *format_string,
    va_list argument_list);
int32_t ft_log_vwrite(t_log_level level, const char *format_string, va_list argument_list);
int32_t logger_deliver_sync(t_log_level level, const ft_string &message, ft_bool structured);
void ft_log_process_message(const ft_string &message);
int32_t logger_emit_structured(t_log_level level, const char *message,
    const s_log_field *fields, ft_size_t field_count);

int32_t log_sink_prepare_thread_safety(s_log_sink *sink);
void log_sink_teardown_thread_safety(s_log_sink *sink);
int32_t log_sink_lock(const s_log_sink *sink, ft_bool *lock_acquired);
void log_sink_unlock(const s_log_sink *sink, ft_bool lock_acquired);

int32_t file_sink_prepare_thread_safety(s_file_sink *sink);
void file_sink_teardown_thread_safety(s_file_sink *sink);
int32_t file_sink_lock(const s_file_sink *sink, ft_bool *lock_acquired);
void file_sink_unlock(const s_file_sink *sink, ft_bool lock_acquired);

int32_t network_sink_prepare_thread_safety(s_network_sink *sink);
void network_sink_teardown_thread_safety(s_network_sink *sink);
int32_t network_sink_lock(const s_network_sink *sink, ft_bool *lock_acquired);
void network_sink_unlock(const s_network_sink *sink, ft_bool lock_acquired);

#ifdef LIBFT_TEST_BUILD
void ft_log_destroy_async_runtime_for_tests(void);
void ft_log_destroy_remote_health_for_tests(void);
#endif

#endif
