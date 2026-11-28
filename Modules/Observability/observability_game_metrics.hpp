#ifndef OBSERVABILITY_GAME_METRICS_HPP
#define OBSERVABILITY_GAME_METRICS_HPP

#include "../Errno/errno.hpp"

struct ft_game_observability_labels
{
    const char *event_name;
    const char *entity;
    const char *attribute;
};

struct ft_game_observability_sample
{
    ft_game_observability_labels labels;
    int64_t delta_value;
    int64_t total_value;
    const char *unit;
    int32_t error_code;
    const char *error_tag;
    ft_bool success;
};

typedef void (*ft_game_observability_exporter)(const ft_game_observability_sample &sample);

int32_t observability_game_metrics_initialize(ft_game_observability_exporter exporter);
// A callback already captured by a recording thread may complete after shutdown.
int32_t observability_game_metrics_shutdown(void);
// Callbacks run synchronously on calling threads and may run concurrently.
void observability_game_metrics_record(const ft_game_observability_sample &sample);

// Preserve the configured thread-safety flag. Exporter publication is always atomic.
int32_t observability_game_metrics_enable_thread_safety(void);
int32_t observability_game_metrics_disable_thread_safety(void);
ft_bool observability_game_metrics_is_thread_safe(void);

#endif
