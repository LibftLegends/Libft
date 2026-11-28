#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t query_test_make_item(int32_t id, int32_t count,
    int32_t rarity, ft_sharedptr<game_item> *item)
{
    int32_t result = item->initialize(new game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = item->get()->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    item->get()->set_item_id(id);
    item->get()->set_stack_size(count);
    item->get()->set_max_stack(64);
    item->get()->set_rarity(rarity);
    item->get()->set_width(1);
    item->get()->set_height(1);
    return (FT_ERR_SUCCESS);
}

static int32_t query_test_add_ingredient(
    ft_vector<game_crafting_ingredient> &ingredients,
    int32_t item_id, int32_t count, int32_t rarity)
{
    game_crafting_ingredient ingredient;
    int32_t result = ingredient.initialize(item_id, count, rarity);
    if (result == FT_ERR_SUCCESS)
        result = ingredients.push_back(ingredient);
    return (result);
}

FT_TEST(test_game_crafting_query_reports_state_and_duplicate_group_deficits)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_vector<game_crafting_missing_ingredient> missing;
    game_crafting_query_result query;
    ft_sharedptr<game_item> common;
    ft_sharedptr<game_item> rare;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(8, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_add_ingredient(ingredients, 17, 3, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_add_ingredient(ingredients, 17, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_add_ingredient(ingredients, 17, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(501,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients), FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_make_item(17, 4, 2, &rare));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_make_item(17, 2, 1, &common));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(rare));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(common));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe(501, inventory,
        &query, &missing));
    FT_ASSERT_EQ(FT_TRUE, query.found);
    FT_ASSERT_EQ(FT_TRUE, query.known);
    FT_ASSERT_EQ(FT_FALSE, query.craftable);
    FT_ASSERT_EQ(1U, query.missing_group_count);
    FT_ASSERT_EQ(1, query.missing_units);
    FT_ASSERT_EQ(1U, missing.size());
    FT_ASSERT_EQ(17, missing[0].item_id);
    FT_ASSERT_EQ(-1, missing[0].rarity);
    FT_ASSERT_EQ(2, missing[0].required_units);
    FT_ASSERT_EQ(1, missing[0].available_units);
    FT_ASSERT_EQ(1, missing[0].missing_units);
    FT_ASSERT_EQ(6, inventory.count_item(17));

    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, crafting.query_recipe(999, inventory,
        &query, &missing));
    FT_ASSERT_EQ(FT_FALSE, query.found);
    FT_ASSERT_EQ(0U, missing.size());
    return (1);
}

FT_TEST(test_game_crafting_query_locked_recipe_and_no_inventory_mutation)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_vector<game_crafting_missing_ingredient> missing;
    game_crafting_query_result query;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_add_ingredient(ingredients, 23, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(502,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients), FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, query_test_make_item(23, 2, 0, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe(502, inventory,
        &query, &missing));
    FT_ASSERT_EQ(FT_TRUE, query.found);
    FT_ASSERT_EQ(FT_FALSE, query.known);
    FT_ASSERT_EQ(FT_FALSE, query.craftable);
    FT_ASSERT_EQ(0U, query.missing_group_count);
    FT_ASSERT_EQ(FT_TRUE, missing.empty());
    FT_ASSERT_EQ(2, inventory.count_item(23));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.unlock_recipe(502));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.query_recipe(502, inventory,
        &query, &missing));
    FT_ASSERT_EQ(FT_TRUE, query.known);
    FT_ASSERT_EQ(FT_TRUE, query.craftable);
    FT_ASSERT_EQ(2, inventory.count_item(23));
    return (1);
}
