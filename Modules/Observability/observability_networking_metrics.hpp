#ifndef OBSERVABILITY_NETWORKING_METRICS_HPP
#define OBSERVABILITY_NETWORKING_METRICS_HPP

#include "../Errno/errno.hpp"

struct ft_networking_observability_labels
{
    const char *component;
    const char *operation;
    const char *target;
    const char *resource;
};

struct ft_networking_observability_sample
{
    ft_networking_observability_labels labels;
    int64_t duration_ms;
    ft_size_t request_bytes;
    ft_size_t response_bytes;
    int32_t status_code;
    int32_t error_code;
    const char *error_tag;
    ft_bool success;
};

typedef void (*ft_networking_observability_exporter)(const ft_networking_observability_sample &sample);

int32_t observability_networking_metrics_initialize(ft_networking_observability_exporter exporter);
int32_t observability_networking_metrics_shutdown(void);
ft_bool observability_networking_metrics_is_active(void);
int32_t observability_networking_metrics_record(const ft_networking_observability_sample &sample);
int32_t observability_networking_metrics_enable_thread_safety(void);
int32_t observability_networking_metrics_disable_thread_safety(void);

#endif
