#include "../Test/networking_test_support.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "networking_structured_fuzz_mutator.hpp"

#include <cstddef>
#include <cstdint>

static void networking_state_machine_fuzz_drain_events(
    networking_message_transport &transport, uint64_t &pending_connection_id)
    noexcept
{
    networking_message_event event;
    uint32_t event_count;

    event_count = 0U;
    while (event_count < 32U
        && transport.receive_event(event) == FT_ERR_SUCCESS)
    {
        if (event.type == networking_message_event_type::CONNECTION_REQUESTED)
            pending_connection_id = event.connection_id;
        event_count += 1U;
    }
    return ;
}

static void networking_state_machine_fuzz_run(const uint8_t *data,
    ft_size_t size) noexcept
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection duplicate_connection;
    networking_message_statistics statistics;
    uint64_t pending_connection_id;
    ft_bool client_initialized;
    ft_bool server_initialized;
    ft_bool duplicate_opened;
    ft_size_t index;

    client_initialized = FT_FALSE;
    server_initialized = FT_FALSE;
    duplicate_opened = FT_FALSE;
    pending_connection_id = 0U;
    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    if (client.initialize(configuration, client_io) != FT_ERR_SUCCESS)
        return ;
    client_initialized = FT_TRUE;
    if (server.initialize(configuration, server_io) != FT_ERR_SUCCESS)
    {
        (void)client.destroy();
        return ;
    }
    server_initialized = FT_TRUE;
    (void)server.listen(endpoint);
    (void)client.open_connection(endpoint, client_connection);
    (void)server.poll();
    networking_state_machine_fuzz_drain_events(server,
        pending_connection_id);
    index = 0U;
    while (index < size)
    {
        uint8_t operation;

        operation = data[index] % 10U;
        if (operation == 0U)
            (void)client.poll();
        else if (operation == 1U)
        {
            (void)server.poll();
            networking_state_machine_fuzz_drain_events(server,
                pending_connection_id);
        }
        else if (operation == 2U && pending_connection_id != 0U)
        {
            (void)server.accept(pending_connection_id);
            pending_connection_id = 0U;
        }
        else if (operation == 3U && pending_connection_id != 0U)
        {
            (void)server.reject(pending_connection_id,
                networking_message_close_reason::APPLICATION);
            pending_connection_id = 0U;
        }
        else if (operation == 4U)
            (void)client_connection.close(
                networking_message_close_reason::APPLICATION, "fuzz close");
        else if (operation == 5U)
            (void)client_connection.abort(
                networking_message_close_reason::PROTOCOL_ERROR);
        else if (operation == 6U && duplicate_opened == FT_FALSE)
        {
            (void)client.open_connection(endpoint, duplicate_connection);
            duplicate_opened = FT_TRUE;
        }
        else if (operation == 7U)
            (void)client_connection.get_statistics(statistics);
        else if (operation == 8U)
        {
            client_io.advance(static_cast<uint64_t>(data[index]) + 1U);
            server_io.advance(static_cast<uint64_t>(data[index]) + 1U);
            (void)client.poll();
            (void)server.poll();
            networking_state_machine_fuzz_drain_events(server,
                pending_connection_id);
        }
        else if (operation == 9U && pending_connection_id != 0U)
            (void)server.accept(pending_connection_id);
        if (client_connection.get_state()
                > networking_message_connection_state::FAILED
            || (duplicate_opened != FT_FALSE
                && duplicate_connection.get_state()
                    > networking_message_connection_state::FAILED))
            break ;
        index += 1U;
    }
    (void)client.poll();
    (void)server.poll();
    if (duplicate_opened != FT_FALSE)
        (void)duplicate_connection.destroy();
    (void)client_connection.destroy();
    if (server_initialized != FT_FALSE)
        (void)server.destroy();
    if (client_initialized != FT_FALSE)
        (void)client.destroy();
    return ;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    ft_size_t bounded_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    bounded_size = static_cast<ft_size_t>(size);
    if (bounded_size > 512U)
        bounded_size = 512U;
    networking_state_machine_fuzz_run(data, bounded_size);
    return (0);
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t *data, size_t size,
    size_t maximum_size, unsigned int seed)
{
    return (networking_structured_fuzz_mutate(data, size, maximum_size,
        seed, 2048U));
}
