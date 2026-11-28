#ifdef __linux__
# include "../test_internal.hpp"
# include "../../Modules/Networking/networking.hpp"
# include "../../Modules/System_utils/test_system_utils_runner.hpp"
# include <sys/socket.h>

FT_TEST(test_networking_epoll_leaves_empty_socket_read_interest_unready)
{
    int32_t sockets[2];
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    int32_t readable[] = {sockets[0], sockets[0]};
    int32_t writable[] = {sockets[0], sockets[0]};
    const int32_t ready = nw_poll(readable, 2, writable, 2, 0);
    nw_close(sockets[0]);
    nw_close(sockets[1]);
    FT_ASSERT_EQ(1, ready);
    FT_ASSERT_EQ(-1, readable[0]);
    FT_ASSERT_EQ(-1, readable[1]);
    FT_ASSERT_EQ(sockets[0], writable[0]);
    FT_ASSERT_EQ(sockets[0], writable[1]);
    return (1);
}
#endif
