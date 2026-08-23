#include "api_http_internal.hpp"
#include "api.hpp"
#include "api_http_common.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/networking.hpp"
#include "../Networking/http2_client.hpp"
#include "../CPP_class/class_string.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Logger/logger.hpp"
#include "../Printf/printf.hpp"
#include "../Time/time.hpp"
#include <errno.h>
#include <cstdio>
#include <utility>
#include "../Template/move.hpp"

#include "../Basic/limits.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#include "../Template/vector.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <unistd.h>
#endif

static ft_bool api_http_apply_timeouts(ft_socket &socket_wrapper, int32_t timeout)
{
    if (timeout <= 0)
        return (FT_TRUE);
#ifdef _WIN32
    DWORD win_timeout;

    win_timeout = static_cast<DWORD>(timeout);
    if (setsockopt(socket_wrapper.get_file_descriptor(), SOL_SOCKET, SO_RCVTIMEO,
            reinterpret_cast<const char*>(&win_timeout), sizeof(win_timeout)) != 0)
        return (FT_FALSE);
    if (setsockopt(socket_wrapper.get_file_descriptor(), SOL_SOCKET, SO_SNDTIMEO,
            reinterpret_cast<const char*>(&win_timeout), sizeof(win_timeout)) != 0)
        return (FT_FALSE);
#else
    struct timeval tv;

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    if (setsockopt(socket_wrapper.get_file_descriptor(), SOL_SOCKET, SO_RCVTIMEO,
            &tv, sizeof(tv)) != 0)
        return (FT_FALSE);
    if (setsockopt(socket_wrapper.get_file_descriptor(), SOL_SOCKET, SO_SNDTIMEO,
            &tv, sizeof(tv)) != 0)
        return (FT_FALSE);
#endif
    return (FT_TRUE);
}

static ft_bool api_http_is_recoverable_send_error(int32_t error_code)
{
    if (error_code == FT_ERR_SOCKET_SEND_FAILED)
        return (FT_TRUE);
    if (error_code == FT_ERR_IO)
        return (FT_TRUE);
#ifdef _WIN32
    if (error_code == ((WSAECONNRESET)))
        return (FT_TRUE);
    if (error_code == ((WSAECONNABORTED)))
        return (FT_TRUE);
#else
    if (error_code == ((ECONNRESET)))
        return (FT_TRUE);
    if (error_code == ((EPIPE)))
        return (FT_TRUE);
#endif
    return (FT_FALSE);
}

ft_bool api_http_should_retry_plain(int32_t error_code)
{
    if (api_http_is_recoverable_send_error(error_code))
        return (FT_TRUE);
    if (error_code == FT_ERR_HTTP_PROTOCOL_MISMATCH)
        return (FT_TRUE);
#ifdef _WIN32
    if (error_code == ((WSAECONNRESET)))
        return (FT_TRUE);
#else
    if (error_code == ((ECONNRESET)))
        return (FT_TRUE);
#endif
    if (error_code == FT_ERR_SOCKET_RECEIVE_FAILED)
        return (FT_TRUE);
    if (error_code == FT_ERR_SOCKET_CONNECT_FAILED)
        return (FT_TRUE);
    return (FT_FALSE);
}

#if NETWORKING_HAS_OPENSSL
static ft_bool api_http_http2_failure_requires_eviction(int32_t error_code)
{
    if (error_code == FT_ERR_SOCKET_RECEIVE_FAILED)
        return (FT_TRUE);
    if (error_code == FT_ERR_SOCKET_SEND_FAILED)
        return (FT_TRUE);
    if (error_code == FT_ERR_SOCKET_CONNECT_FAILED)
        return (FT_TRUE);
    if (error_code == FT_ERR_IO)
        return (FT_TRUE);
    return (FT_FALSE);
}
#endif

static void api_http_reset_plain_socket(api_connection_pool_handle &connection_handle)
{
    ft_bool should_store;

    should_store = connection_handle.should_store;
    connection_handle.socket.close_socket();
    connection_handle.socket.close_socket();
    connection_handle.has_socket = FT_FALSE;
    connection_handle.from_pool = FT_FALSE;
    connection_handle.negotiated_http2 = FT_FALSE;
#if NETWORKING_HAS_OPENSSL
    connection_handle.tls_session = ft_nullptr;
    connection_handle.tls_context = ft_nullptr;
#endif
    connection_handle.should_store = should_store;
    connection_handle.plain_socket_timed_out = FT_FALSE;
    connection_handle.plain_socket_validated = FT_FALSE;
    return ;
}

ft_bool api_http_plain_socket_is_alive(api_connection_pool_handle &connection_handle)
{
    int32_t poll_descriptor;
    int32_t poll_result;
    char peek_byte;
    ssize_t peek_result;

    if (connection_handle.plain_socket_timed_out)
    {
        return (FT_FALSE);
    }
    poll_descriptor = connection_handle.socket.get_file_descriptor();
    if (poll_descriptor < 0)
    {
        return (FT_FALSE);
    }
    poll_result = nw_poll(&poll_descriptor, 1, ft_nullptr, 0, 50);
    if (poll_result < 0)
    {
        return (FT_FALSE);
    }
    if (poll_result == 0)
    {
        return (FT_TRUE);
    }
    if (poll_descriptor == -1)
    {
        return (FT_FALSE);
    }
    peek_byte = 0;
#ifdef _WIN32
    peek_result = connection_handle.socket.receive_data(&peek_byte, 1, MSG_PEEK);
    if (peek_result < 0)
    {
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error == WSAEWOULDBLOCK || last_error == WSAEINTR)
            return (FT_TRUE);
        return (FT_FALSE);
    }
#else
    peek_result = connection_handle.socket.receive_data(&peek_byte, 1, MSG_PEEK | MSG_DONTWAIT);
    if (peek_result < 0)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
            return (FT_TRUE);
        return (FT_FALSE);
    }
#endif
    if (peek_result > 0)
        return (FT_FALSE);
    if (peek_result == 0)
        return (FT_FALSE);
#ifdef _WIN32
    return (FT_FALSE);
#else
    if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
        return (FT_TRUE);
#endif
    return (FT_FALSE);
}

#ifdef DEBUG
static void api_http_plain_debug_log(const char *message);
#else
#define api_http_plain_debug_log(message) do { } while (0)
#endif

ft_bool api_http_prepare_plain_socket(
    api_connection_pool_handle &connection_handle, const char *host,
    uint16_t port, int32_t timeout, int32_t &error_code)
{
    ft_bool pooled_connection;

    api_http_plain_debug_log("prepare_plain_socket: enter");

    if (connection_handle.has_socket)
    {
        if (connection_handle.plain_socket_timed_out)
        {
            api_connection_pool_evict(connection_handle);
            api_http_reset_plain_socket(connection_handle);
        }
        if (!connection_handle.plain_socket_validated)
        {
            if (connection_handle.from_pool == FT_TRUE
                && !api_http_plain_socket_is_alive(connection_handle))
            {
                api_http_plain_debug_log("prepare_plain_socket: reset unvalidated dead");
                api_http_reset_plain_socket(connection_handle);
            }
            else
            {
                connection_handle.plain_socket_validated = FT_TRUE;
                connection_handle.plain_socket_timed_out = FT_FALSE;
                api_http_plain_debug_log("prepare_plain_socket: reuse validated");
                return (FT_TRUE);
            }
        }
        if (api_http_plain_socket_is_alive(connection_handle))
        {
            connection_handle.plain_socket_timed_out = FT_FALSE;
            api_http_plain_debug_log("prepare_plain_socket: reuse alive");
            return (FT_TRUE);
        }
        api_http_plain_debug_log("prepare_plain_socket: reset dead");
        api_http_reset_plain_socket(connection_handle);
    }
    pooled_connection = api_connection_pool_acquire(connection_handle, host, port,
            api_connection_security_mode::PLAIN, ft_nullptr);
    if (pooled_connection)
    {
        api_http_plain_debug_log("prepare_plain_socket: pooled acquire");
        if (!api_http_apply_timeouts(connection_handle.socket, timeout))
        {
            api_http_plain_debug_log("prepare_plain_socket: pooled apply_timeouts failed");
#ifdef _WIN32
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error != 0)
                error_code = (last_error);
            else
                error_code = FT_ERR_CONFIGURATION;
#else
            if (errno != 0)
                error_code = (errno);
            else
                error_code = FT_ERR_CONFIGURATION;
#endif
            api_connection_pool_evict(connection_handle);
            return (FT_FALSE);
        }
        connection_handle.plain_socket_timed_out = FT_FALSE;
        connection_handle.plain_socket_validated = FT_TRUE;
        return (FT_TRUE);
    }
    api_http_plain_debug_log("prepare_plain_socket: create fresh socket");
    SocketConfig config;
    int32_t config_error;
    int32_t socket_error_code;

    config_error = config.initialize();
    if (config_error != FT_ERR_SUCCESS)
    {
        error_code = config_error;
        api_http_plain_debug_log("prepare_plain_socket: config initialize failed");
        return (FT_FALSE);
    }
    config._type = SocketType::CLIENT;
    ft_memset(config._ip, 0, sizeof(config._ip));
    if (host)
        ft_strlcpy(config._ip, host, sizeof(config._ip));
    config._port = port;
    config._recv_timeout = timeout;
    config._send_timeout = timeout;
    (void)connection_handle.socket.destroy();
    socket_error_code = connection_handle.socket.initialize(config);
    if (socket_error_code != FT_ERR_SUCCESS)
    {
        api_http_plain_debug_log("prepare_plain_socket: socket initialize failed");
        if (api_is_configuration_socket_error(socket_error_code))
            error_code = socket_error_code;
        else
            error_code = FT_ERR_SOCKET_CONNECT_FAILED;
        return (FT_FALSE);
    }
    connection_handle.has_socket = FT_TRUE;
    connection_handle.from_pool = FT_FALSE;
    connection_handle.should_store = FT_TRUE;
    connection_handle.security_mode = api_connection_security_mode::PLAIN;
    connection_handle.plain_socket_timed_out = FT_FALSE;
    connection_handle.plain_socket_validated = FT_FALSE;
    api_http_plain_debug_log("prepare_plain_socket: success fresh");
    return (FT_TRUE);
}

#ifdef DEBUG
static void api_http_plain_debug_log(const char *message)
{
    std::FILE *file_pointer;

    file_pointer = std::fopen("api_http_plain_debug.log", "a");
    if (!file_pointer)
        return ;
    std::fprintf(file_pointer, "%s\n", message);
    std::fclose(file_pointer);
    return ;
}
#endif

#if NETWORKING_HAS_OPENSSL
static char *api_http_execute_plain_http2_once(
    api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port, ft_bool &used_http2, int32_t &error_code);

static ft_bool api_http_execute_plain_http2_streaming_once(
    api_connection_pool_handle &connection_handle, const char *method,
    const char *path, const char *host_header, json_group *payload,
    const char *headers, int32_t timeout, const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, ft_bool &connection_close,
    ft_bool &used_http2, int32_t &error_code);

static char *api_http_finalize_downgrade_response(
    api_connection_pool_handle &connection_handle,
    const ft_string &handshake_buffer, ft_size_t header_length,
    ft_bool chunked_encoding, ft_bool has_length, int64_t content_length,
    ft_bool connection_close, int32_t *status, int32_t &error_code)
{
    const char *status_line;
    const char *body_start;
    ft_size_t body_length;
    const char *result_source;
    ft_size_t result_length;
    char *result_body;
    ft_string decoded_body;

    if (decoded_body.is_initialised() == FT_FALSE)
    {
        if (decoded_body.initialize() != FT_ERR_SUCCESS)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = decoded_body.get_error();
            return (ft_nullptr);
        }
    }
    status_line = handshake_buffer.c_str();
    if (status)
    {
        const char *space_position;

        *status = -1;
        space_position = ft_strchr(status_line, ' ');
        if (space_position)
            *status = ft_atoi(space_position + 1);
    }
    if (handshake_buffer.size() < header_length)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    body_start = handshake_buffer.c_str() + header_length;
    body_length = handshake_buffer.size() - header_length;
    result_source = body_start;
    result_length = body_length;
    if (chunked_encoding)
    {
        ft_size_t consumed_length;

        consumed_length = 0;
        if (!api_http_decode_chunked(body_start, body_length,
                decoded_body, consumed_length))
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = FT_ERR_IO;
            return (ft_nullptr);
        }
        result_source = decoded_body.c_str();
        result_length = decoded_body.size();
    }
    else if (has_length)
    {
        ft_size_t expected_length;

        if (content_length < 0)
            expected_length = 0;
        else
            expected_length = static_cast<ft_size_t>(content_length);
        if (body_length < expected_length)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = FT_ERR_IO;
            return (ft_nullptr);
        }
        result_source = body_start;
        result_length = expected_length;
    }
    else
    {
        result_source = body_start;
        result_length = body_length;
    }
    (void)connection_close;
    api_connection_pool_disable_store(connection_handle);
    api_connection_pool_evict(connection_handle);
    result_body = static_cast<char*>(cma_malloc(result_length + 1));
    if (!result_body)
    {
        if (FT_TRUE)
            error_code = FT_ERR_NO_MEMORY;
        else
            error_code = FT_ERR_SUCCESS;
        return (ft_nullptr);
    }
    if (result_length > 0)
        ft_memcpy(result_body, result_source, result_length);
    result_body[result_length] = '\0';
    error_code = FT_ERR_SUCCESS;
    return (result_body);
}
static ft_bool api_http_execute_plain_streaming_once(
    api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t timeout,
    const api_streaming_handler *streaming_handler, ft_bool &connection_close,
    int32_t &error_code);

#endif

static ft_bool api_http_send_request(ft_socket &socket_wrapper,
    const ft_string &request, int32_t &error_code)
{
    ssize_t sent;

    if (request.empty())
    {
        api_http_plain_debug_log("send_request: empty_request");
        return (FT_TRUE);
    }
    if (socket_wrapper.get_file_descriptor() < 0)
        api_http_plain_debug_log("send_request: invalid_fd");
    sent = socket_wrapper.send_all(request.c_str(), request.size());
    if (sent < 0)
    {
        api_http_plain_debug_log("send_request: send_all_failed");
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

static ft_bool api_http_socket_send_callback(const char *data_pointer,
    ft_size_t data_length, void *context, int32_t &error_code)
{
    ft_socket *socket_pointer;
    ssize_t sent_bytes;

    if (!context)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    socket_pointer = static_cast<ft_socket *>(context);
    if (data_length == 0)
        return (FT_TRUE);
    sent_bytes = socket_pointer->send_all(data_pointer, data_length);
    if (sent_bytes < 0)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

static ft_bool api_http_send_payload(ft_socket &socket_wrapper,
    json_group *payload, int32_t &error_code)
{
    if (!payload)
        return (FT_TRUE);
    if (!api_http_stream_json_payload(payload, api_http_socket_send_callback,
            &socket_wrapper, error_code))
        return (FT_FALSE);
    return (FT_TRUE);
}

static void api_http_map_send_error(int32_t &send_error_code)
{
#ifdef _WIN32
    if (send_error_code == ((WSAECONNRESET)))
        send_error_code = FT_ERR_SOCKET_SEND_FAILED;
    if (send_error_code == ((WSAECONNABORTED)))
        send_error_code = FT_ERR_SOCKET_SEND_FAILED;
#else
    if (send_error_code == ((ECONNRESET)))
        send_error_code = FT_ERR_SOCKET_SEND_FAILED;
    if (send_error_code == ((EPIPE)))
        send_error_code = FT_ERR_SOCKET_SEND_FAILED;
#endif
    return ;
}

static ft_bool api_http_prepare_request(const char *method, const char *path,
    const char *host_header, json_group *payload, const char *headers,
    ft_string &request, int32_t &error_code)
{
    ft_size_t payload_length;

    if (request.is_initialised() == FT_FALSE)
    {
        if (request.initialize() != FT_ERR_SUCCESS)
        {
            error_code = request.get_error();
            return (FT_FALSE);
        }
    }
    request.clear();
    if (!method || !path)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    request = method;
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    request += " ";
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    request += path;
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    request += " HTTP/1.1\r\n";
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    if (!host_header)
        host_header = "";
    request += "Host: ";
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    request += host_header;
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    if (headers && headers[0])
    {
        request += "\r\n";
        if (request.get_error() != FT_ERR_SUCCESS)
        {
            error_code = request.get_error();
            return (FT_FALSE);
        }
        request += headers;
        if (request.get_error() != FT_ERR_SUCCESS)
        {
            error_code = request.get_error();
            return (FT_FALSE);
        }
    }
    if (payload)
    {
        if (!api_http_measure_json_payload(payload, payload_length))
        {
            if (FT_TRUE)
                error_code = FT_ERR_IO;
            else
                error_code = FT_ERR_SUCCESS;
            return (FT_FALSE);
        }
        request += "\r\nContent-Type: application/json";
        if (request.get_error() != FT_ERR_SUCCESS)
        {
            error_code = request.get_error();
            return (FT_FALSE);
        }
        if (!api_append_content_length_header(request, payload_length))
        {
            if (FT_TRUE)
                error_code = FT_ERR_IO;
            else
                error_code = FT_ERR_SUCCESS;
            return (FT_FALSE);
        }
    }
    request += "\r\nConnection: keep-alive\r\n\r\n";
    if (request.get_error() != FT_ERR_SUCCESS)
    {
        error_code = request.get_error();
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

ft_bool api_http_stream_invoke_body(
    const api_streaming_handler *streaming_handler, const char *chunk_data,
    ft_size_t chunk_size, ft_bool is_final_chunk, int32_t &error_code)
{
    ft_bool callback_result;
    ft_bool invocation_success;

    if (!streaming_handler)
        return (FT_TRUE);
    callback_result = FT_TRUE;
    invocation_success = streaming_handler->invoke_body_callback(chunk_data,
            chunk_size, is_final_chunk, callback_result);
    if (!invocation_success)
    {
        error_code = FT_ERR_SUCCESS;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    if (!callback_result)
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

void api_http_stream_invoke_headers(
    const api_streaming_handler *streaming_handler, int32_t status_code,
    const char *headers)
{
    if (!streaming_handler)
        return ;
    streaming_handler->invoke_headers_callback(status_code, headers);
    return ;
}

ft_bool api_http_stream_process_chunked_buffer(ft_string &buffer,
    int64_t &chunk_remaining, ft_bool &trailers_pending,
    ft_bool &final_chunk_sent, const api_streaming_handler *streaming_handler,
    int32_t &error_code)
{
    while (FT_TRUE)
    {
        if (trailers_pending)
        {
            const char *trailer_end;
            ft_size_t trailer_length;

            trailer_end = api_http_find_crlf(buffer.c_str(),
                    buffer.c_str() + buffer.size());
            if (!trailer_end)
                break ;
            trailer_length = static_cast<ft_size_t>(trailer_end - buffer.c_str());
            if (buffer.size() < trailer_length + 2)
                break ;
            buffer.erase(0, trailer_length + 2);
            if (trailer_length == 0)
            {
                trailers_pending = FT_FALSE;
                if (!final_chunk_sent)
                {
                    if (!api_http_stream_invoke_body(streaming_handler,
                            ft_nullptr, 0, FT_TRUE, error_code))
                        return (FT_FALSE);
                    final_chunk_sent = FT_TRUE;
                }
            }
            continue ;
        }
        if (chunk_remaining < 0)
        {
            const char *line_end;
            ft_size_t line_length;
            int64_t chunk_size_value;

            line_end = api_http_find_crlf(buffer.c_str(),
                    buffer.c_str() + buffer.size());
            if (!line_end)
                break ;
            line_length = static_cast<ft_size_t>(line_end - buffer.c_str());
            if (buffer.size() < line_length + 2)
                break ;
            if (!api_http_parse_hex(buffer.c_str(),
                    buffer.c_str() + line_length, chunk_size_value))
            {
                error_code = FT_ERR_IO;
                return (FT_FALSE);
            }
            buffer.erase(0, line_length + 2);
            if (chunk_size_value < 0)
            {
                error_code = FT_ERR_IO;
                return (FT_FALSE);
            }
            if (chunk_size_value == 0)
            {
                chunk_remaining = -1;
                trailers_pending = FT_TRUE;
                continue ;
            }
            chunk_remaining = chunk_size_value;
            continue ;
        }
        ft_size_t required_size;

        required_size = static_cast<ft_size_t>(chunk_remaining);
        if (buffer.size() < required_size + 2)
            break ;
        if (!api_http_stream_invoke_body(streaming_handler,
                buffer.c_str(), required_size, FT_FALSE, error_code))
            return (FT_FALSE);
        buffer.erase(0, required_size);
        if (buffer.size() < 2)
        {
            error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        if (buffer.c_str()[0] != '\r' || buffer.c_str()[1] != '\n')
        {
            error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        buffer.erase(0, 2);
        chunk_remaining = -1;
    }
    return (FT_TRUE);
}

static ft_bool api_http_streaming_flush_buffer(ft_string &streaming_body_buffer,
    ft_bool has_length, int64_t content_length, ft_size_t &streaming_delivered,
    ft_bool &final_chunk_sent, ft_bool chunked_encoding,
    int64_t &chunk_stream_remaining, ft_bool &chunk_stream_trailers,
    const api_streaming_handler *streaming_handler, int32_t &error_code)
{
    if (streaming_body_buffer.size() == 0)
        return (FT_TRUE);
    if (has_length)
    {
        while (streaming_body_buffer.size() > 0
            && streaming_delivered < static_cast<ft_size_t>(content_length))
        {
            ft_size_t remaining_length;
            ft_size_t chunk_size;
            ft_bool final_chunk;

            remaining_length = static_cast<ft_size_t>(content_length)
                - streaming_delivered;
            chunk_size = streaming_body_buffer.size();
            if (chunk_size > remaining_length)
                chunk_size = remaining_length;
            final_chunk = (streaming_delivered + chunk_size)
                == static_cast<ft_size_t>(content_length);
            if (!api_http_stream_invoke_body(streaming_handler,
                    streaming_body_buffer.c_str(), chunk_size,
                    final_chunk, error_code))
                return (FT_FALSE);
            streaming_body_buffer.erase(0, chunk_size);
            streaming_delivered += chunk_size;
            if (final_chunk)
            {
                final_chunk_sent = FT_TRUE;
                break ;
            }
        }
        if (final_chunk_sent && streaming_body_buffer.size() > 0)
        {
            error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        return (FT_TRUE);
    }
    if (chunked_encoding)
    {
        if (!api_http_stream_process_chunked_buffer(streaming_body_buffer,
                chunk_stream_remaining, chunk_stream_trailers,
                final_chunk_sent, streaming_handler, error_code))
            return (FT_FALSE);
        if (final_chunk_sent && streaming_body_buffer.size() > 0)
        {
            error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        return (FT_TRUE);
    }
    if (!api_http_stream_invoke_body(streaming_handler,
            streaming_body_buffer.c_str(), streaming_body_buffer.size(),
            FT_FALSE, error_code))
        return (FT_FALSE);
    streaming_body_buffer.clear();
    return (FT_TRUE);
}

static ft_bool api_http_receive_response(api_connection_pool_handle &connection_handle,
    ft_socket &socket_wrapper,
    ft_string &response, ft_size_t &header_length, ft_bool &connection_close,
    ft_bool &chunked_encoding, ft_bool &has_length, int64_t &content_length,
    int32_t &error_code, const api_streaming_handler *streaming_handler,
    const ft_string *prefetched_response)
{
    char buffer[32768 + 1];
    ssize_t received;
    ft_bool streaming_enabled;
    ft_bool headers_complete;
    ft_string header_storage;
    ft_string streaming_body_buffer;
    ft_size_t streaming_delivered;
    ft_bool final_chunk_sent;
    int32_t header_status_code;
    int64_t chunk_stream_remaining;
    ft_bool chunk_stream_trailers;
    ft_size_t prefetched_offset;
    ft_bool use_prefetched;
    ft_size_t receive_retry_count;

    streaming_enabled = (streaming_handler != ft_nullptr);
    headers_complete = FT_FALSE;
    streaming_delivered = 0;
    final_chunk_sent = FT_FALSE;
    header_status_code = -1;
    chunk_stream_remaining = -1;
    chunk_stream_trailers = FT_FALSE;
    prefetched_offset = 0;
    use_prefetched = FT_FALSE;
    receive_retry_count = 0;
    if (prefetched_response)
        use_prefetched = FT_TRUE;
    if (response.is_initialised() == FT_FALSE)
    {
        if (response.initialize() != FT_ERR_SUCCESS)
        {
            error_code = response.get_error();
            return (FT_FALSE);
        }
    }
    if (header_storage.is_initialised() == FT_FALSE)
    {
        if (header_storage.initialize() != FT_ERR_SUCCESS)
        {
            error_code = header_storage.get_error();
            return (FT_FALSE);
        }
    }
    if (streaming_body_buffer.is_initialised() == FT_FALSE)
    {
        if (streaming_body_buffer.initialize() != FT_ERR_SUCCESS)
        {
            error_code = streaming_body_buffer.get_error();
            return (FT_FALSE);
        }
    }
    response.clear();
    header_length = 0;
    connection_close = FT_FALSE;
    chunked_encoding = FT_FALSE;
    has_length = FT_FALSE;
    content_length = 0;
    connection_handle.plain_socket_timed_out = FT_FALSE;
    while (FT_TRUE)
    {
        received = -1;
        if (use_prefetched)
        {
            ft_size_t prefetched_size;

            prefetched_size = prefetched_response->size();
            if (prefetched_offset < prefetched_size)
            {
                ft_size_t chunk_size;

                chunk_size = prefetched_size - prefetched_offset;
                if (chunk_size > sizeof(buffer) - 1)
                    chunk_size = sizeof(buffer) - 1;
                ft_memcpy(buffer,
                    prefetched_response->c_str() + prefetched_offset,
                    chunk_size);
                buffer[chunk_size] = '\0';
                received = static_cast<ssize_t>(chunk_size);
                prefetched_offset += chunk_size;
                if (prefetched_offset >= prefetched_size)
                    use_prefetched = FT_FALSE;
            }
            else
                use_prefetched = FT_FALSE;
        }
        if (!use_prefetched && received < 0)
            received = socket_wrapper.receive_data(buffer, sizeof(buffer) - 1);
        if (received < 0)
        {
            int32_t socket_error_code;
            ft_bool retry_receive;

            socket_error_code = FT_ERR_SUCCESS;
#ifdef _WIN32
            socket_error_code = WSAGetLastError();
            if (socket_error_code == WSAEWOULDBLOCK
                || socket_error_code == WSAETIMEDOUT)
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            else if (socket_error_code != 0)
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            else
                error_code = FT_ERR_IO;
#else
            socket_error_code = errno;
            if (socket_error_code == EAGAIN
                || socket_error_code == EWOULDBLOCK
                || socket_error_code == ETIMEDOUT)
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            else if (socket_error_code != 0)
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            else
                error_code = FT_ERR_IO;
#endif
            retry_receive = FT_FALSE;
#ifdef _WIN32
            if (socket_error_code == WSAECONNABORTED
                || socket_error_code == WSAECONNRESET)
                retry_receive = FT_TRUE;
#else
            if (socket_error_code == ECONNABORTED
                || socket_error_code == ECONNRESET
                || socket_error_code == EPIPE)
                retry_receive = FT_TRUE;
#endif
            if (retry_receive && receive_retry_count < 3)
            {
                receive_retry_count += 1;
                time_sleep_ms(10);
                continue ;
            }
            api_http_plain_debug_log("receive_response: received_lt_zero");
            if (error_code == FT_ERR_SOCKET_RECEIVE_FAILED)
                connection_handle.plain_socket_timed_out = FT_TRUE;
#ifdef DEBUG
            char error_buffer[128];

            pf_snprintf(error_buffer, sizeof(error_buffer),
                "receive_response: socket_error=%d", socket_error_code);
            api_http_plain_debug_log(error_buffer);
#endif
            return (FT_FALSE);
        }
        if (received == 0)
        {
            if (!headers_complete)
            {
                api_http_plain_debug_log("receive_response: eof_before_headers");
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
                return (FT_FALSE);
            }
            if (!streaming_enabled)
            {
                ft_size_t body_size;

                body_size = 0;
                if (response.size() > header_length)
                    body_size = response.size() - header_length;
                if (!chunked_encoding && has_length)
                {
                    ft_size_t expected_size;

                    if (content_length <= 0)
                        expected_size = 0;
                    else
                        expected_size = static_cast<ft_size_t>(content_length);
                    if (body_size < expected_size)
                    {
                        api_http_plain_debug_log("receive_response: body_too_short");
                        error_code = FT_ERR_IO;
                        return (FT_FALSE);
                    }
                }
                if (chunked_encoding)
                {
                    ft_size_t consumed_length;

                    if (!api_http_chunked_body_complete(
                            response.c_str() + header_length,
                            body_size, consumed_length))
                    {
                        api_http_plain_debug_log("receive_response: chunked_incomplete");
                        error_code = FT_ERR_IO;
                        return (FT_FALSE);
                    }
                }
                connection_close = FT_TRUE;
                break ;
            }
            if (!api_http_streaming_flush_buffer(streaming_body_buffer,
                    has_length, content_length, streaming_delivered,
                    final_chunk_sent, chunked_encoding,
                    chunk_stream_remaining, chunk_stream_trailers,
                    streaming_handler, error_code))
                return (FT_FALSE);
            if (chunked_encoding)
            {
                if (!final_chunk_sent
                    || chunk_stream_trailers
                    || chunk_stream_remaining >= 0
                    || streaming_body_buffer.size() > 0)
                {
                    api_http_plain_debug_log("receive_response: streaming_chunked_incomplete");
                    error_code = FT_ERR_IO;
                    return (FT_FALSE);
                }
                break ;
            }
            if (has_length)
            {
                if (static_cast<int64_t>(streaming_delivered)
                    != content_length)
                {
                    api_http_plain_debug_log("receive_response: streaming_length_mismatch");
                    error_code = FT_ERR_IO;
                    return (FT_FALSE);
                }
                if (!final_chunk_sent)
                {
                    if (!api_http_stream_invoke_body(streaming_handler,
                            ft_nullptr, 0, FT_TRUE, error_code))
                        return (FT_FALSE);
                    final_chunk_sent = FT_TRUE;
                }
                break ;
            }
            if (!api_http_stream_invoke_body(streaming_handler,
                    ft_nullptr, 0, FT_TRUE, error_code))
                return (FT_FALSE);
            final_chunk_sent = FT_TRUE;
            break ;
        }
        if (!headers_complete)
        {
            buffer[received] = '\0';
            response.append(buffer, static_cast<ft_size_t>(received));
            if (response.get_error() != FT_ERR_SUCCESS)
            {
                api_http_plain_debug_log("receive_response: append_header_failed");
                error_code = response.get_error();
                return (FT_FALSE);
            }
            if (response.size() >= 14)
            {
                if (ft_strncmp(response.c_str(), "PRI * HTTP/2.0", 14) == 0)
                {
                    error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
                    return (FT_FALSE);
                }
            }
            const char *headers_start;
            const char *headers_end;

            headers_start = response.c_str();
            headers_end = ft_strstr(response.c_str(), "\r\n\r\n");
            if (!headers_end)
            {
                if (!streaming_enabled && response.size() >= sizeof(buffer))
                {
                    api_http_plain_debug_log("receive_response: header_buffer_full");
                    error_code = FT_ERR_IO;
                    return (FT_FALSE);
                }
                continue ;
            }
            headers_end += 2;
            header_length = static_cast<ft_size_t>(headers_end - headers_start) + 2;
            api_http_parse_headers(headers_start, headers_end,
                connection_close, chunked_encoding, has_length,
                content_length);
            if (!chunked_encoding && !has_length)
                connection_close = FT_TRUE;
            header_storage.assign(response.c_str(), header_length);
            if (header_storage.get_error() != FT_ERR_SUCCESS)
            {
                api_http_plain_debug_log("receive_response: header_storage_failed");
                error_code = header_storage.get_error();
                return (FT_FALSE);
            }
            if (streaming_enabled)
            {
                const char *status_space;
                ft_size_t body_length;

                status_space = ft_strchr(header_storage.c_str(), ' ');
                if (status_space)
                    header_status_code = ft_atoi(status_space + 1);
                api_http_stream_invoke_headers(streaming_handler,
                    header_status_code, header_storage.c_str());
                body_length = response.size() - header_length;
                if (body_length > 0)
                {
                    streaming_body_buffer.append(
                        response.c_str() + header_length, body_length);
                    if (streaming_body_buffer.get_error() != FT_ERR_SUCCESS)
                    {
                        api_http_plain_debug_log("receive_response: streaming_append_failed");
                        error_code = streaming_body_buffer.get_error();
                        return (FT_FALSE);
                    }
                    if (!api_http_streaming_flush_buffer(
                            streaming_body_buffer, has_length,
                            content_length, streaming_delivered,
                            final_chunk_sent, chunked_encoding,
                            chunk_stream_remaining, chunk_stream_trailers,
                            streaming_handler, error_code))
                        return (FT_FALSE);
                    if (final_chunk_sent)
                        return (FT_TRUE);
                }
                response = header_storage;
            }
            if (!streaming_enabled)
            {
                ft_size_t body_length;

                body_length = response.size() - header_length;
                if (chunked_encoding)
                {
                    ft_size_t consumed_length;

                    if (api_http_chunked_body_complete(
                            response.c_str() + header_length, body_length,
                            consumed_length))
                    {
                        if (consumed_length <= body_length)
                            return (FT_TRUE);
                    }
                }
                else if (has_length)
                {
                    if (body_length >= static_cast<ft_size_t>(content_length))
                        return (FT_TRUE);
                }
                else if (response.size() >= sizeof(buffer))
                {
                    api_http_plain_debug_log("receive_response: headers_complete_no_length_buffer_full");
                    error_code = FT_ERR_IO;
                    return (FT_FALSE);
                }
            }
            headers_complete = FT_TRUE;
            continue ;
        }
        if (!streaming_enabled)
        {
            buffer[received] = '\0';
            response.append(buffer, static_cast<ft_size_t>(received));
            if (response.get_error() != FT_ERR_SUCCESS)
            {
                api_http_plain_debug_log("receive_response: append_body_failed");
                error_code = response.get_error();
                return (FT_FALSE);
            }
            if (chunked_encoding)
            {
                ft_size_t body_size;
                ft_size_t consumed_length;

                body_size = response.size() - header_length;
                if (api_http_chunked_body_complete(
                        response.c_str() + header_length, body_size,
                        consumed_length))
                {
                    if (consumed_length <= body_size)
                        return (FT_TRUE);
                }
            }
            else if (has_length)
            {
                ft_size_t body_size;

                body_size = response.size() - header_length;
                if (body_size >= static_cast<ft_size_t>(content_length))
                    return (FT_TRUE);
            }
            continue ;
        }
        streaming_body_buffer.append(buffer, static_cast<ft_size_t>(received));
        if (streaming_body_buffer.get_error() != FT_ERR_SUCCESS)
        {
            api_http_plain_debug_log("receive_response: streaming_body_append_failed");
            error_code = streaming_body_buffer.get_error();
            return (FT_FALSE);
        }
        if (!api_http_streaming_flush_buffer(streaming_body_buffer, has_length,
                content_length, streaming_delivered, final_chunk_sent,
                chunked_encoding, chunk_stream_remaining,
                chunk_stream_trailers, streaming_handler, error_code))
            return (FT_FALSE);
        if (final_chunk_sent)
            return (FT_TRUE);
    }
    if (response.size() == 0)
    {
        error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
        return (FT_FALSE);
    }
    if (streaming_enabled)
        return (FT_TRUE);
    const char *headers_start;
    const char *headers_end;

    headers_start = response.c_str();
    headers_end = ft_strstr(response.c_str(), "\r\n\r\n");
    if (!headers_end)
    {
        api_http_plain_debug_log("receive_response: final_header_parse_failed");
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    headers_end += 2;
    header_length = static_cast<ft_size_t>(headers_end - headers_start) + 2;
    api_http_parse_headers(headers_start, headers_end, connection_close,
        chunked_encoding, has_length, content_length);
    return (FT_TRUE);
}

static char *api_http_execute_plain_once(
    api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    int32_t &error_code, const ft_string *prefetched_response,
    ft_bool skip_send)
{
    ft_socket &socket_wrapper = connection_handle.socket;
    ft_bool should_skip_send;

    should_skip_send = skip_send;
    if (should_skip_send != FT_FALSE)
    {
        if (prefetched_response == ft_nullptr
            || prefetched_response->size() == 0)
            should_skip_send = FT_FALSE;
    }
    if (should_skip_send != FT_FALSE && !prefetched_response)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    if (!api_http_apply_timeouts(socket_wrapper, timeout))
    {
        api_http_plain_debug_log("plain_once: apply_timeouts failed");
#ifdef _WIN32
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error != 0)
            error_code = (last_error);
        else
            error_code = FT_ERR_CONFIGURATION;
#else
        if (errno != 0)
            error_code = (errno);
        else
            error_code = FT_ERR_CONFIGURATION;
#endif
        return (ft_nullptr);
    }

    ft_bool send_failed;
    int32_t send_error_code;

    send_failed = FT_FALSE;
    send_error_code = FT_ERR_SUCCESS;
    if (!should_skip_send)
    {
        ft_string request;

        if (!api_http_prepare_request(method, path, host_header, payload,
                headers, request, error_code))
        {
            api_http_plain_debug_log("plain_once: prepare_request failed");
            return (ft_nullptr);
        }
#ifdef DEBUG
        char request_length_buffer[128];

        pf_snprintf(request_length_buffer, sizeof(request_length_buffer),
            "plain_once: request_size=%" FT_UINT64_FORMAT, request.size());
        api_http_plain_debug_log(request_length_buffer);
#endif
        if (!api_http_send_request(socket_wrapper, request, error_code))
        {
            api_http_plain_debug_log("plain_once: send_request failed");
            send_failed = FT_TRUE;
            send_error_code = error_code;
            api_connection_pool_disable_store(connection_handle);
            api_http_map_send_error(send_error_code);
            if (!api_http_is_recoverable_send_error(send_error_code))
                return (ft_nullptr);
        }
        else if (!api_http_send_payload(socket_wrapper, payload, error_code))
        {
            api_http_plain_debug_log("plain_once: send_payload failed");
            send_failed = FT_TRUE;
            send_error_code = error_code;
            api_connection_pool_disable_store(connection_handle);
            api_http_map_send_error(send_error_code);
            if (!api_http_is_recoverable_send_error(send_error_code))
                return (ft_nullptr);
        }
    }

    ft_string response;
    ft_size_t header_length;
    ft_bool connection_close;
    ft_bool chunked_encoding;
    ft_bool has_length;
    int64_t content_length;

    if (send_failed)
        error_code = FT_ERR_SUCCESS;
    else
        time_sleep_ms(10);
    if (!api_http_receive_response(connection_handle, socket_wrapper, response, header_length,
            connection_close, chunked_encoding, has_length, content_length,
            error_code, ft_nullptr, prefetched_response))
    {
        api_http_plain_debug_log("plain_once: receive_response failed");
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
        if (send_failed)
            error_code = send_error_code;
        return (ft_nullptr);
    }
    if (send_failed)
        connection_close = FT_TRUE;
    if (status)
    {
        *status = -1;
        const char *space = ft_strchr(response.c_str(), ' ');
        if (space)
            *status = ft_atoi(space + 1);
    }
    const char *body_start = response.c_str() + header_length;
    ft_size_t body_length = response.size() - header_length;
    ft_string decoded_body;
    const char *result_source;
    ft_size_t result_length;

    if (decoded_body.initialize() != FT_ERR_SUCCESS)
    {
        api_http_plain_debug_log("plain_once: decoded_body initialize failed");
        error_code = decoded_body.get_error();
        return (ft_nullptr);
    }
    result_source = body_start;
    result_length = body_length;
    if (chunked_encoding)
    {
        ft_size_t consumed_length;

        if (!api_http_decode_chunked(body_start, body_length, decoded_body,
                consumed_length))
        {
            api_http_plain_debug_log("plain_once: chunked decode failed");
            error_code = FT_ERR_IO;
            (void)decoded_body.destroy();
            return (ft_nullptr);
        }
        result_source = decoded_body.c_str();
        result_length = decoded_body.size();
    }
    else if (has_length)
    {
        ft_size_t expected_length;
        ft_size_t index;

        expected_length = static_cast<ft_size_t>(content_length);
        if (body_length < expected_length)
        {
            api_http_plain_debug_log("plain_once: body too short");
            error_code = FT_ERR_IO;
            (void)decoded_body.destroy();
            return (ft_nullptr);
        }
        decoded_body.clear();
        index = 0;
        while (index < expected_length)
        {
            decoded_body.append(body_start[index]);
            if (decoded_body.get_error() != FT_ERR_SUCCESS)
            {
                error_code = decoded_body.get_error();
                (void)decoded_body.destroy();
                return (ft_nullptr);
            }
            index++;
        }
        result_source = decoded_body.c_str();
        result_length = decoded_body.size();
    }
    else
    {
        decoded_body.clear();
        decoded_body.append(body_start);
        if (decoded_body.get_error() != FT_ERR_SUCCESS)
        {
            api_http_plain_debug_log("plain_once: append failed");
            error_code = decoded_body.get_error();
            (void)decoded_body.destroy();
            return (ft_nullptr);
        }
        result_source = decoded_body.c_str();
        result_length = decoded_body.size();
    }
    if (connection_close)
        api_connection_pool_disable_store(connection_handle);
    char *result_body;

    result_body = static_cast<char*>(cma_malloc(result_length + 1));
    if (!result_body)
    {
        if (FT_TRUE)
            error_code = FT_ERR_NO_MEMORY;
        else
            error_code = FT_ERR_SUCCESS;
        (void)decoded_body.destroy();
        return (ft_nullptr);
    }
    if (result_length > 0)
        ft_memcpy(result_body, result_source, result_length);
    result_body[result_length] = '\0';
    (void)decoded_body.destroy();
    error_code = FT_ERR_SUCCESS;
    return (result_body);
}

static ft_bool api_http_execute_plain_streaming_once(
    api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t timeout,
    const api_streaming_handler *streaming_handler, ft_bool &connection_close,
    int32_t &error_code)
{
    ft_socket &socket_wrapper = connection_handle.socket;

    connection_close = FT_FALSE;
    if (!api_http_apply_timeouts(socket_wrapper, timeout))
    {
#ifdef _WIN32
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error != 0)
            error_code = (last_error);
        else
            error_code = FT_ERR_CONFIGURATION;
#else
        if (errno != 0)
            error_code = (errno);
        else
            error_code = FT_ERR_CONFIGURATION;
#endif
        return (FT_FALSE);
    }

    ft_string request;
    if (!api_http_prepare_request(method, path, host_header, payload, headers,
            request, error_code))
        return (FT_FALSE);
    ft_bool send_failed;
    int32_t send_error_code;

    send_failed = FT_FALSE;
    send_error_code = FT_ERR_SUCCESS;
    if (!api_http_send_request(socket_wrapper, request, error_code))
    {
        send_failed = FT_TRUE;
        send_error_code = error_code;
        api_connection_pool_disable_store(connection_handle);
        api_http_map_send_error(send_error_code);
        if (!api_http_is_recoverable_send_error(send_error_code))
            return (FT_FALSE);
    }
    else if (!api_http_send_payload(socket_wrapper, payload, error_code))
    {
        send_failed = FT_TRUE;
        send_error_code = error_code;
        api_connection_pool_disable_store(connection_handle);
        api_http_map_send_error(send_error_code);
        if (!api_http_is_recoverable_send_error(send_error_code))
            return (FT_FALSE);
    }

    ft_string response;
    ft_size_t header_length;
    ft_bool chunked_encoding;
    ft_bool has_length;
    int64_t content_length;

    chunked_encoding = FT_FALSE;
    has_length = FT_FALSE;
    content_length = 0;
    if (send_failed)
        error_code = FT_ERR_SUCCESS;
    else
        time_sleep_ms(10);
    if (!api_http_receive_response(connection_handle, socket_wrapper, response, header_length,
            connection_close, chunked_encoding, has_length, content_length,
            error_code, streaming_handler, ft_nullptr))
    {
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
        if (send_failed)
            error_code = send_error_code;
        return (FT_FALSE);
    }
    if (send_failed)
        connection_close = FT_TRUE;
    error_code = FT_ERR_SUCCESS;
    return (FT_TRUE);
}

ft_bool api_http_execute_plain_streaming(
    api_connection_pool_handle &connection_handle, const char *method,
    const char *path, const char *host_header, json_group *payload,
    const char *headers, int32_t timeout, const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, int32_t &error_code)
{
    int32_t max_attempts;
    int32_t attempt_index;
    int32_t initial_delay;
    int32_t current_delay;
    int32_t max_delay;
    int32_t multiplier;
    int32_t allowed_attempts;
    ft_bool implicit_retry_added;

    max_attempts = api_retry_get_max_attempts(retry_policy);
    initial_delay = api_retry_get_initial_delay(retry_policy);
    max_delay = api_retry_get_max_delay(retry_policy);
    multiplier = api_retry_get_multiplier(retry_policy);
    current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    attempt_index = 0;
    allowed_attempts = max_attempts;
    implicit_retry_added = FT_FALSE;
    while (attempt_index < allowed_attempts)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (FT_FALSE);
        ft_bool socket_ready;
        ft_bool should_retry;

        socket_ready = api_http_prepare_plain_socket(connection_handle, host,
                port, timeout, error_code);
        if (socket_ready)
        {
            ft_bool connection_close;
            ft_bool success;

            connection_close = FT_FALSE;
            success = api_http_execute_plain_streaming_once(
                    connection_handle, method, path, host_header, payload,
                    headers, timeout, streaming_handler, connection_close,
                    error_code);
            if (success)
            {
                api_retry_circuit_record_success(connection_handle,
                    retry_policy);
                if (connection_close)
                    api_connection_pool_disable_store(connection_handle);
                return (FT_TRUE);
            }
        }
        should_retry = api_http_should_retry_plain(error_code);
        if (!should_retry)
            return (FT_FALSE);
        api_retry_circuit_record_failure(connection_handle, retry_policy);
        if (!implicit_retry_added && retry_policy == ft_nullptr)
        {
            allowed_attempts = 2;
            implicit_retry_added = FT_TRUE;
        }
        api_connection_pool_evict(connection_handle);
        attempt_index++;
        if (attempt_index >= allowed_attempts)
            break ;
        if (current_delay > 0)
        {
            int32_t sleep_delay;

            sleep_delay = api_retry_prepare_delay(current_delay, max_delay);
            if (sleep_delay > 0)
                time_sleep_ms(static_cast<uint32_t>(sleep_delay));
        }
        current_delay = api_retry_next_delay(current_delay, max_delay,
                multiplier);
        if (current_delay <= 0)
            current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (FT_FALSE);
}

#if NETWORKING_HAS_OPENSSL
static ft_bool api_http_execute_plain_http2_streaming_once(
    api_connection_pool_handle &connection_handle, const char *method,
    const char *path, const char *host_header, json_group *payload,
    const char *headers, int32_t timeout, const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, ft_bool &connection_close,
    ft_bool &used_http2, int32_t &error_code)
{
    ft_socket &socket_wrapper = connection_handle.socket;
    const char *client_preface;
    ssize_t sent_bytes;
    http2_frame settings_frame;
    ft_string encoded_frame;
    ft_string handshake_buffer;
    ft_bool received_settings_frame;
    ft_bool received_settings_ack;
    ft_bool ack_sent;
    ft_bool handshake_confirmed;
    ft_bool protocol_downgrade;

    (void)retry_policy;
    (void)port;
    connection_close = FT_TRUE;
    used_http2 = FT_FALSE;
    error_code = FT_ERR_SUCCESS;
    if (settings_frame.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (encoded_frame.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (handshake_buffer.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (!streaming_handler)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    if (!method || !path)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    if (payload)
    {
        error_code = FT_ERR_UNSUPPORTED_TYPE;
        return (FT_FALSE);
    }
    if (!api_http_apply_timeouts(socket_wrapper, timeout))
    {
#ifdef _WIN32
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error != 0)
            error_code = (last_error);
        else
            error_code = FT_ERR_CONFIGURATION;
#else
        if (errno != 0)
            error_code = (errno);
        else
            error_code = FT_ERR_CONFIGURATION;
#endif
        return (FT_FALSE);
    }
    client_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    sent_bytes = socket_wrapper.send_all(client_preface, ft_strlen(client_preface));
    if (sent_bytes < 0)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SOCKET_SEND_FAILED;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    if (!settings_frame.set_type(0x4))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    if (!settings_frame.set_flags(0x0))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    if (!settings_frame.set_stream_identifier(0))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    settings_frame.clear_payload();
    if (!http2_encode_frame(settings_frame, encoded_frame, error_code))
    {
        api_connection_pool_disable_store(connection_handle);
        api_connection_pool_evict(connection_handle);
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    sent_bytes = socket_wrapper.send_all(encoded_frame.c_str(), encoded_frame.size());
    if (sent_bytes < 0)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SOCKET_SEND_FAILED;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    handshake_buffer.clear();
    if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = handshake_buffer.get_error();
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    received_settings_frame = FT_FALSE;
    received_settings_ack = FT_FALSE;
    ack_sent = FT_FALSE;
    handshake_confirmed = FT_FALSE;
    protocol_downgrade = FT_FALSE;
    while (!received_settings_frame || !received_settings_ack)
    {
        char receive_buffer[1024];
        ssize_t received_bytes;

        received_bytes = socket_wrapper.receive_data(receive_buffer, sizeof(receive_buffer));
        if (received_bytes <= 0)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            api_connection_pool_evict(connection_handle);
            return (FT_FALSE);
        }
        handshake_buffer.append(receive_buffer, static_cast<ft_size_t>(received_bytes));
        if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = handshake_buffer.get_error();
            api_connection_pool_evict(connection_handle);
            return (FT_FALSE);
        }
        if (handshake_buffer.size() >= 5)
        {
            if (ft_strncmp(handshake_buffer.c_str(), "HTTP/", 5) == 0)
            {
                api_connection_pool_disable_store(connection_handle);
                protocol_downgrade = FT_TRUE;
                break ;
            }
        }
        ft_size_t parse_offset;

        parse_offset = 0;
        while (FT_TRUE)
        {
            http2_frame incoming_frame;
            int32_t frame_error;
            ft_size_t previous_offset;

            if (incoming_frame.initialize() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_NO_MEMORY;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }

            previous_offset = parse_offset;
            frame_error = FT_ERR_SUCCESS;
            if (!http2_decode_frame(reinterpret_cast<const unsigned char*>(handshake_buffer.c_str()),
                    handshake_buffer.size(), parse_offset, incoming_frame, frame_error))
            {
                if (frame_error == FT_ERR_OUT_OF_RANGE)
                {
                    parse_offset = previous_offset;
                    break ;
                }
                api_connection_pool_disable_store(connection_handle);
                error_code = frame_error;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            uint8_t incoming_type;
            uint8_t incoming_flags;

            if (!incoming_frame.get_type(incoming_type))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (!incoming_frame.get_flags(incoming_flags))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (incoming_type == 0x4)
            {
                if ((incoming_flags & 0x1) != 0)
                    received_settings_ack = FT_TRUE;
                else
                {
                    received_settings_frame = FT_TRUE;
                    handshake_confirmed = FT_TRUE;
                    if (!ack_sent)
                    {
                        http2_frame ack_frame;
                        ft_string ack_encoded;
                        ssize_t ack_bytes;

                        if (ack_frame.initialize() != FT_ERR_SUCCESS)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_NO_MEMORY;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        if (ack_encoded.initialize() != FT_ERR_SUCCESS)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_NO_MEMORY;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }

                        if (!ack_frame.set_type(0x4))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        if (!ack_frame.set_flags(0x1))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        if (!ack_frame.set_stream_identifier(0))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        ack_frame.clear_payload();
                        if (!http2_encode_frame(ack_frame, ack_encoded, error_code))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            api_connection_pool_evict(connection_handle);
                            if (error_code == FT_ERR_SUCCESS)
                                error_code = FT_ERR_IO;
                            return (FT_FALSE);
                        }
                        ack_bytes = socket_wrapper.send_all(ack_encoded.c_str(), ack_encoded.size());
                        if (ack_bytes < 0)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SOCKET_SEND_FAILED;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        ack_sent = FT_TRUE;
                    }
                }
            }
        }
        if (protocol_downgrade)
            break ;
        if (parse_offset > 0)
        {
            handshake_buffer.erase(0, parse_offset);
            if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = handshake_buffer.get_error();
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
        }
    }
    if (protocol_downgrade)
    {
        api_connection_pool_evict(connection_handle);
        error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
        return (FT_FALSE);
    }
    if (!handshake_confirmed)
    {
        api_connection_pool_disable_store(connection_handle);
        api_connection_pool_evict(connection_handle);
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    used_http2 = FT_TRUE;
    ft_vector<http2_header_field> request_headers;
    http2_header_field header_entry;
    ft_string compressed_headers;
    http2_frame headers_frame;
    ft_string headers_encoded;
    const char *authority_value;

    if (request_headers.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (header_entry.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (compressed_headers.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (headers_frame.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (headers_encoded.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }

    authority_value = host_header;
    if (!authority_value || authority_value[0] == '\0')
        authority_value = host;
    if (!header_entry.assign_from_cstr(":method", method))
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    request_headers.push_back(header_entry);
    if (!header_entry.assign_from_cstr(":path", path))
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    request_headers.push_back(header_entry);
    if (!header_entry.assign_from_cstr(":scheme", "http"))
    {
        error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    request_headers.push_back(header_entry);
    if (authority_value && authority_value[0] != '\0')
    {
        if (!header_entry.assign_from_cstr(":authority", authority_value))
        {
            error_code = FT_ERR_IO;
            return (FT_FALSE);
        }
        request_headers.push_back(header_entry);
    }
    if (headers && headers[0])
    {
        const char *header_cursor;

        header_cursor = headers;
        while (*header_cursor != '\0')
        {
            ft_size_t index;
            ft_string header_name;
            ft_string header_value;

            if (header_name.initialize() != FT_ERR_SUCCESS)
            {
                error_code = FT_ERR_NO_MEMORY;
                return (FT_FALSE);
            }
            if (header_value.initialize() != FT_ERR_SUCCESS)
            {
                error_code = FT_ERR_NO_MEMORY;
                return (FT_FALSE);
            }
            index = 0;
            while (header_cursor[index] && header_cursor[index] != ':'
                && header_cursor[index] != '\r')
            {
                header_name.append(header_cursor[index]);
                if (header_name.get_error() != FT_ERR_SUCCESS)
                {
                    error_code = header_name.get_error();
                    return (FT_FALSE);
                }
                index++;
            }
            while (header_cursor[index] == ':' || header_cursor[index] == ' ')
                index++;
            while (header_cursor[index] && header_cursor[index] != '\r'
                && header_cursor[index] != '\n')
            {
                header_value.append(header_cursor[index]);
                if (header_value.get_error() != FT_ERR_SUCCESS)
                {
                    error_code = header_value.get_error();
                    return (FT_FALSE);
                }
                index++;
            }
            if (header_name.size() > 0)
            {
                if (!header_entry.assign(header_name, header_value))
                {
                    error_code = FT_ERR_IO;
                    return (FT_FALSE);
                }
                request_headers.push_back(header_entry);
            }
            while (header_cursor[index] == '\r' || header_cursor[index] == '\n')
                index++;
            header_cursor += index;
        }
    }
    if (!http2_compress_headers(request_headers, compressed_headers, error_code))
    {
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    if (!headers_frame.set_type(0x1))
    {
        error_code = FT_ERR_SUCCESS;
        return (FT_FALSE);
    }
    uint8_t header_flags;

    header_flags = 0x4;
    header_flags |= 0x1;
    if (!headers_frame.set_flags(header_flags))
    {
        error_code = FT_ERR_SUCCESS;
        return (FT_FALSE);
    }
    if (!headers_frame.set_stream_identifier(1))
    {
        error_code = FT_ERR_SUCCESS;
        return (FT_FALSE);
    }
    if (!headers_frame.set_payload(compressed_headers))
    {
        error_code = FT_ERR_SUCCESS;
        return (FT_FALSE);
    }
    if (!http2_encode_frame(headers_frame, headers_encoded, error_code))
    {
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (FT_FALSE);
    }
    sent_bytes = socket_wrapper.send_all(headers_encoded.c_str(), headers_encoded.size());
    if (sent_bytes < 0)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SOCKET_SEND_FAILED;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    ft_string response_buffer;
    ft_string header_block;
    ft_vector<http2_header_field> response_headers;
    ft_string header_lines;
    ft_bool end_stream_received;
    ft_bool headers_dispatched;
    ft_bool final_chunk_sent;

    if (response_buffer.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (header_block.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (response_headers.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    if (header_lines.initialize() != FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_NO_MEMORY;
        return (FT_FALSE);
    }
    response_buffer = handshake_buffer;
    if (response_buffer.get_error() != FT_ERR_SUCCESS)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = response_buffer.get_error();
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    end_stream_received = FT_FALSE;
    headers_dispatched = FT_FALSE;
    final_chunk_sent = FT_FALSE;
    while (!end_stream_received)
    {
        ft_size_t parse_offset;
        ft_bool frame_parsed;

        parse_offset = 0;
        frame_parsed = FT_FALSE;
        while (FT_TRUE)
        {
            http2_frame incoming_frame;
            int32_t frame_error;
            ft_size_t previous_offset;

            if (incoming_frame.initialize() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_NO_MEMORY;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            previous_offset = parse_offset;
            frame_error = FT_ERR_SUCCESS;
            if (!http2_decode_frame(reinterpret_cast<const unsigned char*>(response_buffer.c_str()),
                    response_buffer.size(), parse_offset, incoming_frame, frame_error))
            {
                if (frame_error == FT_ERR_OUT_OF_RANGE)
                {
                    parse_offset = previous_offset;
                    break ;
                }
                api_connection_pool_disable_store(connection_handle);
                error_code = frame_error;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            frame_parsed = FT_TRUE;
            uint8_t incoming_type;
            uint8_t incoming_flags;
            uint32_t incoming_stream;
            ft_string payload_copy;

            if (payload_copy.initialize() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_NO_MEMORY;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }

            if (!incoming_frame.get_type(incoming_type))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (!incoming_frame.get_flags(incoming_flags))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (!incoming_frame.get_stream_identifier(incoming_stream))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (!incoming_frame.copy_payload(payload_copy))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (incoming_type == 0x4)
            {
                if ((incoming_flags & 0x1) == 0)
                {
                    http2_frame ack_frame;
                    ft_string ack_buffer;
                    ssize_t ack_sent_bytes;

                    if (ack_frame.initialize() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_NO_MEMORY;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (ack_buffer.initialize() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_NO_MEMORY;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }

                    if (!ack_frame.set_type(0x4))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!ack_frame.set_flags(0x1))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!ack_frame.set_stream_identifier(0))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_IO;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    ack_frame.clear_payload();
                    if (!http2_encode_frame(ack_frame, ack_buffer, error_code))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        api_connection_pool_evict(connection_handle);
                        if (error_code == FT_ERR_SUCCESS)
                            error_code = FT_ERR_IO;
                        return (FT_FALSE);
                    }
                    ack_sent_bytes = socket_wrapper.send_all(ack_buffer.c_str(), ack_buffer.size());
                    if (ack_sent_bytes < 0)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SOCKET_SEND_FAILED;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                }
                continue ;
            }
            if (incoming_type == 0x6)
            {
                if ((incoming_flags & 0x1) == 0)
                {
                    http2_frame ping_ack;
                    ft_string ping_buffer;
                    ssize_t ping_sent_bytes;

                    if (ping_ack.initialize() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_NO_MEMORY;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (ping_buffer.initialize() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_NO_MEMORY;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }

                    if (!ping_ack.set_type(0x6))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!ping_ack.set_flags(0x1))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!ping_ack.set_stream_identifier(0))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!ping_ack.set_payload(payload_copy))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SUCCESS;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (!http2_encode_frame(ping_ack, ping_buffer, error_code))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        api_connection_pool_evict(connection_handle);
                        if (error_code == FT_ERR_SUCCESS)
                            error_code = FT_ERR_IO;
                        return (FT_FALSE);
                    }
                    ping_sent_bytes = socket_wrapper.send_all(ping_buffer.c_str(), ping_buffer.size());
                    if (ping_sent_bytes < 0)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_SOCKET_SEND_FAILED;
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                }
                continue ;
            }
            if (incoming_type == 0x8)
                continue ;
            if (incoming_type == 0x7)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_IO;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            if (incoming_stream != 1)
                continue ;
            if (incoming_type == 0x1 || incoming_type == 0x9)
            {
                header_block.append(payload_copy.c_str(), payload_copy.size());
                if (header_block.get_error() != FT_ERR_SUCCESS)
                {
                    api_connection_pool_disable_store(connection_handle);
                    error_code = header_block.get_error();
                    api_connection_pool_evict(connection_handle);
                    return (FT_FALSE);
                }
                if ((incoming_flags & 0x4) != 0)
                {
                    int32_t status_code;

                    status_code = -1;
                    if (!http2_decompress_headers(header_block, response_headers, error_code))
                    {
                        if (error_code == FT_ERR_SUCCESS)
                            error_code = FT_ERR_IO;
                        api_connection_pool_disable_store(connection_handle);
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    header_lines.clear();
                    if (header_lines.get_error() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = header_lines.get_error();
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    ft_size_t header_count;
                    ft_size_t header_index;

                    header_count = response_headers.size();
                    header_index = 0;
                    while (header_index < header_count)
                    {
                        const http2_header_field &response_entry = response_headers[header_index];
                        ft_string name_copy;
                        ft_string value_copy;
                        const char *name_cstr;
                        const char *value_cstr;

                        if (!response_entry.copy_name(name_copy))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_IO;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        if (!response_entry.copy_value(value_copy))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_IO;
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        name_cstr = name_copy.c_str();
                        if (name_copy.get_error() != FT_ERR_SUCCESS)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = name_copy.get_error();
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        value_cstr = value_copy.c_str();
                        if (value_copy.get_error() != FT_ERR_SUCCESS)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = value_copy.get_error();
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        if (name_cstr && name_cstr[0] == ':')
                        {
                            if (ft_strcmp(name_cstr, ":status") == 0)
                                status_code = ft_atoi(value_cstr);
                        }
                        else
                        {
                            ft_size_t append_index;

                            append_index = 0;
                            while (name_cstr && name_cstr[append_index])
                            {
                                header_lines.append(name_cstr[append_index]);
                                if (header_lines.get_error() != FT_ERR_SUCCESS)
                                {
                                    api_connection_pool_disable_store(connection_handle);
                                    error_code = header_lines.get_error();
                                    api_connection_pool_evict(connection_handle);
                                    return (FT_FALSE);
                                }
                                append_index++;
                            }
                            header_lines += ": ";
                            if (header_lines.get_error() != FT_ERR_SUCCESS)
                            {
                                api_connection_pool_disable_store(connection_handle);
                                error_code = header_lines.get_error();
                                api_connection_pool_evict(connection_handle);
                                return (FT_FALSE);
                            }
                            append_index = 0;
                            while (value_cstr && value_cstr[append_index])
                            {
                                header_lines.append(value_cstr[append_index]);
                                if (header_lines.get_error() != FT_ERR_SUCCESS)
                                {
                                    api_connection_pool_disable_store(connection_handle);
                                    error_code = header_lines.get_error();
                                    api_connection_pool_evict(connection_handle);
                                    return (FT_FALSE);
                                }
                                append_index++;
                            }
                            header_lines += "\r\n";
                            if (header_lines.get_error() != FT_ERR_SUCCESS)
                            {
                                api_connection_pool_disable_store(connection_handle);
                                error_code = header_lines.get_error();
                                api_connection_pool_evict(connection_handle);
                                return (FT_FALSE);
                            }
                        }
                        header_index++;
                    }
                    const char *header_text;

                    header_text = header_lines.c_str();
                    if (header_lines.get_error() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = header_lines.get_error();
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    api_http_stream_invoke_headers(streaming_handler, status_code,
                        header_text);
                    headers_dispatched = FT_TRUE;
                    header_block.clear();
                    if (header_block.get_error() != FT_ERR_SUCCESS)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = header_block.get_error();
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                }
                if ((incoming_flags & 0x1) != 0)
                {
                    if (!final_chunk_sent)
                    {
                        if (!api_http_stream_invoke_body(streaming_handler, ft_nullptr,
                                0, FT_TRUE, error_code))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            api_connection_pool_evict(connection_handle);
                            return (FT_FALSE);
                        }
                        final_chunk_sent = FT_TRUE;
                    }
                    end_stream_received = FT_TRUE;
                }
                continue ;
            }
            if (incoming_type == 0x0)
            {
                ft_size_t data_length;
                const char *data_pointer;
                ft_bool final_flag;

                data_length = payload_copy.size();
                if (payload_copy.get_error() != FT_ERR_SUCCESS)
                {
                    api_connection_pool_disable_store(connection_handle);
                    error_code = payload_copy.get_error();
                    api_connection_pool_evict(connection_handle);
                    return (FT_FALSE);
                }
                data_pointer = payload_copy.c_str();
                if (payload_copy.get_error() != FT_ERR_SUCCESS)
                {
                    api_connection_pool_disable_store(connection_handle);
                    error_code = payload_copy.get_error();
                    api_connection_pool_evict(connection_handle);
                    return (FT_FALSE);
                }
                final_flag = FT_FALSE;
                if ((incoming_flags & 0x1) != 0)
                    final_flag = FT_TRUE;
                if (data_length > 0 || final_flag)
                {
                    if (!api_http_stream_invoke_body(streaming_handler, data_pointer,
                            data_length, final_flag, error_code))
                    {
                        api_connection_pool_disable_store(connection_handle);
                        api_connection_pool_evict(connection_handle);
                        return (FT_FALSE);
                    }
                    if (final_flag)
                        final_chunk_sent = FT_TRUE;
                }
                if (final_flag)
                    end_stream_received = FT_TRUE;
                continue ;
            }
        }
        if (parse_offset > 0)
        {
            response_buffer.erase(0, parse_offset);
            if (response_buffer.get_error() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = response_buffer.get_error();
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
        }
        if (end_stream_received)
            break ;
        if (!frame_parsed)
        {
            char receive_buffer[16384];
            ssize_t received_bytes;

            received_bytes = socket_wrapper.receive_data(receive_buffer,
                    sizeof(receive_buffer));
            if (received_bytes <= 0)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
            response_buffer.append(receive_buffer,
                static_cast<ft_size_t>(received_bytes));
            if (response_buffer.get_error() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = response_buffer.get_error();
                api_connection_pool_evict(connection_handle);
                return (FT_FALSE);
            }
        }
    }
    if (!headers_dispatched)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_IO;
        api_connection_pool_evict(connection_handle);
        return (FT_FALSE);
    }
    if (!final_chunk_sent)
    {
        if (!api_http_stream_invoke_body(streaming_handler, ft_nullptr, 0, FT_TRUE,
                error_code))
        {
            api_connection_pool_disable_store(connection_handle);
            api_connection_pool_evict(connection_handle);
            return (FT_FALSE);
        }
    }
    api_connection_pool_disable_store(connection_handle);
    api_connection_pool_evict(connection_handle);
    error_code = FT_ERR_SUCCESS;
    return (FT_TRUE);
}

#endif

#if NETWORKING_HAS_OPENSSL
ft_bool api_http_execute_plain_http2_streaming(
    api_connection_pool_handle &connection_handle, const char *method,
    const char *path, const char *host_header, json_group *payload,
    const char *headers, int32_t timeout, const char *host, uint16_t port,
    const api_retry_policy *retry_policy,
    const api_streaming_handler *streaming_handler, ft_bool &used_http2,
    int32_t &error_code)
{
    int32_t max_attempts;
    int32_t attempt_index;
    int32_t initial_delay;
    int32_t current_delay;
    int32_t max_delay;
    int32_t multiplier;
    ft_bool http2_used_local;

    used_http2 = FT_FALSE;
    if (headers && headers[0])
    {
        error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
        return (FT_FALSE);
    }
    max_attempts = api_retry_get_max_attempts(retry_policy);
    initial_delay = api_retry_get_initial_delay(retry_policy);
    max_delay = api_retry_get_max_delay(retry_policy);
    multiplier = api_retry_get_multiplier(retry_policy);
    current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    attempt_index = 0;
    http2_used_local = FT_FALSE;
    while (attempt_index < max_attempts)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (FT_FALSE);
        ft_bool socket_ready;
        ft_bool should_retry;

        socket_ready = api_http_prepare_plain_socket(connection_handle, host,
                port, timeout, error_code);
        if (socket_ready)
        {
            ft_bool connection_close;
            ft_bool success;

            connection_close = FT_FALSE;
            http2_used_local = FT_FALSE;
            success = api_http_execute_plain_http2_streaming_once(
                    connection_handle, method, path, host_header, payload,
                    headers, timeout, host, port, retry_policy,
                    streaming_handler, connection_close, http2_used_local,
                    error_code);
            if (success)
            {
                used_http2 = http2_used_local;
                api_retry_circuit_record_success(connection_handle,
                    retry_policy);
                if (connection_close)
                    api_connection_pool_disable_store(connection_handle);
                return (FT_TRUE);
            }
            if (error_code == FT_ERR_HTTP_PROTOCOL_MISMATCH)
                break ;
        }
        should_retry = api_http_should_retry_plain(error_code);
        if (!should_retry)
            return (FT_FALSE);
        api_retry_circuit_record_failure(connection_handle, retry_policy);
        api_connection_pool_evict(connection_handle);
        attempt_index++;
        if (attempt_index >= max_attempts)
            break ;
        if (current_delay > 0)
        {
            int32_t sleep_delay;

            sleep_delay = api_retry_prepare_delay(current_delay, max_delay);
            if (sleep_delay > 0)
                time_sleep_ms(static_cast<uint32_t>(sleep_delay));
        }
        current_delay = api_retry_next_delay(current_delay, max_delay,
                multiplier);
        if (current_delay <= 0)
            current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (FT_FALSE);
}
#endif

#if NETWORKING_HAS_OPENSSL
char *api_http_execute_plain_http2(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port,
    const api_retry_policy *retry_policy, ft_bool &used_http2, int32_t &error_code)
{
    int32_t max_attempts;
    int32_t attempt_index;
    int32_t initial_delay;
    int32_t current_delay;
    int32_t max_delay;
    int32_t multiplier;
    ft_bool http2_used_local;
    ft_bool downgrade_due_to_connect_refused;

    if (headers && headers[0])
    {
        error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
        return (ft_nullptr);
    }
    max_attempts = api_retry_get_max_attempts(retry_policy);
    initial_delay = api_retry_get_initial_delay(retry_policy);
    max_delay = api_retry_get_max_delay(retry_policy);
    multiplier = api_retry_get_multiplier(retry_policy);
    current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    attempt_index = 0;
    http2_used_local = FT_FALSE;
    downgrade_due_to_connect_refused = FT_FALSE;
    while (attempt_index < max_attempts)
    {
        if (!api_retry_circuit_allow(connection_handle, retry_policy,
                error_code))
            return (ft_nullptr);
        ft_bool socket_ready;
        ft_bool should_retry;

        socket_ready = api_http_prepare_plain_socket(connection_handle, host,
                port, timeout, error_code);
        if (socket_ready)
        {
            http2_used_local = FT_FALSE;
            char *result_body;

            result_body = api_http_execute_plain_http2_once(connection_handle,
                    method, path, host_header, payload, headers, status,
                    timeout, host, port, http2_used_local, error_code);
            if (result_body)
            {
                used_http2 = http2_used_local;
                api_retry_circuit_record_success(connection_handle,
                    retry_policy);
                return (result_body);
            }
            if (error_code == FT_ERR_HTTP_PROTOCOL_MISMATCH)
                break ;
        }
        else
        {
            ft_bool connect_refused;

            connect_refused = FT_FALSE;
            if (error_code == FT_ERR_SOCKET_CONNECT_FAILED)
                connect_refused = FT_TRUE;
#ifdef _WIN32
            if (error_code == ((WSAECONNREFUSED)))
                connect_refused = FT_TRUE;
#else
            if (error_code == ((ECONNREFUSED)))
                connect_refused = FT_TRUE;
#endif
            if (connect_refused)
            {
                downgrade_due_to_connect_refused = FT_TRUE;
                api_retry_circuit_record_failure(connection_handle,
                    retry_policy);
                break ;
            }
        }
        should_retry = api_http_should_retry_plain(error_code);
        if (!should_retry)
            break ;
        if (error_code == FT_ERR_HTTP_PROTOCOL_MISMATCH)
            break ;
        api_retry_circuit_record_failure(connection_handle, retry_policy);
        if (api_http_http2_failure_requires_eviction(error_code))
            api_connection_pool_evict(connection_handle);
        attempt_index++;
        if (attempt_index >= max_attempts)
            break ;
        if (current_delay > 0)
        {
            int32_t sleep_delay;

            sleep_delay = api_retry_prepare_delay(current_delay, max_delay);
            if (sleep_delay > 0)
                time_sleep_ms(static_cast<uint32_t>(sleep_delay));
        }
        current_delay = api_retry_next_delay(current_delay, max_delay,
                multiplier);
        if (current_delay <= 0)
            current_delay = api_retry_prepare_delay(initial_delay,
                    max_delay);
    }
    if (downgrade_due_to_connect_refused)
    {
        char *fallback_body;

        api_connection_pool_evict(connection_handle);
        fallback_body = api_http_execute_plain(connection_handle, method, path,
                host_header, payload, headers, status, timeout, host, port,
                retry_policy, error_code);
        if (fallback_body)
        {
            used_http2 = FT_FALSE;
            return (fallback_body);
        }
        used_http2 = FT_FALSE;
        return (ft_nullptr);
    }
    used_http2 = FT_FALSE;
    if (error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (ft_nullptr);
}

#endif

char *api_http_execute_plain(api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port,
    const api_retry_policy *retry_policy, int32_t &error_code)
{
    int32_t max_attempts;
    int32_t attempt_index;
    int32_t initial_delay;
    int32_t current_delay;
    int32_t max_delay;
    int32_t multiplier;
    int32_t last_meaningful_error;
    ft_bool has_meaningful_error;

    max_attempts = api_retry_get_max_attempts(retry_policy);
    initial_delay = api_retry_get_initial_delay(retry_policy);
    max_delay = api_retry_get_max_delay(retry_policy);
    multiplier = api_retry_get_multiplier(retry_policy);
    current_delay = api_retry_prepare_delay(initial_delay, max_delay);
    attempt_index = 0;
    last_meaningful_error = FT_ERR_SUCCESS;
    has_meaningful_error = FT_FALSE;
    api_http_plain_debug_log("execute_plain: enter");
    while (attempt_index < max_attempts)
    {
        ft_bool socket_ready;
        ft_bool should_retry;

        api_http_plain_debug_log("execute_plain: before_prepare");
        socket_ready = api_http_prepare_plain_socket(connection_handle, host,
                port, timeout, error_code);
        api_http_plain_debug_log("execute_plain: after_prepare");
        if (socket_ready)
        {
            char *result_body;

            api_http_plain_debug_log("execute_plain: before_once");
            result_body = api_http_execute_plain_once(connection_handle, method,
                    path, host_header, payload, headers, status, timeout,
                    error_code, ft_nullptr, FT_FALSE);
            api_http_plain_debug_log("execute_plain: after_once");
            if (result_body)
            {
                api_retry_circuit_record_success(connection_handle,
                    retry_policy);
                api_http_plain_debug_log("execute_plain: success");
                return (result_body);
            }
        }
        should_retry = api_http_should_retry_plain(error_code);
        if (!should_retry)
        {
            api_http_plain_debug_log("execute_plain: no_retry");
            return (ft_nullptr);
        }
        api_retry_circuit_record_failure(connection_handle, retry_policy);
        if (error_code != FT_ERR_SOCKET_CONNECT_FAILED
            && error_code != FT_ERR_SUCCESS)
        {
            last_meaningful_error = error_code;
            has_meaningful_error = FT_TRUE;
        }
        api_connection_pool_evict(connection_handle);
        attempt_index++;
        if (error_code == FT_ERR_SOCKET_CONNECT_FAILED)
        {
            if (attempt_index >= max_attempts)
            {
                if (has_meaningful_error)
                {
                    error_code = last_meaningful_error;
                }
                break ;
            }
            time_sleep_ms(50);
            continue ;
        }
        if (attempt_index >= max_attempts)
            break ;
        if (current_delay > 0)
        {
            int32_t sleep_delay;

            sleep_delay = api_retry_prepare_delay(current_delay, max_delay);
            if (sleep_delay > 0)
                time_sleep_ms(static_cast<uint32_t>(sleep_delay));
        }
        current_delay = api_retry_next_delay(current_delay, max_delay,
                multiplier);
        if (current_delay <= 0)
            current_delay = api_retry_prepare_delay(initial_delay,
                    max_delay);
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    return (ft_nullptr);
}

#if NETWORKING_HAS_OPENSSL
static char *api_http_execute_plain_http2_once(
    api_connection_pool_handle &connection_handle,
    const char *method, const char *path, const char *host_header,
    json_group *payload, const char *headers, int32_t *status, int32_t timeout,
    const char *host, uint16_t port, ft_bool &used_http2, int32_t &error_code)
{
    ft_socket &socket_wrapper = connection_handle.socket;
    const char *client_preface;
    ssize_t sent_bytes;
    http2_frame settings_frame;
    ft_string encoded_frame;
    ft_string handshake_buffer;
    ft_bool received_settings_frame;
    ft_bool received_settings_ack;
    ft_bool ack_sent;
    ft_bool handshake_confirmed;
    ft_bool protocol_downgrade;

    used_http2 = FT_FALSE;
    error_code = FT_ERR_SUCCESS;
    if (!method || !path)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    (void)host_header;
    (void)payload;
    (void)headers;
    (void)status;
    (void)port;
    if (!api_http_apply_timeouts(socket_wrapper, timeout))
    {
#ifdef _WIN32
        int32_t last_error;

        last_error = WSAGetLastError();
        if (last_error != 0)
            error_code = (last_error);
        else
            error_code = FT_ERR_CONFIGURATION;
#else
        if (errno != 0)
            error_code = (errno);
        else
            error_code = FT_ERR_CONFIGURATION;
#endif
        return (ft_nullptr);
    }
    if (settings_frame.initialize() != FT_ERR_SUCCESS)
    {
        error_code = settings_frame.get_error();
        return (ft_nullptr);
    }
    if (encoded_frame.initialize() != FT_ERR_SUCCESS)
    {
        error_code = encoded_frame.get_error();
        return (ft_nullptr);
    }
    if (handshake_buffer.initialize() != FT_ERR_SUCCESS)
    {
        error_code = handshake_buffer.get_error();
        return (ft_nullptr);
    }
    client_preface = "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n";
    sent_bytes = socket_wrapper.send_all(client_preface, ft_strlen(client_preface));
    if (sent_bytes < 0)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SOCKET_SEND_FAILED;
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    if (!settings_frame.set_type(0x4))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    if (!settings_frame.set_flags(0x0))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    if (!settings_frame.set_stream_identifier(0))
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SUCCESS;
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    settings_frame.clear_payload();
    if (!http2_encode_frame(settings_frame, encoded_frame, error_code))
    {
        api_connection_pool_disable_store(connection_handle);
        api_connection_pool_evict(connection_handle);
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    sent_bytes = socket_wrapper.send_all(encoded_frame.c_str(), encoded_frame.size());
    if (sent_bytes < 0)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = FT_ERR_SOCKET_SEND_FAILED;
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    handshake_buffer.clear();
    if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
    {
        api_connection_pool_disable_store(connection_handle);
        error_code = handshake_buffer.get_error();
        api_connection_pool_evict(connection_handle);
        return (ft_nullptr);
    }
    received_settings_frame = FT_FALSE;
    received_settings_ack = FT_FALSE;
    ack_sent = FT_FALSE;
    handshake_confirmed = FT_FALSE;
    protocol_downgrade = FT_FALSE;
    while (!received_settings_frame || !received_settings_ack)
    {
        char receive_buffer[1024];
        ssize_t received_bytes;

        received_bytes = socket_wrapper.receive_data(receive_buffer, sizeof(receive_buffer));
        if (received_bytes <= 0)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
            api_connection_pool_evict(connection_handle);
            return (ft_nullptr);
        }
        handshake_buffer.append(receive_buffer, static_cast<ft_size_t>(received_bytes));
        if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
        {
            api_connection_pool_disable_store(connection_handle);
            error_code = handshake_buffer.get_error();
            api_connection_pool_evict(connection_handle);
            return (ft_nullptr);
        }
        if (handshake_buffer.size() >= 5)
        {
            if (ft_strncmp(handshake_buffer.c_str(), "HTTP/", 5) == 0)
            {
                api_connection_pool_disable_store(connection_handle);
                protocol_downgrade = FT_TRUE;
                break ;
            }
        }
        ft_size_t parse_offset;

        parse_offset = 0;
        while (FT_TRUE)
        {
            http2_frame incoming_frame;
            int32_t frame_error;
            ft_size_t previous_offset;

            if (incoming_frame.initialize() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_NO_MEMORY;
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
            previous_offset = parse_offset;
            frame_error = FT_ERR_SUCCESS;
            if (!http2_decode_frame(reinterpret_cast<const unsigned char*>(handshake_buffer.c_str()),
                    handshake_buffer.size(), parse_offset, incoming_frame, frame_error))
            {
                if (frame_error == FT_ERR_OUT_OF_RANGE)
                {
                    parse_offset = previous_offset;
                    break ;
                }
                api_connection_pool_disable_store(connection_handle);
                error_code = frame_error;
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
            uint8_t incoming_type;
            uint8_t incoming_flags;

            if (!incoming_frame.get_type(incoming_type))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
            if (!incoming_frame.get_flags(incoming_flags))
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SUCCESS;
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
            if (incoming_type == 0x4)
            {
                if ((incoming_flags & 0x1) != 0)
                    received_settings_ack = FT_TRUE;
                else
                {
                    received_settings_frame = FT_TRUE;
                    handshake_confirmed = FT_TRUE;
                    if (!ack_sent)
                    {
                        http2_frame ack_frame;
                        ft_string ack_encoded;
                        ssize_t ack_bytes;

                        if (!ack_frame.set_type(0x4))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (ft_nullptr);
                        }
                        if (!ack_frame.set_flags(0x1))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (ft_nullptr);
                        }
                        if (!ack_frame.set_stream_identifier(0))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SUCCESS;
                            api_connection_pool_evict(connection_handle);
                            return (ft_nullptr);
                        }
                        ack_frame.clear_payload();
                        if (!http2_encode_frame(ack_frame, ack_encoded, error_code))
                        {
                            api_connection_pool_disable_store(connection_handle);
                            api_connection_pool_evict(connection_handle);
                            if (error_code == FT_ERR_SUCCESS)
                                error_code = FT_ERR_IO;
                            return (ft_nullptr);
                        }
                        ack_bytes = socket_wrapper.send_all(ack_encoded.c_str(), ack_encoded.size());
                        if (ack_bytes < 0)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_SOCKET_SEND_FAILED;
                            api_connection_pool_evict(connection_handle);
                            return (ft_nullptr);
                        }
                        ack_sent = FT_TRUE;
                    }
                }
            }
        }
        if (protocol_downgrade)
            break ;
        if (parse_offset > 0)
            handshake_buffer.erase(0, parse_offset);
    }
    if (protocol_downgrade)
    {
        char *fallback_body;
        ft_bool downgrade_headers_ready;
        ft_bool downgrade_complete;
        ft_size_t downgrade_header_length;
        ft_bool downgrade_chunked_encoding;
        ft_bool downgrade_has_length;
        int64_t downgrade_content_length;
        ft_bool downgrade_connection_close;
        ft_bool downgrade_fully_buffered;

        downgrade_headers_ready = FT_FALSE;
        downgrade_complete = FT_FALSE;
        downgrade_header_length = 0;
        downgrade_chunked_encoding = FT_FALSE;
        downgrade_has_length = FT_FALSE;
        downgrade_content_length = 0;
        downgrade_connection_close = FT_FALSE;
        downgrade_fully_buffered = FT_FALSE;
        while (!downgrade_complete)
        {
            const char *headers_start;
            const char *headers_end;
            ft_bool body_ready;

            headers_start = handshake_buffer.c_str();
            headers_end = ft_strstr(handshake_buffer.c_str(), "\r\n\r\n");
            if (headers_end)
            {
                headers_end += 2;
                downgrade_header_length = static_cast<ft_size_t>(headers_end - headers_start) + 2;
                if (!downgrade_headers_ready)
                {
                    downgrade_headers_ready = FT_TRUE;
                    api_http_parse_headers(headers_start, headers_end,
                        downgrade_connection_close, downgrade_chunked_encoding,
                        downgrade_has_length, downgrade_content_length);
                }
            }
            body_ready = FT_FALSE;
            if (downgrade_headers_ready)
            {
                if (downgrade_chunked_encoding)
                {
                    ft_size_t body_size;
                    ft_size_t consumed_length;

                    body_size = handshake_buffer.size() - downgrade_header_length;
                    if (body_size > 0)
                    {
                        consumed_length = 0;
                        if (api_http_chunked_body_complete(
                                handshake_buffer.c_str() + downgrade_header_length,
                                body_size, consumed_length))
                        {
                            if (consumed_length <= body_size)
                                body_ready = FT_TRUE;
                        }
                    }
                }
                else if (downgrade_has_length)
                {
                    if (downgrade_content_length <= 0)
                        body_ready = FT_TRUE;
                    else if (handshake_buffer.size()
                        >= downgrade_header_length
                            + static_cast<ft_size_t>(downgrade_content_length))
                        body_ready = FT_TRUE;
                }
                else
                    body_ready = FT_TRUE;
            }
            if (downgrade_headers_ready && body_ready)
            {
                downgrade_connection_close = FT_TRUE;
                downgrade_complete = FT_TRUE;
                downgrade_fully_buffered = FT_TRUE;
                break ;
            }
            char receive_buffer[1024];
            ssize_t received_bytes;

            received_bytes = socket_wrapper.receive_data(receive_buffer, sizeof(receive_buffer));
            if (received_bytes < 0)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = FT_ERR_SOCKET_RECEIVE_FAILED;
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
            if (received_bytes == 0)
            {
                if (!downgrade_headers_ready)
                {
                    api_connection_pool_disable_store(connection_handle);
                    error_code = FT_ERR_IO;
                    api_connection_pool_evict(connection_handle);
                    return (ft_nullptr);
                }
                if (!body_ready)
                {
                    if (downgrade_chunked_encoding)
                    {
                        api_connection_pool_disable_store(connection_handle);
                        error_code = FT_ERR_IO;
                        api_connection_pool_evict(connection_handle);
                        return (ft_nullptr);
                    }
                    if (downgrade_has_length)
                    {
                        ft_size_t expected_length;
                        ft_size_t body_size;

                        expected_length = 0;
                        if (downgrade_content_length > 0)
                            expected_length = static_cast<ft_size_t>(downgrade_content_length);
                        body_size = 0;
                        if (handshake_buffer.size() > downgrade_header_length)
                            body_size = handshake_buffer.size() - downgrade_header_length;
                        if (body_size < expected_length)
                        {
                            api_connection_pool_disable_store(connection_handle);
                            error_code = FT_ERR_IO;
                            api_connection_pool_evict(connection_handle);
                            return (ft_nullptr);
                        }
                        body_ready = FT_TRUE;
                    }
                }
                downgrade_connection_close = FT_TRUE;
                downgrade_complete = FT_TRUE;
                if (body_ready)
                    downgrade_fully_buffered = FT_TRUE;
                break ;
            }
            handshake_buffer.append(receive_buffer, static_cast<ft_size_t>(received_bytes));
            if (handshake_buffer.get_error() != FT_ERR_SUCCESS)
            {
                api_connection_pool_disable_store(connection_handle);
                error_code = handshake_buffer.get_error();
                api_connection_pool_evict(connection_handle);
                return (ft_nullptr);
            }
        }

        if (downgrade_complete)
        {
            int32_t fallback_error_code;

            if (downgrade_fully_buffered)
            {
                fallback_body = api_http_finalize_downgrade_response(connection_handle,
                        handshake_buffer, downgrade_header_length,
                        downgrade_chunked_encoding, downgrade_has_length,
                        downgrade_content_length, downgrade_connection_close,
                        status, error_code);
                if (fallback_body)
                {
                    used_http2 = FT_FALSE;
                    return (fallback_body);
                }
                if (error_code == FT_ERR_SUCCESS)
                    error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
            }
            else
            {
                fallback_body = api_http_execute_plain_once(connection_handle, method,
                        path, host_header, payload, headers, status, timeout,
                        error_code, &handshake_buffer, FT_TRUE);
                if (fallback_body)
                {
                    used_http2 = FT_FALSE;
                    return (fallback_body);
                }
                api_connection_pool_evict(connection_handle);
                fallback_error_code = error_code;
                fallback_body = api_http_finalize_downgrade_response(connection_handle,
                        handshake_buffer, downgrade_header_length,
                        downgrade_chunked_encoding, downgrade_has_length,
                        downgrade_content_length, downgrade_connection_close,
                        status, error_code);
                if (fallback_body)
                {
                    used_http2 = FT_FALSE;
                    return (fallback_body);
                }
                if (error_code == FT_ERR_SUCCESS)
                    error_code = fallback_error_code;
                if (error_code == FT_ERR_SUCCESS)
                    error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
            }
        }
        api_connection_pool_evict(connection_handle);
        if (!api_http_prepare_plain_socket(connection_handle, host, port, timeout,
                error_code))
        {
            if (error_code == FT_ERR_SUCCESS)
                error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
            api_connection_pool_disable_store(connection_handle);
            return (ft_nullptr);
        }
        fallback_body = api_http_execute_plain_once(connection_handle, method,
                path, host_header, payload, headers, status, timeout,
                error_code, ft_nullptr, FT_FALSE);
        if (fallback_body)
        {
            used_http2 = FT_FALSE;
            return (fallback_body);
        }
        api_connection_pool_evict(connection_handle);
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_HTTP_PROTOCOL_MISMATCH;
        api_connection_pool_disable_store(connection_handle);
        return (ft_nullptr);
    }
    api_connection_pool_disable_store(connection_handle);
    api_connection_pool_evict(connection_handle);
    if (handshake_confirmed)
        used_http2 = FT_TRUE;
    error_code = FT_ERR_UNSUPPORTED_TYPE;
    return (ft_nullptr);
}
#endif
