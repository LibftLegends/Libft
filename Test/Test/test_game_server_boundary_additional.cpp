#include "../../Modules/Game/game_server.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Networking/websocket_client.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>
#include <thread>

static ft_bool game_server_boundary_local_sockets_available(void)
{
    int32_t socket_fd;
    struct sockaddr_in bind_address;
    int bind_result;

    errno = 0;
    socket_fd = nw_socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0)
    {
        if (errno == EPERM || errno == EACCES)
            return (FT_FALSE);
        return (FT_TRUE);
    }
    ft_memset(&bind_address, 0, sizeof(bind_address));
    bind_address.sin_family = AF_INET;
    bind_address.sin_port = 0;
    bind_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    errno = 0;
    bind_result = nw_bind(socket_fd,
            reinterpret_cast<struct sockaddr *>(&bind_address),
            sizeof(bind_address));
    nw_close(socket_fd);
    if (bind_result < 0 && (errno == EPERM || errno == EACCES))
        return (FT_FALSE);
    return (FT_TRUE);
}

static void game_server_join_callback(int32_t client_id)
{
    (void)client_id;
    return ;
}

static void game_server_leave_callback(int32_t client_id)
{
    (void)client_id;
    return ;
}

FT_TEST(test_game_server_null_join_callback_is_accepted)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_join_callback(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_server_null_leave_callback_is_accepted)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_leave_callback(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_server_callbacks_can_be_replaced)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_join_callback(game_server_join_callback);
    value.set_leave_callback(game_server_leave_callback);
    value.set_join_callback(ft_nullptr);
    value.set_leave_callback(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_server_run_once_before_start_is_safe)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.run_once();
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, value.get_error());
    return (1);
}

FT_TEST(test_game_server_zero_port_can_bind_ephemeral_socket)
{
    game_server value;
    int32_t start_result;

    if (game_server_boundary_local_sockets_available() == FT_FALSE)
        return (1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    start_result = value.start("127.0.0.1", 0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, start_result);
    return (1);
}

FT_TEST(test_game_server_invalid_ip_is_reported)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, value.start("256.256.256.256", 1));
    return (1);
}

FT_TEST(test_game_server_thread_safety_cycle)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_server_repeated_run_once_is_safe)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.run_once();
    value.run_once();
    value.run_once();
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, value.get_error());
    return (1);
}

FT_TEST(test_game_server_broadcast_skips_serialization_without_clients)
{
    game_server server;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = server.broadcast_world_locked();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    return (1);
}

FT_TEST(test_game_server_world_update_matches_scheduler_json_bytes)
{
    game_server server;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize(world));
    ft_string actual;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.serialize_world_locked(actual));
    FT_ASSERT_STR_EQ("{\n  \"world\": {\n    \"event_count\": \"0\"\n  }\n}\n", actual.c_str());
    ft_string request;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request.initialize(
        "{\"event\":{\"duration\":-1,\"id\":0}}"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, server.handle_message_locked(-1, request));
    constexpr char LARGE_EVENT[] = "{\"event\":{\"duration\":2147483647,\"id\":2147483647}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request.assign(LARGE_EVENT, sizeof(LARGE_EVENT) - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, request));
    int32_t index = 1;
    while (index < 22)
    {
        ft_sharedptr<game_event> event(new game_event());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
        event->set_id(INT32_MAX - index);
        event->set_duration(INT32_MAX - index);
        world->schedule_event(event);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, world->get_error());
        ++index;
    }
    const auto &scheduler = world->get_event_scheduler();
    json_group *groups = serialize_event_scheduler(scheduler);
    FT_ASSERT(groups != ft_nullptr);
    char *legacy = json_write_to_string(groups);
    json_free_groups(groups);
    FT_ASSERT(legacy != ft_nullptr);
    const int32_t encode_result = server.serialize_world_locked(actual);
    const ft_bool identical = actual == legacy;
    cma_free(legacy);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encode_result);
    FT_ASSERT_EQ(FT_TRUE, identical);
    const ft_size_t encoded_size = actual.size();
    const char *storage = actual.c_str();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t failure = server.serialize_world_locked(actual);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failure);
    FT_ASSERT_EQ(encoded_size, actual.size());
    FT_ASSERT(actual.c_str() == storage);
    FT_ASSERT_EQ(22U, scheduler->size());
    return (1);
}

FT_TEST(test_game_server_request_selection_keeps_priority_and_first_match)
{
    game_server server;
    ft_string message;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.initialize(
        "{\"event\":{\"id\":3,\"duration\":2},"
        "\"leave\":{\"id\":4},\"join\":{\"id\":7,\"id\":8},"
        "\"join\":{\"id\":9},\"world_revision\":{\"action\":\"status\"}}"));
    uint32_t callbacks = 0U;
    server.set_message_callback([](int32_t handle, const char *text, void *context) -> int32_t
    {
        auto *count = static_cast<uint32_t *>(context);
        if (handle != -1 || ft_strstr(text, "world_revision") == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        ++*count;
        return (FT_ERR_SUCCESS);
    }, &callbacks);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, message));
    FT_ASSERT_EQ(1U, callbacks);
    FT_ASSERT_EQ(0U, server._clients.size());
    server.set_message_callback(ft_nullptr, ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, message));
    FT_ASSERT_EQ(1U, server._clients.size());
    const auto *joined = server._clients.find(7);
    FT_ASSERT(joined != server._clients.end());
    FT_ASSERT_EQ(-1, joined->value);
    return (1);
}

FT_TEST(test_game_server_parsed_callback_receives_selected_revision_after_move)
{
    game_server source;
    game_server server;
    ft_string message;
    constexpr char PAYLOAD[] = "{\"join\":{\"id\":7},"
        "\"world_revision\":{\"action\":\"status\"},"
        "\"world_revision\":{\"action\":\"other\"}}";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.initialize(PAYLOAD));
    uint32_t callbacks = 0U;
    source.set_parsed_message_callback([](int32_t, const char *raw,
        json_group &revision, void *context) -> int32_t
    {
        const json_item *action = json_find_item(&revision, "action");
        if (action == ft_nullptr || ft_strcmp(action->value, "status") != 0
            || ft_strstr(raw, "\"join\"") == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        ++*static_cast<uint32_t *>(context);
        return (FT_ERR_SUCCESS);
    }, &callbacks);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, message));
    FT_ASSERT_EQ(1U, callbacks);
    FT_ASSERT_EQ(0U, server._clients.size());
    server.set_message_callback(ft_nullptr, ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.handle_message_locked(-1, message));
    FT_ASSERT_EQ(1U, callbacks);
    FT_ASSERT_EQ(1U, server._clients.size());
    return (1);
}

FT_TEST(test_game_server_join_reports_client_map_growth_failure)
{
    game_server server;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.initialize());
    const ft_size_t capacity = server._clients.capacity();
    ft_size_t index = 0U;
    while (index < capacity)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, server.join_client_locked(static_cast<int32_t>(index), -1));
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = server.join_client_locked(static_cast<int32_t>(capacity), -1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(capacity, server._clients.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.join_client_locked(static_cast<int32_t>(capacity), -1));
    FT_ASSERT_EQ(capacity + 1U, server._clients.size());
    return (1);
}

FT_TEST(test_game_server_join_broadcasts_world_on_loopback)
{
    if (game_server_boundary_local_sockets_available() == FT_FALSE)
        return (1);
    game_server source;
    game_server server;
    ft_sharedptr<game_world> world(new game_world());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(world));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.start("127.0.0.1", 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server.move(source));
    uint16_t port = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server._server->get_port(port));
    ft_websocket_client client;
    ft_string outgoing;
    ft_string received;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, outgoing.initialize("{\"join\":{\"id\":7}}"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, received.initialize());
    int32_t server_result = FT_ERR_INVALID_STATE;
    std::thread worker([&server, &server_result]()
    {
        server.run_once();
        server_result = server.get_error();
    });
    int32_t client_result = client.connect("127.0.0.1", port, "/");
    if (client_result == FT_ERR_SUCCESS)
        client_result = client.send_text(outgoing);
    if (client_result != FT_ERR_SUCCESS)
        client.close();
    worker.join();
    auto *entry = server._clients.find(7);
    if (entry == server._clients.end())
        return (0);
    const int32_t server_socket = entry->value;
    if (client_result == FT_ERR_SUCCESS && server_result == FT_ERR_SUCCESS)
    {
        int32_t client_socket = client._socket.get();
        if (nw_poll(&client_socket, 1, ft_nullptr, 0, 1000) > 0)
            client_result = client.receive_text(received);
        else
            client_result = FT_ERR_INVALID_OPERATION;
    }
    client.close();
    nw_close(server_socket);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, server_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_result);
    json_group *groups = json_read_from_string(received.c_str());
    FT_ASSERT(groups != ft_nullptr);
    json_group *world_group = json_find_group(groups, "world");
    const json_item *count = ft_nullptr;
    if (world_group != ft_nullptr)
        count = json_find_item(world_group, "event_count");
    const ft_bool empty = count != ft_nullptr && ft_strcmp(count->value, "0") == 0;
    json_free_groups(groups);
    FT_ASSERT_EQ(FT_TRUE, empty);
    return (1);
}

FT_TEST(test_game_server_destroy_twice_is_safe)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_game_server_reinitialize_after_destroy)
{
    game_server value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    return (1);
}
