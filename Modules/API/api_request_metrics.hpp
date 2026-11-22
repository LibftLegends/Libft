#ifndef API_REQUEST_METRICS_HPP
#define API_REQUEST_METRICS_HPP

#include "../Basic/limits.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Observability/observability_networking_metrics.hpp"
#include "../Time/time.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"

struct json_group;

struct api_request_metrics_guard
{
    uint8_t _initialised_state;
    t_monotonic_time_point _start_time;
    ft_string _endpoint;
    const char *_method;
    const char *_resource;
    ft_size_t _request_bytes;
    char **_result_body;
    int32_t *_status_pointer;
    int32_t *_error_pointer;
    ft_bool _enabled;

    void abort_lifecycle_error(const char *method_name,
        const char *reason) const noexcept;
    void copy_sample_context(const api_request_metrics_guard &other) noexcept;
    api_request_metrics_guard() noexcept;
    api_request_metrics_guard(const api_request_metrics_guard &other) noexcept = delete;
    api_request_metrics_guard(api_request_metrics_guard &&other) noexcept = delete;
    api_request_metrics_guard &operator=(const api_request_metrics_guard &other) = delete;
    api_request_metrics_guard &operator=(api_request_metrics_guard &&other) = delete;
    int32_t initialize(const char *host, uint16_t port,
        const char *method, const char *resource, ft_size_t request_bytes,
        char **result_body, int32_t *status_pointer, int32_t *error_pointer) noexcept;
    int32_t initialize_for_request(const char *host, uint16_t port,
        const char *method, const char *resource, const char *headers, json_group *payload,
        char **result_body, int32_t *status_pointer, int32_t *error_pointer) noexcept;
    int32_t initialize(const api_request_metrics_guard &other) noexcept;
    int32_t initialize(api_request_metrics_guard &&other) noexcept;
    int32_t move(api_request_metrics_guard &other) noexcept;
    int32_t destroy() noexcept;
    ~api_request_metrics_guard() noexcept;
};

#endif
