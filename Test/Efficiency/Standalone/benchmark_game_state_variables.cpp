#include "../../../Modules/Game/game_state.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t VARIABLE_COUNT = 64U;
    constexpr uint32_t UPDATE_ITERATIONS = 20000U;
    constexpr uint32_t ERASE_ITERATIONS = 256U;
    game_state state;
    ft_string keys[VARIABLE_COUNT];
    ft_string first;
    ft_string second;
    if (state.initialize() != FT_ERR_SUCCESS
        || first.initialize("initial variable value") != FT_ERR_SUCCESS
        || second.initialize("updated variable value") != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    char key[] = "variable-aa";
    while (index < VARIABLE_COUNT)
    {
        key[9] = static_cast<char>('a' + index / 26U);
        key[10] = static_cast<char>('a' + index % 26U);
        if (keys[index].initialize(key) != FT_ERR_SUCCESS)
            return (1);
        state.set_variable(keys[index], first);
        if (state.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    index = 0U;
    uint64_t checksum = 0U;
    const auto update_start = std::chrono::steady_clock::now();
    while (index < UPDATE_ITERATIONS)
    {
        const ft_string *input = &first;
        if ((index & 1U) != 0U)
            input = &second;
        state.set_variable(keys[0], *input);
        const ft_string *stored = state.get_variable(keys[0]);
        if (stored == ft_nullptr)
            return (1);
        checksum += stored->size();
        ++index;
    }
    const double update_elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - update_start).count();
    double erase_elapsed = 0.0;
    index = 0U;
    while (index < ERASE_ITERATIONS)
    {
        const ft_string &removed_key = keys[index % VARIABLE_COUNT];
        const auto erase_start = std::chrono::steady_clock::now();
        state.remove_variable(removed_key);
        erase_elapsed += std::chrono::duration<double, std::nano>(
            std::chrono::steady_clock::now() - erase_start).count();
        if (state.get_error() != FT_ERR_SUCCESS || state.get_variable(removed_key) != ft_nullptr)
            return (1);
        state.set_variable(removed_key, first);
        if (state.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (checksum != (first.size() + second.size()) * (UPDATE_ITERATIONS / 2U))
        return (1);
    std::printf("variables %.3f %.3f %" PRIu64 "\n", update_elapsed / UPDATE_ITERATIONS,
        erase_elapsed / ERASE_ITERATIONS, checksum);
    return (0);
}
