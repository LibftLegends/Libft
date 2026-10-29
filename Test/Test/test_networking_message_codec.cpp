#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_message_fragmentation_test_support.hpp"

class networking_codec_test_io : public networking_datagram_io
{
    public:
        networking_codec_test_io() noexcept
        {
            return ;
        }

        ~networking_codec_test_io() noexcept override
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
            return (0U);
        }
};

FT_TEST(test_networking_message_codec_rejects_header_mutations_and_truncation)
{
    networking_codec_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint source;
    const uint8_t payload[1] = {0x5aU};
    uint8_t packet[128];
    uint32_t size;
    ft_size_t packet_size;

    ft_memset(&source, 0, sizeof(source));
    source.address.ss_family = AF_INET;
    source.length = static_cast<socklen_t>(sizeof(sockaddr_in));
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    size = 0U;
    while (size < sizeof(packet))
    {
        ft_memset(packet, 0, sizeof(packet));
        if (size >= 1U)
            packet[0] = 0x4cU;
        if (size >= 2U)
            packet[1] = 1U;
        if (size >= 3U)
            packet[2] = 0U;
        if (size >= 4U)
            packet[3] = 0U;
        FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
            source, packet, size));
        size += 1U;
    }
    packet[0] = 0U;
    packet[1] = 1U;
    packet[2] = 0U;
    packet[3] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    packet_size = networking_fragmentation_make_packet(packet, 1U, 1U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, transport.process_datagram(
        source, packet, packet_size));
    packet[0] = 0x4cU;
    packet[1] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    packet[1] = 1U;
    packet[2] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(
        source, packet, sizeof(packet)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_codec_valid_frame_truncation_and_field_matrix)
{
    networking_fragmentation_test_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_connection unexpected_connection;
    networking_message_endpoint source;
    networking_received_message received;
    const uint8_t payload[4] = {0x11U, 0x22U, 0x33U, 0x44U};
    uint8_t packet[128];
    ft_size_t packet_size;
    ft_size_t truncated_size;
    int32_t parse_result;

    networking_test_prepare_endpoint(source);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(source));
    packet_size = networking_fragmentation_make_packet(packet, 801U, 1U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source, packet,
        packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload), received.payload.size());

    truncated_size = 0U;
    while (truncated_size < packet_size)
    {
        packet_size = networking_fragmentation_make_packet(packet,
            802U + truncated_size, 2U + truncated_size,
            sizeof(payload), 0U, 1U, payload,
            static_cast<uint16_t>(sizeof(payload)));
        parse_result = transport.process_datagram(source, packet,
            truncated_size);
        if (truncated_size < packet_size - sizeof(payload))
            FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, parse_result);
        else
            FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, parse_result);
        truncated_size += 1U;
    }

    packet_size = networking_fragmentation_make_packet(packet, 900U, 100U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[0] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[0] = 0x4cU;
    packet[1] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[1] = 1U;
    packet[2] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 901U, 101U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[3] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet[2] = 2U;
    packet[3] = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 902U, 102U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[11] = 78U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.get_connection(78U,
        unexpected_connection));
    packet_size = networking_fragmentation_make_packet(packet, 909U, 109U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[11] = 79U;
    packet[3] = 0xffU;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE,
        transport.get_connection(79U, unexpected_connection));
    packet_size = networking_fragmentation_make_packet(packet, 903U, 103U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[44] = 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 904U, 104U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[44] = 5U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 904U, 104U,
        sizeof(payload), UINT32_MAX, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 906U, 106U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[84] = 5U;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 907U, 107U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[84] = 3U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 908U, 108U,
        sizeof(payload), 0U, 0U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, transport.process_datagram(source,
        packet, packet_size));
    packet_size = networking_fragmentation_make_packet(packet, 905U, 105U,
        sizeof(payload), 0U, 1U, payload,
        static_cast<uint16_t>(sizeof(payload)));
    packet[packet_size] = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, transport.process_datagram(source,
        packet, packet_size + 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}
