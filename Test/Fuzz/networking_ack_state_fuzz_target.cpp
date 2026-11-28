#include "../Test/networking_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_structured_fuzz_mutator.hpp"

#include <cstddef>
#include <cstdint>

static void networking_ack_state_fuzz_run(const uint8_t *data,
    ft_size_t size) noexcept
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_message_statistics statistics;
    uint8_t payload[64];
    ft_size_t payload_size;
    ft_size_t index;
    uint32_t operation_count;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    if (client.initialize(configuration, client_io) != FT_ERR_SUCCESS)
        return ;
    if (server.initialize(configuration, server_io) != FT_ERR_SUCCESS)
    {
        (void)client.destroy();
        return ;
    }
    if (client.open_connection(endpoint, client_connection) != FT_ERR_SUCCESS
        || server.open_connection(endpoint, server_connection) != FT_ERR_SUCCESS)
    {
        (void)client.destroy();
        (void)server.destroy();
        return ;
    }
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.lane = 0U;
    options.channel = 0U;
    options.expiry_milliseconds = 0U;
    operation_count = static_cast<uint32_t>(size);
    if (operation_count > 512U)
        operation_count = 512U;
    index = 0U;
    while (index < operation_count)
    {
        uint8_t operation = data[index] % 8U;

        if (operation == 0U || operation == 1U)
        {
            payload_size = 1U + static_cast<ft_size_t>(data[index] % 64U);
            ft_memset(payload, data[index], payload_size);
            options.channel = static_cast<uint32_t>(data[index] % 4U);
            options.lane = static_cast<uint8_t>(data[index] % 4U);
            options.delivery = networking_message_delivery::RELIABLE_ORDERED;
            (void)client_connection.send_message(payload, payload_size, options);
        }
        else if (operation == 2U)
        {
            (void)client.poll();
        }
        else if (operation == 3U)
        {
            (void)server.poll();
        }
        else if (operation == 4U)
        {
            client_io.advance(static_cast<uint64_t>(data[index]) + 1U);
            server_io.advance(static_cast<uint64_t>(data[index]) + 1U);
        }
        else if (operation == 5U)
        {
            client_io.drop_next_datagram();
            (void)client.poll();
        }
        else if (operation == 6U)
        {
            (void)client.get_statistics(client_connection.get_id(), statistics);
        }
        else
        {
            (void)client_connection.close();
        }
        index += 1U;
    }
    index = 0U;
    while (index < 4U)
    {
        (void)client.poll();
        (void)server.poll();
        index += 1U;
    }
    (void)client_connection.destroy();
    (void)server_connection.destroy();
    (void)client.destroy();
    (void)server.destroy();
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
    networking_ack_state_fuzz_run(data, bounded_size);
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 4096U));
}
