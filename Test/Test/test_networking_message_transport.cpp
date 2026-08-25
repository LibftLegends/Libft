#include "../test_internal.hpp"
#include "../../Modules/Networking/message_transport.hpp"
#include "../../Modules/Networking/networking_secure_channel.hpp"
#include "../../Modules/Networking/networking_handshake.hpp"
#include "../../Modules/Networking/networking_simulator.hpp"
#include "../../Modules/Networking/networking_nat_traversal.hpp"
#include "networking_nat_test_support.hpp"
#include "../../Modules/Observability/observability_networking_metrics.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_cma_failure_injection.hpp"
#include "crypto_test_hooks.hpp"
#include "networking_test_hooks.hpp"
#include "networking_test_support.hpp"

static const ft_size_t NETWORKING_MEMORY_DATAGRAM_CAPACITY = 2048U;

static ft_bool networking_test_bytes_differ(const uint8_t *left,
    const uint8_t *right, ft_size_t length) noexcept
{
    ft_size_t index = 0U;

    while (index < length)
    {
        if (left[index] != right[index])
            return (FT_TRUE);
        index += 1U;
    }
    return (FT_FALSE);
}

static uint32_t g_networking_observability_export_count = 0U;
static ft_networking_observability_sample g_networking_observability_sample;

static void networking_observability_test_exporter(
    const ft_networking_observability_sample &sample)
{
    g_networking_observability_export_count += 1U;
    g_networking_observability_sample = sample;
    return ;
}

struct networking_memory_datagram
{
    networking_message_endpoint source;
    uint8_t payload[NETWORKING_MEMORY_DATAGRAM_CAPACITY];
    ft_size_t size;

    networking_memory_datagram() noexcept
        : source(), payload(), size(0U)
    {
        ft_memset(&this->source, 0, sizeof(this->source));
        ft_memset(this->payload, 0, sizeof(this->payload));
        return ;
    }
};

class networking_memory_io : public networking_datagram_io
{
    private:
        networking_memory_io *_peer;
        uint64_t _now;
        ft_deque<networking_memory_datagram> _incoming;
        ft_bool _drop_next;
        ft_bool _use_source;
        networking_message_endpoint _source;

    public:
        networking_memory_io() noexcept
            : _peer(ft_nullptr), _now(0U), _incoming(), _drop_next(FT_FALSE),
              _use_source(FT_FALSE), _source()
        {
            (void)this->_incoming.initialize();
            ft_memset(&this->_source, 0, sizeof(this->_source));
            return ;
        }

        ~networking_memory_io() noexcept
        {
            (void)this->_incoming.destroy();
            return ;
        }

        void connect(networking_memory_io &peer) noexcept
        {
            this->_peer = &peer;
            return ;
        }

        void advance(uint64_t milliseconds) noexcept
        {
            this->_now += milliseconds;
            return ;
        }

        void drop_next_datagram() noexcept
        {
            this->_drop_next = FT_TRUE;
            return ;
        }

        int32_t inject_datagram(const networking_message_endpoint &source,
            const uint8_t *data, ft_size_t size) noexcept
        {
            networking_memory_datagram datagram;

            if (data == ft_nullptr && size != 0U)
                return (FT_ERR_INVALID_POINTER);
            if (size > NETWORKING_MEMORY_DATAGRAM_CAPACITY)
                return (FT_ERR_OUT_OF_RANGE);
            datagram.source = source;
            datagram.size = size;
            if (size != 0U)
                ft_memcpy(datagram.payload, data, size);
            this->_incoming.push_back(datagram);
            return (FT_ERR_SUCCESS);
        }

        void set_source(const networking_message_endpoint &source) noexcept
        {
            this->_source = source;
            this->_use_source = FT_TRUE;
            return ;
        }

        int32_t send_datagram(const networking_message_endpoint &destination,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            networking_memory_datagram datagram;

            (void)destination;
            if (this->_peer == ft_nullptr || (data == ft_nullptr && size != 0U))
                return (FT_ERR_SOCKET_SEND_FAILED);
            if (size > NETWORKING_MEMORY_DATAGRAM_CAPACITY)
                return (FT_ERR_OUT_OF_RANGE);
            if (this->_drop_next != FT_FALSE)
            {
                this->_drop_next = FT_FALSE;
                return (FT_ERR_SUCCESS);
            }
            if (this->_use_source != FT_FALSE)
                datagram.source = this->_source;
            else
                datagram.source = destination;
            datagram.size = size;
            if (size != 0U)
                ft_memcpy(datagram.payload, data, size);
            this->_peer->_incoming.push_back(datagram);
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_datagram(networking_message_endpoint &source,
            uint8_t *data, ft_size_t capacity, ft_size_t *received_size) noexcept override
        {
            networking_memory_datagram datagram;

            if (received_size == ft_nullptr || data == ft_nullptr)
                return (FT_ERR_INVALID_POINTER);
            *received_size = 0U;
            if (this->_incoming.empty())
                return (FT_ERR_EMPTY);
            datagram = this->_incoming.front();
            this->_incoming.pop_front();
            if (datagram.size > capacity)
                return (FT_ERR_OUT_OF_RANGE);
            source = datagram.source;
            if (datagram.size != 0U)
                ft_memcpy(data, datagram.payload, datagram.size);
            *received_size = datagram.size;
            return (FT_ERR_SUCCESS);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (this->_now);
        }
};

static void networking_message_prepare_endpoint(networking_message_endpoint &endpoint) noexcept
{
    ft_memset(&endpoint, 0, sizeof(endpoint));
    endpoint.length = 1U;
    return ;
}

FT_TEST(test_networking_exports_aggregate_observability)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;

    (void)observability_networking_metrics_shutdown();
    (void)observability_networking_metrics_disable_thread_safety();
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_networking_metrics_enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_networking_metrics_initialize(
        networking_observability_test_exporter));
    g_networking_observability_export_count = 0U;
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.export_observability());
    FT_ASSERT_EQ(1U, g_networking_observability_export_count);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0U),
        g_networking_observability_sample.request_bytes);
    FT_ASSERT_EQ(static_cast<ft_size_t>(0U),
        g_networking_observability_sample.response_bytes);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, observability_networking_metrics_shutdown());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        observability_networking_metrics_disable_thread_safety());
    return (1);
}

struct networking_test_callback_state
{
    networking_message_transport *transport;
    uint32_t calls;
    ft_bool observed_statistics;

    networking_test_callback_state() noexcept
        : transport(ft_nullptr), calls(0U), observed_statistics(FT_FALSE)
    {
        return ;
    }
};

static void networking_test_event_callback(
    const networking_message_event &event, void *user_data) noexcept
{
    networking_test_callback_state *state;
    networking_message_statistics statistics;

    state = static_cast<networking_test_callback_state *>(user_data);
    if (state == ft_nullptr)
        return ;
    state->calls += 1U;
    if (state->transport != ft_nullptr
        && state->transport->get_statistics(event.connection_id,
            statistics) == FT_ERR_SUCCESS)
        state->observed_statistics = FT_TRUE;
    return ;
}

FT_TEST(test_networking_message_reliable_fragmented_roundtrip)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    ft_size_t received_count;
    uint8_t payload[700U] = {0U};
    uint32_t index;
    int32_t poll_index;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.maximum_datagram_size = 128U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint, server_connection));
    index = 0U;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<uint8_t>(index & 0xffU);
        index += 1U;
    }
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 4U;
    options.lane = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload), options));
    poll_index = 0;
    while (poll_index < 32)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        client_io.advance(1U);
        server_io.advance(1U);
        poll_index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(&received, 1U,
        received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(static_cast<ft_size_t>(700U), received.payload.size());
    FT_ASSERT_EQ(4U, received.channel);
    FT_ASSERT_EQ(static_cast<uint8_t>(1U), received.lane);
    FT_ASSERT_EQ(static_cast<int32_t>(payload[699U]), static_cast<int32_t>(received.payload[699U]));
    FT_ASSERT_EQ(FT_ERR_EMPTY, server.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_unreliable_and_statistics)
{
    networking_memory_io first_io;
    networking_memory_io second_io;
    networking_message_transport_config configuration;
    networking_message_transport first;
    networking_message_transport second;
    networking_message_endpoint endpoint;
    networking_message_connection first_connection;
    networking_message_connection second_connection;
    networking_message_send_options options;
    networking_received_message received;
    networking_message_event event;
    networking_message_statistics statistics;
    const char *text;

    first_io.connect(second_io);
    second_io.connect(first_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    text = "snapshot";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize(configuration, first_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize(configuration, second_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.open_connection(endpoint, first_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.open_connection(endpoint, second_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    options.delivery = networking_message_delivery::UNRELIABLE_SEQUENCED;
    options.channel = 8U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_connection.send_message(text, 8U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::MESSAGE_AVAILABLE, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.receive_message(received));
    FT_ASSERT_EQ(static_cast<ft_size_t>(8U), received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_connection.get_statistics(statistics));
    FT_ASSERT(statistics.messages_sent >= 1U);
    FT_ASSERT(statistics.packets_sent >= 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_networking_message_ack_ranges_acknowledge_gaps)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_message_statistics statistics;
    const char first[] = "one";
    const char second[] = "two";
    const char third[] = "three";

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(first,
        sizeof(first) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_ACK_RANGE_GROWTH));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_ACK_RANGE_GROWTH));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    client_io.drop_next_datagram();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(second,
        sizeof(second) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(third,
        sizeof(third) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.get_statistics(statistics));
    FT_ASSERT(statistics.packets_acknowledged >= 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_uses_custom_crypto_backend)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    const char *text;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    text = "custom backend";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint, server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(text, 14U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(14U, received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('c'), static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.update_key_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_connection.update_key_epoch(1U));
    text = "epoch one";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(text, 9U,
        options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(9U, received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_authenticated_key_update_request)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_event event;
    networking_received_message received;
    networking_message_send_options options;
    const char payload[] = "rotated";

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_connection(event.connection_id,
        server_connection));
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        server_connection.get_state());
    server_io.drop_next_datagram();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.request_key_update(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    client_io.advance(configuration.retransmission_timeout_milliseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_connection.request_key_update(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload) - 1U, received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_authenticated_handshake_blocks_early_data)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_received_message received;
    networking_message_send_options options;
    networking_message_event event;
    networking_message_peer_identity identity;
    const char payload[] = "handshake";

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, client_connection));
    FT_ASSERT_EQ(networking_message_connection_state::HANDSHAKING,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, client_connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.get_remote_identity(identity));
    FT_ASSERT_EQ(FT_TRUE, identity.authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(payload) - 1U, received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('h'), static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_previous_receive_key_expiry)
{
    networking_secure_channel channel;
    const uint8_t key[32] = {0U};
    const uint8_t initialization_vector[12] = {0U};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.initialize_directional(key, key,
        initialization_vector, initialization_vector));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.update_receive_key_epoch(1U));
    FT_ASSERT_EQ(FT_TRUE, channel.has_receive_epoch(0U));
    FT_ASSERT_EQ(FT_TRUE, channel.has_receive_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.clear_previous_receive_key());
    FT_ASSERT_EQ(FT_FALSE, channel.has_receive_epoch(0U));
    FT_ASSERT_EQ(FT_TRUE, channel.has_receive_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, channel.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_pins_authenticated_peer_key)
{
    networking_handshake preview_client;
    networking_handshake preview_server;
    networking_message_transport_config client_configuration;
    networking_message_transport_config server_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_event event;
    ft_vector<uint8_t> preview_hello;
    uint8_t pinned_key[32];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x13579bdfU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_client.initialize(
        networking_handshake_role::CLIENT, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_server.initialize(
        networking_handshake_role::SERVER, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_server.get_local_hello(preview_hello));
    FT_ASSERT_EQ(80U, preview_hello.size());
    ft_memcpy(pinned_key, &preview_hello[48], sizeof(pinned_key));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, preview_hello.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x13579bdfU));

    client_configuration.enable_authenticated_handshake = FT_TRUE;
    client_configuration.enable_peer_key_pinning = FT_TRUE;
    ft_memcpy(client_configuration.pinned_peer_public_key, pinned_key,
        sizeof(pinned_key));
    server_configuration.enable_authenticated_handshake = FT_TRUE;
    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(client_configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(server_configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}

FT_TEST(test_networking_message_transport_rejects_wrong_pinned_peer_key)
{
    networking_message_transport_config client_configuration;
    networking_message_transport_config server_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_event event;
    uint8_t wrong_key[32];
    uint32_t index;

    index = 0U;
    while (index < sizeof(wrong_key))
    {
        wrong_key[index] = static_cast<uint8_t>(index + 1U);
        index += 1U;
    }
    client_configuration.enable_authenticated_handshake = FT_TRUE;
    client_configuration.enable_peer_key_pinning = FT_TRUE;
    ft_memcpy(client_configuration.pinned_peer_public_key, wrong_key,
        sizeof(wrong_key));
    server_configuration.enable_authenticated_handshake = FT_TRUE;
    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(client_configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(server_configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::FAILED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_retry_cookie_is_stateless_until_verified)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_event event;
    const uint8_t retry_secret[32] = {0x01U, 0x02U, 0x03U, 0x04U,
        0x05U, 0x06U, 0x07U, 0x08U, 0x09U, 0x0aU, 0x0bU, 0x0cU,
        0x0dU, 0x0eU, 0x0fU, 0x10U, 0x11U, 0x12U, 0x13U, 0x14U,
        0x15U, 0x16U, 0x17U, 0x18U, 0x19U, 0x1aU, 0x1bU, 0x1cU,
        0x1dU, 0x1eU, 0x1fU, 0x20U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    configuration.enable_retry_cookies = FT_TRUE;
    ft_memcpy(configuration.retry_cookie_secret, retry_secret,
        sizeof(retry_secret));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(networking_message_connection_state::HANDSHAKING,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(networking_message_connection_state::HANDSHAKING,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::HANDSHAKING,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_retransmits_duplicate_handshake_safely)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_event event;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    client_io.advance(configuration.retransmission_timeout_milliseconds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_worker_can_sleep_and_stop)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;

    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT_EQ(FT_TRUE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_worker_owns_mutating_commands)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    networking_message_statistics statistics;
    const char payload[] = "worker";

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint, connection));
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        connection.get_state());
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 3U;
    options.lane = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.configure_lane(1U, 12U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.get_statistics(statistics));
    FT_ASSERT_EQ(1U, statistics.messages_sent);
    FT_ASSERT_EQ(1U, statistics.lane_messages_sent[1U]);
    FT_ASSERT_EQ(sizeof(payload) - 1U,
        statistics.lane_bytes_sent[1U]);
    FT_ASSERT_EQ(12U, statistics.lane_priority_weight[1U]);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, transport.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_validates_and_migrates_paths)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_endpoint alternate_endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_message_event event;
    networking_message_statistics statistics;
    const char payload[] = "path";

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    alternate_endpoint = endpoint;
    reinterpret_cast<uint8_t *>(&alternate_endpoint.address)[0] = 0x7fU;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    while (server.receive_event(event) == FT_ERR_SUCCESS)
    {
    }
    client_io.set_source(alternate_endpoint);
    options.delivery = networking_message_delivery::UNRELIABLE;
    options.channel = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::PATH_CHANGED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_connection.get_statistics(statistics));
    FT_ASSERT_EQ(1U, statistics.path_migrations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_advertises_receive_flow_credit)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config client_configuration;
    networking_message_transport_config server_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    uint8_t payload[64] = {0U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    client_configuration.enable_encryption = FT_FALSE;
    server_configuration.enable_encryption = FT_FALSE;
    server_configuration.maximum_reassembly_bytes = 32U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(client_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(server_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 1U;
    FT_ASSERT_EQ(FT_ERR_FULL, client_connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_named_failure_hook_is_semantic)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    const char payload[] = "failure";

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_OUTGOING_FRAME_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_named_failure_hooks_cover_worker_command_simulator_nat)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_simulated_datagram_io simulator;
    networking_simulator_config simulator_configuration;
    networking_nat_traversal traversal;
    networking_nat_candidate candidate;

    networking_message_prepare_endpoint(endpoint);
    candidate.endpoint = endpoint;
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_WORKER_CREATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, transport.start_worker());
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_WORKER_CREATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_COMMAND_ENQUEUE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, transport.open_connection(endpoint,
        connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(io,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_SIMULATOR_QUEUE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, simulator.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_DROP, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_NAT_CANDIDATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, traversal.add_local_candidate(candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_optional_thread_safety_lifecycle)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;

    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    FT_ASSERT_EQ(FT_FALSE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_TRUE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT_EQ(FT_TRUE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_FALSE, transport.is_worker_running());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_event_callback_is_deferred)
{
    networking_memory_io io;
    networking_memory_io peer;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_test_callback_state callback_state;

    io.connect(peer);
    peer.connect(io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    callback_state.transport = &transport;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.set_event_callback(
        &networking_test_event_callback, &callback_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    FT_ASSERT_EQ(0U, callback_state.calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.poll());
    FT_ASSERT(callback_state.calls >= 1U);
    FT_ASSERT_EQ(FT_TRUE, callback_state.observed_statistics);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.set_event_callback(ft_nullptr,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_worker_callbacks_are_owner_dispatched)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_test_callback_state callback_state;

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    callback_state.transport = &transport;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.set_event_callback(
        &networking_test_event_callback, &callback_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.start_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    FT_ASSERT_EQ(0U, callback_state.calls);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.dispatch_callbacks());
    FT_ASSERT(callback_state.calls >= 1U);
    FT_ASSERT_EQ(FT_TRUE, callback_state.observed_statistics);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.stop_worker());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_named_mutex_failure_is_semantic)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;

    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_MUTEX_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, transport.enable_thread_safety());
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_MUTEX_ALLOCATE));
    FT_ASSERT_EQ(FT_FALSE, transport.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_allocation_failure_is_transactional)
{
    test_cma_failure_controller controller;
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    int32_t result;

    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    result = transport.open_connection(endpoint, connection);
    FT_ASSERT(result != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    return (1);
}

FT_TEST(test_networking_message_transport_lane_limits_and_flush)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    ft_size_t received_count;
    const char payload[] = "flush";

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.configure_lane(2U, 4U, 0U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        client_connection.configure_lane(4U, 1U, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.set_queue_limits(1024U));
    options.lane = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(payload,
        sizeof(payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.flush());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(&received, 1U,
        received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(sizeof(payload) - 1U, received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_close_frame_and_peer_events)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_event event;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.close(
        networking_message_close_reason::PROTOCOL_ERROR, "test close"));
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::PEER_CLOSING, event.type);
    FT_ASSERT_EQ(static_cast<int32_t>(
        networking_message_close_reason::PROTOCOL_ERROR), event.reason);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CLOSED, event.type);
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        server_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_message_transport_graceful_close_drains_reliable_data)
{
    networking_memory_io client_io;
    networking_memory_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    networking_message_event event;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(
        "drain", 5U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.close(
        networking_message_close_reason::APPLICATION, "draining"));
    FT_ASSERT_EQ(networking_message_connection_state::DRAINING,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(5U, received.payload.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::MESSAGE_AVAILABLE,
        event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::PEER_CLOSING, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CLOSED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_authentication_and_replay)
{
    networking_secure_channel sender;
    networking_secure_channel receiver;
    const uint8_t key[32] = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
        0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU, 0x10U,
        0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U,
        0x19U, 0x1aU, 0x1bU, 0x1cU, 0x1dU, 0x1eU, 0x1fU, 0x20U};
    const uint8_t initialization_vector[12] = {0x11U, 0x12U, 0x13U, 0x14U,
        0x15U, 0x16U, 0x17U, 0x18U, 0x19U, 0x1aU, 0x1bU, 0x1cU};
    const uint8_t associated_data[3] = {0xa1U, 0xa2U, 0xa3U};
    const uint8_t plaintext[] = {'s', 'e', 'c', 'u', 'r', 'e'};
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> decrypted;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize(key, sizeof(key),
        initialization_vector, sizeof(initialization_vector)));
    FT_ASSERT(sender.seal(7U, associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext), ciphertext, authentication_tag));
    FT_ASSERT(receiver.open(7U, associated_data, sizeof(associated_data), &ciphertext[0],
        ciphertext.size(), authentication_tag, decrypted));
    FT_ASSERT_EQ(sizeof(plaintext), decrypted.size());
    authentication_tag[0] = static_cast<uint8_t>(authentication_tag[0] ^ 1U);
    FT_ASSERT(!receiver.open(7U, associated_data, sizeof(associated_data), &ciphertext[0],
        ciphertext.size(), authentication_tag, decrypted));
    FT_ASSERT(receiver.is_replay(7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.update_key_epoch(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.update_key_epoch(1U));
    FT_ASSERT_EQ(1U, sender.get_key_epoch());
    FT_ASSERT_EQ(1U, receiver.get_key_epoch());
    FT_ASSERT(sender.seal(1U, associated_data, sizeof(associated_data), plaintext,
        sizeof(plaintext), ciphertext, authentication_tag));
    FT_ASSERT(receiver.open(1U, associated_data, sizeof(associated_data),
        &ciphertext[0], ciphertext.size(), authentication_tag, decrypted));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_secure_channel_directional_keys_and_nonce_reuse)
{
    networking_secure_channel sender;
    networking_secure_channel receiver;
    const uint8_t sender_key[32] = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
        0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU, 0x10U,
        0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U,
        0x19U, 0x1aU, 0x1bU, 0x1cU, 0x1dU, 0x1eU, 0x1fU, 0x20U};
    const uint8_t receiver_key[32] = {0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U,
        0x29U, 0x2aU, 0x2bU, 0x2cU, 0x2dU, 0x2eU, 0x2fU, 0x30U,
        0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U,
        0x39U, 0x3aU, 0x3bU, 0x3cU, 0x3dU, 0x3eU, 0x3fU, 0x40U};
    const uint8_t sender_initialization_vector[12] = {0x41U, 0x42U, 0x43U, 0x44U,
        0x45U, 0x46U, 0x47U, 0x48U, 0x49U, 0x4aU, 0x4bU, 0x4cU};
    const uint8_t receiver_initialization_vector[12] = {0x51U, 0x52U, 0x53U, 0x54U,
        0x55U, 0x56U, 0x57U, 0x58U, 0x59U, 0x5aU, 0x5bU, 0x5cU};
    const uint8_t plaintext[] = {'d', 'i', 'r', 'e', 'c', 't'};
    uint8_t authentication_tag[16];
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> decrypted;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.initialize_directional(sender_key, receiver_key,
        sender_initialization_vector, receiver_initialization_vector));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.initialize_directional(receiver_key, sender_key,
        receiver_initialization_vector, sender_initialization_vector));
    FT_ASSERT(sender.seal(1U, ft_nullptr, 0U, plaintext, sizeof(plaintext),
        ciphertext, authentication_tag));
    FT_ASSERT(!sender.seal(1U, ft_nullptr, 0U, plaintext, sizeof(plaintext),
        ciphertext, authentication_tag));
    FT_ASSERT(receiver.open(1U, ft_nullptr, 0U, &ciphertext[0], ciphertext.size(),
        authentication_tag, decrypted));
    FT_ASSERT_EQ(sizeof(plaintext), decrypted.size());
    FT_ASSERT(receiver.seal(2U, ft_nullptr, 0U, plaintext, sizeof(plaintext),
        ciphertext, authentication_tag));
    FT_ASSERT(sender.open(2U, ft_nullptr, 0U, &ciphertext[0], ciphertext.size(),
        authentication_tag, decrypted));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sender.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receiver.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_transcript_finished_and_retry_cookie)
{
    networking_handshake client;
    networking_handshake server;
    networking_message_endpoint endpoint;
    networking_message_endpoint other_endpoint;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> server_hello;
    const uint8_t secret[32] = {0x01U, 0x02U, 0x03U, 0x04U, 0x05U, 0x06U, 0x07U, 0x08U,
        0x09U, 0x0aU, 0x0bU, 0x0cU, 0x0dU, 0x0eU, 0x0fU, 0x10U,
        0x11U, 0x12U, 0x13U, 0x14U, 0x15U, 0x16U, 0x17U, 0x18U,
        0x19U, 0x1aU, 0x1bU, 0x1cU, 0x1dU, 0x1eU, 0x1fU, 0x20U};
    const uint8_t hello_digest[32] = {0x21U, 0x22U, 0x23U, 0x24U, 0x25U, 0x26U, 0x27U, 0x28U,
        0x29U, 0x2aU, 0x2bU, 0x2cU, 0x2dU, 0x2eU, 0x2fU, 0x30U,
        0x31U, 0x32U, 0x33U, 0x34U, 0x35U, 0x36U, 0x37U, 0x38U,
        0x39U, 0x3aU, 0x3bU, 0x3cU, 0x3dU, 0x3eU, 0x3fU, 0x40U};
    uint8_t client_send_key[32];
    uint8_t client_receive_key[32];
    uint8_t client_send_iv[12];
    uint8_t client_receive_iv[12];
    uint8_t server_send_key[32];
    uint8_t server_receive_key[32];
    uint8_t server_send_iv[12];
    uint8_t server_receive_iv[12];
    uint8_t client_finished[32];
    uint8_t server_finished[32];
    uint8_t cookie[40];

    networking_message_prepare_endpoint(endpoint);
    networking_message_prepare_endpoint(other_endpoint);
    other_endpoint.address = endpoint.address;
    other_endpoint.length = endpoint.length;
    reinterpret_cast<uint8_t *>(&other_endpoint.address)[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(networking_handshake_role::CLIENT, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(networking_handshake_role::SERVER, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(client_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(&server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept_peer_hello(&client_hello[0], client_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_traffic_keys(client_send_key,
        client_receive_key, client_send_iv, client_receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_traffic_keys(server_send_key,
        server_receive_key, server_send_iv, server_receive_iv));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_key, server_receive_key, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_key, server_send_key, 32U));
    FT_ASSERT_EQ(0, ft_memcmp(client_send_iv, server_receive_iv, 12U));
    FT_ASSERT_EQ(0, ft_memcmp(client_receive_iv, server_send_iv, 12U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.create_finished(client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.verify_finished(client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.create_finished(server_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.verify_finished(server_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_handshake::create_retry_cookie(secret,
        endpoint, hello_digest, 100U, cookie));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_handshake::verify_retry_cookie(secret,
        endpoint, hello_digest, 150U, 100U, cookie));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, networking_handshake::verify_retry_cookie(
        secret, other_endpoint, hello_digest, 150U, 100U, cookie));
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, networking_handshake::verify_retry_cookie(
        secret, endpoint, hello_digest, 201U, 100U, cookie));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_state_failure_is_transactional)
{
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> server_hello;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(
        networking_handshake_role::CLIENT, 11U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(
        networking_handshake_role::SERVER, 12U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(client_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(&server_hello[0],
        server_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept_peer_hello(&client_hello[0],
        client_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_HANDSHAKE_STATE));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, client.derive_keys());
    FT_ASSERT_EQ(networking_handshake_state::FAILED, client.get_state());
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_HANDSHAKE_STATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

class networking_test_probe : public networking_nat_probe_io
{
    public:
        uint32_t calls;

        networking_test_probe() noexcept : calls(0U)
        {
            return ;
        }

        ~networking_test_probe() noexcept
        {
            return ;
        }

        int32_t send_probe(const networking_message_endpoint &local,
            const networking_message_endpoint &remote, uint64_t attempt_id) noexcept override
        {
            (void)local;
            (void)remote;
            (void)attempt_id;
            this->calls += 1U;
            return (FT_ERR_SUCCESS);
        }
};

class networking_test_candidate_provider : public networking_nat_candidate_provider
{
    public:
        networking_message_endpoint endpoint;

        networking_test_candidate_provider() noexcept : endpoint()
        {
            networking_message_prepare_endpoint(this->endpoint);
            return ;
        }

        ~networking_test_candidate_provider() noexcept
        {
            return ;
        }

        int32_t gather(ft_vector<networking_nat_candidate> &candidates) noexcept override
        {
            networking_nat_candidate first;
            networking_nat_candidate second;

            first.endpoint = this->endpoint;
            first.type = networking_nat_candidate_type::HOST;
            first.priority = 1000U;
            second.endpoint = this->endpoint;
            second.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
            second.priority = 900U;
            if (candidates.push_back(first) != FT_ERR_SUCCESS
                || candidates.push_back(second) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
            return (FT_ERR_SUCCESS);
        }
};

class networking_test_relay : public networking_nat_relay_io
{
    public:
        uint32_t opens;
        uint64_t current_time;

        networking_test_relay() noexcept : opens(0U), current_time(42U)
        {
            return ;
        }

        ~networking_test_relay() noexcept
        {
            return ;
        }

        int32_t open_relay(uint64_t local_peer_id, uint64_t remote_peer_id,
            uint64_t attempt_id, networking_message_endpoint &relay_endpoint) noexcept override
        {
            (void)local_peer_id;
            (void)remote_peer_id;
            (void)attempt_id;
            networking_message_prepare_endpoint(relay_endpoint);
            this->opens += 1U;
            return (FT_ERR_SUCCESS);
        }

        int32_t send_relay(const networking_message_endpoint &relay_endpoint,
            const uint8_t *data, ft_size_t size) noexcept override
        {
            (void)relay_endpoint;
            (void)data;
            (void)size;
            return (FT_ERR_SUCCESS);
        }

        int32_t receive_relay(const networking_message_endpoint &relay_endpoint,
            uint8_t *data, ft_size_t capacity,
            ft_size_t *received_size) noexcept override
        {
            (void)relay_endpoint;
            (void)data;
            (void)capacity;
            if (received_size != ft_nullptr)
                *received_size = 0U;
            return (FT_ERR_EMPTY);
        }

        uint64_t now_milliseconds() const noexcept override
        {
            return (this->current_time);
        }

        int32_t close_relay(const networking_message_endpoint &relay_endpoint) noexcept override
        {
            (void)relay_endpoint;
            return (FT_ERR_SUCCESS);
        }
};

class networking_test_peer_ticket_verifier
    : public networking_message_peer_ticket_verifier
{
    public:
        networking_test_peer_ticket_verifier() noexcept
        {
            return ;
        }

        ~networking_test_peer_ticket_verifier() noexcept
        {
            return ;
        }

        ft_bool verify(const networking_message_peer_connect_ticket &ticket)
            noexcept override
        {
            if (ticket.peer_id == 2U && ticket.attempt_id == 3U)
                return (FT_TRUE);
            return (FT_FALSE);
    }
};

class networking_test_nat_ticket_verifier : public networking_nat_ticket_verifier
{
    public:
        networking_test_nat_ticket_verifier() noexcept
        {
            return ;
        }

        ~networking_test_nat_ticket_verifier() noexcept
        {
            return ;
        }

        ft_bool verify(const networking_nat_peer_ticket &ticket) noexcept override
        {
            if (ticket.signature.empty() == FT_FALSE)
                return (FT_TRUE);
            return (FT_FALSE);
        }
};

FT_TEST(test_networking_message_transport_connect_peer_requires_ticket_verifier)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_peer_connect_ticket ticket;
    networking_test_peer_ticket_verifier verifier;
    networking_message_connection connection;
    networking_message_endpoint endpoint;

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    ticket.peer_id = 2U;
    ticket.attempt_id = 3U;
    ticket.expires_at = 1000U;
    ticket.candidate_count = 1U;
    ticket.candidates[0] = endpoint;
    ticket.signature_length = 1U;
    ticket.signature[0] = 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE, transport.connect_peer(ticket,
        connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.connect_peer(ticket, verifier,
        connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_is_seeded_and_delays_datagrams)
{
    networking_memory_io first_base;
    networking_memory_io second_base;
    networking_simulated_datagram_io first_simulator;
    networking_simulator_config simulator_configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[3] = {1U, 2U, 3U};
    uint8_t output[3] = {0U, 0U, 0U};
    ft_size_t received_size;

    first_base.connect(second_base);
    second_base.connect(first_base);
    networking_message_prepare_endpoint(endpoint);
    simulator_configuration.fixed_latency_milliseconds = 10U;
    simulator_configuration.random_seed = 42U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_simulator.initialize(first_base, simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_simulator.send_datagram(endpoint, input, sizeof(input)));
    received_size = 0U;
    FT_ASSERT_EQ(FT_ERR_EMPTY, second_base.receive_datagram(source, output, sizeof(output), &received_size));
    first_simulator.advance(9U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, first_simulator.receive_datagram(source, output, sizeof(output), &received_size));
    first_simulator.advance(1U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, first_simulator.receive_datagram(source, output, sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_base.receive_datagram(source, output, sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3U), received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_scripted_packet_outcomes)
{
    networking_memory_io base;
    networking_memory_io peer;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[2] = {4U, 5U};
    uint8_t output[2] = {0U, 0U};
    ft_size_t received_size;

    base.connect(peer);
    peer.connect(base);
    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(base, configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_DROP, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(2U,
        networking_simulator_script_action::SCRIPT_DELAY, 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    received_size = 0U;
    FT_ASSERT_EQ(FT_ERR_EMPTY, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    simulator.advance(4U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    simulator.advance(1U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2U), received_size);
    FT_ASSERT_EQ(static_cast<int32_t>(4U), static_cast<int32_t>(output[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_corruption_duplication_and_mtu)
{
    networking_memory_io base;
    networking_memory_io peer;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[3] = {8U, 9U, 10U};
    uint8_t output[3] = {0U, 0U, 0U};
    ft_size_t received_size;

    base.connect(peer);
    peer.connect(base);
    networking_message_prepare_endpoint(endpoint);
    configuration.random_seed = 11U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(base, configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_CORRUPT, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    simulator.advance(0U);
    (void)simulator.receive_datagram(source, output, sizeof(output),
        &received_size);
    received_size = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(sizeof(input), received_size);
    FT_ASSERT(networking_test_bytes_differ(output, input, sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.clear_script());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(2U,
        networking_simulator_script_action::SCRIPT_DUPLICATE, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    simulator.advance(1U);
    (void)simulator.receive_datagram(source, output, sizeof(output),
        &received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.clear_script());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(3U,
        networking_simulator_script_action::SCRIPT_MTU_DROP, 0U));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_rejects_oversized_datagrams)
{
    networking_memory_io base;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    uint8_t input[3] = {8U, 9U, 10U};

    networking_message_prepare_endpoint(endpoint);
    configuration.maximum_datagram_size = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(base, configuration));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_reorders_and_limits_traffic)
{
    networking_memory_io first_base;
    networking_memory_io second_base;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t first_input[1] = {1U};
    uint8_t second_input[1] = {2U};
    uint8_t output[1] = {0U};
    ft_size_t received_size;

    first_base.connect(second_base);
    second_base.connect(first_base);
    networking_message_prepare_endpoint(endpoint);
    configuration.fixed_latency_milliseconds = 10U;
    configuration.reorder_parts_per_million = 500000U;
    configuration.random_seed = 3U;
    configuration.maximum_pending_datagrams = 2U;
    configuration.bandwidth_bytes_per_second = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(first_base, configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, first_input,
        sizeof(first_input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, second_input,
        sizeof(second_input)));
    FT_ASSERT_EQ(FT_ERR_FULL, simulator.send_datagram(endpoint, first_input,
        sizeof(first_input)));
    simulator.advance(10U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_base.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<int32_t>(2U), static_cast<int32_t>(output[0]));
    simulator.advance(11U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_base.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<int32_t>(1U), static_cast<int32_t>(output[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_message_reliable_delivery_survives_simulated_drop)
{
    networking_memory_io client_base;
    networking_memory_io server_base;
    networking_simulated_datagram_io client_io;
    networking_simulated_datagram_io server_io;
    networking_simulator_config simulator_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    ft_size_t received_count;
    int32_t index;

    client_base.connect(server_base);
    server_base.connect(client_base);
    networking_message_prepare_endpoint(endpoint);
    simulator_configuration.fixed_latency_milliseconds = 2U;
    simulator_configuration.random_seed = 7U;
    transport_configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.initialize(client_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.initialize(server_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_DROP, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(transport_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(transport_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message("simulated",
        9U, options));
    index = 0;
    while (index < 80)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        client_io.advance(10U);
        server_io.advance(10U);
        index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(&received, 1U,
        received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(static_cast<ft_size_t>(9U), received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('s'),
        static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    return (1);
}

FT_TEST(test_networking_message_reliable_delivery_under_seeded_impairment)
{
    networking_memory_io client_base;
    networking_memory_io server_base;
    networking_simulated_datagram_io client_io;
    networking_simulated_datagram_io server_io;
    networking_simulator_config simulator_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    const char first_payload[] = "first";
    const char second_payload[] = "second";
    const char third_payload[] = "third";
    ft_size_t received_count;
    int32_t index;

    client_base.connect(server_base);
    server_base.connect(client_base);
    networking_message_prepare_endpoint(endpoint);
    simulator_configuration.fixed_latency_milliseconds = 1U;
    simulator_configuration.loss_parts_per_million = 300000U;
    simulator_configuration.duplicate_parts_per_million = 100000U;
    simulator_configuration.reorder_parts_per_million = 300000U;
    simulator_configuration.random_seed = 123U;
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.retransmission_timeout_milliseconds = 20U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.initialize(client_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.initialize(server_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(transport_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(transport_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(first_payload,
        sizeof(first_payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(second_payload,
        sizeof(second_payload) - 1U, options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(third_payload,
        sizeof(third_payload) - 1U, options));
    index = 0;
    while (index < 300)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        client_io.advance(10U);
        server_io.advance(10U);
        index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_messages(&received, 1U,
        received_count));
    FT_ASSERT_EQ(1U, received_count);
    FT_ASSERT_EQ(sizeof(first_payload) - 1U,
        received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('f'),
        static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(second_payload) - 1U,
        received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('s'),
        static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
    FT_ASSERT_EQ(sizeof(third_payload) - 1U,
        received.payload.size());
    FT_ASSERT_EQ(static_cast<int32_t>('t'),
        static_cast<int32_t>(received.payload[0]));
    FT_ASSERT_EQ(FT_ERR_EMPTY, server.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    return (1);
}

FT_TEST(test_networking_message_reliable_model_preserves_many_messages)
{
    networking_memory_io client_base;
    networking_memory_io server_base;
    networking_simulated_datagram_io client_io;
    networking_simulated_datagram_io server_io;
    networking_simulator_config simulator_configuration;
    networking_message_transport_config transport_configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_send_options options;
    networking_received_message received;
    networking_message_statistics statistics;
    uint8_t payloads[16U][12U];
    uint32_t payload_index;
    uint32_t byte_index;
    uint32_t poll_index;

    client_base.connect(server_base);
    server_base.connect(client_base);
    networking_message_prepare_endpoint(endpoint);
    simulator_configuration.fixed_latency_milliseconds = 1U;
    simulator_configuration.loss_parts_per_million = 300000U;
    simulator_configuration.duplicate_parts_per_million = 100000U;
    simulator_configuration.reorder_parts_per_million = 300000U;
    simulator_configuration.random_seed = 0x9e3779b9U;
    transport_configuration.enable_encryption = FT_FALSE;
    transport_configuration.retransmission_timeout_milliseconds = 10U;
    transport_configuration.acknowledgement_delay_milliseconds = 1U;
    transport_configuration.maximum_queued_bytes = 65536U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.initialize(client_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.initialize(server_base,
        simulator_configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(transport_configuration,
        client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(transport_configuration,
        server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 9U;
    payload_index = 0U;
    while (payload_index < 16U)
    {
        byte_index = 0U;
        while (byte_index < sizeof(payloads[payload_index]))
        {
            payloads[payload_index][byte_index] = static_cast<uint8_t>(
                payload_index * 17U + byte_index);
            byte_index += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.send_message(
            payloads[payload_index], sizeof(payloads[payload_index]), options));
        payload_index += 1U;
    }
    poll_index = 0U;
    while (poll_index < 1000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        client_io.advance(10U);
        server_io.advance(10U);
        poll_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_connection.get_statistics(statistics));
    FT_ASSERT_EQ(16U, statistics.messages_received);
    payload_index = 0U;
    while (payload_index < 16U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_message(received));
        FT_ASSERT_EQ(sizeof(payloads[payload_index]), received.payload.size());
        byte_index = 0U;
        while (byte_index < sizeof(payloads[payload_index]))
        {
            FT_ASSERT_EQ(static_cast<int32_t>(payloads[payload_index][byte_index]),
                static_cast<int32_t>(received.payload[byte_index]));
            byte_index += 1U;
        }
        payload_index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_EMPTY, server.receive_message(received));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    return (1);
}

FT_TEST(test_networking_malformed_datagrams_are_bounded)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    uint8_t corpus[128];
    uint32_t size;
    uint32_t index;

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    size = 0U;
    while (size <= sizeof(corpus))
    {
        index = 0U;
        while (index < sizeof(corpus))
        {
            corpus[index] = static_cast<uint8_t>((index * 37U + size) & 0xffU);
            index += 1U;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, io.inject_datagram(endpoint, corpus, size));
        (void)transport.poll();
        size += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.open_connection(endpoint,
        connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_deterministic_datagram_fuzz_corpus)
{
    networking_memory_io io;
    networking_message_transport_config configuration;
    networking_message_transport transport;
    networking_message_endpoint endpoint;
    uint8_t corpus[1536];
    uint32_t iteration;
    uint32_t size;
    uint32_t index;
    uint32_t state;

    networking_message_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.initialize(configuration, io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.listen(endpoint));
    state = 0x6d2b79f5U;
    iteration = 0U;
    while (iteration < 512U)
    {
        size = (iteration * 193U) % static_cast<uint32_t>(sizeof(corpus) + 1U);
        index = 0U;
        while (index < size)
        {
            state = state * 1664525U + 1013904223U;
            corpus[index] = static_cast<uint8_t>(state >> 24U);
            index += 1U;
        }
        if (size >= 4U && iteration % 3U == 0U)
        {
            corpus[0] = 0x4cU;
            corpus[1] = 1U;
            corpus[2] = static_cast<uint8_t>(iteration % 12U);
            corpus[3] = static_cast<uint8_t>(iteration >> 8U);
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, io.inject_datagram(endpoint, corpus, size));
        (void)transport.poll();
        iteration += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, transport.destroy());
    return (1);
}

FT_TEST(test_networking_nat_candidate_selection_and_relay_fallback)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_candidate;
    networking_nat_peer_ticket ticket;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_message_endpoint endpoint;
    networking_test_probe probe;
    networking_test_nat_ticket_verifier verifier;

    networking_message_prepare_endpoint(endpoint);
    local_candidate.endpoint = endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::RELAY;
    remote_candidate.priority = 900U;
    ticket.peer_id = 2U;
    ticket.attempt_id = 3U;
    ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.signature.push_back(1U);
    ticket.candidates.push_back(remote_candidate);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE,
        traversal.set_peer_ticket(ticket, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.get_selected_pair(selected_local, selected_remote));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(1U, probe.calls);
    FT_ASSERT(!traversal.needs_relay(0U));
    FT_ASSERT(!traversal.needs_relay(50U));
    FT_ASSERT(traversal.needs_relay(200U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.mark_probe_success(endpoint, endpoint,
        3U));
    FT_ASSERT(!traversal.needs_relay(200U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    return (1);
}

FT_TEST(test_networking_nat_rejects_invalid_ticket_without_partial_commit)
{
    networking_nat_traversal traversal;
    networking_nat_candidate local_candidate;
    networking_nat_candidate remote_candidate;
    networking_nat_candidate invalid_candidate;
    networking_nat_candidate selected_local;
    networking_nat_candidate selected_remote;
    networking_nat_peer_ticket valid_ticket;
    networking_nat_peer_ticket invalid_ticket;
    networking_message_endpoint endpoint;
    networking_test_nat_ticket_verifier verifier;

    networking_message_prepare_endpoint(endpoint);
    local_candidate.endpoint = endpoint;
    local_candidate.type = networking_nat_candidate_type::HOST;
    local_candidate.priority = 1000U;
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::HOST;
    remote_candidate.priority = 900U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_ticket.initialize());
    valid_ticket.peer_id = 2U;
    valid_ticket.attempt_id = 3U;
    valid_ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_ticket.candidates.push_back(remote_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_ticket.initialize());
    invalid_ticket.peer_id = 8U;
    invalid_ticket.attempt_id = 9U;
    invalid_ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_ticket.signature.push_back(2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_ticket.candidates.push_back(invalid_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.add_local_candidate(local_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(valid_ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        traversal.set_peer_ticket(invalid_ticket, 0U, verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.get_selected_pair(selected_local, selected_remote));
    FT_ASSERT_EQ(endpoint.length, selected_remote.endpoint.length);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, valid_ticket.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_gathers_and_probes_candidate_pairs_incrementally)
{
    networking_nat_traversal traversal;
    networking_test_candidate_provider provider;
    networking_nat_peer_ticket ticket;
    networking_nat_candidate remote_candidate;
    networking_test_probe probe;
    networking_test_relay relay;
    networking_message_endpoint endpoint;
    networking_test_nat_ticket_verifier verifier;

    networking_message_prepare_endpoint(endpoint);
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::RELAY;
    remote_candidate.priority = 800U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 9U;
    ticket.expires_at = 1000U;
    ticket.signature.push_back(1U);
    ticket.candidates.push_back(remote_candidate);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.gather_candidates(provider));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(1U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, traversal.probe_next(1U, 10U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.probe_next(11U, 10U, probe));
    FT_ASSERT_EQ(2U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, traversal.probe_next(21U, 10U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_RELAY_RECORD));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, traversal.fallback_to_relay(12U, relay));
    FT_ASSERT(!traversal.using_relay());
    FT_ASSERT_EQ(1U, networking_test_failure_attempt_count(
        NETWORKING_TEST_RELAY_RECORD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.fallback_to_relay(12U, relay));
    FT_ASSERT_EQ(1U, relay.opens);
    FT_ASSERT(traversal.using_relay());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_batches_probes_and_binds_success_to_attempt)
{
    networking_nat_traversal traversal;
    networking_test_candidate_provider provider;
    networking_test_probe probe;
    networking_nat_peer_ticket ticket;
    networking_nat_candidate remote_candidate;
    networking_message_endpoint endpoint;
    uint32_t sent_probes;
    networking_test_nat_ticket_verifier verifier;

    networking_message_prepare_endpoint(endpoint);
    remote_candidate.endpoint = endpoint;
    remote_candidate.type = networking_nat_candidate_type::SERVER_REFLEXIVE;
    remote_candidate.priority = 900U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.initialize());
    ticket.peer_id = 2U;
    ticket.attempt_id = 17U;
    ticket.expires_at = 1000U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.signature.push_back(1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.candidates.push_back(remote_candidate));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.initialize(1U, 100U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.gather_candidates(provider));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.set_peer_ticket(ticket, 0U,
        verifier));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.begin(1U, probe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.probe_batch(11U, 10U, 3U,
        probe, sent_probes));
    FT_ASSERT_EQ(3U, sent_probes);
    FT_ASSERT_EQ(4U, probe.calls);
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        traversal.mark_probe_success(endpoint, endpoint, 16U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        traversal.mark_probe_success(endpoint, endpoint, 17U));
    FT_ASSERT(traversal.has_direct_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, traversal.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ticket.destroy());
    return (1);
}

FT_TEST(test_networking_nat_relay_datagram_adapter_forwards_lifecycle)
{
    networking_test_relay relay;
    networking_nat_relay_datagram_io adapter;
    networking_message_endpoint endpoint;
    networking_message_endpoint other_endpoint;
    uint8_t payload[2] = {0x41U, 0x42U};
    ft_size_t received_size;

    networking_message_prepare_endpoint(endpoint);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, relay.open_relay(1U, 2U, 3U, endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, adapter.initialize(relay, endpoint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, adapter.send_datagram(endpoint, payload,
        sizeof(payload)));
    FT_ASSERT_EQ(42U, adapter.now_milliseconds());
    FT_ASSERT_EQ(FT_ERR_EMPTY, adapter.receive_datagram(other_endpoint,
        payload, sizeof(payload), &received_size));
    other_endpoint.length = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, adapter.send_datagram(
        other_endpoint, payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, adapter.destroy());
    FT_ASSERT_EQ(1U, relay.opens);
    return (1);
}

FT_TEST(test_networking_nat_model_covers_restricted_and_blocked_paths)
{
    const networking_nat_model_type model_types[6] = {
        networking_nat_model_type::FULL_CONE,
        networking_nat_model_type::ADDRESS_RESTRICTED,
        networking_nat_model_type::PORT_RESTRICTED,
        networking_nat_model_type::SYMMETRIC,
        networking_nat_model_type::DOUBLE_NAT,
        networking_nat_model_type::UDP_BLOCKED};
    networking_nat_test_model model;
    networking_message_endpoint local;
    networking_message_endpoint remote;
    networking_message_endpoint same_host_other_port;
    networking_message_endpoint other_host_same_port;
    networking_message_endpoint ipv6_remote;
    const sockaddr_in *remote_address;
    sockaddr_in *same_host_other_port_address;
    sockaddr_in *other_host_same_port_address;
    ft_size_t index;
    int32_t send_result;

    networking_message_prepare_endpoint(local);
    networking_message_prepare_endpoint(remote);
    ft_memset(&local.address, 0, sizeof(local.address));
    ft_memset(&remote.address, 0, sizeof(remote.address));
    reinterpret_cast<sockaddr_in *>(&local.address)->sin_family = AF_INET;
    reinterpret_cast<sockaddr_in *>(&remote.address)->sin_family = AF_INET;
    reinterpret_cast<sockaddr_in *>(&local.address)->sin_port = htons(3000U);
    reinterpret_cast<sockaddr_in *>(&remote.address)->sin_port = htons(4000U);
    reinterpret_cast<sockaddr_in *>(&local.address)->sin_addr.s_addr
        = htonl(0x7f000001U);
    reinterpret_cast<sockaddr_in *>(&remote.address)->sin_addr.s_addr
        = htonl(0x7f000001U);
    local.length = sizeof(sockaddr_in);
    remote.length = sizeof(sockaddr_in);
    ft_memset(&ipv6_remote, 0, sizeof(ipv6_remote));
    reinterpret_cast<sockaddr_in6 *>(&ipv6_remote.address)->sin6_family
        = AF_INET6;
    reinterpret_cast<sockaddr_in6 *>(&ipv6_remote.address)->sin6_port
        = htons(4000U);
    reinterpret_cast<sockaddr_in6 *>(&ipv6_remote.address)
        ->sin6_addr.s6_addr[15] = 1U;
    ipv6_remote.length = sizeof(sockaddr_in6);
    remote_address = reinterpret_cast<const sockaddr_in *>(&remote.address);
    same_host_other_port = remote;
    other_host_same_port = remote;
    same_host_other_port_address = reinterpret_cast<sockaddr_in *>(
        &same_host_other_port.address);
    other_host_same_port_address = reinterpret_cast<sockaddr_in *>(
        &other_host_same_port.address);
    same_host_other_port_address->sin_port = htons(4001U);
    other_host_same_port_address->sin_addr.s_addr
        = remote_address->sin_addr.s_addr ^ htonl(1U);
    index = 0U;
    while (index < 6U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, model.initialize(model_types[index]));
        send_result = model.send_probe(local, remote, 9U);
        if (model_types[index] == networking_nat_model_type::UDP_BLOCKED)
        {
            FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, send_result);
        }
        else
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, send_result);
            if (model_types[index] == networking_nat_model_type::FULL_CONE)
                FT_ASSERT(model.accepts_response(other_host_same_port));
            if (model_types[index]
                == networking_nat_model_type::ADDRESS_RESTRICTED)
            {
                FT_ASSERT(model.accepts_response(same_host_other_port));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::PORT_RESTRICTED)
                FT_ASSERT(!model.accepts_response(same_host_other_port));
            if (model_types[index] == networking_nat_model_type::SYMMETRIC)
            {
                FT_ASSERT(model.accepts_response(remote));
                FT_ASSERT(!model.accepts_response(other_host_same_port));
            }
            if (model_types[index] == networking_nat_model_type::DOUBLE_NAT)
                FT_ASSERT(!model.accepts_response(remote));
        }
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        model.initialize(networking_nat_model_type::FULL_CONE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, model.send_probe(local, remote, 9U));
    FT_ASSERT(!model.accepts_response(ipv6_remote));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, model.initialize(
        networking_nat_model_type::FULL_CONE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, model.send_probe(local, ipv6_remote, 9U));
    FT_ASSERT(model.accepts_response(ipv6_remote));
    return (1);
}

FT_TEST(test_networking_udp_datagram_adapter_loopback)
{
    networking_udp_datagram_io server_io;
    networking_udp_datagram_io client_io;
    SocketConfig server_configuration;
    SocketConfig client_configuration;
    networking_message_endpoint destination;
    networking_message_endpoint source;
    struct sockaddr_storage server_address;
    socklen_t address_length;
    uint16_t server_port;
    const uint8_t payload[4] = {'p', 'i', 'n', 'g'};
    uint8_t received_payload[4] = {0U, 0U, 0U, 0U};
    ft_size_t received_size;
    uint32_t receive_attempts;
    int32_t receive_result;

    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_configuration.initialize());
    server_configuration._type = SocketType::SERVER;
    server_configuration._address_family = AF_INET;
    server_configuration._protocol = IPPROTO_UDP;
    server_configuration._port = 0U;
    std::strncpy(server_configuration._ip, "127.0.0.1", sizeof(server_configuration._ip) - 1U);
    server_configuration._ip[sizeof(server_configuration._ip) - 1U] = '\0';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.initialize(server_configuration));
    address_length = sizeof(server_address);
    FT_ASSERT_EQ(0, getsockname(server_io.get_file_descriptor(),
        reinterpret_cast<struct sockaddr *>(&server_address), &address_length));
    server_port = ntohs(reinterpret_cast<struct sockaddr_in *>(&server_address)->sin_port);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_configuration.initialize());
    client_configuration._type = SocketType::CLIENT;
    client_configuration._address_family = AF_INET;
    client_configuration._protocol = IPPROTO_UDP;
    client_configuration._port = server_port;
    std::strncpy(client_configuration._ip, "127.0.0.1", sizeof(client_configuration._ip) - 1U);
    client_configuration._ip[sizeof(client_configuration._ip) - 1U] = '\0';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.initialize(client_configuration));
    ft_memset(&destination, 0, sizeof(destination));
    destination.address = server_address;
    destination.length = address_length;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.send_datagram(destination, payload, sizeof(payload)));
    receive_result = FT_ERR_EMPTY;
    received_size = 0U;
    receive_attempts = 0U;
    while (receive_result == FT_ERR_EMPTY && receive_attempts < 100000U)
    {
        receive_result = server_io.receive_datagram(source, received_payload,
            sizeof(received_payload), &received_size);
        receive_attempts += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, receive_result);
    FT_ASSERT_EQ(sizeof(payload), received_size);
    FT_ASSERT_EQ(static_cast<int32_t>('p'), static_cast<int32_t>(received_payload[0]));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_io.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_configuration.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_configuration.destroy());
    return (1);
}
