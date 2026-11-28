#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "networking_structured_fuzz_mutator.hpp"
#include <cstdint>

class networking_fuzz_datagram_io : public networking_datagram_io
{
    public:
        networking_fuzz_datagram_io() noexcept
        {
            return ;
        }

        ~networking_fuzz_datagram_io() noexcept override
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
            uint8_t *data, ft_size_t capacity,
            ft_size_t *received_size) noexcept override
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

static void networking_fuzz_write_u64(uint8_t *output,
    uint64_t value) noexcept
{
    uint32_t index = 0U;

    while (index < 8U)
    {
        output[index] = static_cast<uint8_t>(value >> (56U - index * 8U));
        index += 1U;
    }
}

static void networking_fuzz_write_u32(uint8_t *output,
    uint32_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 24U);
    output[1] = static_cast<uint8_t>(value >> 16U);
    output[2] = static_cast<uint8_t>(value >> 8U);
    output[3] = static_cast<uint8_t>(value);
}

static void networking_fuzz_write_u16(uint8_t *output,
    uint16_t value) noexcept
{
    output[0] = static_cast<uint8_t>(value >> 8U);
    output[1] = static_cast<uint8_t>(value);
}

static void networking_fuzz_run_transport(const uint8_t *data,
    ft_size_t size, ft_bool enable_encryption) noexcept
{
    networking_fuzz_datagram_io datagram_io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    uint8_t structured_packet[256];
    ft_size_t structured_size;
    ft_size_t payload_size;
    ft_size_t index;
    uint64_t message_identifier;
    uint64_t packet_number_value;
    uint32_t channel;
    uint8_t lane;

    configuration.enable_encryption = enable_encryption;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = static_cast<socklen_t>(sizeof(struct sockaddr_in));
    if (transport.initialize(configuration, datagram_io) != FT_ERR_SUCCESS)
        return ;
    if (transport.listen(source) != FT_ERR_SUCCESS)
    {
        (void)transport.destroy();
        return ;
    }
    structured_size = size;
    if (structured_size < 85U)
        structured_size = 85U;
    if (structured_size > sizeof(structured_packet))
        structured_size = sizeof(structured_packet);
    index = 0U;
    while (index < structured_size)
    {
        structured_packet[index] = 0U;
        index += 1U;
    }
    payload_size = structured_size - 85U;
    message_identifier = 1U;
    packet_number_value = 1U;
    channel = 0U;
    lane = 0U;
    if (size > 0U)
        message_identifier = static_cast<uint64_t>(data[0U]) + 1U;
    if (size > 1U)
        packet_number_value = static_cast<uint64_t>(data[1U]) + 1U;
    if (size > 2U)
        channel = data[2U];
    if (size > 3U)
        lane = static_cast<uint8_t>(data[3U] % 4U);
    structured_packet[0] = 0x4cU;
    structured_packet[1] = 1U;
    structured_packet[2] = 1U;
    if (size != 0U)
        structured_packet[3] = static_cast<uint8_t>(data[0U] % 3U);
    else
        structured_packet[3] = 0U;
    networking_fuzz_write_u64(structured_packet + 4U, 1U);
    networking_fuzz_write_u64(structured_packet + 12U, message_identifier);
    networking_fuzz_write_u64(structured_packet + 20U, packet_number_value);
    networking_fuzz_write_u64(structured_packet + 28U, 0U);
    networking_fuzz_write_u64(structured_packet + 36U, 0U);
    networking_fuzz_write_u32(structured_packet + 61U, channel);
    structured_packet[65] = lane;
    structured_packet[66] = 1U;
    networking_fuzz_write_u32(structured_packet + 67U,
        static_cast<uint32_t>(payload_size));
    networking_fuzz_write_u32(structured_packet + 71U, 0U);
    networking_fuzz_write_u64(structured_packet + 75U, 1U);
    networking_fuzz_write_u16(structured_packet + 83U,
        static_cast<uint16_t>(payload_size));
    index = 0U;
    while (index < payload_size)
    {
        if (size != 0U)
            structured_packet[85U + index] = data[index % size];
        else
            structured_packet[85U + index] = static_cast<uint8_t>(index * 29U + 7U);
        index += 1U;
    }
    (void)transport.process_datagram(source, structured_packet,
        structured_size);
    (void)transport.process_datagram(source, data, size);
    (void)transport.poll();
    (void)transport.destroy();
    return ;
}

/* The libFuzzer ABI requires this exact exported symbol and 32-bit result. */
extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data,
    ft_size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = size;
    if (bounded_size > 65536U)
        bounded_size = 65536U;
    networking_fuzz_run_transport(data, bounded_size, FT_FALSE);
    networking_fuzz_run_transport(data, bounded_size, FT_TRUE);
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 65536U));
}
