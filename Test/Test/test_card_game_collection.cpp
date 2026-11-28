#include "../test_internal.hpp"
#include "../../Modules/CardGame/card_game_collection.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static card_game_set_entry card_game_collection_test_entry(
    uint32_t printing_id, uint32_t rarity_mask, uint32_t treatment_id,
    uint32_t forced_rarity_mask) noexcept
{
    card_game_set_entry entry;

    entry.printing_id = printing_id;
    entry.definition_id = printing_id + 100U;
    entry.set_id = 1U;
    entry.collector_number = printing_id;
    entry.rarity_mask = rarity_mask;
    entry.forced_rarity_mask = forced_rarity_mask;
    entry.selection_weight = 1U;
    entry.treatment_id = treatment_id;
    entry.supplemental = FT_FALSE;
    entry.deck_legal = FT_TRUE;
    return (entry);
}

FT_TEST(test_card_game_collection_weighted_draw_preserves_seeded_order)
{
    card_game_collection_engine engine;
    card_game_product_definition product{};
    card_game_pack_result pack{};
    const uint32_t weights[] = {1U, 3U, 7U};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    uint32_t index = 0U;
    while (index < 3U)
    {
        card_game_set_entry entry = card_game_collection_test_entry(index + 1U,
            CARD_GAME_RARITY_COMMON, 0U, 0U);
        entry.selection_weight = weights[index];
        FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_set_entry(entry));
        ++index;
    }
    product.product_id = 10U;
    product.slot_count = 1U;
    product.slots[0] = {1U, CARD_GAME_RARITY_COMMON, 0U, 3U, 5U, FT_FALSE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_product(product));
    uint64_t seed = 0x123456789abcdef1ULL;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.open_product(10U, &seed, &pack));
    FT_ASSERT_EQ(3U, pack.entry_count);
    FT_ASSERT_EQ(2U, pack.printing_ids[0]);
    FT_ASSERT_EQ(1U, pack.printing_ids[1]);
    FT_ASSERT_EQ(3U, pack.printing_ids[2]);
    FT_ASSERT_EQ(0xdac132b57f5db5eaULL, seed);
    return (1);
}

#ifdef LIBFT_TEST_BUILD
FT_TEST(test_card_game_collection_pack_quantity_overflow_leaves_all_counts_unchanged)
{
    card_game_collection_engine engine;
    card_game_product_definition product{};
    card_game_pack_result pack{};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_set_entry(
        card_game_collection_test_entry(7U, CARD_GAME_RARITY_COMMON, 0U, 0U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_set_entry(
        card_game_collection_test_entry(9U, CARD_GAME_RARITY_RARE, 0U, 0U)));
    product.product_id = 10U;
    product.slot_count = 2U;
    product.slots[0] = {1U, CARD_GAME_RARITY_COMMON, 0U, 1U, 1U, FT_TRUE};
    product.slots[1] = {1U, CARD_GAME_RARITY_RARE, 0U, 2U, 1U, FT_TRUE};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_product(product));
    engine._quantities[1] = UINT32_MAX - 1U;
    pack.product_id = 99U;
    uint64_t seed = 123U;
    FT_ASSERT_EQ(FT_ERR_FULL, engine.open_product(10U, &seed, &pack));
    uint32_t quantity = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_collection(7U, &quantity));
    FT_ASSERT_EQ(0U, quantity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_collection(9U, &quantity));
    FT_ASSERT_EQ(UINT32_MAX - 1U, quantity);
    FT_ASSERT_EQ(99U, pack.product_id);
    return (1);
}
#endif

FT_TEST(test_card_game_collection_opens_deterministic_configured_slots)
{
    card_game_collection_engine first;
    card_game_collection_engine second;
    card_game_product_definition product;
    card_game_pack_result first_pack;
    card_game_pack_result second_pack;
    card_game_set_entry entry;
    uint64_t first_seed;
    uint64_t second_seed;
    uint32_t index;
    uint32_t quantity;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    entry = card_game_collection_test_entry(1U, CARD_GAME_RARITY_COMMON, 0U,
        0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_set_entry(entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_set_entry(entry));
    entry = card_game_collection_test_entry(2U, CARD_GAME_RARITY_RARE, 0U,
        0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_set_entry(entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_set_entry(entry));
    entry = card_game_collection_test_entry(3U, CARD_GAME_RARITY_RARE, 9U,
        CARD_GAME_RARITY_RARE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_set_entry(entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_set_entry(entry));
    product.product_id = 5U;
    product.slot_count = 2U;
    product.slots[0].set_id = 1U;
    product.slots[0].rarity_mask = CARD_GAME_RARITY_COMMON;
    product.slots[0].treatment_id = 0U;
    product.slots[0].card_count = 1U;
    product.slots[0].selection_weight = 1U;
    product.slots[0].allow_duplicates = FT_TRUE;
    product.slots[1].set_id = 1U;
    product.slots[1].rarity_mask = CARD_GAME_RARITY_RARE;
    product.slots[1].treatment_id = 0U;
    product.slots[1].card_count = 2U;
    product.slots[1].selection_weight = 1U;
    product.slots[1].allow_duplicates = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.register_product(product));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.register_product(product));
    first_seed = 0x123456789abcdef1ULL;
    second_seed = first_seed;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.open_product(5U, &first_seed,
        &first_pack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.open_product(5U, &second_seed,
        &second_pack));
    FT_ASSERT_EQ(3U, first_pack.entry_count);
    index = 0U;
    while (index < first_pack.entry_count)
    {
        FT_ASSERT_EQ(first_pack.printing_ids[index],
            second_pack.printing_ids[index]);
        index += 1U;
    }
    FT_ASSERT_EQ(1U, first_pack.printing_ids[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.get_collection(1U, &quantity));
    FT_ASSERT_EQ(1U, quantity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.destroy());
    return (1);
}

FT_TEST(test_card_game_collection_forced_rarity_and_unavailable_product_are_safe)
{
    card_game_collection_engine engine;
    card_game_product_definition product;
    card_game_pack_result pack;
    card_game_set_entry entry;
    uint64_t random_state;
    uint32_t quantity;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    entry = card_game_collection_test_entry(10U, CARD_GAME_RARITY_COMMON,
        0U, CARD_GAME_RARITY_COMMON);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_set_entry(entry));
    product.product_id = 7U;
    product.slot_count = 1U;
    product.slots[0].set_id = 1U;
    product.slots[0].rarity_mask = CARD_GAME_RARITY_RARE;
    product.slots[0].treatment_id = 0U;
    product.slots[0].card_count = 1U;
    product.slots[0].selection_weight = 1U;
    product.slots[0].allow_duplicates = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_product(product));
    FT_ASSERT_EQ(1U, engine.product_count());
    random_state = 1U;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.open_product(7U, &random_state,
        &pack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_collection(10U, &quantity));
    FT_ASSERT_EQ(0U, quantity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}

FT_TEST(test_card_game_collection_enforces_slot_duplicate_policy)
{
    card_game_collection_engine engine;
    card_game_product_definition product;
    card_game_pack_result pack;
    card_game_set_entry entry;
    uint64_t random_state;
    uint32_t quantity;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.initialize());
    entry = card_game_collection_test_entry(20U, CARD_GAME_RARITY_COMMON,
        0U, 0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_set_entry(entry));
    product.product_id = 8U;
    product.slot_count = 1U;
    product.slots[0].set_id = 1U;
    product.slots[0].rarity_mask = CARD_GAME_RARITY_COMMON;
    product.slots[0].treatment_id = 0U;
    product.slots[0].card_count = 2U;
    product.slots[0].selection_weight = 1U;
    product.slots[0].allow_duplicates = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.register_product(product));
    random_state = 7U;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, engine.open_product(8U, &random_state,
        &pack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.get_collection(20U, &quantity));
    FT_ASSERT_EQ(0U, quantity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, engine.destroy());
    return (1);
}
