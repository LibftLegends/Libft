#ifndef OBSERVABILITY_HPP
# define OBSERVABILITY_HPP

#include "../Errno/errno.hpp"
#include "observability_histogram.hpp"

enum ft_observability_module
{
    FT_OBSERVABILITY_MODULE_FILE = 1,
    FT_OBSERVABILITY_MODULE_NETWORKING = 2,
    FT_OBSERVABILITY_MODULE_PARSER = 3
};

enum ft_observability_trace_phase
{
    FT_OBSERVABILITY_TRACE_START = 1,
    FT_OBSERVABILITY_TRACE_FINISH = 2
};

struct ft_error_context
{
    int32_t error_code;
    int64_t platform_error;
    const char *module;
    const char *operation;
    const char *resource;
    const char *detail;
};

struct ft_observability_trace_event
{
    ft_observability_module module;
    ft_observability_trace_phase phase;
    const char *operation;
    const char *resource;
    int32_t error_code;
    ft_size_t bytes_read;
    ft_size_t bytes_written;
};

struct ft_observability_counters
{
    uint64_t operations;
    uint64_t successes;
    uint64_t failures;
    uint64_t bytes_read;
    uint64_t bytes_written;
};

typedef void (*ft_observability_trace_hook)(const ft_observability_trace_event &event);

void    observability_error_context_clear(ft_error_context *context) noexcept;
int32_t observability_error_context_set(ft_error_context *context,
            const char *module, const char *operation, const char *resource,
            int32_t error_code, const char *detail,
            int64_t platform_error) noexcept;
// Hooks run on emitting threads. Replacing a hook does not wait for in-flight calls.
int32_t observability_set_trace_hook(ft_observability_trace_hook hook) noexcept;
void    observability_clear_trace_hook(void) noexcept;
int32_t observability_trace_emit(const ft_observability_trace_event *event) noexcept;
int32_t observability_record_operation(ft_observability_module module,
            ft_bool success, ft_size_t bytes_read,
            ft_size_t bytes_written) noexcept;
// Counter fields are sampled and reset independently of concurrent operations.
int32_t observability_get_counters(ft_observability_module module,
            ft_observability_counters *counters) noexcept;
int32_t observability_reset_counters(ft_observability_module module) noexcept;

#endif
