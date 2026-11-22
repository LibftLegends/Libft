#ifndef NETWORKING_WEBSOCKET_FRAME_INTERNAL_HPP
# define NETWORKING_WEBSOCKET_FRAME_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "networking.hpp"
#include "../CMA/CMA.hpp"

static inline void websocket_copy_masked_payload(unsigned char *destination,
    const unsigned char *payload, ft_size_t length, const unsigned char *mask_key)
{
    ft_size_t index = 0;
    while (length - index >= 4U)
    {
        destination[index] = payload[index] ^ mask_key[0];
        destination[index + 1U] = payload[index + 1U] ^ mask_key[1];
        destination[index + 2U] = payload[index + 2U] ^ mask_key[2];
        destination[index + 3U] = payload[index + 3U] ^ mask_key[3];
        index += 4U;
    }
    ft_size_t mask_index = 0;
    while (index < length)
    {
        destination[index] = payload[index] ^ mask_key[mask_index];
        ++index;
        ++mask_index;
    }
    return ;
}

static inline int32_t websocket_send_bytes(int32_t socket_descriptor, const void *data, ft_size_t length)
{
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    ft_size_t sent = 0U;
    while (sent < length)
    {
        ft_size_t remaining = length - sent;
        if (remaining > static_cast<ft_size_t>(FT_INT32_MAX))
            remaining = FT_INT32_MAX;
        const ssize_t result = nw_send(socket_descriptor, bytes + sent, remaining, 0);
        if (result <= 0)
            return (FT_ERR_INVALID_OPERATION);
        sent += static_cast<ft_size_t>(result);
    }
    return (FT_ERR_SUCCESS);
}

static inline int32_t websocket_send_frame(int32_t socket_descriptor, unsigned char opcode,
    const unsigned char *payload, ft_size_t length, const unsigned char *mask_key)
{
    if (socket_descriptor < 0 || (payload == ft_nullptr && length != 0)
        || length > static_cast<ft_size_t>(SIZE_MAX) - 14U
        || length > static_cast<ft_size_t>(FT_LLONG_MAX))
        return (FT_ERR_INVALID_OPERATION);
    unsigned char header[14];
    header[0] = opcode;
    ft_size_t header_length = 2U;
    if (length <= 125U)
        header[1] = static_cast<unsigned char>(length);
    else if (length <= 65535U)
    {
        header[1] = 126U;
        header[2] = static_cast<unsigned char>(length >> 8U);
        header[3] = static_cast<unsigned char>(length);
        header_length = 4U;
    }
    else
    {
        header[1] = 127U;
        ft_size_t index = 0;
        while (index < 8U)
        {
            header[2U + index] = static_cast<unsigned char>(length >> ((7U - index) * 8U));
            ++index;
        }
        header_length = 10U;
    }
    if (mask_key != ft_nullptr)
    {
        header[1] |= 0x80U;
        ft_memcpy_fixed<4U>(header + header_length, mask_key);
        header_length += 4U;
    }
    unsigned char stack_frame[1024];
    unsigned char *frame = stack_frame;
    const ft_size_t frame_length = header_length + length;
    if (frame_length > sizeof(stack_frame))
    {
        frame = static_cast<unsigned char *>(cma_malloc(frame_length));
        if (frame == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
    }
    ft_memcpy(frame, header, header_length);
    if (mask_key != ft_nullptr)
        websocket_copy_masked_payload(frame + header_length, payload, length, mask_key);
    else if (length != 0U)
        ft_memcpy(frame + header_length, payload, length);
    const int32_t send_result = websocket_send_bytes(socket_descriptor, frame, frame_length);
    if (frame != stack_frame)
        cma_free(frame);
    return (send_result);
}

#endif
