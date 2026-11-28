#include "../../../Modules/Game/game_world_delta.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_interests(uint32_t group_count, uint32_t clients_per_group)
{
    constexpr uint32_t ITERATIONS = 4096U;
    constexpr uint64_t WORLD_ID = 1U;
    const uint32_t count = group_count * clients_per_group;
    game_world_delta_interest_set interests;
    ft_vector<uint64_t> live_clients;
    if (interests.initialize() != FT_ERR_SUCCESS || live_clients.initialize() != FT_ERR_SUCCESS)
        return (1);

    uint32_t index = 0U;
    const auto subscribe_start = std::chrono::steady_clock::now();
    while (index < count)
    {
        const int32_t chunk_x = static_cast<int32_t>(index % group_count) - 64;
        if (interests.subscribe(index + 1U, WORLD_ID, chunk_x, 0, 1U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double subscribe_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - subscribe_start).count();
    if (interests.size() != count)
        return (1);
    index = 0U;
    while (index < count)
    {
        const int32_t chunk_x = static_cast<int32_t>(index % group_count) - 64;
        if (interests.acknowledge_snapshot(index + 1U, WORLD_ID, chunk_x, 0, 1U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }

    const int32_t final_chunk_x = static_cast<int32_t>(group_count - 1U) - 64;
    index = 0U;
    const auto acknowledge_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (interests.acknowledge_revision(count, WORLD_ID, final_chunk_x, 0, index + 2U)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double acknowledge_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - acknowledge_start).count();

    index = 0U;
    uint64_t checksum = 0U;
    const auto collect_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (interests.collect_live_clients(WORLD_ID, final_chunk_x, 0, live_clients) != FT_ERR_SUCCESS
            || live_clients.size() != clients_per_group)
            return (1);
        checksum += live_clients[0U] + live_clients[clients_per_group - 1U];
        ++index;
    }
    const double collect_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - collect_start).count();
    index = 0U;
    while (index < clients_per_group)
    {
        if (live_clients[index] != (index + 1U) * group_count)
            return (1);
        ++index;
    }
    std::printf("groups %" PRIu32 " subscribe %.2f ns/client acknowledge %.2f ns/ack"
        " collect %.2f ns/list checksum=%" PRIu64 "\n", group_count, subscribe_ns / count,
        acknowledge_ns / ITERATIONS, collect_ns / ITERATIONS, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--single") == 0)
        return (measure_interests(1U, 1U));
    if (measure_interests(1U, 64U) != 0 || measure_interests(128U, 32U) != 0)
        return (1);
    return (0);
}
