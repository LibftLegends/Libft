#ifdef _WIN32
# include <winsock2.h>
# include "../test_internal.hpp"
# include "../../Modules/Basic/limits.hpp"
# include "../../Modules/Networking/networking.hpp"
# include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_socket_wrappers_reject_unrepresentable_winsock_lengths)
{
    constexpr ft_size_t OVERSIZED = (ft_size_t{1} << 32) + 1;
    const int32_t socket_fd = nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    FT_ASSERT(socket_fd >= 0);
    u_long nonblocking = 1;
    const int32_t mode_result = ioctlsocket(static_cast<SOCKET>(socket_fd), FIONBIO,
        &nonblocking);
    if (mode_result != 0)
        nw_close(socket_fd);
    FT_ASSERT_EQ(0, mode_result);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr *socket_address = reinterpret_cast<sockaddr *>(&address);
    socklen_t address_size = sizeof(address);
    const int32_t bind_result = nw_bind(socket_fd, socket_address, address_size);
    if (bind_result != 0)
        nw_close(socket_fd);
    FT_ASSERT_EQ(0, bind_result);
    const int32_t address_result = getsockname(static_cast<SOCKET>(socket_fd),
        socket_address, &address_size);
    if (address_result != 0)
        nw_close(socket_fd);
    FT_ASSERT_EQ(0, address_result);
    char byte = 'X';

    const ssize_t send_result = nw_send(socket_fd, &byte, OVERSIZED, 0);
    const int32_t send_error = WSAGetLastError();
    const ssize_t receive_result = nw_recv(socket_fd, &byte, OVERSIZED, 0);
    const int32_t receive_error = WSAGetLastError();
    const ssize_t send_to_result = nw_sendto(socket_fd, &byte, OVERSIZED, 0,
        socket_address, address_size);
    const int32_t send_to_error = WSAGetLastError();
    const ssize_t receive_from_result = nw_recvfrom(socket_fd, &byte, OVERSIZED, 0,
        socket_address, &address_size);
    const int32_t receive_from_error = WSAGetLastError();
    FT_ASSERT_EQ(0, nw_close(socket_fd));

    FT_ASSERT_EQ(-1, send_result);
    FT_ASSERT_EQ(-1, receive_result);
    FT_ASSERT_EQ(-1, send_to_result);
    FT_ASSERT_EQ(-1, receive_from_result);
    FT_ASSERT_EQ(WSAEMSGSIZE, send_error);
    FT_ASSERT_EQ(WSAEMSGSIZE, receive_error);
    FT_ASSERT_EQ(WSAEMSGSIZE, send_to_error);
    FT_ASSERT_EQ(WSAEMSGSIZE, receive_from_error);
    FT_ASSERT_EQ('X', byte);
    return (1);
}
#endif
