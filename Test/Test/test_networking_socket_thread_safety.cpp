#include "../test_internal.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Networking/socket_class.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include <thread>
#include <atomic>
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# define CLOSE_SOCKET closesocket
typedef SOCKET socket_file_descriptor_type;
static const socket_file_descriptor_type NETWORKING_INVALID_SOCKET_DESCRIPTOR = INVALID_SOCKET;
#else
# if defined(_WIN32) || defined(_WIN64)
#  include <winsock2.h>
#  include <ws2tcpip.h>
# else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
# endif
# include <sys/time.h>
# include <unistd.h>
# define CLOSE_SOCKET close
typedef int socket_file_descriptor_type;
static const socket_file_descriptor_type NETWORKING_INVALID_SOCKET_DESCRIPTOR = FT_SOCKET_DESCRIPTOR_CAST(-1);
#endif

static ft_bool networking_socket_is_valid_file_descriptor(socket_file_descriptor_type file_descriptor)
{
#ifdef _WIN32
    if (file_descriptor == INVALID_SOCKET)
        return (FT_FALSE);
#else
    if (file_descriptor < 0)
        return (FT_FALSE);
#endif
    return (FT_TRUE);
}

static socket_file_descriptor_type networking_socket_create_server(uint16_t &port)
{
    socket_file_descriptor_type server_fd;
    struct sockaddr_in address;
    socklen_t length;

    server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (networking_socket_is_valid_file_descriptor(server_fd) == FT_FALSE)
        return (NETWORKING_INVALID_SOCKET_DESCRIPTOR);
    std::memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (::bind(server_fd, reinterpret_cast<struct sockaddr*>(&address), sizeof(address)) != 0)
    {
        CLOSE_SOCKET(server_fd);
        return (NETWORKING_INVALID_SOCKET_DESCRIPTOR);
    }
    if (::listen(server_fd, 4) != 0)
    {
        CLOSE_SOCKET(server_fd);
        return (NETWORKING_INVALID_SOCKET_DESCRIPTOR);
    }
    length = sizeof(address);
    if (::getsockname(server_fd, reinterpret_cast<struct sockaddr*>(&address), &length) != 0)
    {
        CLOSE_SOCKET(server_fd);
        return (NETWORKING_INVALID_SOCKET_DESCRIPTOR);
    }
    port = ntohs(address.sin_port);
    return (server_fd);
}

static void networking_socket_configure_client(SocketConfig &config, uint16_t port)
{
    config._type = SocketType::CLIENT;
    std::strncpy(config._ip, "127.0.0.1", sizeof(config._ip) - 1);
    config._ip[sizeof(config._ip) - 1] = '\0';
    config._port = port;
    config._address_family = AF_INET;
    config._non_blocking = true;
    config._recv_timeout = 200;
    config._send_timeout = 200;
    return ;
}

static void networking_socket_set_receive_timeout(socket_file_descriptor_type file_descriptor, int milliseconds)
{
#ifdef _WIN32
    DWORD timeout_value;

    timeout_value = static_cast<DWORD>(milliseconds);
    (void)setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char *>(&timeout_value), sizeof(timeout_value));
#else
    struct timeval timeout_value;

    timeout_value.tv_sec = milliseconds / 1000;
    timeout_value.tv_usec = (milliseconds % 1000) * 1000;
    (void)setsockopt(file_descriptor, SOL_SOCKET, SO_RCVTIMEO,
        &timeout_value, sizeof(timeout_value));
#endif
    return ;
}

static ft_bool networking_socket_receive_timed_out(void)
{
#ifdef _WIN32
    int last_error_code;

    last_error_code = WSAGetLastError();
    if (last_error_code == WSAETIMEDOUT)
        return (FT_TRUE);
    if (last_error_code == WSAEWOULDBLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
#else
    if (errno == EAGAIN)
        return (FT_TRUE);
    if (errno == EWOULDBLOCK)
        return (FT_TRUE);
    return (FT_FALSE);
#endif
}

FT_TEST(test_networking_socket_send_all_thread_safety)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    uint16_t server_port;
    socket_file_descriptor_type server_fd;
    SocketConfig client_config;
    ft_socket client_socket;
    std::thread accept_thread;
    socket_file_descriptor_type accepted_fd;
    std::atomic<bool> inspector_running;
    std::thread inspector_thread;
    std::thread send_thread;
    std::thread reader_thread;
    const char message[] = "pingdata";
    int message_length;
    int send_iterations;
    int expected_total;
    std::atomic<int> received_total;
    std::atomic<bool> thread_failed;
    std::atomic<bool> send_completed;
    int wait_iterations;

    server_fd = networking_socket_create_server(server_port);
    FT_ASSERT(networking_socket_is_valid_file_descriptor(server_fd) == FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_config.initialize());
    networking_socket_configure_client(client_config, server_port);
    accepted_fd = NETWORKING_INVALID_SOCKET_DESCRIPTOR;
    accept_thread = std::thread([server_fd, &accepted_fd]() {
        struct sockaddr_in client_addr;
        socklen_t length;

        std::memset(&client_addr, 0, sizeof(client_addr));
        length = sizeof(client_addr);
        accepted_fd = ::accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &length);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.initialize(client_config));
    accept_thread.join();
    FT_ASSERT(networking_socket_is_valid_file_descriptor(accepted_fd) == FT_TRUE);
    networking_socket_set_receive_timeout(accepted_fd, 200);
    message_length = static_cast<int>(sizeof(message) - 1);
    send_iterations = 60;
    expected_total = message_length * send_iterations;
    received_total.store(0);
    thread_failed.store(false);
    send_completed.store(false);
    reader_thread = std::thread([accepted_fd, expected_total, &received_total,
        &send_completed, &thread_failed]() {
        char buffer[128];
        int local_total;
        int idle_iterations;

        local_total = 0;
        idle_iterations = 0;
        while (local_total < expected_total)
        {
            ssize_t bytes_read;

            bytes_read = ::recv(accepted_fd, buffer, sizeof(buffer), 0);
            if (bytes_read > 0)
            {
                local_total += static_cast<int>(bytes_read);
                idle_iterations = 0;
                continue ;
            }
            if (bytes_read == 0)
                break ;
            if (networking_socket_receive_timed_out() == FT_TRUE)
            {
                if (send_completed.load() == false)
                    continue ;
                idle_iterations++;
                if (idle_iterations < 5)
                    continue ;
                break ;
            }
            thread_failed.store(true);
            break ;
        }
        received_total.store(local_total);
        return ;
    });
    inspector_running.store(true);
    inspector_thread = std::thread([&client_socket, &inspector_running, &thread_failed]() {
        while (inspector_running.load())
        {
            int descriptor;

            descriptor = client_socket.get_file_descriptor();
            if (descriptor < 0)
            {
                thread_failed.store(true);
                return ;
            }
            pt_thread_sleep(1);
        }
        return ;
    });
    send_thread = std::thread([&client_socket, message, message_length, send_iterations, &thread_failed, &send_completed]() {
        int iteration;

        iteration = 0;
        while (iteration < send_iterations)
        {
            ssize_t bytes_sent;

            bytes_sent = client_socket.send_all(message, message_length, 0);
            if (static_cast<int>(bytes_sent) != message_length)
            {
                thread_failed.store(true);
                return ;
            }
            iteration++;
        }
        send_completed.store(true);
        return ;
    });
    wait_iterations = 0;
    while (!send_completed.load() && wait_iterations < 5000)
    {
        pt_thread_sleep(1);
        wait_iterations++;
    }
    if (!send_completed.load())
    {
        thread_failed.store(true);
        client_socket.close_socket();
        CLOSE_SOCKET(accepted_fd);
    }
    send_thread.join();
    wait_iterations = 0;
    while (received_total.load() < expected_total && wait_iterations < 5000)
    {
        pt_thread_sleep(1);
        wait_iterations++;
    }
    if (received_total.load() < expected_total)
    {
        thread_failed.store(true);
        client_socket.close_socket();
        CLOSE_SOCKET(accepted_fd);
    }
    inspector_running.store(false);
    inspector_thread.join();
    reader_thread.join();
    FT_ASSERT(thread_failed.load() == false);
    FT_ASSERT_EQ(expected_total, received_total.load());
    CLOSE_SOCKET(accepted_fd);
    CLOSE_SOCKET(server_fd);
    client_socket.close_socket();
    return (1);
}

FT_TEST(test_networking_socket_receive_close_thread_safety)
{
    if (networking_test_local_ipv4_available() == FT_FALSE)
        return (1);
    uint16_t server_port;
    socket_file_descriptor_type server_fd;
    SocketConfig client_config;
    ft_socket client_socket;
    std::thread accept_thread;
    socket_file_descriptor_type accepted_fd;
    std::thread sender_thread;
    std::thread receiver_thread;
    std::atomic<bool> received_once;
    std::atomic<bool> close_requested;
    std::thread closer_thread;

    server_fd = networking_socket_create_server(server_port);
    FT_ASSERT(networking_socket_is_valid_file_descriptor(server_fd) == FT_TRUE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_config.initialize());
    networking_socket_configure_client(client_config, server_port);
    accepted_fd = NETWORKING_INVALID_SOCKET_DESCRIPTOR;
    accept_thread = std::thread([server_fd, &accepted_fd]() {
        struct sockaddr_in client_addr;
        socklen_t length;

        std::memset(&client_addr, 0, sizeof(client_addr));
        length = sizeof(client_addr);
        accepted_fd = ::accept(server_fd, reinterpret_cast<struct sockaddr*>(&client_addr), &length);
        return ;
    });
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.initialize(client_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, client_socket.enable_thread_safety());
    accept_thread.join();
    FT_ASSERT(networking_socket_is_valid_file_descriptor(accepted_fd) == FT_TRUE);
    received_once.store(false);
    close_requested.store(false);
    sender_thread = std::thread([accepted_fd, &close_requested]() {
        const char payload[] = "closure-test-payload";
        int iteration;

        iteration = 0;
        while (iteration < 40 && !close_requested.load())
        {
            ::send(accepted_fd, payload, sizeof(payload) - 1, 0);
            pt_thread_sleep(1);
            iteration++;
        }
        return ;
    });
    receiver_thread = std::thread([&client_socket, &received_once]() {
        char buffer[64];
        int loop_index;

        loop_index = 0;
        while (loop_index < 80)
        {
            ssize_t result;

            result = client_socket.receive_data(buffer, sizeof(buffer), 0);
            if (result > 0)
                received_once.store(true);
            if (result < 0)
                break;
            loop_index++;
        }
        return ;
    });
    closer_thread = std::thread([&client_socket, &received_once, &close_requested]() {
        int wait_loops;

        wait_loops = 0;
        while (!received_once.load() && wait_loops < 200)
        {
            pt_thread_sleep(1);
            wait_loops++;
        }
        close_requested.store(true);
        client_socket.close_socket();
        return ;
    });
    receiver_thread.join();
    closer_thread.join();
    sender_thread.join();
    CLOSE_SOCKET(accepted_fd);
    CLOSE_SOCKET(server_fd);
    client_socket.close_socket();
    return (1);
}
