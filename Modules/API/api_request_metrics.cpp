#include "api_request_metrics.hpp"
#include "api_http_common.hpp"
#include "../Template/move.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"
#include <charconv>

void api_request_metrics_guard::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

api_request_metrics_guard::api_request_metrics_guard() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _start_time(),
      _endpoint(),
      _method(ft_nullptr),
      _resource(ft_nullptr),
      _request_bytes(0),
      _result_body(ft_nullptr),
      _status_pointer(ft_nullptr),
      _error_pointer(ft_nullptr),
      _enabled(FT_FALSE)
{
    return ;
}

int32_t api_request_metrics_guard::initialize(const char *host, uint16_t port,
    const char *method, const char *resource, ft_size_t request_bytes,
    char **result_body, int32_t *status_pointer, int32_t *error_pointer) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->abort_lifecycle_error("api_request_metrics_guard::initialize",
            "called while object is already initialised");
    this->_start_time = time_monotonic_point_now();
    const char *endpoint_host = host;
    if (!endpoint_host)
        endpoint_host = "(null)";
    int32_t endpoint_error = this->_endpoint.initialize(endpoint_host);
    if (endpoint_error == FT_ERR_SUCCESS && port != 0)
    {
        char suffix[6] = {':'};
        const auto port_text = std::to_chars(suffix + 1, suffix + sizeof(suffix), port);
        if (port_text.ec == std::errc{})
            endpoint_error = this->_endpoint.append(suffix,
                static_cast<ft_size_t>(port_text.ptr - suffix));
        else
            endpoint_error = FT_ERR_OUT_OF_RANGE;
    }
    if (endpoint_error != FT_ERR_SUCCESS)
    {
        (void)this->_endpoint.destroy();
        this->_enabled = FT_FALSE;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (endpoint_error);
    }
    this->_method = method;
    this->_resource = resource;
    this->_request_bytes = request_bytes;
    this->_result_body = result_body;
    this->_status_pointer = status_pointer;
    this->_error_pointer = error_pointer;
    this->_enabled = FT_TRUE;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t api_request_metrics_guard::initialize_for_request(const char *host, uint16_t port,
    const char *method, const char *resource, const char *headers, json_group *payload,
    char **result_body, int32_t *status_pointer, int32_t *error_pointer) noexcept
{
    if (!observability_networking_metrics_is_active())
        return (FT_ERR_SUCCESS);
    ft_size_t request_bytes = 0;
    if (host)
        request_bytes += ft_strlen_size_t(host);
    if (method)
        request_bytes += ft_strlen_size_t(method);
    if (resource)
        request_bytes += ft_strlen_size_t(resource);
    if (headers)
        request_bytes += ft_strlen_size_t(headers);
    ft_size_t payload_bytes = 0;
    if (payload && api_http_measure_json_payload(payload, payload_bytes)
        && payload_bytes <= FT_SYSTEM_SIZE_MAX - request_bytes)
        request_bytes += payload_bytes;
    return (this->initialize(host, port, method, resource, request_bytes,
        result_body, status_pointer, error_pointer));
}

void api_request_metrics_guard::copy_sample_context(const api_request_metrics_guard &other) noexcept
{
    this->_start_time = other._start_time;
    this->_method = other._method;
    this->_resource = other._resource;
    this->_request_bytes = other._request_bytes;
    this->_result_body = other._result_body;
    this->_status_pointer = other._status_pointer;
    this->_error_pointer = other._error_pointer;
    this->_enabled = other._enabled;
    return ;
}

int32_t api_request_metrics_guard::initialize(
    const api_request_metrics_guard &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        other.abort_lifecycle_error("api_request_metrics_guard::initialize(copy)",
            "source is uninitialised");
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const int32_t endpoint_error = this->_endpoint.initialize(other._endpoint.c_str());
    if (endpoint_error != FT_ERR_SUCCESS)
        return (endpoint_error);
    this->copy_sample_context(other);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t api_request_metrics_guard::initialize(
    api_request_metrics_guard &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        other.abort_lifecycle_error("api_request_metrics_guard::initialize(move)",
            "source is uninitialised");
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    const int32_t move_error = this->_endpoint.move(other._endpoint);
    if (move_error != FT_ERR_SUCCESS)
    {
        (void)this->_endpoint.destroy();
        return (move_error);
    }
    this->copy_sample_context(other);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t api_request_metrics_guard::move(api_request_metrics_guard &other) noexcept
{
    return (this->initialize(ft_move(other)));
}

int32_t api_request_metrics_guard::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t endpoint_error = this->_endpoint.destroy();
    this->_enabled = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (endpoint_error);
}

api_request_metrics_guard::~api_request_metrics_guard() noexcept
{
    t_monotonic_time_point finish_time;
    int64_t duration_ms;
    ft_size_t response_bytes;
    int32_t status_value;
    int32_t error_value;
    ft_networking_observability_sample sample;
    const char *operation_label;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    if (!this->_enabled || !observability_networking_metrics_is_active())
    {
        (void)this->destroy();
        return ;
    }
    finish_time = time_monotonic_point_now();
    duration_ms = time_monotonic_point_diff_ms(this->_start_time, finish_time);
    if (duration_ms < 0)
        duration_ms = 0;
    response_bytes = 0;
    if (this->_result_body && *this->_result_body)
        response_bytes = ft_strlen_size_t(*this->_result_body);
    status_value = -1;
    if (this->_status_pointer)
        status_value = *this->_status_pointer;
    error_value = FT_ERR_SUCCESS;
    if (this->_error_pointer)
        error_value = *this->_error_pointer;
    sample.labels.component = "api";
    operation_label = this->_method;
    if (!operation_label)
        operation_label = "UNKNOWN";
    sample.labels.operation = operation_label;
    sample.labels.target = this->_endpoint.c_str();
    sample.labels.resource = this->_resource;
    if (!sample.labels.resource)
        sample.labels.resource = "/";
    sample.duration_ms = duration_ms;
    sample.request_bytes = this->_request_bytes;
    sample.response_bytes = response_bytes;
    sample.status_code = status_value;
    sample.error_code = error_value;
    sample.error_tag = ft_nullptr;
    if (error_value == FT_ERR_SUCCESS)
        sample.success = FT_TRUE;
    else
        sample.success = FT_FALSE;
    observability_networking_metrics_record(sample);
    (void)this->destroy();
    return ;
}
