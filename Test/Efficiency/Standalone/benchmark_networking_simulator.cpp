#include "../../Test/networking_test_support.hpp"
#include "../../../Modules/Networking/networking_simulator.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_idle_poll()
{
    constexpr uint32_t DATAGRAM_COUNT = 4096;
    constexpr uint32_t POLL_COUNT = 2000;
    networking_test_idle_datagram_io base;
    networking_simulator_config configuration;
    networking_simulated_datagram_io simulator;
    networking_message_endpoint endpoint;
    networking_message_endpoint source;
    networking_test_prepare_endpoint(endpoint);
    configuration.fixed_latency_milliseconds = POLL_COUNT;
    configuration.maximum_pending_datagrams = DATAGRAM_COUNT;
    if (simulator.initialize(base, configuration) != FT_ERR_SUCCESS)
        return (1);
    const uint8_t packet = 1U;
    uint32_t queued = 0U;
    while (queued < DATAGRAM_COUNT)
    {
        if (simulator.send_datagram(endpoint, &packet, sizeof(packet)) != FT_ERR_SUCCESS)
            return (1);
        ++queued;
    }
    uint8_t output = 0U;
    ft_size_t received_size = 0U;
    uint32_t polls = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (polls < POLL_COUNT)
    {
        if (simulator.receive_datagram(source, &output, sizeof(output), &received_size)
            != FT_ERR_EMPTY)
            return (1);
        simulator.advance(1U);
        ++polls;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (base.sent_datagrams != 0U
        || simulator.receive_datagram(source, &output, sizeof(output), &received_size)
            != FT_ERR_EMPTY || base.sent_datagrams != DATAGRAM_COUNT)
        return (1);
    std::printf("%.3f us per idle poll (%u polls, %u pending datagrams)\n",
        elapsed_us / POLL_COUNT, POLL_COUNT, DATAGRAM_COUNT);
    return (0);
}

static int32_t measure_script_actions(ft_bool descending)
{
    constexpr uint32_t ACTION_COUNT = 4096;
    networking_test_idle_datagram_io base;
    networking_simulator_config configuration;
    networking_simulated_datagram_io simulator;
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    if (simulator.initialize(base, configuration) != FT_ERR_SUCCESS)
        return (1);
    uint32_t registered = 0U;
    const auto registration_start = std::chrono::steady_clock::now();
    while (registered < ACTION_COUNT)
    {
        uint32_t packet_ordinal = registered + 1U;
        if (descending)
            packet_ordinal = ACTION_COUNT - registered;
        if (simulator.add_script_action(packet_ordinal,
                networking_simulator_script_action::SCRIPT_DROP, 0U) != FT_ERR_SUCCESS)
            return (1);
        ++registered;
    }
    const auto send_start = std::chrono::steady_clock::now();
    const uint8_t packet = 1U;
    uint32_t sent = 0U;
    while (sent < ACTION_COUNT)
    {
        if (simulator.send_datagram(endpoint, &packet, sizeof(packet)) != FT_ERR_SUCCESS)
            return (1);
        ++sent;
    }
    const double registration_us = std::chrono::duration<double, std::micro>(
        send_start - registration_start).count();
    const double send_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - send_start).count();
    networking_message_endpoint source;
    uint8_t output = 0U;
    ft_size_t received_size = 0U;
    if (simulator.receive_datagram(source, &output, sizeof(output), &received_size) != FT_ERR_EMPTY
        || base.sent_datagrams != 0U)
        return (1);
    const char *order = "ascending";
    if (descending)
        order = "descending";
    std::printf("%s script: %.3f us per registration, %.3f us per scripted send (%u actions)\n",
        order, registration_us / ACTION_COUNT, send_us / ACTION_COUNT, ACTION_COUNT);
    return (0);
}

int32_t main()
{
    if (measure_idle_poll() != 0 || measure_script_actions(FT_FALSE) != 0
        || measure_script_actions(FT_TRUE) != 0)
        return (1);
    return (0);
}
