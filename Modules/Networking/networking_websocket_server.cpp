#include "websocket_server.hpp"
#include "networking_websocket_frame_internal.hpp"
#include "networking_websocket_handshake_internal.hpp"
#include "networking.hpp"
#include "socket_class.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compression/compression.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstring>
#include <cerrno>
#include <climits>
#include <new>
#include <zlib.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"

static char websocket_ascii_lower(char character)
{
    if (character >= 'A' && character <= 'Z')
        return (static_cast<char>(character + 32));
    return (character);
}

static int32_t websocket_case_insensitive_compare(const char *left, const char *right, ft_size_t length)
{
    ft_size_t index_value;

    index_value = 0;
    while (index_value < length)
    {
        unsigned char left_value;
        unsigned char right_value;

        left_value = static_cast<unsigned char>(websocket_ascii_lower(left[index_value]));
        right_value = static_cast<unsigned char>(websocket_ascii_lower(right[index_value]));
        if (left_value != right_value)
            return (static_cast<int32_t>(left_value) - static_cast<int32_t>(right_value));
        index_value++;
    }
    return (FT_ERR_SUCCESS);
}

static const char *websocket_find_header_value(const ft_string &request,
    const char *header_name, ft_size_t &value_length)
{
    const char  *cursor;
    const char  *line_end;
    ft_size_t header_length;
    ft_size_t line_length;
    ft_size_t offset;

    cursor = request.c_str();
    header_length = ft_strlen(header_name);
    while (*cursor != '\0')
    {
        line_end = ft_strstr(cursor, "\r\n");
        if (line_end != ft_nullptr)
            line_length = static_cast<ft_size_t>(line_end - cursor);
        else
            line_length = ft_strlen(cursor);
        if (line_length >= header_length)
        {
            if (websocket_case_insensitive_compare(cursor, header_name, header_length) == 0)
            {
                offset = header_length;
                while (offset < line_length && (cursor[offset] == ' ' || cursor[offset] == '\t'))
                    offset++;
                value_length = line_length - offset;
                return (cursor + offset);
            }
        }
        if (line_end == ft_nullptr)
            break ;
        cursor = line_end + 2;
    }
    value_length = 0;
    return (ft_nullptr);
}

static ft_bool websocket_header_contains_token(const ft_string &request,
    const char *header_name, const char *token)
{
    ft_size_t value_length;
    const char  *value_start;
    ft_size_t token_length;
    ft_size_t index_value;

    value_start = websocket_find_header_value(request, header_name, value_length);
    if (value_start == ft_nullptr || value_length == 0)
        return (FT_FALSE);
    token_length = ft_strlen(token);
    if (token_length == 0)
        return (FT_FALSE);
    if (token_length > value_length)
        return (FT_FALSE);
    index_value = 0;
    while (index_value + token_length <= value_length)
    {
        if (websocket_case_insensitive_compare(value_start + index_value, token, token_length) == 0)
        {
            if (index_value > 0)
            {
                char previous_character;

                previous_character = value_start[index_value - 1];
                if (previous_character != ' ' && previous_character != '\t'
                    && previous_character != ';' && previous_character != ',')
                {
                    index_value++;
                    continue ;
                }
            }
            if (index_value + token_length < value_length)
            {
                char next_character;

                next_character = value_start[index_value + token_length];
                if (next_character != ' ' && next_character != '\t'
                    && next_character != ';' && next_character != ',' && next_character != '\0')
                {
                    index_value++;
                    continue ;
                }
            }
            return (FT_TRUE);
        }
        index_value++;
    }
    return (FT_FALSE);
}

static int32_t websocket_recv_exact(int32_t client_fd, void *buffer, ft_size_t length)
{
    ft_size_t total_received;
    ssize_t bytes_received;

    total_received = 0;
    while (total_received < length)
    {
        bytes_received = nw_recv(client_fd,
                static_cast<unsigned char *>(buffer) + total_received,
                length - total_received, 0);
        if (bytes_received <= 0)
            return (FT_ERR_INVALID_OPERATION);
        total_received += static_cast<ft_size_t>(bytes_received);
    }
    return (FT_ERR_SUCCESS);
}

static constexpr ft_size_t WEBSOCKET_DEFLATE_TAIL_SIZE = 4U;
static constexpr uInt WEBSOCKET_ZLIB_CHUNK_SIZE = 4096U;

static int32_t websocket_permessage_deflate_inflate(const unsigned char *payload,
    ft_size_t payload_length, ft_string &message)
{
    if ((payload == ft_nullptr && payload_length != 0U) || payload_length > UINT_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string output;
    const int32_t initialise_error = output.initialize();
    if (initialise_error != FT_ERR_SUCCESS)
        return (initialise_error);
    z_stream stream{};
    if (inflateInit2(&stream, -15) != Z_OK)
        return (FT_ERR_INVALID_ARGUMENT);

    unsigned char tail[WEBSOCKET_DEFLATE_TAIL_SIZE] = {0x00U, 0x00U, 0xFFU, 0xFFU};
    stream.next_in = const_cast<unsigned char *>(payload);
    stream.avail_in = static_cast<uInt>(payload_length);
    ft_bool tail_pending = FT_TRUE;
    int32_t result = FT_ERR_SUCCESS;
    char output_chunk[WEBSOCKET_ZLIB_CHUNK_SIZE];
    while (FT_TRUE)
    {
        if (stream.avail_in == 0U && tail_pending)
        {
            stream.next_in = tail;
            stream.avail_in = sizeof(tail);
            tail_pending = FT_FALSE;
        }
        stream.next_out = reinterpret_cast<Bytef *>(output_chunk);
        stream.avail_out = sizeof(output_chunk);
        const int32_t zlib_result = inflate(&stream, Z_SYNC_FLUSH);
        if ((zlib_result != Z_OK && zlib_result != Z_STREAM_END && zlib_result != Z_BUF_ERROR)
            || (zlib_result == Z_BUF_ERROR && stream.avail_in != 0U))
        {
            result = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        const ft_size_t produced_bytes = sizeof(output_chunk) - stream.avail_out;
        if (produced_bytes != 0U)
        {
            result = output.append(output_chunk, produced_bytes);
            if (result != FT_ERR_SUCCESS)
                break ;
        }
        if (zlib_result == Z_STREAM_END || (zlib_result == Z_BUF_ERROR && !tail_pending))
            break ;
    }
    inflateEnd(&stream);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (message.assign(output.c_str(), output.size()));
}

static int32_t websocket_permessage_deflate_deflate(const ft_string &message,
    ft_string &compressed)
{
    const ft_size_t input_size = message.size();
    if (input_size > UINT_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    z_stream stream{};
    if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY) != Z_OK)
        return (FT_ERR_INVALID_ARGUMENT);
    stream.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(message.c_str()));
    stream.avail_in = static_cast<uInt>(input_size);
    char output_chunk[WEBSOCKET_ZLIB_CHUNK_SIZE];
    int32_t result = FT_ERR_SUCCESS;
    while (FT_TRUE)
    {
        stream.next_out = reinterpret_cast<Bytef *>(output_chunk);
        stream.avail_out = sizeof(output_chunk);
        const int32_t zlib_result = deflate(&stream, Z_SYNC_FLUSH);
        const ft_bool input_flushed = (stream.avail_in == 0U && stream.avail_out != 0U);
        if (zlib_result != Z_OK && zlib_result != Z_STREAM_END
            && !(zlib_result == Z_BUF_ERROR && input_flushed))
        {
            result = FT_ERR_INVALID_ARGUMENT;
            break ;
        }
        const ft_size_t produced_bytes = sizeof(output_chunk) - stream.avail_out;
        if (produced_bytes != 0U)
        {
            result = compressed.append(output_chunk, produced_bytes);
            if (result != FT_ERR_SUCCESS)
                break ;
        }
        if (zlib_result == Z_STREAM_END || input_flushed)
            break ;
    }
    deflateEnd(&stream);
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (compressed.size() < WEBSOCKET_DEFLATE_TAIL_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

ft_websocket_server::ft_websocket_server() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _server_socket(ft_nullptr), _mutex(ft_nullptr)
{
    return ;
}

ft_websocket_server::~ft_websocket_server() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t ft_websocket_server::move(ft_websocket_server &other) noexcept
{
    return (this->initialize(static_cast<ft_websocket_server &&>(other)));
}

int32_t ft_websocket_server::initialize(const ft_websocket_server &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_websocket_server::initialize(const ft_websocket_server &)",
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

int32_t ft_websocket_server::initialize(ft_websocket_server &&other) noexcept
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

int32_t ft_websocket_server::initialize() noexcept
{
    int32_t connection_states_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_websocket_server::initialize", "initialize called on initialised instance");
    this->_mutex = ft_nullptr;
    this->_server_socket = ft_nullptr;
    connection_states_error = this->_connection_states.initialize();
    if (connection_states_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connection_states_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_server::destroy() noexcept
{
    int32_t disable_error;
    int32_t states_destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    if (this->_server_socket)
    {
        this->_server_socket->close_socket();
        delete this->_server_socket;
        this->_server_socket = ft_nullptr;
    }
    states_destroy_error = this->_connection_states.destroy();
    if (states_destroy_error != FT_ERR_SUCCESS)
        return (states_destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_server::enable_thread_safety() noexcept
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

int32_t ft_websocket_server::disable_thread_safety() noexcept
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

ft_bool ft_websocket_server::is_thread_safe() const noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

void ft_websocket_server::store_connection_state_locked(int32_t client_fd,
    ft_bool permessage_deflate_enabled)
{
    s_connection_state state;

    state._permessage_deflate_enabled = permessage_deflate_enabled;
    this->_connection_states.insert(client_fd, state);
    return ;
}

void ft_websocket_server::remove_connection_state_locked(int32_t client_fd)
{
    this->_connection_states.remove(client_fd);
    return ;
}

ft_bool ft_websocket_server::connection_supports_permessage_deflate_locked(int32_t client_fd) const
{
    const Pair<int32_t, s_connection_state> *entry_pointer;

    entry_pointer = this->_connection_states.find(client_fd);
    if (entry_pointer == this->_connection_states.end())
        return (FT_FALSE);
    return (entry_pointer->value._permessage_deflate_enabled);
}

int32_t ft_websocket_server::start(const char *ip_address, uint16_t port, int32_t address_family, ft_bool non_blocking)
{
    SocketConfig configuration;
    int32_t lock_error;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_server::start");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    initialize_error = configuration.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    configuration._type = SocketType::SERVER;
    ft_strncpy(configuration._ip, ip_address, sizeof(configuration._ip) - 1);
    configuration._ip[sizeof(configuration._ip) - 1] = '\0';
    configuration._port = port;
    configuration._address_family = address_family;
    configuration._non_blocking = non_blocking;
    configuration._recv_timeout = 5000;
    configuration._send_timeout = 5000;
    if (this->_server_socket)
    {
        this->_server_socket->close_socket();
        delete this->_server_socket;
        this->_server_socket = ft_nullptr;
    }
    this->_server_socket = new (std::nothrow) ft_socket();
    if (!this->_server_socket)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->_server_socket->initialize(configuration) != FT_ERR_SUCCESS)
    {
        delete this->_server_socket;
        this->_server_socket = ft_nullptr;
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->_connection_states.clear();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_server::perform_handshake_locked(int32_t client_fd)
{
    constexpr ft_size_t MAX_HANDSHAKE_SIZE = 8192U;
    static constexpr char KEY_HEADER[] = "Sec-WebSocket-Key: ";
    ft_string request;
    ft_string response;
    if (request.initialize() != FT_ERR_SUCCESS || response.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    this->remove_connection_state_locked(client_fd);
    if (websocket_read_handshake(client_fd, request, MAX_HANDSHAKE_SIZE) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    const char *key_line = ft_strstr(request.c_str(), KEY_HEADER);
    if (key_line == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    key_line += sizeof(KEY_HEADER) - 1U;
    const char *line_end = ft_strstr(key_line, "\r\n");
    if (line_end == ft_nullptr)
        return (FT_ERR_INVALID_OPERATION);
    char accept[WEBSOCKET_ACCEPT_KEY_LENGTH + 1U];
    if (!websocket_compute_accept_key(key_line, static_cast<ft_size_t>(line_end - key_line), accept))
        return (FT_ERR_INVALID_OPERATION);
    const ft_bool permessage_deflate_enabled = websocket_header_contains_token(request,
        "Sec-WebSocket-Extensions:", "permessage-deflate");
    if (response.append("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n"
            "Connection: Upgrade\r\nSec-WebSocket-Accept: ") != FT_ERR_SUCCESS
        || response.append(accept, WEBSOCKET_ACCEPT_KEY_LENGTH) != FT_ERR_SUCCESS
        || response.append("\r\n") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (permessage_deflate_enabled
        && response.append("Sec-WebSocket-Extensions: permessage-deflate; server_no_context_takeover; "
            "client_no_context_takeover\r\n") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (response.append("\r\n") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (websocket_send_bytes(client_fd, response.c_str(), response.size()) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    this->store_connection_state_locked(client_fd, permessage_deflate_enabled);
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_server::send_pong_locked(int32_t client_fd, const unsigned char *payload, ft_size_t length)
{
    return (websocket_send_frame(client_fd, 0x8AU, payload, length, ft_nullptr));
}

int32_t ft_websocket_server::receive_frame_locked(int32_t client_fd, ft_string &message)
{
    unsigned char header[2];
    unsigned char mask_key[4];
    ft_size_t payload_length;
    ssize_t bytes_received;
    unsigned char *payload;
    ft_size_t index_value;
    unsigned char opcode;
    unsigned char rsv_bits;
    ft_bool permessage_deflate_enabled;
    ft_bool is_final;
    ft_bool masked;
    ft_bool is_control_frame;
    int32_t inflate_error;

    message.clear();
    permessage_deflate_enabled = this->connection_supports_permessage_deflate_locked(client_fd);
    while (FT_TRUE)
    {
        if (websocket_recv_exact(client_fd, header, 2) != FT_ERR_SUCCESS)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        opcode = header[0] & 0x0F;
        rsv_bits = header[0] & 0x70;
        is_final = ((header[0] & 0x80) != 0);
        masked = ((header[1] & 0x80) != 0);
        payload_length = static_cast<ft_size_t>(header[1] & 0x7F);
        is_control_frame = ((opcode & 0x08) != 0);
        if (is_final == FT_FALSE)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if ((rsv_bits & 0x20) != 0 || (rsv_bits & 0x10) != 0)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if ((rsv_bits & 0x40) != 0 && permessage_deflate_enabled == FT_FALSE)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (is_control_frame != FT_FALSE && rsv_bits != 0)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (payload_length == 126)
        {
            unsigned char extended[2];

            if (websocket_recv_exact(client_fd, extended, 2) != FT_ERR_SUCCESS)
            {
                this->remove_connection_state_locked(client_fd);
                return (FT_ERR_INVALID_OPERATION);
            }
            payload_length = static_cast<ft_size_t>((extended[0] << 8) | extended[1]);
        }
        else if (payload_length == 127)
        {
            unsigned char extended[8];
            ft_size_t shift_index;

            if (websocket_recv_exact(client_fd, extended, 8) != FT_ERR_SUCCESS)
            {
                this->remove_connection_state_locked(client_fd);
                return (FT_ERR_INVALID_OPERATION);
            }
            payload_length = 0;
            shift_index = 0;
            while (shift_index < 8)
            {
                payload_length = (payload_length << 8) | extended[shift_index];
                shift_index++;
            }
        }
        if (is_control_frame != FT_FALSE && payload_length > 125)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (payload_length > compression_max_size)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (masked == FT_FALSE)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (websocket_recv_exact(client_fd, mask_key, 4) != FT_ERR_SUCCESS)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        if (payload_length > 0)
            payload = static_cast<unsigned char *>(cma_malloc(payload_length));
        else
            payload = ft_nullptr;
        if (payload_length > 0 && payload == ft_nullptr)
        {
            this->remove_connection_state_locked(client_fd);
            return (FT_ERR_INVALID_OPERATION);
        }
        index_value = 0;
        while (index_value < payload_length)
        {
            bytes_received = nw_recv(client_fd, payload + index_value, payload_length - index_value, 0);
            if (bytes_received <= 0)
            {
                if (payload != ft_nullptr)
                    cma_free(payload);
                this->remove_connection_state_locked(client_fd);
                return (FT_ERR_INVALID_OPERATION);
            }
            index_value += static_cast<ft_size_t>(bytes_received);
        }
        websocket_copy_masked_payload(payload, payload, payload_length, mask_key);
        if (opcode == 0x9)
        {
            if (this->send_pong_locked(client_fd, payload, payload_length) != 0)
            {
                if (payload != ft_nullptr)
                    cma_free(payload);
                this->remove_connection_state_locked(client_fd);
                return (FT_ERR_INVALID_OPERATION);
            }
            if (payload != ft_nullptr)
                cma_free(payload);
            continue ;
        }
        if (opcode == 0xA)
        {
            if (payload != ft_nullptr)
                cma_free(payload);
            continue ;
        }
        if (opcode == 0x1)
        {
            if ((rsv_bits & 0x40) != 0)
            {
                inflate_error = websocket_permessage_deflate_inflate(payload, payload_length, message);
                if (inflate_error != FT_ERR_SUCCESS)
                {
                    if (payload != ft_nullptr)
                        cma_free(payload);
                    this->remove_connection_state_locked(client_fd);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            else
            {
                message.clear();
                if (payload_length != 0U && message.append(
                        reinterpret_cast<const char *>(payload), payload_length) != FT_ERR_SUCCESS)
                {
                    cma_free(payload);
                    this->remove_connection_state_locked(client_fd);
                    return (FT_ERR_INVALID_OPERATION);
                }
            }
            if (payload != ft_nullptr)
                cma_free(payload);
            return (FT_ERR_SUCCESS);
        }
        if (payload != ft_nullptr)
            cma_free(payload);
        this->remove_connection_state_locked(client_fd);
        return (FT_ERR_INVALID_OPERATION);
    }
}

int32_t ft_websocket_server::run_once(int32_t &client_fd, ft_string &message)
{
    struct sockaddr_storage client_address;
    socklen_t address_length;
    int32_t result;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_server::run_once");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (this->_server_socket == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    address_length = sizeof(client_address);
    client_fd = nw_accept(this->_server_socket->get_file_descriptor(), reinterpret_cast<struct sockaddr*>(&client_address), &address_length);
    if (client_fd < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (this->perform_handshake_locked(client_fd) != 0)
    {
        nw_close(client_fd);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    result = this->receive_frame_locked(client_fd, message);
    if (result != 0)
    {
        nw_close(client_fd);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_websocket_server::send_text(int32_t client_fd, const ft_string &message)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_server::send_text");
    if (client_fd < 0)
        return (FT_ERR_INVALID_OPERATION);
    return (this->send_text_many(&client_fd, 1U, message));
}

int32_t ft_websocket_server::send_text_many(const int32_t *client_descriptors,
    ft_size_t count, const ft_string &message)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_server::send_text_many");
    if (count == 0U)
        return (FT_ERR_SUCCESS);
    if (client_descriptors == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);

    ft_string compressed_payload;
    ft_size_t client_index = 0U;
    int32_t result = FT_ERR_SUCCESS;
    while (client_index < count && result == FT_ERR_SUCCESS)
    {
        const int32_t client_fd = client_descriptors[client_index];
        if (client_fd < 0)
            result = FT_ERR_INVALID_OPERATION;
        else if (this->connection_supports_permessage_deflate_locked(client_fd))
        {
            // All negotiated connections use server_no_context_takeover.
            if (!compressed_payload.is_initialised())
            {
                result = compressed_payload.initialize();
                if (result == FT_ERR_SUCCESS && websocket_permessage_deflate_deflate(
                        message, compressed_payload) != FT_ERR_SUCCESS)
                    result = FT_ERR_INVALID_OPERATION;
            }
            if (result == FT_ERR_SUCCESS)
                result = websocket_send_frame(client_fd, 0xC1U,
                    reinterpret_cast<const unsigned char *>(compressed_payload.c_str()),
                    compressed_payload.size() - WEBSOCKET_DEFLATE_TAIL_SIZE, ft_nullptr);
        }
        else
            result = websocket_send_frame(client_fd, 0x81U,
                reinterpret_cast<const unsigned char *>(message.c_str()), message.size(), ft_nullptr);
        ++client_index;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

int32_t ft_websocket_server::get_port(uint16_t &port_value) const
{
    struct sockaddr_storage local_address;
    socklen_t address_length;
    int32_t server_fd;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_websocket_server::get_port");
    port_value = 0;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (this->_server_socket == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    server_fd = this->_server_socket->get_file_descriptor();
    if (server_fd < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    address_length = sizeof(local_address);
    if (getsockname(server_fd, reinterpret_cast<struct sockaddr*>(&local_address), &address_length) != 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    if (local_address.ss_family == AF_INET)
    {
        const struct sockaddr_in *ipv4_address;

        ipv4_address = reinterpret_cast<const struct sockaddr_in*>(&local_address);
        port_value = ntohs(ipv4_address->sin_port);
    }
    else if (local_address.ss_family == AF_INET6)
    {
        const struct sockaddr_in6 *ipv6_address;

        ipv6_address = reinterpret_cast<const struct sockaddr_in6*>(&local_address);
        port_value = ntohs(ipv6_address->sin6_port);
    }
    else
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_OPERATION);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}
