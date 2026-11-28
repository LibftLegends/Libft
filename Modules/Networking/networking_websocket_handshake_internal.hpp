#ifndef NETWORKING_WEBSOCKET_HANDSHAKE_INTERNAL_HPP
# define NETWORKING_WEBSOCKET_HANDSHAKE_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "networking.hpp"
#include "../Compression/compression_base64_internal.hpp"
#include "../Encryption/encryption_digest_internal.hpp"

static constexpr ft_size_t WEBSOCKET_ACCEPT_KEY_LENGTH = 28U;

static inline ft_bool websocket_compute_accept_key(const char *key, ft_size_t key_length,
    char (&accept)[WEBSOCKET_ACCEPT_KEY_LENGTH + 1U]) noexcept
{
    static constexpr char WEBSOCKET_ACCEPT_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    sha1_digest_state state;
    uint8_t digest[20];
    sha1_digest_initialize(state);
    sha1_digest_update(state, reinterpret_cast<const uint8_t *>(key), key_length);
    sha1_digest_update(state, reinterpret_cast<const uint8_t *>(WEBSOCKET_ACCEPT_GUID),
        sizeof(WEBSOCKET_ACCEPT_GUID) - 1U);
    sha1_digest_final(state, digest);
    return (compression_base64_encode_into(digest, sizeof(digest), accept, sizeof(accept)));
}

static inline int32_t websocket_read_handshake(int32_t socket_descriptor, ft_string &headers,
    ft_size_t size_limit)
{
    char buffer[1024];
    while (FT_TRUE)
    {
        const ft_size_t previous_size = headers.size();
        if (previous_size >= size_limit)
            return (FT_ERR_INVALID_OPERATION);
        ft_size_t read_size = sizeof(buffer);
        if (read_size > size_limit - previous_size)
            read_size = size_limit - previous_size;
        const ssize_t received = nw_recv(socket_descriptor, buffer, read_size, 0);
        if (received <= 0)
            return (FT_ERR_INVALID_OPERATION);
        const char *terminator = static_cast<const char *>(ft_memchr(buffer, '\0',
                    static_cast<ft_size_t>(received)));
        ft_size_t header_bytes = static_cast<ft_size_t>(received);
        if (terminator != ft_nullptr)
            header_bytes = static_cast<ft_size_t>(terminator - buffer);
        const int32_t append_result = headers.append(buffer, header_bytes);
        if (append_result != FT_ERR_SUCCESS)
            return (append_result);
        ft_size_t scan_offset = 0U;
        if (previous_size > 3U)
            scan_offset = previous_size - 3U;
        if (ft_strstr(headers.c_str() + scan_offset, "\r\n\r\n") != ft_nullptr)
            return (FT_ERR_SUCCESS);
    }
}

#endif
