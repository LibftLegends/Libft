#include "../test_internal.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Networking/socket_handle.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t test_socket_handle_bind(int32_t descriptor)
{
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return (nw_bind(descriptor, reinterpret_cast<const sockaddr *>(&address),
            sizeof(address)));
}

FT_TEST(test_networking_socket_handle_self_reset_keeps_descriptor_open)
{
    ft_socket_handle handle;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(descriptor >= 0);
    FT_ASSERT(handle.reset(descriptor));
    FT_ASSERT(handle.reset(descriptor));
    FT_ASSERT_EQ(0, test_socket_handle_bind(handle.get()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.destroy());
    return (1);
}

FT_TEST(test_networking_socket_handle_self_move_preserves_ownership)
{
    ft_socket_handle handle;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, handle.initialize());
    const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(descriptor >= 0);
    FT_ASSERT(handle.reset(descriptor));
    const int32_t move_result = handle.move(handle);
    if (handle._socket_fd < 0)
        nw_close(descriptor);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_result);
    FT_ASSERT_EQ(descriptor, handle._socket_fd);
    FT_ASSERT_EQ(0, test_socket_handle_bind(handle.get()));

    ft_socket_handle destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(handle));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, handle._initialised_state);
    FT_ASSERT_EQ(-1, handle._socket_fd);
    FT_ASSERT_EQ(descriptor, destination.get());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_networking_socket_handle_copy_owns_an_independent_descriptor)
{
    ft_socket_handle source;
    ft_socket_handle destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    const int32_t descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(descriptor >= 0);
    FT_ASSERT(source.reset(descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    const int32_t copy_descriptor = destination.get();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT(copy_descriptor != descriptor);
    FT_ASSERT_EQ(0, test_socket_handle_bind(copy_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_networking_socket_handle_move_keeps_source_when_destination_close_fails)
{
    ft_socket_handle source;
    ft_socket_handle destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    const int32_t source_descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(source_descriptor >= 0);
    FT_ASSERT(source.reset(source_descriptor));
    const int32_t destination_descriptor = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(destination_descriptor >= 0);
    FT_ASSERT(destination.reset(destination_descriptor));
    FT_ASSERT_EQ(0, nw_close(destination_descriptor));

    FT_ASSERT_EQ(FT_ERR_SOCKET_CLOSE_FAILED, destination.move(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(-1, destination._socket_fd);
    FT_ASSERT_EQ(source_descriptor, source.get());
    FT_ASSERT_EQ(0, test_socket_handle_bind(source_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    return (1);
}
