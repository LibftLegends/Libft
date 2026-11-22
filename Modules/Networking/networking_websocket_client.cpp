#include "websocket_client.hpp"
#include "networking_websocket_frame_internal.hpp"
#include "networking_websocket_handshake_internal.hpp"
#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../RNG/rng.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include <cstring>
#include <cstdio>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <unistd.h>
#endif

static int32_t websocket_recv_exact(int32_t socket_fd, void *buffer, ft_size_t length)
{
    ft_size_t total_received;
    ssize_t bytes_received;

    total_received = 0;
    while (total_received < length)
    {
        bytes_received = nw_recv(socket_fd,
                static_cast<unsigned char *>(buffer) + total_received,
                length - total_received, 0);
        if (bytes_received <= 0)
            return (FT_ERR_INVALID_OPERATION);
        total_received += static_cast<ft_size_t>(bytes_received);
    }
    return (FT_ERR_SUCCESS);
}

ft_websocket_client::ft_websocket_client() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _socket(), _mutex(ft_nullptr)
{
    return ;
}

ft_websocket_client::~ft_websocket_client() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_websocket_client::move(ft_websocket_client &other) noexcept
{
    return (this->initialize(static_cast<ft_websocket_client &&>(other)));
}

int32_t ft_websocket_client::initialize(const ft_websocket_client &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_websocket_client::initialize(const ft_websocket_client &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    return (this->initialize());
}

int32_t ft_websocket_client::initialize(ft_websocket_client &&other) noexcept
{
    int32_t initialize_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    initialize_error = this->initialize(other);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_client::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_websocket_client::initialize", "initialize called on initialised instance");
    if (this->_socket.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_client::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    (void)this->close_locked();
    (void)this->_socket.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_client::enable_thread_safety() noexcept
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

int32_t ft_websocket_client::disable_thread_safety() noexcept
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

ft_bool ft_websocket_client::is_thread_safe() const noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t ft_websocket_client::close_locked()
{
    if (!this->_socket.close())
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

void ft_websocket_client::close()
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_client::close");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    (void)this->close_locked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_websocket_client::perform_handshake_locked(const char *host, const char *path)
{
    const int32_t socket_descriptor = this->_socket.get();
    if (socket_descriptor < 0)
        return (FT_ERR_INVALID_OPERATION);
    unsigned char nonce[16];
    ft_size_t index = 0U;
    while (index < sizeof(nonce))
    {
        const uint32_t random_value = ft_random_uint32();
        nonce[index++] = static_cast<unsigned char>(random_value);
        nonce[index++] = static_cast<unsigned char>(random_value >> 8U);
        nonce[index++] = static_cast<unsigned char>(random_value >> 16U);
        nonce[index++] = static_cast<unsigned char>(random_value >> 24U);
    }
    char key[25];
    if (!compression_base64_encode_into(nonce, sizeof(nonce), key, sizeof(key)))
        return (FT_ERR_INVALID_OPERATION);
    ft_string request;
    ft_string response;
    if (request.initialize() != FT_ERR_SUCCESS || response.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (request.append("GET ") != FT_ERR_SUCCESS || request.append(path) != FT_ERR_SUCCESS
        || request.append(" HTTP/1.1\r\nHost: ") != FT_ERR_SUCCESS
        || request.append(host) != FT_ERR_SUCCESS
        || request.append("\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
            "Sec-WebSocket-Version: 13\r\nSec-WebSocket-Key: ") != FT_ERR_SUCCESS
        || request.append(key, sizeof(key) - 1U) != FT_ERR_SUCCESS
        || request.append("\r\n\r\n") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (websocket_send_bytes(socket_descriptor, request.c_str(), request.size()) != FT_ERR_SUCCESS
        || websocket_read_handshake(socket_descriptor, response, FT_SYSTEM_SIZE_MAX) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    static constexpr char ACCEPT_HEADER[] = "Sec-WebSocket-Accept: ";
    const char *accept_line = ft_strstr(response.c_str(), ACCEPT_HEADER);
    if (accept_line == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    accept_line += sizeof(ACCEPT_HEADER) - 1U;
    const char *line_end = ft_strstr(accept_line, "\r\n");
    char expected[WEBSOCKET_ACCEPT_KEY_LENGTH + 1U];
    if (line_end == ft_nullptr || static_cast<ft_size_t>(line_end - accept_line) != WEBSOCKET_ACCEPT_KEY_LENGTH
        || !websocket_compute_accept_key(key, sizeof(key) - 1U, expected)
        || ft_memcmp(accept_line, expected, WEBSOCKET_ACCEPT_KEY_LENGTH) != 0)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_client::connect(const char *host, uint16_t port, const char *path)
{
    struct addrinfo address_hints;
    struct addrinfo *address_info;
    char port_string[8];
    int32_t result;
    int32_t new_socket_fd;
    int32_t lock_error;

    if (host == ft_nullptr || path == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_client::connect");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    ft_memset(&address_hints, 0, sizeof(address_hints));
    address_hints.ai_family = AF_UNSPEC;
    address_hints.ai_socktype = SOCK_STREAM;
    std::snprintf(port_string, sizeof(port_string), "%u", port);
    if (getaddrinfo(host, port_string, &address_hints, &address_info) != 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    new_socket_fd = nw_socket(address_info->ai_family, address_info->ai_socktype, address_info->ai_protocol);
    if (new_socket_fd < 0)
    {
        freeaddrinfo(address_info);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (!this->_socket.reset(new_socket_fd))
    {
        (void)nw_close(new_socket_fd);
        freeaddrinfo(address_info);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    result = nw_connect(this->_socket.get(), address_info->ai_addr,
            FT_NETWORKING_SOCKLEN_CAST(address_info->ai_addrlen));
    freeaddrinfo(address_info);
    if (result < 0)
    {
        (void)this->close_locked();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->perform_handshake_locked(host, path) != 0)
    {
        (void)this->close_locked();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

static int32_t websocket_send_masked_frame(int32_t socket_descriptor, unsigned char opcode,
    const unsigned char *payload, ft_size_t length)
{
    if (socket_descriptor < 0 || length > 65535U)
        return (FT_ERR_INVALID_OPERATION);
    const uint32_t mask_value = ft_random_uint32();
    const unsigned char mask_key[] = {
        static_cast<unsigned char>(mask_value),
        static_cast<unsigned char>(mask_value >> 8U),
        static_cast<unsigned char>(mask_value >> 16U),
        static_cast<unsigned char>(mask_value >> 24U)
    };
    return (websocket_send_frame(socket_descriptor, opcode, payload, length, mask_key));
}

int32_t ft_websocket_client::send_pong_locked(const unsigned char *payload, ft_size_t length)
{
    return (websocket_send_masked_frame(this->_socket.get(), 0x8AU, payload, length));
}

int32_t ft_websocket_client::send_text_locked(const ft_string &message)
{
    const int32_t socket_descriptor = this->_socket.get();
    if (socket_descriptor < 0)
        return (FT_ERR_INVALID_OPERATION);
    return (websocket_send_masked_frame(socket_descriptor, 0x81U,
        reinterpret_cast<const unsigned char *>(message.c_str()), message.size()));
}

int32_t ft_websocket_client::send_text(const ft_string &message)
{
    int32_t lock_error;
    int32_t send_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_client::send_text");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    send_result = this->send_text_locked(message);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (send_result);
}

int32_t ft_websocket_client::receive_text_locked(ft_string &message)
{
    unsigned char header[2];
    unsigned char mask_key[4];
    ft_bool mask_bit_set;
    ft_size_t payload_length;
    ssize_t bytes_received;
    unsigned char *payload;
    ft_size_t index_value;
    unsigned char opcode;
    int32_t socket_fd;

    message.clear();
    socket_fd = this->_socket.get();
    if (socket_fd < 0)
        return (FT_ERR_INVALID_OPERATION);
    while (FT_TRUE)
    {
        if (websocket_recv_exact(socket_fd, header, 2) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        opcode = header[0] & 0x0F;
        mask_bit_set = (header[1] & 0x80) != 0;
        payload_length = static_cast<ft_size_t>(header[1] & 0x7F);
        if (payload_length == 126)
        {
            unsigned char extended[2];

            if (websocket_recv_exact(socket_fd, extended, 2) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            payload_length = static_cast<ft_size_t>((extended[0] << 8) | extended[1]);
        }
        else if (payload_length == 127)
            return (FT_ERR_INVALID_OPERATION);
        if (mask_bit_set)
        {
            if (websocket_recv_exact(socket_fd, mask_key, 4) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
        }
        payload = ft_nullptr;
        if (payload_length != 0U)
        {
            payload = static_cast<unsigned char *>(cma_malloc(payload_length));
            if (payload == ft_nullptr)
                return (FT_ERR_INVALID_OPERATION);
        }
        index_value = 0;
        while (index_value < payload_length)
        {
            bytes_received = nw_recv(socket_fd, payload + index_value, payload_length - index_value, 0);
            if (bytes_received <= 0)
            {
                cma_free(payload);
                return (FT_ERR_INVALID_OPERATION);
            }
            index_value += static_cast<ft_size_t>(bytes_received);
        }
        if (mask_bit_set)
        {
            websocket_copy_masked_payload(payload, payload, payload_length, mask_key);
        }
        if (opcode == 0x9)
        {
            if (this->send_pong_locked(payload, payload_length) != 0)
            {
                cma_free(payload);
                return (FT_ERR_INVALID_OPERATION);
            }
            cma_free(payload);
            continue ;
        }
        if (opcode == 0xA)
        {
            cma_free(payload);
            continue ;
        }
        if (opcode == 0x1)
        {
            if (payload_length == 0U)
                return (FT_ERR_SUCCESS);
            const int32_t append_result = message.append(reinterpret_cast<const char *>(payload),
                payload_length);
            cma_free(payload);
            if (append_result != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_OPERATION);
            return (FT_ERR_SUCCESS);
        }
        cma_free(payload);
        return (FT_ERR_INVALID_OPERATION);
    }
}

int32_t ft_websocket_client::receive_text(ft_string &message)
{
    int32_t lock_error;
    int32_t receive_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_client::receive_text");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    receive_result = this->receive_text_locked(message);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (receive_result);
}
