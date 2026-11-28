#include "../../../Modules/Game/game_state.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_move(uint32_t count)
{
    constexpr uint32_t ITERATIONS = 100U;
    game_state first;
    game_state second;
    if (first.initialize() != FT_ERR_SUCCESS)
        return (1);
    ft_sharedptr<game_character> character(new game_character());
    ft_string variable;
    if (!character || variable.initialize("state value") != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    char name[] = "variable-aa";
    while (index < count)
    {
        ft_string key;
        name[9] = static_cast<char>('a' + index / 26U);
        name[10] = static_cast<char>('a' + index % 26U);
        if (key.initialize(name) != FT_ERR_SUCCESS
            || first.add_character(character) != FT_ERR_SUCCESS)
            return (1);
        first.set_variable(key, variable);
        if (first.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    game_state *source = &first;
    game_state *destination = &second;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_state *previous_source = source;
        source = destination;
        destination = previous_source;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const uint64_t checksum = source->get_worlds().size() + source->get_characters().size();
    if (checksum != count + 1U)
        return (1);
    if (count != 0U)
    {
        ft_string key;
        if (key.initialize("variable-aa") != FT_ERR_SUCCESS)
            return (1);
        const ft_string *stored = source->get_variable(key);
        if (stored == ft_nullptr || *stored != variable)
            return (1);
    }
    std::printf("move_%u %.3f %" PRIu64 "\n", count, elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_move(0U) != 0)
        return (1);
    return (measure_move(32U));
}
