#include "http_server.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../../Test/Test/networking_test_hooks.hpp"
#else
# define NETWORKING_TEST_SHOULD_FAIL(point) FT_FALSE
#endif
#include "networking.hpp"
#include "../Basic/basic.hpp"
#include "../Time/time.hpp"
#include "../Observability/observability_networking_metrics.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <new>
#include <inttypes.h>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
#endif

ft_http_server::ft_http_server() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _server_socket(),
      _non_blocking(FT_FALSE), _run_once_active(false), _mutex(ft_nullptr)
{
    return ;
}

ft_http_server::~ft_http_server() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t ft_http_server::move(ft_http_server &other) noexcept
{
    return (this->initialize(static_cast<ft_http_server &&>(other)));
}

int32_t ft_http_server::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_http_server::initialize",
            "initialize called on initialised instance");
    this->_mutex = ft_nullptr;
    this->_non_blocking = FT_FALSE;
    this->_run_once_active.store(false);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_http_server::initialize(const ft_http_server &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "ft_http_server::initialize(const ft_http_server &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    this->_non_blocking = other._non_blocking;
    return (FT_ERR_SUCCESS);
}

int32_t ft_http_server::initialize(ft_http_server &&other) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t ft_http_server::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    this->_run_once_active.store(false);
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    (void)this->_server_socket.destroy();
    this->_non_blocking = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_http_server::enable_thread_safety() noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_mutex = new (std::nothrow) pt_recursive_mutex();
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (this->_mutex->initialize() != FT_ERR_SUCCESS)
    {
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        return (FT_ERR_INVALID_OPERATION);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_http_server::disable_thread_safety() noexcept
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_http_server::is_thread_safe() const noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_http_server::start(const char *ip_address, uint16_t port, int32_t address_family, ft_bool non_blocking)
{
    SocketConfig configuration;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_server::start");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);

    if (configuration.initialize() != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    configuration._type = SocketType::SERVER;
    ft_strncpy(configuration._ip, ip_address, sizeof(configuration._ip) - 1);
    configuration._ip[sizeof(configuration._ip) - 1] = '\0';
    configuration._port = port;
    configuration._backlog = 10;
    configuration._protocol = IPPROTO_TCP;
    configuration._address_family = address_family;
    configuration._reuse_address = FT_TRUE;
    configuration._non_blocking = non_blocking;
    configuration._recv_timeout = 5000;
    configuration._send_timeout = 5000;
    if (this->_server_socket.initialize(configuration) != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_non_blocking = non_blocking;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

static int32_t parse_request(const ft_string &request, ft_string &body, ft_bool &is_post)
{
    const char *request_data;
    const char *body_separator;

    request_data = request.c_str();
    if (ft_strncmp(request_data, "POST ", 5) == 0)
        is_post = FT_TRUE;
    else if (ft_strncmp(request_data, "GET ", 4) == 0)
        is_post = FT_FALSE;
    else
        return (FT_ERR_INVALID_ARGUMENT);
    body_separator = ft_strstr(request_data, "\r\n\r\n");
    if (body_separator)
    {
        body_separator += 4;
        body = body_separator;
    }
    else
        body.clear();
    return (FT_ERR_SUCCESS);
}

static void http_server_record_metrics(const char *method, ft_size_t request_bytes,
    ft_size_t response_bytes, int32_t status_code, int32_t result, int32_t error_code,
    t_monotonic_time_point start_time)
{
    ft_networking_observability_sample sample;
    t_monotonic_time_point finish_time;
    int64_t duration_ms;
    finish_time = time_monotonic_point_now();
    duration_ms = time_monotonic_point_diff_ms(start_time, finish_time);
    if (duration_ms < 0)
        duration_ms = 0;
    if (result == 0)
        error_code = FT_ERR_SUCCESS;
    sample.labels.component = "http_server";
    sample.labels.operation = method;
    sample.labels.target = "listener";
    sample.labels.resource = NULL;
    sample.duration_ms = duration_ms;
    sample.request_bytes = request_bytes;
    sample.response_bytes = response_bytes;
    sample.status_code = status_code;
    sample.error_code = error_code;
    if (error_code == FT_ERR_SUCCESS)
    {
        sample.success = FT_TRUE;
        sample.error_tag = "ok";
    }
    else
    {
        sample.success = FT_FALSE;
        sample.error_tag = ft_strerror(error_code);
    }
    observability_networking_metrics_record(sample);
    return ;
}

static char http_server_to_lower(char character)
{
    if (character >= 'A' && character <= 'Z')
        return (static_cast<char>(character - 'A' + 'a'));
    return (character);
}

static ft_bool http_server_token_equals(const char *token_start, ft_size_t token_length, const char *token)
{
    ft_size_t index;

    index = 0;
    while (index < token_length && token[index] != '\0')
    {
        char left_hand_side_character;
        char right_hand_side_character;

        left_hand_side_character = http_server_to_lower(token_start[index]);
        right_hand_side_character = http_server_to_lower(token[index]);
        if (left_hand_side_character != right_hand_side_character)
            return (FT_FALSE);
        index++;
    }
    if (index != token_length || token[index] != '\0')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool http_server_request_wants_keep_alive(const ft_string &request, ft_size_t header_length)
{
    const char *data;
    const char *connection_header;
    const char *value_pointer;
    ft_bool keep_alive;
    ft_size_t header_limit;

    data = request.c_str();
    header_limit = header_length;
    if (header_limit > request.size())
        header_limit = request.size();
    keep_alive = FT_TRUE;
    if (ft_strnstr(data, "HTTP/1.0", header_limit) != ft_nullptr)
        keep_alive = FT_FALSE;
    connection_header = ft_strnstr(data, "Connection:", header_limit);
    if (connection_header == ft_nullptr)
        connection_header = ft_strnstr(data, "connection:", header_limit);
    if (connection_header == ft_nullptr)
        return (keep_alive);
    value_pointer = connection_header;
    while (*value_pointer != '\0' && value_pointer < data + header_limit && *value_pointer != ':')
        value_pointer++;
    if (*value_pointer != ':')
        return (keep_alive);
    value_pointer++;
    while (*value_pointer == ' ' || *value_pointer == '\t')
        value_pointer++;
    while (value_pointer < data + header_limit
        && *value_pointer != '\0' && *value_pointer != '\r'
        && *value_pointer != '\n')
    {
        const char *token_start;
        ft_size_t token_length;

        token_start = value_pointer;
        while (value_pointer < data + header_limit
            && *value_pointer != '\0' && *value_pointer != '\r'
            && *value_pointer != '\n' && *value_pointer != ','
            && *value_pointer != ';')
        {
            value_pointer++;
        }
        token_length = static_cast<ft_size_t>(value_pointer - token_start);
        while (token_length > 0
            && (token_start[token_length - 1] == ' '
                || token_start[token_length - 1] == '\t'))
        {
            token_length--;
        }
        if (token_length > 0)
        {
            if (http_server_token_equals(token_start, token_length, "close") != FT_FALSE)
                keep_alive = FT_FALSE;
            if (http_server_token_equals(token_start, token_length, "keep-alive") != FT_FALSE)
                keep_alive = FT_TRUE;
        }
        while (*value_pointer == ' ' || *value_pointer == '\t'
            || *value_pointer == ',' || *value_pointer == ';')
        {
            value_pointer++;
        }
    }
    return (keep_alive);
}


int32_t ft_http_server::run_once() noexcept
{
    int32_t result;
    int32_t lock_error;
    bool expected_running;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_server::run_once");
    expected_running = false;
    if (!this->_run_once_active.compare_exchange_strong(expected_running, true))
        return (FT_ERR_INVALID_OPERATION);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_run_once_active.store(false);
        return (FT_ERR_INVALID_OPERATION);
    }
    result = this->run_once_locked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->_run_once_active.store(false);
    return (result);
}

int32_t ft_http_server::run_once_locked()
{
    t_monotonic_time_point start_time;
    struct sockaddr_storage client_address;
    socklen_t address_length;
    int32_t client_socket;
    int32_t server_fd;
    ft_bool connection_active;
    int32_t overall_result;
    int32_t processed_requests;
    int32_t last_error_code;
    const int32_t max_keep_alive_requests = 100;

    start_time = time_monotonic_point_now();
    server_fd = this->_server_socket.get_file_descriptor();
    if (server_fd < 0)
        return (FT_ERR_INVALID_OPERATION);
    address_length = sizeof(client_address);
    client_socket = nw_accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_address), &address_length);
    if (client_socket < 0)
    {
#ifdef _WIN32
        int32_t last_error;

        last_error = WSAGetLastError();
        if (this->_non_blocking != FT_FALSE && last_error == WSAEWOULDBLOCK)
            return (FT_ERR_SUCCESS);
        last_error_code = FT_ERR_SOCKET_ACCEPT_FAILED;
#else
        int32_t last_error;

        last_error = errno;
        if (this->_non_blocking != FT_FALSE && (last_error == EAGAIN || last_error == EWOULDBLOCK))
            return (FT_ERR_SUCCESS);
        last_error_code = FT_ERR_SOCKET_ACCEPT_FAILED;
#endif
        return (FT_ERR_INVALID_OPERATION);
    }
    connection_active = FT_TRUE;
    overall_result = 0;
    processed_requests = 0;
    last_error_code = FT_ERR_SUCCESS;
    ft_string pending_data;
    if (pending_data.initialize() != FT_ERR_SUCCESS)
    {
        nw_close(client_socket);
        return (FT_ERR_NO_MEMORY);
    }
    while (connection_active != FT_FALSE)
    {
        t_monotonic_time_point request_start_time;
        ft_string request;
        ft_string body;
        ft_string response;
        ft_string current_request;
        ft_bool header_complete;
        ft_bool request_complete;
        ft_bool has_content_length;
        ft_size_t expected_body_length;
        ft_size_t header_length;
        const ft_size_t max_request_size = 65536;
        ft_bool metrics_enabled;
        const char *method_label;
        ft_size_t request_bytes;
        ft_size_t response_bytes;
        int32_t status_code_value;
        ft_bool is_post;
        int32_t parse_error;
        ft_bool request_failed;
        int32_t request_result;
        ft_bool should_keep_alive;
        char buffer[1024];
        ssize_t bytes_received;
        if (request.initialize() != FT_ERR_SUCCESS
            || body.initialize() != FT_ERR_SUCCESS
            || response.initialize() != FT_ERR_SUCCESS
            || current_request.initialize() != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            return (FT_ERR_NO_MEMORY);
        }

        request_start_time = time_monotonic_point_now();
        header_complete = FT_FALSE;
        request_complete = FT_FALSE;
        has_content_length = FT_FALSE;
        expected_body_length = 0;
        header_length = 0;
        metrics_enabled = FT_TRUE;
        method_label = "UNKNOWN";
        request_bytes = 0;
        response_bytes = 0;
        status_code_value = 0;
        is_post = FT_FALSE;
        parse_error = FT_ERR_SUCCESS;
        request_failed = FT_FALSE;
        request_result = 1;
        should_keep_alive = FT_FALSE;
        request = pending_data;
        pending_data.clear();
        while (request_complete == FT_FALSE)
        {
            if (header_complete == FT_FALSE)
            {
                const char *full_request;
                const char *header_end_pointer;

                full_request = request.c_str();
                header_end_pointer = ft_strstr(full_request, "\r\n\r\n");
                if (header_end_pointer != ft_nullptr)
                {
                    const char *content_length_header;
                    const char *content_length_key;
                    ft_size_t content_length_key_length;

                    header_end_pointer += 4;
                    header_length = static_cast<ft_size_t>(header_end_pointer - full_request);
                    header_complete = FT_TRUE;
                    content_length_key = "Content-Length:";
                    content_length_key_length = sizeof("Content-Length:") - 1;
                    content_length_header = ft_strnstr(full_request, content_length_key, header_length);
                    if (content_length_header != ft_nullptr)
                    {
                        const char *length_value_pointer;
                        ft_bool has_length_digits;

                        length_value_pointer = content_length_header + content_length_key_length;
                        while (*length_value_pointer != '\0'
                            && ft_isspace(static_cast<unsigned char>(*length_value_pointer)) != 0)
                            length_value_pointer++;
                        if (*length_value_pointer == '\0'
                            || ft_isdigit(static_cast<unsigned char>(*length_value_pointer)) == 0)
                        {
                            nw_close(client_socket);
                            overall_result = 1;
                            last_error_code = FT_ERR_INVALID_ARGUMENT;
                            request_failed = FT_TRUE;
                            break ;
                        }
                        has_length_digits = FT_FALSE;
                        expected_body_length = 0;
                        while (*length_value_pointer != '\0'
                            && ft_isdigit(static_cast<unsigned char>(*length_value_pointer)) != 0)
                        {
                            ft_size_t digit_value;

                            has_length_digits = FT_TRUE;
                            digit_value = static_cast<ft_size_t>(*length_value_pointer - '0');
                            if (expected_body_length > (max_request_size - digit_value) / 10)
                            {
                                nw_close(client_socket);
                                overall_result = 1;
                                last_error_code = FT_ERR_INVALID_ARGUMENT;
                                request_failed = FT_TRUE;
                                break ;
                            }
                            expected_body_length = expected_body_length * 10 + digit_value;
                            length_value_pointer++;
                        }
                        if (request_failed != FT_FALSE)
                            break ;
                        if (has_length_digits == FT_FALSE)
                        {
                            nw_close(client_socket);
                            overall_result = 1;
                            last_error_code = FT_ERR_INVALID_ARGUMENT;
                            request_failed = FT_TRUE;
                            break ;
                        }
                        if (expected_body_length > max_request_size)
                        {
                            nw_close(client_socket);
                            overall_result = 1;
                            last_error_code = FT_ERR_INVALID_ARGUMENT;
                            request_failed = FT_TRUE;
                            break ;
                        }
                        has_content_length = FT_TRUE;
                    }
                }
            }
            if (request_failed != FT_FALSE)
                break ;
            if (header_complete != FT_FALSE)
            {
                ft_size_t current_body_size;

                if (request.size() >= header_length)
                    current_body_size = request.size() - header_length;
                else
                    current_body_size = 0;
                if (has_content_length != FT_FALSE)
                {
                    if (current_body_size >= expected_body_length)
                        request_complete = FT_TRUE;
                }
                else
                    request_complete = FT_TRUE;
            }
            if (request_complete != FT_FALSE)
                break ;
            bytes_received = nw_recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received < 0)
            {
                nw_close(client_socket);
#ifdef _WIN32
                last_error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
#else
                last_error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
#endif
                overall_result = 1;
                request_failed = FT_TRUE;
                break ;
            }
            if (bytes_received == 0)
            {
                if (request.empty() != FT_FALSE && header_complete == FT_FALSE)
                {
                    nw_close(client_socket);
                    return (FT_ERR_SUCCESS);
                }
                nw_close(client_socket);
                overall_result = 1;
                last_error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
                request_failed = FT_TRUE;
                break ;
            }
            buffer[bytes_received] = '\0';
            request.append(buffer);
            if (request.size() > max_request_size)
            {
                nw_close(client_socket);
                overall_result = 1;
                last_error_code = FT_ERR_INVALID_ARGUMENT;
                request_failed = FT_TRUE;
                break ;
            }
        }
        if (request_failed != FT_FALSE)
        {
            if (metrics_enabled != FT_FALSE)
                http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, request_result, last_error_code, request_start_time);
            connection_active = FT_FALSE;
            break ;
        }
        ft_size_t consumed_length;

        consumed_length = header_length;
        if (has_content_length != FT_FALSE)
            consumed_length += expected_body_length;
        if (request.size() > consumed_length)
            pending_data.assign(request.c_str() + consumed_length, request.size() - consumed_length);
        else
            pending_data.clear();
        current_request.assign(request.c_str(), consumed_length);
        request_bytes = consumed_length;
        parse_error = parse_request(current_request, body, is_post);
        if (parse_error != FT_ERR_SUCCESS)
        {
            nw_close(client_socket);
            overall_result = 1;
            last_error_code = parse_error;
            if (metrics_enabled != FT_FALSE)
                http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, 1, last_error_code, request_start_time);
            connection_active = FT_FALSE;
            break ;
        }
        if (is_post)
            method_label = "POST";
        else
            method_label = "GET";
        should_keep_alive = http_server_request_wants_keep_alive(current_request, header_length);
        if (processed_requests >= max_keep_alive_requests - 1)
            should_keep_alive = FT_FALSE;
        if (is_post && !body.empty())
        {
            pf_snprintf(buffer, sizeof(buffer), FT_UINT64_DECIMAL_FORMAT,
                body.size());
            response.append("HTTP/1.1 200 OK\r\nContent-Length: ");
            response.append(buffer);
            if (should_keep_alive != FT_FALSE)
                response.append("\r\nConnection: keep-alive\r\n\r\n");
            else
                response.append("\r\nConnection: close\r\n\r\n");
            response.append(body);
        }
        else if (!is_post)
        {
            response.append("HTTP/1.1 200 OK\r\nContent-Length: 3\r\n");
            if (should_keep_alive != FT_FALSE)
                response.append("Connection: keep-alive\r\n\r\n");
            else
                response.append("Connection: close\r\n\r\n");
            response.append("GET");
        }
        else
        {
            response.append("HTTP/1.1 200 OK\r\nContent-Length: 0\r\n");
            if (should_keep_alive != FT_FALSE)
                response.append("Connection: keep-alive\r\n\r\n");
            else
                response.append("Connection: close\r\n\r\n");
        }
        const char *response_data;
        ft_size_t total_sent;
        ssize_t send_result;

        response_data = response.c_str();
        total_sent = 0;
        response_bytes = response.size();
        ft_bool remote_closed_during_send;

        remote_closed_during_send = FT_FALSE;
        while (total_sent < response.size())
        {
            if (NETWORKING_TEST_SHOULD_FAIL(
                    NETWORKING_TEST_HTTP_SERVER_SEND) != FT_FALSE)
                send_result = -1;
            else
                send_result = nw_send(client_socket, response_data + total_sent,
                    response.size() - total_sent, 0);
            if (send_result <= 0)
            {
#ifdef _WIN32
                int32_t last_error;

                last_error = WSAGetLastError();
                if (send_result == 0
                    || (send_result < 0
                        && (last_error == WSAECONNRESET || last_error == WSAECONNABORTED
                            || last_error == WSAESHUTDOWN)))
                {
                    overall_result = 1;
                    last_error_code = FT_ERR_SOCKET_SEND_FAILED;
                    request_result = 1;
                    if (metrics_enabled != FT_FALSE)
                        http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, request_result, last_error_code, request_start_time);
                    connection_active = FT_FALSE;
                    remote_closed_during_send = FT_TRUE;
                    break ;
                }
#else
                if (send_result == 0
                    || (send_result < 0 && (errno == EPIPE || errno == ECONNRESET)))
                {
                    overall_result = 1;
                    last_error_code = FT_ERR_SOCKET_SEND_FAILED;
                    request_result = 1;
                    if (metrics_enabled != FT_FALSE)
                        http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, request_result, last_error_code, request_start_time);
                    connection_active = FT_FALSE;
                    remote_closed_during_send = FT_TRUE;
                    break ;
                }
#endif
                nw_close(client_socket);
                if (send_result < 0)
                    last_error_code = FT_ERR_SOCKET_SEND_FAILED;
                else
                    last_error_code = FT_ERR_SOCKET_SEND_FAILED;
                overall_result = 1;
                request_result = 1;
                if (metrics_enabled != FT_FALSE)
                    http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, request_result, last_error_code, request_start_time);
                connection_active = FT_FALSE;
                break ;
            }
            total_sent += static_cast<ft_size_t>(send_result);
        }
        if (remote_closed_during_send != FT_FALSE)
            break ;
        else if (connection_active == FT_FALSE)
            break ;
        status_code_value = 200;
        request_result = 0;
        if (metrics_enabled != FT_FALSE)
            http_server_record_metrics(method_label, request_bytes, response_bytes, status_code_value, request_result, FT_ERR_SUCCESS, request_start_time);
        processed_requests++;
        if (should_keep_alive == FT_FALSE)
            connection_active = FT_FALSE;
    }
    nw_close(client_socket);
    if (overall_result == 0)
        return (FT_ERR_SUCCESS);
    return (FT_ERR_INVALID_OPERATION);
}
