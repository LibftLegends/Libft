#include "observability.hpp"
#include "../Basic/class_nullptr.hpp"
#include <atomic>

struct ft_observability_counter_storage
{
    std::atomic<uint64_t> operations;
    std::atomic<uint64_t> successes;
    std::atomic<uint64_t> failures;
    std::atomic<uint64_t> bytes_read;
    std::atomic<uint64_t> bytes_written;
};

static ft_observability_counter_storage g_file_counters;
static ft_observability_counter_storage g_networking_counters;
static ft_observability_counter_storage g_parser_counters;
static std::atomic<ft_observability_trace_hook> g_trace_hook(ft_nullptr);

static ft_observability_counter_storage *observability_get_counter_storage(
    ft_observability_module module) noexcept
{
    if (module == FT_OBSERVABILITY_MODULE_FILE)
        return (&g_file_counters);
    if (module == FT_OBSERVABILITY_MODULE_NETWORKING)
        return (&g_networking_counters);
    if (module == FT_OBSERVABILITY_MODULE_PARSER)
        return (&g_parser_counters);
    return (ft_nullptr);
}

void observability_error_context_clear(ft_error_context *context) noexcept
{
    if (context == ft_nullptr)
        return ;
    context->error_code = FT_ERR_SUCCESS;
    context->platform_error = 0;
    context->module = ft_nullptr;
    context->operation = ft_nullptr;
    context->resource = ft_nullptr;
    context->detail = ft_nullptr;
    return ;
}

int32_t observability_error_context_set(ft_error_context *context,
    const char *module, const char *operation, const char *resource,
    int32_t error_code, const char *detail, int64_t platform_error) noexcept
{
    if (context == ft_nullptr || module == ft_nullptr || operation == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    context->error_code = error_code;
    context->platform_error = platform_error;
    context->module = module;
    context->operation = operation;
    context->resource = resource;
    context->detail = detail;
    return (FT_ERR_SUCCESS);
}

int32_t observability_set_trace_hook(ft_observability_trace_hook hook) noexcept
{
    g_trace_hook.store(hook, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

void observability_clear_trace_hook(void) noexcept
{
    g_trace_hook.store(ft_nullptr, std::memory_order_release);
    return ;
}

int32_t observability_trace_emit(const ft_observability_trace_event *event) noexcept
{
    ft_observability_trace_hook hook;

    if (event == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    hook = g_trace_hook.load(std::memory_order_acquire);
    if (hook == ft_nullptr)
        return (FT_ERR_SUCCESS);
    hook(*event);
    return (FT_ERR_SUCCESS);
}

int32_t observability_record_operation(ft_observability_module module,
    ft_bool success, ft_size_t bytes_read, ft_size_t bytes_written) noexcept
{
    ft_observability_counter_storage *storage;

    storage = observability_get_counter_storage(module);
    if (storage == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    storage->operations.fetch_add(1, std::memory_order_relaxed);
    if (success == FT_TRUE)
        storage->successes.fetch_add(1, std::memory_order_relaxed);
    else
        storage->failures.fetch_add(1, std::memory_order_relaxed);
    if (bytes_read != 0)
        storage->bytes_read.fetch_add(bytes_read, std::memory_order_relaxed);
    if (bytes_written != 0)
        storage->bytes_written.fetch_add(bytes_written, std::memory_order_relaxed);
    return (FT_ERR_SUCCESS);
}

int32_t observability_get_counters(ft_observability_module module,
    ft_observability_counters *counters) noexcept
{
    ft_observability_counter_storage *storage;

    if (counters == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    storage = observability_get_counter_storage(module);
    if (storage == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    counters->operations = storage->operations.load(std::memory_order_relaxed);
    counters->successes = storage->successes.load(std::memory_order_relaxed);
    counters->failures = storage->failures.load(std::memory_order_relaxed);
    counters->bytes_read = storage->bytes_read.load(std::memory_order_relaxed);
    counters->bytes_written = storage->bytes_written.load(std::memory_order_relaxed);
    return (FT_ERR_SUCCESS);
}

int32_t observability_reset_counters(ft_observability_module module) noexcept
{
    ft_observability_counter_storage *storage;

    storage = observability_get_counter_storage(module);
    if (storage == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    storage->operations.store(0, std::memory_order_relaxed);
    storage->successes.store(0, std::memory_order_relaxed);
    storage->failures.store(0, std::memory_order_relaxed);
    storage->bytes_read.store(0, std::memory_order_relaxed);
    storage->bytes_written.store(0, std::memory_order_relaxed);
    return (FT_ERR_SUCCESS);
}
