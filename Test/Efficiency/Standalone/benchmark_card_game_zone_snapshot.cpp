#include "../../../Modules/CardGame/card_game_zone_store.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ZONE_COUNT = 8U;
    constexpr uint32_t ITERATIONS = 16U;
    constexpr uint32_t ENTRY_COUNT = ZONE_COUNT * FT_CARD_GAME_MAX_PLAYERS
        * FT_CARD_GAME_MAX_CARDS;
    const ft_bool shuffled = argument_count == 2 && ft_strcmp(arguments[1], "--shuffled") == 0;
    card_game_zone_store store;
    card_game_zone_store_snapshot snapshot{};
    if (store.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t zone_id = 1U;
    while (zone_id <= ZONE_COUNT)
    {
        if (store.register_zone({zone_id, FT_CARD_GAME_MAX_CARDS, 1U, FT_TRUE})
            != FT_ERR_SUCCESS)
            return (1);
        ++zone_id;
    }
    uint32_t player_id = 0U;
    uint32_t instance_id = 1U;
    while (player_id < FT_CARD_GAME_MAX_PLAYERS)
    {
        zone_id = 1U;
        while (zone_id <= ZONE_COUNT)
        {
            uint32_t index = 0U;
            while (index < FT_CARD_GAME_MAX_CARDS)
            {
                uint32_t assigned_id = instance_id;
                if (shuffled)
                    assigned_id = ((instance_id * 127U) % ENTRY_COUNT) + 1U;
                if (store.insert_bottom(player_id, zone_id, {assigned_id, zone_id}, 0U)
                    != FT_ERR_SUCCESS)
                    return (1);
                ++instance_id;
                ++index;
            }
            ++zone_id;
        }
        ++player_id;
    }
    if (store.get_snapshot(&snapshot) != FT_ERR_SUCCESS)
        return (1);

    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (store.apply_snapshot(snapshot) != FT_ERR_SUCCESS)
        {
            (void)store.release_snapshot(&snapshot);
            return (1);
        }
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    const uint32_t entry_count = snapshot.entry_count;
    (void)store.release_snapshot(&snapshot);
    card_game_zone_entry entry{};
    uint32_t expected_last_id = entry_count;
    if (shuffled)
        expected_last_id = 1U;
    if (store.peek_bottom(FT_CARD_GAME_MAX_PLAYERS - 1U, ZONE_COUNT, &entry)
        != FT_ERR_SUCCESS || entry.instance_id != expected_last_id)
        return (1);
    std::printf("zone snapshot %" PRIu32 " entries %.3f ms/restore\n",
        entry_count, elapsed / ITERATIONS);
    return (0);
}
