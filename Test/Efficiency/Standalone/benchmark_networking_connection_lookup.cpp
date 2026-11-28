#include "../../Test/networking_test_support.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_lookup(uint32_t connection_count)
{
    constexpr uint32_t MAX_CONNECTIONS = 512U;
    constexpr uint32_t ITERATIONS = 30000U;
    networking_test_idle_datagram_io io;
    networking_message_transport_config configuration;
    configuration.enable_encryption = FT_FALSE;
    configuration.enable_thread_safety = FT_TRUE;
    configuration.maximum_connections = MAX_CONNECTIONS;
    configuration.maximum_events = MAX_CONNECTIONS;
    networking_message_transport transport;
    networking_message_connection connections[MAX_CONNECTIONS];
    networking_message_endpoint endpoint;
    networking_test_prepare_endpoint(endpoint);
    if (transport.initialize(configuration, io) != FT_ERR_SUCCESS)
        return (1);
    uint32_t opened = 0U;
    while (opened < connection_count)
    {
        if (transport.open_connection(endpoint, connections[opened]) != FT_ERR_SUCCESS)
            return (1);
        ++opened;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (connections[iteration % connection_count].get_state()
                != networking_message_connection_state::CONNECTED)
            return (1);
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%u connections: %.3f us per state lookup (%u lookups)\n",
        connection_count, elapsed_us / ITERATIONS, ITERATIONS);
    return (0);
}

int main()
{
    if (measure_lookup(8U) != 0 || measure_lookup(512U) != 0)
        return (1);
    return (0);
}
