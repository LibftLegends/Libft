#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "networking_test_support.hpp"
#include "../../Modules/Networking/networking_simulator.hpp"
#include "../../Modules/Errno/errno.hpp"

static ft_bool networking_simulation_bytes_differ(const uint8_t *left,
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

FT_TEST(test_networking_simulator_is_seeded_and_delays_datagrams)
{
    networking_test_memory_datagram_io first_base;
    networking_test_memory_datagram_io second_base;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[3] = {1U, 2U, 3U};
    uint8_t output[3] = {0U, 0U, 0U};
    ft_size_t received_size;

    first_base.connect(second_base);
    second_base.connect(first_base);
    networking_test_prepare_endpoint(endpoint);
    configuration.fixed_latency_milliseconds = 10U;
    configuration.random_seed = 42U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(first_base,
        configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    received_size = 0U;
    FT_ASSERT_EQ(FT_ERR_EMPTY, second_base.receive_datagram(source, output,
        sizeof(output), &received_size));
    simulator.advance(9U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    simulator.advance(1U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_base.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3U), received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_scripted_packet_outcomes)
{
    networking_test_memory_datagram_io base;
    networking_test_memory_datagram_io peer;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[2] = {4U, 5U};
    uint8_t output[2] = {0U, 0U};
    ft_size_t received_size;

    base.connect(peer);
    peer.connect(base);
    networking_test_prepare_endpoint(endpoint);
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
    FT_ASSERT_EQ(FT_ERR_EMPTY, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    simulator.advance(1U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, simulator.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2U), received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_corruption_duplication_and_mtu)
{
    networking_test_memory_datagram_io base;
    networking_test_memory_datagram_io peer;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    uint8_t input[3] = {8U, 9U, 10U};
    uint8_t output[3] = {0U, 0U, 0U};
    ft_size_t received_size;

    base.connect(peer);
    peer.connect(base);
    networking_test_prepare_endpoint(endpoint);
    configuration.random_seed = 11U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(base, configuration));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.add_script_action(1U,
        networking_simulator_script_action::SCRIPT_CORRUPT, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    simulator.advance(0U);
    (void)simulator.receive_datagram(source, output, sizeof(output),
        &received_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, peer.receive_datagram(source, output,
        sizeof(output), &received_size));
    FT_ASSERT_EQ(sizeof(input), received_size);
    FT_ASSERT(networking_simulation_bytes_differ(output, input,
        sizeof(input)));
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
    networking_test_memory_datagram_io base;
    networking_simulated_datagram_io simulator;
    networking_simulator_config configuration;
    networking_message_endpoint endpoint;
    uint8_t input[3] = {8U, 9U, 10U};

    networking_test_prepare_endpoint(endpoint);
    configuration.maximum_datagram_size = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(base, configuration));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, simulator.send_datagram(endpoint, input,
        sizeof(input)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.destroy());
    return (1);
}

FT_TEST(test_networking_simulator_reorders_and_limits_traffic)
{
    networking_test_memory_datagram_io first_base;
    networking_test_memory_datagram_io second_base;
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
    networking_test_prepare_endpoint(endpoint);
    configuration.fixed_latency_milliseconds = 10U;
    configuration.reorder_parts_per_million = 500000U;
    configuration.random_seed = 3U;
    configuration.maximum_pending_datagrams = 2U;
    configuration.bandwidth_bytes_per_second = 2U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, simulator.initialize(first_base,
        configuration));
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
