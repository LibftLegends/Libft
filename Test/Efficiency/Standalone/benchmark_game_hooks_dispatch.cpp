#include "../../../Modules/Game/game_hooks.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_dispatch(ft_bool with_listener)
{
    constexpr uint32_t ITERATIONS = 100000U;
    game_hooks hooks;
    game_character character;
    if (hooks.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint64_t expected = 0U;
    if (with_listener)
    {
        hooks.set_on_character_damaged([&checksum](game_character &, int32_t damage, uint8_t type)
        {
            checksum += static_cast<uint64_t>(damage) + type;
        });
        if (hooks.get_error() != FT_ERR_SUCCESS)
            return (1);
        expected = ITERATIONS * 9U;
    }
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        hooks.invoke_on_character_damaged(character, 7, 2);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != expected)
        return (1);
    std::printf("dispatch_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(with_listener),
        elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_dispatch(FT_FALSE) != 0)
        return (1);
    return (measure_dispatch(FT_TRUE));
}
