#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_structured_fuzz_mutator.hpp"

#include <cstddef>
#include <cstdint>

class networking_codec_fuzz_io : public networking_datagram_io
{
    public:
        networking_codec_fuzz_io() noexcept
        {
            return ;
        }

        ~networking_codec_fuzz_io() noexcept override
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

static void networking_codec_fuzz_write_u64(uint8_t *output,
    uint64_t value) noexcept
{
    uint32_t index;

    index = 0U;
    while (index < 8U)
    {
        output[index] = static_cast<uint8_t>(value >> (56U - index * 8U));
        index += 1U;
    }
    return ;
}

static void networking_codec_fuzz_run(const uint8_t *data,
    ft_size_t size) noexcept
{
    networking_codec_fuzz_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    uint8_t packet[512];
    ft_size_t packet_size;
    ft_size_t payload_size;
    ft_size_t index;
    int32_t initialize_result;

    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = static_cast<socklen_t>(sizeof(struct sockaddr_in));
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    initialize_result = transport.initialize(configuration, io);
    if (initialize_result != FT_ERR_SUCCESS)
        return ;
    if (transport.listen(source) != FT_ERR_SUCCESS)
    {
        (void)transport.destroy();
        return ;
    }
    ft_memset(packet, 0, sizeof(packet));
    packet[0] = 0x4cU;
    packet[1] = 1U;
    packet[2] = 1U;
    packet[3] = 0U;
    networking_codec_fuzz_write_u64(packet + 4U, 1U);
    networking_codec_fuzz_write_u64(packet + 12U, 1U);
    networking_codec_fuzz_write_u64(packet + 20U, 1U);
    networking_codec_fuzz_write_u64(packet + 28U, 0U);
    networking_codec_fuzz_write_u64(packet + 36U, 0U);
    packet_size = 85U;
    payload_size = size;
    if (payload_size > sizeof(packet) - packet_size)
        payload_size = sizeof(packet) - packet_size;
    packet[66] = 1U;
    packet[65] = 0U;
    packet[64] = 0U;
    packet[62] = 0U;
    packet[63] = 0U;
    packet[67] = static_cast<uint8_t>(payload_size >> 24U);
    packet[68] = static_cast<uint8_t>(payload_size >> 16U);
    packet[69] = static_cast<uint8_t>(payload_size >> 8U);
    packet[70] = static_cast<uint8_t>(payload_size);
    packet[71] = 0U;
    packet[72] = 0U;
    packet[73] = 0U;
    packet[74] = 0U;
    networking_codec_fuzz_write_u64(packet + 75U, 1U);
    packet[83] = static_cast<uint8_t>(payload_size >> 8U);
    packet[84] = static_cast<uint8_t>(payload_size);
    index = 0U;
    while (index < payload_size)
    {
        packet[85U + index] = data[index];
        index += 1U;
    }
    index = 0U;
    while (index < size && index < 128U)
    {
        packet[data[index] % 85U] ^= data[index];
        index += 1U;
    }
    packet_size += payload_size;
    (void)transport.process_datagram(source, packet, packet_size);
    (void)transport.process_datagram(source, data, size);
    (void)transport.poll();
    (void)transport.destroy();
    return ;
}

extern "C" int32_t LLVMFuzzerTestOneInput(const uint8_t *data,
    ft_size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = size;
    if (bounded_size > 4096U)
        bounded_size = 4096U;
    networking_codec_fuzz_run(data, bounded_size);
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 128U));
}
