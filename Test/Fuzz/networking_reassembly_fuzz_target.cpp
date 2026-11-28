#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_structured_fuzz_mutator.hpp"
#include <cstddef>
#include <cstdint>

class networking_reassembly_fuzz_io : public networking_datagram_io
{
    public:
        networking_reassembly_fuzz_io() noexcept
        {
            return ;
        }

        ~networking_reassembly_fuzz_io() noexcept override
        {
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)destination;
            (void)data;
            (void)size;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size)
            noexcept override
        {
            (void)source;
            (void)data;
            (void)capacity;
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (1U);
        }
};

static void networking_reassembly_write_u64(uint8_t *output,
    uint64_t value) noexcept
{
    uint32_t index = 0U;

    while (index < 8U)
    {
        output[index] = static_cast<uint8_t>(value >> (56U - index * 8U));
        index += 1U;
    }
}

static void networking_reassembly_write_u32(uint8_t *output,
    uint32_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 24U);
    output[1] = static_cast<uint8_t>(value >> 16U);
    output[2] = static_cast<uint8_t>(value >> 8U);
    output[3] = static_cast<uint8_t>(value);
}

static void networking_reassembly_write_u16(uint8_t *output,
    uint16_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 8U);
    output[1] = static_cast<uint8_t>(value);
}

static ft_size_t networking_reassembly_make_packet(uint8_t *packet,
    uint64_t packet_number, uint64_t message_id, uint32_t total_size,
    uint32_t fragment_offset, uint8_t fragment_count,
    const uint8_t *payload, uint16_t payload_size) noexcept
{
    ft_size_t offset = 0U;
    uint32_t index;

    packet[offset++] = 0x4cU;
    packet[offset++] = 1U;
    packet[offset++] = 1U;
    packet[offset++] = 0U;
    networking_reassembly_write_u64(packet + offset, 77U);
    offset += 8U;
    networking_reassembly_write_u64(packet + offset, message_id);
    offset += 8U;
    networking_reassembly_write_u64(packet + offset, packet_number);
    offset += 8U;
    networking_reassembly_write_u64(packet + offset, 0U);
    offset += 8U;
    networking_reassembly_write_u64(packet + offset, 0U);
    offset += 8U;
    packet[offset++] = 0U;
    index = 0U;
    while (index < 4U)
    {
        networking_reassembly_write_u16(packet + offset, 0U);
        offset += 2U;
        networking_reassembly_write_u16(packet + offset, 0U);
        offset += 2U;
        index += 1U;
    }
    networking_reassembly_write_u32(packet + offset, 0U);
    offset += 4U;
    packet[offset++] = 0U;
    packet[offset++] = fragment_count;
    networking_reassembly_write_u32(packet + offset, total_size);
    offset += 4U;
    networking_reassembly_write_u32(packet + offset, fragment_offset);
    offset += 4U;
    networking_reassembly_write_u64(packet + offset, 1U);
    offset += 8U;
    networking_reassembly_write_u16(packet + offset, payload_size);
    offset += 2U;
    if (payload_size != 0U)
        ft_memcpy(packet + offset, payload, payload_size);
    return (offset + payload_size);
}

static void networking_reassembly_fuzz_run(const uint8_t *data,
    ft_size_t size) noexcept
{
    networking_reassembly_fuzz_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    networking_received_message message;
    uint8_t first_packet[400];
    uint8_t second_packet[400];
    uint8_t first_payload[256];
    uint8_t second_payload[256];
    uint32_t total_size;
    uint32_t first_size;
    uint32_t second_size;
    uint32_t index;

    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = static_cast<socklen_t>(sizeof(sockaddr_in));
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.maximum_message_size =
        static_cast<uint32_t>(sizeof(first_payload));
    configuration.maximum_reassembly_bytes = 512U;
    if (transport.initialize(configuration, io) != FT_ERR_SUCCESS)
        return ;
    if (transport.listen(source) != FT_ERR_SUCCESS)
    {
        (void)transport.destroy();
        return ;
    }
    total_size = 2U;
    if (size != 0U)
        total_size += static_cast<uint32_t>(data[0U]
            % (sizeof(first_payload) - 1U));
    first_size = total_size / 2U;
    if (first_size == 0U)
        first_size = 1U;
    second_size = total_size - first_size;
    index = 0U;
    while (index < first_size)
    {
        if (size != 0U)
            first_payload[index] = data[index % size];
        else
            first_payload[index] = static_cast<uint8_t>(index * 17U + 5U);
        index += 1U;
    }
    index = 0U;
    while (index < second_size)
    {
        if (size != 0U)
            second_payload[index] = data[(index + first_size) % size];
        else
            second_payload[index] = static_cast<uint8_t>(index * 29U + 3U);
        index += 1U;
    }
    index = static_cast<uint32_t>(networking_reassembly_make_packet(
        first_packet, 1U, 900U, total_size, 0U, 2U, first_payload,
        static_cast<uint16_t>(first_size)));
    (void)transport.process_datagram(source, first_packet, index);
    first_payload[0] ^= 0xffU;
    index = static_cast<uint32_t>(networking_reassembly_make_packet(
        first_packet, 2U, 900U, total_size, 0U, 2U, first_payload,
        static_cast<uint16_t>(first_size)));
    (void)transport.process_datagram(source, first_packet, index);
    first_payload[0] ^= 0xffU;
    index = static_cast<uint32_t>(networking_reassembly_make_packet(
        second_packet, 3U, 900U, total_size, first_size, 2U, second_payload,
        static_cast<uint16_t>(second_size)));
    (void)transport.process_datagram(source, second_packet, index);
    (void)transport.receive_message(message);
    (void)transport.destroy();
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    if (data == ft_nullptr && size != 0U)
        return (0);
    networking_reassembly_fuzz_run(data, static_cast<ft_size_t>(size));
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 256U));
}
