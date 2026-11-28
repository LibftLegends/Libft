#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Printf/printf.hpp"
#include "../Sink/sink.hpp"
#include "system_utils.hpp"
#include <algorithm>
#include <mutex>
#include <new>
#include <vector>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

typedef std::vector<t_su_resource_tracer> t_tracer_vector;

static t_tracer_vector    g_su_resource_tracers;
static std::mutex                         g_su_resource_tracers_mutex;
static const char                        *g_su_abort_reason = ft_nullptr;
static std::mutex                         g_su_abort_reason_mutex;

static void su_log_resource_tracing_event(const char *reason)
{
    char message_buffer[512] = {};

    if (reason == ft_nullptr)
        (void)pf_snprintf(message_buffer, sizeof(message_buffer),
            "Resource tracers invoked without a reason context");
    else
        (void)pf_snprintf(message_buffer, sizeof(message_buffer),
            "Resource tracers invoked due to: %s", reason);
    (void)sink_record_message(3, message_buffer);
    return ;
}

int32_t su_register_resource_tracer(t_su_resource_tracer tracer)
{
    ft_bool lock_acquired;

    if (tracer == ft_nullptr)
        return (-1);
    lock_acquired = FT_FALSE;
    g_su_resource_tracers_mutex.lock();
    lock_acquired = FT_TRUE;
    ft_size_t index = 0;
    ft_size_t count = g_su_resource_tracers.size();
    while (index < count)
    {
        if (g_su_resource_tracers[index] == tracer)
        {
            if (lock_acquired)
                g_su_resource_tracers_mutex.unlock();
            return (-1);
        }
        index += 1;
    }
    try
    {
        g_su_resource_tracers.push_back(tracer);
    }
    catch (const std::bad_alloc &)
    {
        if (lock_acquired)
            g_su_resource_tracers_mutex.unlock();
        return (-1);
    }
    if (lock_acquired)
        g_su_resource_tracers_mutex.unlock();
    return (0);
}

int32_t su_unregister_resource_tracer(t_su_resource_tracer tracer)
{
    ft_bool lock_acquired;

    if (tracer == ft_nullptr)
        return (-1);
    lock_acquired = FT_FALSE;
    g_su_resource_tracers_mutex.lock();
    lock_acquired = FT_TRUE;
    ft_size_t index = 0;
    ft_size_t count = g_su_resource_tracers.size();
    while (index < count)
    {
        if (g_su_resource_tracers[index] == tracer)
        {
            g_su_resource_tracers.erase(g_su_resource_tracers.begin() + static_cast<t_tracer_vector::difference_type>(index));
            if (lock_acquired)
                g_su_resource_tracers_mutex.unlock();
            return (0);
        }
        index += 1;
    }
    if (lock_acquired)
        g_su_resource_tracers_mutex.unlock();
    return (-1);
}

void su_clear_resource_tracers(void)
{
    t_tracer_vector removed;
    g_su_resource_tracers_mutex.lock();
    removed.swap(g_su_resource_tracers);
    g_su_resource_tracers_mutex.unlock();
    return ;
}

void su_run_resource_tracers(const char *reason)
{
    constexpr ft_size_t INLINE_TRACER_COUNT = 8;
    t_su_resource_tracer inline_tracers[INLINE_TRACER_COUNT];
    t_tracer_vector local_tracers;
    const t_su_resource_tracer *snapshot = inline_tracers;

    g_su_resource_tracers_mutex.lock();
    const ft_size_t count = g_su_resource_tracers.size();
    if (count <= INLINE_TRACER_COUNT)
        std::copy_n(g_su_resource_tracers.begin(), count, inline_tracers);
    else
    {
        try
        {
            local_tracers = g_su_resource_tracers;
        }
        catch (const std::bad_alloc &)
        {
            g_su_resource_tracers_mutex.unlock();
            return ;
        }
        snapshot = local_tracers.data();
    }
    g_su_resource_tracers_mutex.unlock();
    su_log_resource_tracing_event(reason);
    ft_size_t run_index = 0;
    while (run_index < count)
    {
        snapshot[run_index](reason);
        run_index += 1;
    }
    return ;
}

void su_internal_set_abort_reason(const char *reason)
{
    g_su_abort_reason_mutex.lock();
    g_su_abort_reason = reason;
    g_su_abort_reason_mutex.unlock();
    return ;
}

const char *su_internal_take_abort_reason(void)
{
    const char *reason;

    g_su_abort_reason_mutex.lock();
    reason = g_su_abort_reason;
    g_su_abort_reason = ft_nullptr;
    g_su_abort_reason_mutex.unlock();
    return (reason);
}
