#include "../../../Modules/CardGame/card_game_collection.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    const ft_bool small = argument_count == 2 && ft_strcmp(arguments[1], "--small") == 0;
    uint32_t iterations = 1000U;
    uint32_t catalog_size = FT_CARD_GAME_MAX_SET_ENTRIES;
    uint32_t pack_size = FT_CARD_GAME_MAX_PACK_ENTRIES;
    if (small)
    {
        iterations = 100000U;
        catalog_size = 8U;
        pack_size = 3U;
    }
    card_game_collection_engine store;
    if (store.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 1U;
    while (index <= catalog_size)
    {
        const card_game_set_entry entry{
            index, index, 1U, index, CARD_GAME_RARITY_COMMON, 0U,
            index, 0U, FT_FALSE, FT_TRUE
        };
        if (store.register_set_entry(entry) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    card_game_product_definition product{};
    product.product_id = 1U;
    product.slot_count = 1U;
    product.slots[0] = {1U, CARD_GAME_RARITY_COMMON, 0U, pack_size, 3U, FT_FALSE};
    if (store.register_product(product) != FT_ERR_SUCCESS)
        return (1);
    uint64_t random_state = 0x123456789abcdef1ULL;
    uint32_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        card_game_pack_result pack;
        if (store.open_product(1U, &random_state, &pack) != FT_ERR_SUCCESS
            || pack.entry_count != pack_size)
            return (1);
        uint32_t entry = 0U;
        while (entry < pack.entry_count)
        {
            checksum = checksum * 31U + pack.printing_ids[entry];
            ++entry;
        }
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    index = 1U;
    while (index <= catalog_size)
    {
        uint32_t quantity;
        if (store.get_collection(index, &quantity) != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + quantity;
        ++index;
    }
    checksum ^= static_cast<uint32_t>(random_state ^ (random_state >> 32U));
    std::printf("collection checksum %" PRIu32 " %.3f us/op\n", checksum, elapsed / iterations);
    return (0);
}
