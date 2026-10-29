#include "../../Modules/CardGame/card_game_zone_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_card_game_zone_store_supports_configured_owner_zones)
{
    card_game_zone_store store;
    card_game_zone_store_definition definition;
    card_game_zone_entry first;
    card_game_zone_entry second;
    card_game_zone_entry result;
    uint64_t random_state;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize());
    definition.zone_id = 10U;
    definition.capacity = 3U;
    definition.allowed_card_type_mask = 1U << 0U;
    definition.owner_scoped = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_zone(definition));
    first.instance_id = 11U;
    first.card_id = 101U;
    second.instance_id = 12U;
    second.card_id = 102U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_bottom(2U, 10U, first, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_top(2U, 10U, second, 0U));
    first.instance_id = 13U;
    first.card_id = 103U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_at(2U, 10U, 1U, first, 0U));
    FT_ASSERT_EQ(3U, store.size(2U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.inspect(2U, 10U, 1U, &result));
    FT_ASSERT_EQ(13U, result.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.remove(2U, 10U, 13U, &result));
    FT_ASSERT_EQ(2U, store.size(2U, 10U));
    FT_ASSERT_EQ(0U, store.size(1U, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.peek_top(2U, 10U, &result));
    FT_ASSERT_EQ(12U, result.instance_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.peek_bottom(2U, 10U, &result));
    FT_ASSERT_EQ(11U, result.instance_id);
    random_state = 0xabcdef123456789ULL;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.shuffle(2U, 10U, &random_state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.pop_top(2U, 10U, &result));
    FT_ASSERT(result.instance_id == 11U || result.instance_id == 12U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.pop_bottom(2U, 10U, &result));
    FT_ASSERT(result.instance_id == 11U || result.instance_id == 12U);
    FT_ASSERT_EQ(FT_ERR_EMPTY, store.pop_top(2U, 10U, &result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    return (1);
}

FT_TEST(test_card_game_zone_store_enforces_type_capacity_and_duplicates)
{
    card_game_zone_store store;
    card_game_zone_store_definition definition;
    card_game_zone_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize());
    definition.zone_id = 11U;
    definition.capacity = 1U;
    definition.allowed_card_type_mask = 1U << 1U;
    definition.owner_scoped = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_zone(definition));
    entry.instance_id = 21U;
    entry.card_id = 201U;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, store.insert_top(0U, 11U, entry,
        0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_top(0U, 11U, entry, 1U));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, store.insert_top(0U, 11U, entry, 1U));
    entry.instance_id = 22U;
    entry.card_id = 202U;
    FT_ASSERT_EQ(FT_ERR_FULL, store.insert_top(0U, 11U, entry, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    return (1);
}

FT_TEST(test_card_game_zone_store_moves_instances_atomically)
{
    card_game_zone_store store;
    card_game_zone_store moved_store;
    card_game_zone_store_definition source_definition;
    card_game_zone_store_definition destination_definition;
    card_game_zone_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize());
    source_definition.zone_id = 20U;
    source_definition.capacity = 2U;
    source_definition.allowed_card_type_mask = 1U << 0U;
    source_definition.owner_scoped = FT_TRUE;
    destination_definition.zone_id = 21U;
    destination_definition.capacity = 1U;
    destination_definition.allowed_card_type_mask = 1U << 0U;
    destination_definition.owner_scoped = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_zone(source_definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.register_zone(destination_definition));
    entry.instance_id = 31U;
    entry.card_id = 301U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_bottom(0U, 20U, entry, 0U));
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, store.insert_bottom(0U, 21U, entry,
        0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.move_instance(0U, 20U, 21U, 31U,
        0U));
    FT_ASSERT_EQ(0U, store.size(0U, 20U));
    FT_ASSERT_EQ(1U, store.size(0U, 21U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, store.move_instance(0U, 21U, 20U,
        31U, 1U));
    FT_ASSERT_EQ(1U, store.size(0U, 21U));
    entry.instance_id = 32U;
    entry.card_id = 302U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.insert_bottom(0U, 20U, entry, 0U));
    FT_ASSERT_EQ(FT_ERR_FULL, store.move_instance(0U, 20U, 21U, 32U, 0U));
    FT_ASSERT_EQ(1U, store.size(0U, 21U));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, store.move_instance(0U, 20U, 21U, 31U,
        0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_store.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_store.move(store));
    FT_ASSERT_EQ(1U, moved_store.size(0U, 20U));
    FT_ASSERT_EQ(1U, moved_store.size(0U, 21U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_store.destroy());
    return (1);
}
