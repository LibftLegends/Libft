#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t make_prepared_craft_item(int32_t item_id, int32_t count,
    ft_sharedptr<game_item> *item)
{
    int32_t result;

    if (item == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = item->initialize(new game_item());
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = item->get()->initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    item->get()->set_item_id(item_id);
    item->get()->set_max_stack(64);
    item->get()->set_stack_size(count);
    item->get()->set_width(1);
    item->get()->set_height(1);
    return (FT_ERR_SUCCESS);
}

static int32_t append_prepared_ingredient(
    ft_vector<game_crafting_ingredient> &ingredients, int32_t item_id,
    int32_t count, int32_t rarity)
{
    game_crafting_ingredient ingredient;
    int32_t result = ingredient.initialize(item_id, count, rarity);

    if (result != FT_ERR_SUCCESS)
        return (result);
    return (ingredients.push_back(ingredient));
}

static int32_t register_prepared_recipe(game_crafting &crafting,
    int32_t recipe_id, int32_t input_id, int32_t input_count,
    int32_t output_id, int32_t output_count, ft_bool unlocked)
{
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> output;
    int32_t result = ingredients.initialize();

    if (result == FT_ERR_SUCCESS)
        result = append_prepared_ingredient(ingredients, input_id,
            input_count, -1);
    if (result == FT_ERR_SUCCESS)
        result = make_prepared_craft_item(output_id, 1, &output);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (crafting.register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, output_count, unlocked));
}

FT_TEST(test_prepared_craft_missing_recipe_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND,
        crafting.prepare_craft(inventory, 900, 1, &prepared));
    FT_ASSERT_EQ(3, inventory.count_item(7));
    FT_ASSERT_EQ(FT_FALSE, prepared.is_ready());
    return (1);
}

FT_TEST(test_prepared_craft_locked_recipe_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 51, 7,
        1, 8, 1, FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 2, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        crafting.prepare_craft(inventory, 51, 1, &prepared));
    FT_ASSERT_EQ(2, inventory.count_item(7));
    return (1);
}

FT_TEST(test_prepared_craft_output_full_rolls_back_all_inputs)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 52, 7,
        1, 8, 65, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_FULL,
        crafting.prepare_craft(inventory, 52, 1, &prepared));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_duplicate_ingredients_share_inventory_units)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(8, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(53,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        output, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR,
        crafting.prepare_craft(inventory, 53, 1, &prepared));
    FT_ASSERT_EQ(3, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_batch_overflow_does_not_change_inventory)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 54, 7,
        2, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 4, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crafting.prepare_craft(inventory, 54, FT_INT32_MAX, &prepared));
    FT_ASSERT_EQ(4, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_prepared_craft_uses_recipe_owned_output_definition)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> recipe_output;
    ft_sharedptr<game_item> caller_output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, append_prepared_ingredient(ingredients,
        7, 1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(8, 1,
        &recipe_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(55,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        recipe_output, 2));
    recipe_output->set_item_id(80);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(99, 1,
        &caller_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        crafting.craft_item(inventory, 55, caller_output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 55, 1, &prepared));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.commit_craft(inventory, &prepared));
    FT_ASSERT_EQ(0, inventory.count_item(7));
    FT_ASSERT_EQ(2, inventory.count_item(8));
    FT_ASSERT_EQ(0, inventory.count_item(80));
    FT_ASSERT_EQ(0, inventory.count_item(99));
    return (1);
}

FT_TEST(test_prepared_craft_rejects_changed_inventory_snapshot)
{
    game_crafting crafting;
    game_inventory inventory;
    game_crafting_prepared prepared;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> extra;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_prepared_recipe(crafting, 56, 7,
        1, 8, 1, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(7, 2, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        crafting.prepare_craft(inventory, 56, 1, &prepared));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_prepared_craft_item(9, 1, &extra));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(extra));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.commit_craft(inventory,
        &prepared));
    FT_ASSERT_EQ(2, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(9));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}
