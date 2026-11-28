#include "../../../Modules/CardGame/card_game_ordered_zone.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 1000000U;
    const ft_bool lookup = argument_count == 2 && ft_strcmp(arguments[1], "--lookup") == 0;
    const ft_bool middle = argument_count == 2 && ft_strcmp(arguments[1], "--middle") == 0;
    const ft_bool shuffle = argument_count == 2 && ft_strcmp(arguments[1], "--shuffle") == 0;
    uint32_t capacity = FT_CARD_GAME_MAX_CARDS;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--small") == 0)
        capacity = 8U;
    card_game_ordered_zone zone;
    if (zone.initialize(capacity, FT_TRUE) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 1U;
    while (index <= capacity)
    {
        if (zone.push_bottom(index) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint32_t checksum = 0U;
    uint64_t random_state = 1234567U;
    uint32_t iterations = ITERATIONS;
    if (shuffle == FT_TRUE)
        iterations /= 10U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    if (shuffle == FT_TRUE)
    {
        while (index < iterations)
        {
            if (zone.shuffle(&random_state) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
    }
    else if (lookup == FT_TRUE)
    {
        while (index < ITERATIONS)
        {
            checksum += zone.contains_instance((index % capacity) + 1U);
            ++index;
        }
    }
    else if (middle == FT_TRUE)
    {
        while (index < ITERATIONS)
        {
            card_game_zone_entry entry{};
            if (zone.remove_entry(capacity / 2U + 1U, &entry) != FT_ERR_SUCCESS
                || zone.insert_entry_at(capacity / 2U, entry) != FT_ERR_SUCCESS)
                return (1);
            checksum += entry.instance_id;
            ++index;
        }
    }
    else
    {
        while (index < ITERATIONS)
        {
            uint32_t instance = 0U;
            if (zone.pop_top(&instance) != FT_ERR_SUCCESS
                || zone.push_bottom(instance) != FT_ERR_SUCCESS)
                return (1);
            checksum += instance;
            ++index;
        }
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (shuffle == FT_TRUE)
    {
        checksum = static_cast<uint32_t>(random_state ^ (random_state >> 32U));
        index = 0U;
        while (index < capacity)
        {
            uint32_t instance = 0U;
            if (zone.get(index, &instance) != FT_ERR_SUCCESS)
                return (1);
            checksum = checksum * 31U + instance;
            ++index;
        }
    }
    std::printf("ordered zone %" PRIu32 " cards, checksum %" PRIu32 " %.3f ns/op\n",
        capacity, checksum, elapsed / iterations);
    return (0);
}
