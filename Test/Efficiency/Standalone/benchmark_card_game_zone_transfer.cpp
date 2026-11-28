#include "../../../Modules/CardGame/card_game_zone_store.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t SOURCE_ZONE = FT_CARD_GAME_MAX_ZONES - 1U;
    constexpr uint32_t TARGET_ZONE = FT_CARD_GAME_MAX_ZONES;
    constexpr uint32_t TRANSFER_INSTANCE = 2U * FT_CARD_GAME_MAX_CARDS;
    constexpr uint32_t ITERATIONS = 500000U;
    card_game_zone_store store;
    if (store.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t zone_id = 1U;
    while (zone_id <= TARGET_ZONE)
    {
        if (store.register_zone({zone_id, FT_CARD_GAME_MAX_CARDS, 1U, FT_TRUE})
            != FT_ERR_SUCCESS)
            return (1);
        ++zone_id;
    }
    uint32_t index = 1U;
    while (index < FT_CARD_GAME_MAX_CARDS)
    {
        if (store.insert_bottom(0U, SOURCE_ZONE, {index, 1U}, 0U) != FT_ERR_SUCCESS
            || store.insert_bottom(0U, TARGET_ZONE,
                {FT_CARD_GAME_MAX_CARDS + index, 1U}, 0U) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (store.insert_bottom(0U, SOURCE_ZONE, {TRANSFER_INSTANCE, 2U}, 0U)
        != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (store.move_instance(0U, SOURCE_ZONE, TARGET_ZONE, TRANSFER_INSTANCE, 0U)
            != FT_ERR_SUCCESS
            || store.move_instance(0U, TARGET_ZONE, SOURCE_ZONE, TRANSFER_INSTANCE, 0U)
                != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    card_game_zone_entry entry{};
    if (store.inspect(0U, SOURCE_ZONE, FT_CARD_GAME_MAX_CARDS - 1U, &entry)
        != FT_ERR_SUCCESS || entry.instance_id != TRANSFER_INSTANCE || entry.card_id != 2U
        || store.size(0U, TARGET_ZONE) != FT_CARD_GAME_MAX_CARDS - 1U)
        return (1);
    std::printf("zone transfer %" PRIu32 " occupied source slots %.3f ns/op\n",
        FT_CARD_GAME_MAX_CARDS, elapsed / (2U * ITERATIONS));
    return (0);
}
