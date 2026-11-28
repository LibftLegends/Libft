#include "compatebility_cross_process.hpp"

#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

#include <cerrno>

namespace
{
    constexpr ft_size_t WIRE_NAME_OFFSET = 48U;
    static_assert(WIRE_NAME_OFFSET + sizeof(cross_process_message::shared_memory_name)
        == CMP_CROSS_PROCESS_WIRE_SIZE);

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
    if (wire == ft_nullptr || wire_size < CMP_CROSS_PROCESS_WIRE_SIZE
        || ft_memchr(message.shared_memory_name, '\0',
            sizeof(message.shared_memory_name)) == ft_nullptr)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    const uint64_t stack_base_address = message.stack_base_address;
    const uint64_t remote_memory_address = message.remote_memory_address;
    const uint64_t remote_memory_size = message.remote_memory_size;
    const uint64_t shared_mutex_address = message.shared_mutex_address;
    const uint64_t error_memory_address = message.error_memory_address;
    (void)ft_memmove(wire + WIRE_NAME_OFFSET, message.shared_memory_name,
        sizeof(message.shared_memory_name));
    write_u32(wire, CMP_CROSS_PROCESS_WIRE_MAGIC);
    write_u16(wire + 4U, CMP_CROSS_PROCESS_WIRE_VERSION);
    write_u16(wire + 6U, 0U);
    write_u64(wire + 8U, stack_base_address);
    write_u64(wire + 16U, remote_memory_address);
    write_u64(wire + 24U, remote_memory_size);
    write_u64(wire + 32U, shared_mutex_address);
    write_u64(wire + 40U, error_memory_address);
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_decode_wire(const uint8_t *wire,
    ft_size_t wire_size, cross_process_message &message)
{
    if (wire == ft_nullptr || wire_size != CMP_CROSS_PROCESS_WIRE_SIZE
        || read_u32(wire) != CMP_CROSS_PROCESS_WIRE_MAGIC
        || read_u16(wire + 4U) != CMP_CROSS_PROCESS_WIRE_VERSION
        || read_u16(wire + 6U) != 0U
        || ft_memchr(wire + WIRE_NAME_OFFSET, '\0',
            sizeof(message.shared_memory_name)) == ft_nullptr)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    // Read the scalar fields before writing the possibly overlapping destination.
    const uint64_t stack_base_address = read_u64(wire + 8U);
    const uint64_t remote_memory_address = read_u64(wire + 16U);
    const uint64_t remote_memory_size = read_u64(wire + 24U);
    const uint64_t shared_mutex_address = read_u64(wire + 32U);
    const uint64_t error_memory_address = read_u64(wire + 40U);
    (void)ft_memmove(message.shared_memory_name, wire + WIRE_NAME_OFFSET,
        sizeof(message.shared_memory_name));
    message.stack_base_address = stack_base_address;
    message.remote_memory_address = remote_memory_address;
    message.remote_memory_size = remote_memory_size;
    message.shared_mutex_address = shared_mutex_address;
    message.error_memory_address = error_memory_address;
    return (FT_ERR_SUCCESS);
}
