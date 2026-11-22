#include "observability_networking_metrics.hpp"
#include "../Basic/class_nullptr.hpp"
#include <atomic>

static void observability_networking_no_exporter(const ft_networking_observability_sample &)
{
    return ;
}

// Null means disabled. The local callback represents an enabled service with no exporter.
static std::atomic<ft_networking_observability_exporter>
    g_observability_networking_exporter{nullptr};

static int32_t observability_networking_set_exporter(
    ft_networking_observability_exporter exporter)
{
    auto current = g_observability_networking_exporter.load(std::memory_order_acquire);
    while (current != nullptr)
    {
        if (g_observability_networking_exporter.compare_exchange_weak(
                current, exporter, std::memory_order_acq_rel, std::memory_order_acquire))
            return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_STATE);
}

int32_t observability_networking_metrics_initialize(ft_networking_observability_exporter exporter)
{
    if (!exporter)
        return (FT_ERR_INVALID_ARGUMENT);
    return (observability_networking_set_exporter(exporter));
}

int32_t observability_networking_metrics_shutdown(void)
{
    return (observability_networking_set_exporter(observability_networking_no_exporter));
}

ft_bool observability_networking_metrics_is_active(void)
{
    const auto exporter = g_observability_networking_exporter.load(std::memory_order_acquire);
    return (exporter && exporter != observability_networking_no_exporter);
}

int32_t observability_networking_metrics_record(const ft_networking_observability_sample &sample)
{
    const auto exporter = g_observability_networking_exporter.load(std::memory_order_acquire);
    if (!exporter)
        return (FT_ERR_INVALID_STATE);
    if (exporter == observability_networking_no_exporter)
        return (FT_ERR_SUCCESS);

    ft_networking_observability_sample exported_sample = sample;
    if (exported_sample.error_tag == ft_nullptr)
    {
        if (exported_sample.error_code == FT_ERR_SUCCESS)
            exported_sample.error_tag = "ok";
        else
            exported_sample.error_tag = ft_strerror(exported_sample.error_code);
    }
    if (exported_sample.success == FT_FALSE && exported_sample.error_code == FT_ERR_SUCCESS)
        exported_sample.error_code = FT_ERR_INTERNAL;
    exporter(exported_sample);
    return (FT_ERR_SUCCESS);
}

int32_t observability_networking_metrics_enable_thread_safety(void)
{
    ft_networking_observability_exporter disabled = nullptr;
    (void)g_observability_networking_exporter.compare_exchange_strong(disabled,
        observability_networking_no_exporter, std::memory_order_acq_rel,
        std::memory_order_acquire);
    return (FT_ERR_SUCCESS);
}

int32_t observability_networking_metrics_disable_thread_safety(void)
{
    g_observability_networking_exporter.store(nullptr, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}
