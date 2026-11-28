#include "../test_internal.hpp"
#include "../../Modules/Networking/networking.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_networking_socket_config_initialize_sets_defaults)
{
    SocketConfig configuration;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.initialize());
    FT_ASSERT_EQ(SocketType::SERVER, configuration._type);
    FT_ASSERT_EQ(0, ft_strcmp(configuration._ip, "127.0.0.1"));
    FT_ASSERT_EQ(8080, configuration._port);
    FT_ASSERT_EQ(10, configuration._backlog);
    FT_ASSERT_EQ(IPPROTO_TCP, configuration._protocol);
    FT_ASSERT_EQ(AF_INET, configuration._address_family);
    FT_ASSERT_EQ(true, configuration._reuse_address);
    FT_ASSERT_EQ(false, configuration._non_blocking);
    FT_ASSERT_EQ(5000, configuration._recv_timeout);
    FT_ASSERT_EQ(5000, configuration._send_timeout);
    FT_ASSERT_EQ(0, ft_strcmp(configuration._multicast_group, ""));
    FT_ASSERT_EQ(0, ft_strcmp(configuration._multicast_interface, ""));
    return (1);
}

FT_TEST(test_networking_socket_config_destroy_resets_fields)
{
    SocketConfig configuration;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.initialize());
    std::strncpy(configuration._ip, "10.1.2.3", sizeof(configuration._ip) - 1);
    configuration._ip[sizeof(configuration._ip) - 1] = '\0';
    configuration._port = 4242;
    configuration._backlog = 4;
    configuration._protocol = IPPROTO_UDP;
    configuration._address_family = AF_INET6;
    configuration._reuse_address = false;
    configuration._non_blocking = true;
    configuration._recv_timeout = 1000;
    configuration._send_timeout = 2000;
    std::strncpy(configuration._multicast_group, "239.0.0.1",
        sizeof(configuration._multicast_group) - 1);
    configuration._multicast_group[sizeof(configuration._multicast_group) - 1] = '\0';
    std::strncpy(configuration._multicast_interface, "eth0",
        sizeof(configuration._multicast_interface) - 1);
    configuration._multicast_interface[sizeof(configuration._multicast_interface) - 1] = '\0';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.destroy());
    FT_ASSERT_EQ(0, ft_strcmp(configuration._ip, ""));
    FT_ASSERT_EQ(0, configuration._port);
    FT_ASSERT_EQ(0, configuration._backlog);
    FT_ASSERT_EQ(0, configuration._protocol);
    FT_ASSERT_EQ(0, configuration._address_family);
    FT_ASSERT_EQ(false, configuration._reuse_address);
    FT_ASSERT_EQ(false, configuration._non_blocking);
    FT_ASSERT_EQ(0, configuration._recv_timeout);
    FT_ASSERT_EQ(0, configuration._send_timeout);
    FT_ASSERT_EQ(0, ft_strcmp(configuration._multicast_group, ""));
    FT_ASSERT_EQ(0, ft_strcmp(configuration._multicast_interface, ""));
    return (1);
}

FT_TEST(test_networking_socket_config_copy_preserves_bounded_padding)
{
    SocketConfig source;
    SocketConfig destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    ft_memset(source._ip, 'x', sizeof(source._ip));
    ft_memset(source._multicast_group, 'y', sizeof(source._multicast_group));
    ft_memset(source._multicast_interface, 'z', sizeof(source._multicast_interface));
    source._multicast_group[3] = '\0';
    source._multicast_interface[0] = '\0';
    source._port = 4300U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(0, ft_memcmp(source._ip, destination._ip, sizeof(source._ip) - 1U));
    FT_ASSERT_EQ('\0', destination._ip[sizeof(destination._ip) - 1U]);
    const char expected_group[sizeof(source._multicast_group)] = "yyy";
    const char expected_interface[sizeof(source._multicast_interface)] = {};
    FT_ASSERT_EQ(0, ft_memcmp(expected_group, destination._multicast_group,
        sizeof(expected_group)));
    FT_ASSERT_EQ(0, ft_memcmp(expected_interface, destination._multicast_interface,
        sizeof(expected_interface)));
    FT_ASSERT_EQ(4300U, destination._port);
    return (1);
}

FT_TEST(test_networking_socket_config_self_move_keeps_values)
{
    SocketConfig configuration;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.initialize());
    configuration._port = 4500U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, configuration.move(configuration));
    FT_ASSERT_EQ(4500U, configuration._port);
    FT_ASSERT_EQ(0, ft_strcmp(configuration._ip, "127.0.0.1"));
    return (1);
}
