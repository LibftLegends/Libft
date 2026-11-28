#include "../../../Modules/Game/game_world_replay.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr ft_size_t PAYLOAD_SIZE = 1048576U;
    constexpr uint32_t ITERATIONS = 256U;
    ft_string payload;
    ft_string exported;
    if (payload.initialize(PAYLOAD_SIZE, 's') != FT_ERR_SUCCESS
        || exported.initialize() != FT_ERR_SUCCESS)
        return (1);
    std::chrono::steady_clock::duration elapsed{};
    uint32_t index = 0U;
    while (index < ITERATIONS)
    {
        game_world_replay_session source;
        game_world_replay_session destination;
        if (source.initialize() != FT_ERR_SUCCESS
            || source.import_snapshot(payload) != FT_ERR_SUCCESS)
            return (1);
        const auto start = std::chrono::steady_clock::now();
        const int32_t error = destination.move(source);
        elapsed += std::chrono::steady_clock::now() - start;
        if (error != FT_ERR_SUCCESS)
            return (1);
        if (index + 1U == ITERATIONS && destination.export_snapshot(exported) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (exported.size() != payload.size()
        || ft_memcmp(exported.c_str(), payload.c_str(), PAYLOAD_SIZE) != 0)
        return (1);
    std::printf("replay move %.3f us/session bytes=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>(exported.size()));
    return (0);
}
