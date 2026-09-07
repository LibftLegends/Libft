#include "compatebility_cross_process.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

#include <cerrno>
#include <cstring>

namespace
{
    static void write_u16(uint8_t *destination, uint16_t value)
    {
        destination[0] = static_cast<uint8_t>(value >> 8U);
        destination[1] = static_cast<uint8_t>(value);
    }

    static void write_u32(uint8_t *destination, uint32_t value)
    {
        destination[0] = static_cast<uint8_t>(value >> 24U);
        destination[1] = static_cast<uint8_t>(value >> 16U);
        destination[2] = static_cast<uint8_t>(value >> 8U);
        destination[3] = static_cast<uint8_t>(value);
    }

    static void write_u64(uint8_t *destination, uint64_t value)
    {
        uint32_t high;
        uint32_t low;

        high = static_cast<uint32_t>(value >> 32U);
        low = static_cast<uint32_t>(value);
        write_u32(destination, high);
        write_u32(destination + 4U, low);
    }

    static uint16_t read_u16(const uint8_t *source)
    {
        return (static_cast<uint16_t>(static_cast<uint16_t>(source[0]) << 8U)
            | static_cast<uint16_t>(source[1]));
    }

    static uint32_t read_u32(const uint8_t *source)
    {
        return (static_cast<uint32_t>(source[0]) << 24U
            | static_cast<uint32_t>(source[1]) << 16U
            | static_cast<uint32_t>(source[2]) << 8U
            | static_cast<uint32_t>(source[3]));
    }

    static uint64_t read_u64(const uint8_t *source)
    {
        return (static_cast<uint64_t>(read_u32(source)) << 32U
            | static_cast<uint64_t>(read_u32(source + 4U)));
    }
}

int32_t cmp_cross_process_encode_wire(const cross_process_message &message,
    uint8_t *wire, ft_size_t wire_size)
{
    ft_size_t offset;

    if (wire == ft_nullptr || wire_size < CMP_CROSS_PROCESS_WIRE_SIZE
        || std::memchr(message.shared_memory_name, '\0',
            sizeof(message.shared_memory_name)) == ft_nullptr)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    ft_memset(wire, 0, CMP_CROSS_PROCESS_WIRE_SIZE);
    write_u32(wire, CMP_CROSS_PROCESS_WIRE_MAGIC);
    write_u16(wire + 4U, CMP_CROSS_PROCESS_WIRE_VERSION);
    write_u16(wire + 6U, 0U);
    offset = 8U;
    write_u64(wire + offset, message.stack_base_address);
    offset += 8U;
    write_u64(wire + offset, message.remote_memory_address);
    offset += 8U;
    write_u64(wire + offset, message.remote_memory_size);
    offset += 8U;
    write_u64(wire + offset, message.shared_mutex_address);
    offset += 8U;
    write_u64(wire + offset, message.error_memory_address);
    offset += 8U;
    ft_memcpy(wire + offset, message.shared_memory_name,
        sizeof(message.shared_memory_name));
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_decode_wire(const uint8_t *wire,
    ft_size_t wire_size, cross_process_message &message)
{
    cross_process_message decoded;
    ft_size_t offset;

    if (wire == ft_nullptr || wire_size != CMP_CROSS_PROCESS_WIRE_SIZE
        || read_u32(wire) != CMP_CROSS_PROCESS_WIRE_MAGIC
        || read_u16(wire + 4U) != CMP_CROSS_PROCESS_WIRE_VERSION
        || read_u16(wire + 6U) != 0U)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    ft_memset(&decoded, 0, sizeof(decoded));
    offset = 8U;
    decoded.stack_base_address = read_u64(wire + offset);
    offset += 8U;
    decoded.remote_memory_address = read_u64(wire + offset);
    offset += 8U;
    decoded.remote_memory_size = read_u64(wire + offset);
    offset += 8U;
    decoded.shared_mutex_address = read_u64(wire + offset);
    offset += 8U;
    decoded.error_memory_address = read_u64(wire + offset);
    offset += 8U;
    ft_memcpy(decoded.shared_memory_name, wire + offset,
        sizeof(decoded.shared_memory_name));
    if (std::memchr(decoded.shared_memory_name, '\0',
        sizeof(decoded.shared_memory_name)) == ft_nullptr)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    message = decoded;
    return (FT_ERR_SUCCESS);
}
