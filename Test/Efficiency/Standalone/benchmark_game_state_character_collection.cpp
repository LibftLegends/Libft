#include "../../../Modules/Game/game_state.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_character_collection(ft_bool thread_safe)
{
    constexpr uint32_t ITERATIONS = 100000U;
    game_state state;
    ft_sharedptr<game_character> character(new game_character());
    if (!character || state.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (thread_safe && state.get_characters().enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (state.add_character(character) != FT_ERR_SUCCESS)
            return (1);
        state.remove_character(0U);
        if (state.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const uint64_t checksum = state.get_characters().size();
    if (checksum != 0U || character.use_count() != 1)
        return (1);
    std::printf("characters_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(thread_safe),
        elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_character_collection(FT_FALSE) != 0)
        return (1);
    return (measure_character_collection(FT_TRUE));
}
