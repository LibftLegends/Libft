#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Networking/networking_handshake.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/Crypto/crypto_test_hooks.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"

enum class networking_reference_connection_phase : uint8_t
{
    LISTENING = 0U,
    CLIENT_CONNECTING = 1U,
    SERVER_REQUESTED = 2U,
    CONNECTED = 3U,
    DRAINING = 4U,
    CLOSED = 5U,
    ABORTED = 6U,
    SIMULTANEOUS_OPEN = 7U
};

static networking_message_connection_state
    networking_reference_expected_client_state(
        networking_reference_connection_phase phase) noexcept
{
    if (phase == networking_reference_connection_phase::CLIENT_CONNECTING
        || phase == networking_reference_connection_phase::SERVER_REQUESTED
        || phase == networking_reference_connection_phase::SIMULTANEOUS_OPEN)
        return (networking_message_connection_state::HANDSHAKING);
    if (phase == networking_reference_connection_phase::CONNECTED)
        return (networking_message_connection_state::CONNECTED);
    if (phase == networking_reference_connection_phase::DRAINING)
        return (networking_message_connection_state::DRAINING);
    if (phase == networking_reference_connection_phase::CLOSED)
        return (networking_message_connection_state::CLOSED);
    if (phase == networking_reference_connection_phase::ABORTED)
        return (networking_message_connection_state::CLOSED);
    return (networking_message_connection_state::IDLE);
}

class networking_state_machine_random_guard
{
    public:
        networking_state_machine_random_guard() noexcept
        {
            return ;
        }

        ~networking_state_machine_random_guard() noexcept
        {
            (void)crypto_test_random_clear();
            return ;
        }
};

FT_TEST(test_networking_handshake_state_machine_rejects_invalid_transitions)
{
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> server_hello;
    uint8_t client_finished[32];
    uint8_t server_finished[32];
    uint8_t saved_magic;
    uint8_t saved_nonce;
    uint8_t retained_peer_key[32];
    networking_state_machine_random_guard random_guard;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x88776655U));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, client.create_finished(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED, client.get_peer_public_key(
        client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(
        networking_handshake_role::CLIENT, 101U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(
        networking_handshake_role::SERVER, 202U));
    FT_ASSERT_EQ(networking_handshake_state::HELLO_READY, client.get_state());
    FT_ASSERT_EQ(networking_handshake_state::HELLO_READY, server.get_state());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, client.get_peer_public_key(client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(client_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    saved_magic = server_hello[0];
    server_hello[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::HELLO_READY, client.get_state());
    server_hello[0] = saved_magic;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(&server_hello[0],
        server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::PEER_HELLO_ACCEPTED,
        client.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(&server_hello[0],
        server_hello.size()));
    saved_nonce = server_hello[16U];
    server_hello[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::PEER_HELLO_ACCEPTED,
        client.get_state());
    server_hello[16U] = saved_nonce;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept_peer_hello(&client_hello[0],
        client_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.derive_keys());
    FT_ASSERT_EQ(networking_handshake_state::KEYS_DERIVED, client.get_state());
    server_hello[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::KEYS_DERIVED, client.get_state());
    server_hello[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, client.create_finished(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, client.get_traffic_keys(ft_nullptr,
        server_finished, client_finished, server_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.create_finished(client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.create_finished(server_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.verify_finished(client_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.verify_finished(server_finished));
    FT_ASSERT_EQ(networking_handshake_state::FINISHED, client.get_state());
    FT_ASSERT_EQ(networking_handshake_state::FINISHED, server.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.clear_traffic_secrets());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.clear_traffic_secrets());
    FT_ASSERT_EQ(networking_handshake_state::FINISHED, client.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_peer_public_key(
        retained_peer_key));
    FT_ASSERT_EQ(FT_TRUE, client.test_secret_state_is_wiped());
    FT_ASSERT_EQ(FT_TRUE, server.test_secret_state_is_wiped());
    server_hello[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::FINISHED, client.get_state());
    server_hello[16U] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_TRUE, client.test_secret_state_is_wiped());
    FT_ASSERT_EQ(FT_TRUE, server.test_secret_state_is_wiped());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_hello.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_hello.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_retry_preserves_hello_and_transcript_bytes)
{
    constexpr ft_size_t HELLO_SIZE = 80U;
    constexpr ft_size_t PREFIXED_HELLO_SIZE = 88U;
    const uint8_t expected_header[] = {
        0x4CU, 0x46U, 0x54U, 0x48U, 2U, 0U, 0U, 0U,
        1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U
    };
    uint8_t cookie[40] = {0x81U, 0x62U, 0x43U};
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> retry_hello;
    ft_vector<uint8_t> server_hello;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(networking_handshake_role::CLIENT,
        UINT64_C(0x0102030405060708)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(networking_handshake_role::SERVER, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(client_hello));
    FT_ASSERT_EQ(HELLO_SIZE, client_hello.size());
    FT_ASSERT_EQ(0, ft_memcmp(expected_header, client_hello.begin(),
        sizeof(expected_header)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.set_retry_cookie(cookie));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retry_hello.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retry_hello.push_back(0xABU));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t failed_copy = client.get_local_hello(retry_hello);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_copy);
    FT_ASSERT_EQ(1U, retry_hello.size());
    FT_ASSERT_EQ(0xABU, retry_hello[0U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(retry_hello));
    FT_ASSERT_EQ(HELLO_SIZE + sizeof(cookie), retry_hello.size());
    FT_ASSERT_EQ(0, ft_memcmp(client_hello.begin(), retry_hello.begin(), HELLO_SIZE));
    FT_ASSERT_EQ(0, ft_memcmp(cookie, retry_hello.begin() + HELLO_SIZE, sizeof(cookie)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t failed_accept = server.accept_peer_hello(retry_hello.begin(),
        retry_hello.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_accept);
    FT_ASSERT(server.get_state() == networking_handshake_state::HELLO_READY);
    FT_ASSERT_EQ(0U, server.get_peer_connection_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept_peer_hello(retry_hello.begin(),
        retry_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(server_hello.begin(),
        server_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.derive_keys());

    uint8_t expected_transcript[2U * PREFIXED_HELLO_SIZE] = {};
    expected_transcript[7U] = HELLO_SIZE;
    expected_transcript[PREFIXED_HELLO_SIZE + 7U] = HELLO_SIZE;
    ft_memcpy(expected_transcript + 8U, client_hello.begin(), HELLO_SIZE);
    ft_memcpy(expected_transcript + PREFIXED_HELLO_SIZE + 8U,
        server_hello.begin(), HELLO_SIZE);
    networking_crypto_backend backend;
    uint8_t send_key[32];
    uint8_t receive_key[32];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];
    uint8_t expected_finished[32];
    uint8_t actual_finished[32];
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_traffic_keys(send_key, receive_key,
        send_iv, receive_iv));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backend.hmac_sha256(send_key, sizeof(send_key),
        expected_transcript, sizeof(expected_transcript), expected_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.create_finished(actual_finished));
    FT_ASSERT_EQ(0, ft_memcmp(expected_finished, actual_finished, sizeof(actual_finished)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.verify_finished(actual_finished));
    return (1);
}

FT_TEST(test_networking_handshake_move_transfers_live_buffers_without_allocating)
{
    networking_handshake source;
    networking_handshake peer;
    networking_handshake destination;
    ft_vector<uint8_t> source_hello;
    ft_vector<uint8_t> peer_hello;
    uint8_t source_finished[32];
    uint8_t moved_finished[32];
    uint8_t peer_finished[32];
    const uint8_t cookie[40] = {0x85U};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(networking_handshake_role::CLIENT, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.initialize(networking_handshake_role::SERVER, 2U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(networking_handshake_role::SERVER, 3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_retry_cookie(cookie));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_local_hello(source_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.get_local_hello(peer_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source.accept_peer_hello(peer_hello.begin(), peer_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        peer.accept_peer_hello(source_hello.begin(), source_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.create_finished(source_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.create_finished(peer_finished));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t move_result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_result);
    FT_ASSERT(source.get_state() == networking_handshake_state::IDLE);
    FT_ASSERT(source.test_secret_state_is_wiped());
    FT_ASSERT(destination.get_state() == networking_handshake_state::KEYS_DERIVED);
    FT_ASSERT_EQ(2U, destination.get_peer_connection_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination.accept_peer_hello(peer_hello.begin(), peer_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.create_finished(moved_finished));
    FT_ASSERT_EQ(0, ft_memcmp(source_finished, moved_finished, sizeof(moved_finished)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.verify_finished(moved_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.verify_finished(peer_finished));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, destination.move(source));
    FT_ASSERT(destination.get_state() == networking_handshake_state::FINISHED);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(networking_handshake_role::CLIENT, 4U));
    return (1);
}

FT_TEST(test_networking_handshake_failure_state_is_terminal)
{
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> client_hello;
    ft_vector<uint8_t> server_hello;
    uint8_t server_finished[32];
    networking_state_machine_random_guard random_guard;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x1742b9c3U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(
        networking_handshake_role::CLIENT, 551U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(
        networking_handshake_role::SERVER, 552U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.get_local_hello(client_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(&server_hello[0],
        server_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept_peer_hello(&client_hello[0],
        client_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.derive_keys());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.create_finished(server_finished));
    server_finished[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED,
        client.verify_finished(server_finished));
    FT_ASSERT_EQ(networking_handshake_state::FAILED, client.get_state());
    FT_ASSERT_EQ(FT_TRUE, client.test_secret_state_is_wiped());
    server_finished[0] ^= 1U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        client.verify_finished(server_finished));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        client.create_finished(server_finished));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_TRUE, client.test_secret_state_is_wiped());
    FT_ASSERT_EQ(FT_TRUE, server.test_secret_state_is_wiped());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_hello.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_hello.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_v2_rejects_legacy_key_schedule)
{
    networking_handshake client;
    networking_handshake server;
    ft_vector<uint8_t> server_hello;
    uint8_t saved_version;
    networking_state_machine_random_guard random_guard;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x10293847U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(
        networking_handshake_role::CLIENT, 303U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(
        networking_handshake_role::SERVER, 404U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_local_hello(server_hello));
    FT_ASSERT_EQ(2U, server_hello[4]);
    saved_version = server_hello[4];
    server_hello[4] = 1U;
    FT_ASSERT_EQ(FT_ERR_PERMISSION_DENIED, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(networking_handshake_state::HELLO_READY, client.get_state());
    server_hello[4] = saved_version;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.accept_peer_hello(
        &server_hello[0], server_hello.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_hello.destroy());
    return (1);
}

FT_TEST(test_networking_connection_transitions_match_reference_trace)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_event event;
    networking_reference_connection_phase reference_phase;
    networking_state_machine_random_guard random_guard;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x5a17c3e9U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.listen(endpoint));
    reference_phase = networking_reference_connection_phase::LISTENING;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    reference_phase = networking_reference_connection_phase::CLIENT_CONNECTING;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTING, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTION_REQUESTED,
        event.type);
    reference_phase = networking_reference_connection_phase::SERVER_REQUESTED;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, server.accept(event.connection_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.get_connection(event.connection_id,
        server_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    reference_phase = networking_reference_connection_phase::CONNECTED;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(networking_message_connection_state::CONNECTED,
        server_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.close(
        networking_message_close_reason::APPLICATION, "model close"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_connection.close(
        networking_message_close_reason::APPLICATION, "duplicate close"));
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        client_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::PEER_CLOSING, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CLOSED, event.type);
    FT_ASSERT_EQ(FT_ERR_EMPTY, server.receive_event(event));
    reference_phase = networking_reference_connection_phase::CLOSED;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(networking_message_connection_state::CLOSED,
        server_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_graceful_close_drains_then_abort_closes)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_send_options options;
    networking_reference_connection_phase reference_phase;
    uint8_t payload[4] = {0x10U, 0x20U, 0x30U, 0x40U};

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_authenticated_handshake = FT_FALSE;
    configuration.enable_retry_cookies = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, connection));
    options.delivery = networking_message_delivery::RELIABLE_ORDERED;
    options.channel = 0U;
    options.lane = 0U;
    options.expiry_milliseconds = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.close(
        networking_message_close_reason::APPLICATION, "drain model"));
    reference_phase = networking_reference_connection_phase::DRAINING;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        connection.get_state());
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, connection.send_message(payload,
        sizeof(payload), options));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.abort(
        networking_message_close_reason::APPLICATION));
    reference_phase = networking_reference_connection_phase::ABORTED;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_timeout_emits_one_terminal_event)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_event event;
    uint32_t retry_index;
    int32_t poll_result;
    networking_state_machine_random_guard random_guard;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    configuration.retransmission_timeout_milliseconds = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x14e8a2c7U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTING, event.type);
    retry_index = 0U;
    poll_result = FT_ERR_SUCCESS;
    while (connection.get_state()
            != networking_message_connection_state::FAILED
        && retry_index < 8U)
    {
        client_io.advance(1000U);
        poll_result = client.poll();
        retry_index += 1U;
    }
    FT_ASSERT_EQ(networking_message_connection_state::FAILED,
        connection.get_state());
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, poll_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::FAILED, event.type);
    FT_ASSERT_EQ(FT_ERR_TIMEOUT, event.reason);
    FT_ASSERT_EQ(connection.get_id(), event.connection_id);
    FT_ASSERT_EQ(FT_ERR_EMPTY, client.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
    FT_ASSERT_EQ(FT_ERR_EMPTY, client.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_handshake_retry_send_failure_emits_terminal_event)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection connection;
    networking_message_event event;
    networking_state_machine_random_guard random_guard;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    configuration.retransmission_timeout_milliseconds = 10U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x7213b9d5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint, connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTING, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_DATAGRAM_SEND));
    client_io.advance(1000U);
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, client.poll());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(networking_message_connection_state::FAILED,
        connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::FAILED, event.type);
    FT_ASSERT_EQ(FT_ERR_SOCKET_SEND_FAILED, event.reason);
    FT_ASSERT_EQ(FT_ERR_EMPTY, client.receive_event(event));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, connection.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}

FT_TEST(test_networking_authenticated_simultaneous_open_selects_one_server)
{
    networking_test_memory_datagram_io client_io;
    networking_test_memory_datagram_io server_io;
    networking_message_transport_config configuration;
    networking_message_transport client;
    networking_message_transport server;
    networking_message_endpoint endpoint;
    networking_message_connection client_connection;
    networking_message_connection server_connection;
    networking_message_event event;
    networking_message_peer_identity identity;
    networking_reference_connection_phase reference_phase;
    uint32_t poll_index;
    networking_state_machine_random_guard random_guard;

    client_io.connect(server_io);
    server_io.connect(client_io);
    networking_test_prepare_endpoint(endpoint);
    configuration.enable_authenticated_handshake = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0xa91370c5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize(configuration, client_io));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(configuration, server_io));
    client._next_connection_id = 17U;
    server._next_connection_id = 29U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.open_connection(endpoint,
        client_connection));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.open_connection(endpoint,
        server_connection));
    FT_ASSERT(client_connection.get_id() != server_connection.get_id());
    reference_phase = networking_reference_connection_phase::SIMULTANEOUS_OPEN;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        server_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTING, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTING, event.type);
    poll_index = 0U;
    while ((client_connection.get_state()
                != networking_message_connection_state::CONNECTED
            || server_connection.get_state()
                != networking_message_connection_state::CONNECTED)
        && poll_index < 4U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, client.poll());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.poll());
        poll_index += 1U;
    }
    reference_phase = networking_reference_connection_phase::CONNECTED;
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        client_connection.get_state());
    FT_ASSERT_EQ(networking_reference_expected_client_state(reference_phase),
        server_connection.get_state());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.receive_event(event));
    FT_ASSERT_EQ(networking_message_event_type::CONNECTED, event.type);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        client_connection.get_remote_identity(identity));
    FT_ASSERT_EQ(FT_TRUE, identity.authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        server_connection.get_remote_identity(identity));
    FT_ASSERT_EQ(FT_TRUE, identity.authenticated);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.destroy());
    return (1);
}
