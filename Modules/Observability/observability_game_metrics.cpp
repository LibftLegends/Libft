#include "observability_game_metrics.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include <atomic>
#include <mutex>

static std::mutex g_observability_game_configuration_mutex;
static ft_bool g_observability_game_thread_safety_enabled = FT_FALSE;
static std::atomic<ft_game_observability_exporter> g_observability_game_exporter{ft_nullptr};

int32_t observability_game_metrics_enable_thread_safety(void)
{
    const std::lock_guard<std::mutex> guard(g_observability_game_configuration_mutex);
    g_observability_game_thread_safety_enabled = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t observability_game_metrics_disable_thread_safety(void)
{
    const std::lock_guard<std::mutex> guard(g_observability_game_configuration_mutex);
    g_observability_game_thread_safety_enabled = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

ft_bool observability_game_metrics_is_thread_safe(void)
{
    const std::lock_guard<std::mutex> guard(g_observability_game_configuration_mutex);
    return (g_observability_game_thread_safety_enabled);
}

int32_t observability_game_metrics_initialize(ft_game_observability_exporter exporter)
{
    if (exporter == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    const std::lock_guard<std::mutex> guard(g_observability_game_configuration_mutex);
    g_observability_game_thread_safety_enabled = FT_TRUE;
    g_observability_game_exporter.store(exporter, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t observability_game_metrics_shutdown(void)
{
    const std::lock_guard<std::mutex> guard(g_observability_game_configuration_mutex);
    g_observability_game_exporter.store(ft_nullptr, std::memory_order_release);
    g_observability_game_thread_safety_enabled = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

void observability_game_metrics_record(const ft_game_observability_sample &sample)
{
    const auto exporter = g_observability_game_exporter.load(std::memory_order_acquire);
    if (exporter == ft_nullptr || sample.labels.event_name == ft_nullptr
        || sample.labels.entity == ft_nullptr
        || sample.labels.attribute == ft_nullptr
        || sample.unit == ft_nullptr)
        return ;
    ft_game_observability_sample exported_sample = sample;
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
    return ;
}
