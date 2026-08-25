#include "../../Modules/Game/game_server.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>

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
