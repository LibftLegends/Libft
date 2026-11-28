#include "../test_internal.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_economy_catalog_replacement_keeps_previous_prices_on_failure)
{
    game_economy_table table;
    game_economy_table incoming;
    game_price_definition original;
    game_price_definition replacement;
    game_price_definition result;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, incoming.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_price_definitions().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(2, 1, 45, 20, 70));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.initialize(9, 3, 165, 60, 185));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.register_price_definition(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, incoming.register_price_definition(replacement));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    table.set_price_definitions(incoming.get_price_definitions());
    const int32_t update_error = table.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, update_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_price_definition(2, result));
    FT_ASSERT_EQ(45, result.get_base_value());
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, table.fetch_price_definition(9, result));
    FT_ASSERT_EQ(45, result.get_base_value());

    table.set_price_definitions(incoming.get_price_definitions());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    table.set_price_definitions(table.get_price_definitions());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.fetch_price_definition(9, result));
    FT_ASSERT_EQ(9, result.get_item_id());
    FT_ASSERT_EQ(3, result.get_rarity());
    FT_ASSERT_EQ(165, result.get_base_value());
    FT_ASSERT_EQ(60, result.get_minimum_value());
    FT_ASSERT_EQ(185, result.get_maximum_value());
    return (1);
}
