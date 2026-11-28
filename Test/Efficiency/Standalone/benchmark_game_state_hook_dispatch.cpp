#include "../../../Modules/Game/game_state.hpp"
#include "../../../Modules/Game/game_hooks.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_dispatch(ft_bool with_listener)
{
    constexpr uint32_t ITERATIONS = 100000U;
    game_state state;
    game_character character;
    if (state.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0U;
    uint64_t expected = 0U;
    if (with_listener)
    {
        ft_sharedptr<game_hooks> hooks(new game_hooks());
        if (!hooks || hooks->initialize() != FT_ERR_SUCCESS)
            return (1);
        hooks->set_on_character_damaged([&checksum](game_character &, int32_t damage, uint8_t type)
        {
            checksum += static_cast<uint64_t>(damage) + type;
        });
        if (hooks->get_error() != FT_ERR_SUCCESS)
            return (1);
        state.set_hooks(hooks);
        if (state.get_error() != FT_ERR_SUCCESS)
            return (1);
        expected = ITERATIONS * 9U;
    }
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        state.dispatch_character_damaged(character, 7, 2);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != expected || state.get_error() != FT_ERR_SUCCESS)
        return (1);
    std::printf("state_dispatch_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(with_listener),
        elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_dispatch(FT_FALSE) != 0)
        return (1);
    return (measure_dispatch(FT_TRUE));
}
