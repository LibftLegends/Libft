#ifndef NETWORKING_MESSAGE_FRAGMENTATION_TEST_SUPPORT_HPP
#define NETWORKING_MESSAGE_FRAGMENTATION_TEST_SUPPORT_HPP

#include "networking_test_support.hpp"

class networking_fragmentation_test_io : public networking_datagram_io
{
    private:
        uint64_t _now_milliseconds;

    public:
        networking_fragmentation_test_io() noexcept : _now_milliseconds(0U)
        {
            return ;
        }

        ~networking_fragmentation_test_io() noexcept override
        {
            return ;
        }

        void advance(uint64_t milliseconds) noexcept
        {
            this->_now_milliseconds += milliseconds;
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)destination;
            if (data == ft_nullptr && size != 0U)
                return (FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            (void)source;
            (void)data;
            (void)capacity;
            if (received_size == ft_nullptr)
                return (FT_ERR_INVALID_ARGUMENT);
            *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (this->_now_milliseconds);
        }
};

static void networking_fragmentation_write_u16(uint8_t *output,
    uint16_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 8U);
    output[1] = static_cast<uint8_t>(value);
    return ;
}

static void networking_fragmentation_write_u32(uint8_t *output,
    uint32_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 24U);
    output[1] = static_cast<uint8_t>(value >> 16U);
    output[2] = static_cast<uint8_t>(value >> 8U);
    output[3] = static_cast<uint8_t>(value);
    return ;
}

static void networking_fragmentation_write_u64(uint8_t *output,
    uint64_t value) noexcept
{
    uint32_t index = 0U;

    while (index < 8U)
    {
        output[index] = static_cast<uint8_t>(value >> (56U - index * 8U));
        index += 1U;
    }
    return ;
}

static ft_size_t networking_fragmentation_make_packet(uint8_t *packet,
    uint64_t message_id, uint64_t packet_number, uint32_t total_size,
    uint32_t fragment_offset, uint8_t fragment_count,
    const uint8_t *payload, uint16_t payload_size,
    uint64_t connection_id = 77U) noexcept
{
    ft_size_t offset = 0U;
    uint32_t index;

    packet[offset++] = 0x4cU;
    packet[offset++] = 1U;
    packet[offset++] = 1U;
    packet[offset++] = static_cast<uint8_t>(
        networking_message_delivery::UNRELIABLE);
    networking_fragmentation_write_u64(packet + offset, connection_id);
    offset += 8U;
    networking_fragmentation_write_u64(packet + offset, message_id);
    offset += 8U;
    networking_fragmentation_write_u64(packet + offset, packet_number);
    offset += 8U;
    networking_fragmentation_write_u64(packet + offset, 0U);
    offset += 8U;
    networking_fragmentation_write_u64(packet + offset, 0U);
    offset += 8U;
    packet[offset++] = 0U;
    index = 0U;
    while (index < 4U)
    {
        networking_fragmentation_write_u16(packet + offset, 0U);
        offset += 2U;
        networking_fragmentation_write_u16(packet + offset, 0U);
        offset += 2U;
        index += 1U;
    }
    networking_fragmentation_write_u32(packet + offset, 0U);
    offset += 4U;
    packet[offset++] = 0U;
    packet[offset++] = fragment_count;
    networking_fragmentation_write_u32(packet + offset, total_size);
    offset += 4U;
    networking_fragmentation_write_u32(packet + offset, fragment_offset);
    offset += 4U;
    networking_fragmentation_write_u64(packet + offset, 1U);
    offset += 8U;
    networking_fragmentation_write_u16(packet + offset, payload_size);
    offset += 2U;
    if (payload_size != 0U)
        ft_memcpy(packet + offset, payload, payload_size);
    return (offset + payload_size);
}

#endif
