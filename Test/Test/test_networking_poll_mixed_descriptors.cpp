#ifdef _WIN32
# include "../test_internal.hpp"
# include "../../Modules/Networking/networking.hpp"
# include "../../Modules/Networking/socket_handle.hpp"
# include "../../Modules/System_utils/test_system_utils_runner.hpp"
# include <io.h>
# include <fcntl.h>

FT_TEST(test_networking_poll_reports_ready_pipe_and_socket_together)
{
    ft_socket_handle socket;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, socket.initialize());
    FT_ASSERT(socket.reset(nw_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)));
    FT_ASSERT(socket.is_valid());

    int32_t pipe_descriptors[2];
    FT_ASSERT_EQ(0, _pipe(pipe_descriptors, 4096, _O_BINARY));
    int32_t readable = pipe_descriptors[0];
    int32_t writable = socket.get();
    const int32_t idle_ready = nw_poll(&readable, 1, &writable, 1, 0);
    const int32_t idle_readable = readable;
    const int32_t idle_writable = writable;

    const int32_t write_result = _write(pipe_descriptors[1], "x", 1);
    readable = pipe_descriptors[0];
    writable = socket.get();
    const int32_t ready = nw_poll(&readable, 1, &writable, 1, 0);
    char byte = '\0';
    int32_t read_result = -1;
    if (write_result == 1)
        read_result = _read(pipe_descriptors[0], &byte, 1);
    _close(pipe_descriptors[1]);
    int32_t end_readable = pipe_descriptors[0];
    const int32_t end_ready = nw_poll(&end_readable, 1, ft_nullptr, 0, 0);
    char end_byte;
    const int32_t end_result = _read(pipe_descriptors[0], &end_byte, 1);
    _close(pipe_descriptors[0]);

    FT_ASSERT_EQ(1, idle_ready);
    FT_ASSERT_EQ(-1, idle_readable);
    FT_ASSERT_EQ(socket.get(), idle_writable);
    FT_ASSERT_EQ(1, write_result);
    FT_ASSERT_EQ(2, ready);
    FT_ASSERT_EQ(pipe_descriptors[0], readable);
    FT_ASSERT_EQ(socket.get(), writable);
    FT_ASSERT_EQ(1, read_result);
    FT_ASSERT_EQ('x', byte);
    FT_ASSERT_EQ(1, end_ready);
    FT_ASSERT_EQ(pipe_descriptors[0], end_readable);
    FT_ASSERT_EQ(0, end_result);
    return (1);
}
#endif
