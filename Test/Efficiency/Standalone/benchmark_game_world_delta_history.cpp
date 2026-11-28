#include "../../../Modules/Game/game_world_delta.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_history(uint32_t capacity)
{
    constexpr uint32_t APPENDS = 65536U;
    constexpr uint32_t RECOVERIES = 32768U;
    game_world_delta_history history;
    ft_vector<game_block_delta> recovered;
    if (history.initialize(capacity) != FT_ERR_SUCCESS
        || recovered.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_block_delta delta = {};
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    while (delta.revision < capacity)
    {
        delta.previous_revision = delta.revision;
        ++delta.revision;
        if (history.append(delta) != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t index = 0U;
    const auto append_start = std::chrono::steady_clock::now();
    while (index < APPENDS)
    {
        delta.previous_revision = delta.revision;
        ++delta.revision;
        if (history.append(delta) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double append_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - append_start).count();
    if (history.size() != capacity || history.get_oldest_revision() != APPENDS + 1U
        || history.get_latest_revision() != delta.revision)
        return (1);

    uint64_t checksum = 0U;
    index = 0U;
    const auto recovery_start = std::chrono::steady_clock::now();
    while (index < RECOVERIES)
    {
        if (history.get_since(delta.revision - 1U, recovered) != FT_ERR_SUCCESS
            || recovered.size() != 1U || recovered[0U].revision != delta.revision)
            return (1);
        checksum += recovered[0U].revision;
        ++index;
    }
    const double recovery_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - recovery_start).count();
    std::printf("capacity %" PRIu32 " append %.2f ns/edit recover %.2f ns/request checksum=%" PRIu64 "\n",
        capacity, append_ns / APPENDS, recovery_ns / RECOVERIES, checksum);
    return (0);
}

static int32_t measure_full_recovery(uint32_t capacity)
{
    constexpr uint32_t RECOVERIES = 1024U;
    game_world_delta_history history;
    ft_vector<game_block_delta> recovered;
    if (history.initialize(capacity) != FT_ERR_SUCCESS
        || recovered.initialize() != FT_ERR_SUCCESS)
        return (1);
    game_block_delta delta = {};
    delta.protocol_version = GAME_WORLD_DELTA_PROTOCOL_VERSION;
    while (delta.revision <= capacity)
    {
        delta.previous_revision = delta.revision;
        ++delta.revision;
        if (history.append(delta) != FT_ERR_SUCCESS)
            return (1);
    }
    if (history.get_since(1U, recovered) != FT_ERR_SUCCESS)
        return (1);
    uint32_t request = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (request < RECOVERIES)
    {
        if (history.get_since(1U, recovered) != FT_ERR_SUCCESS
            || recovered.size() != capacity || recovered[0U].revision != 2U
            || recovered[capacity - 1U].revision != delta.revision)
            return (1);
        checksum += recovered[request % capacity].revision;
        ++request;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("capacity %" PRIu32 " full_recover %.2f ns/request checksum=%" PRIu64 "\n",
        capacity, elapsed / RECOVERIES, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--batch") == 0)
    {
        if (measure_full_recovery(GAME_WORLD_DELTA_DEFAULT_HISTORY_CAPACITY) != 0)
            return (1);
        return (measure_full_recovery(4096U));
    }
    if (measure_history(GAME_WORLD_DELTA_DEFAULT_HISTORY_CAPACITY) != 0
        || measure_history(4096U) != 0)
        return (1);
    return (0);
}
