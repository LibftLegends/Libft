#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t make_crafting_test_item(int32_t item_id,
    int32_t stack_size, ft_sharedptr<game_item> *item)
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
    item->get()->set_stack_size(stack_size);
    item->get()->set_width(1);
    item->get()->set_height(1);
    return (FT_ERR_SUCCESS);
}

static int32_t register_test_recipe(game_crafting &crafting,
    int32_t recipe_id, int32_t item_id, int32_t count, ft_bool unlocked)
{
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient ingredient;
    int32_t operation_error;

    operation_error = ingredients.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = ingredient.initialize(item_id, count, -1);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = ingredients.push_back(ingredient);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    return (crafting.register_recipe(recipe_id,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients),
        unlocked));
}

FT_TEST(test_game_crafting_locked_recipe_requires_unlock)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> replacement_input;
    ft_sharedptr<game_item> output;
    ft_bool unlocked;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_test_recipe(crafting, 41, 7, 1,
        FT_FALSE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(8, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.is_recipe_unlocked(41, &unlocked));
    FT_ASSERT_EQ(FT_FALSE, unlocked);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.craft_item(inventory, 41,
        output));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.unlock_recipe(41));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.is_recipe_unlocked(41, &unlocked));
    FT_ASSERT_EQ(FT_TRUE, unlocked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.craft_item(inventory, 41, output));
    FT_ASSERT_EQ(0, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 1,
        &replacement_input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(replacement_input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.lock_recipe(41));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.is_recipe_unlocked(41, &unlocked));
    FT_ASSERT_EQ(FT_FALSE, unlocked);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, crafting.craft_item(inventory, 41,
        output));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(8));
    return (1);
}

FT_TEST(test_game_crafting_legacy_preserves_slots_and_advances_revision)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_sharedptr<game_item> output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_test_recipe(crafting, 51, 7, 2, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(8, 1, &output));
    game_inventory_snapshot snapshot = {};
    snapshot.capacity = 4U;
    snapshot.next_slot = 512;
    snapshot.used_slots = 3U;
    snapshot.current_weight = 6;
    snapshot.revision = 37U;
    game_inventory_item_snapshot item = {};
    item.slot = 400;
    item.item_id = 7;
    item.stack_size = 3;
    item.max_stack = 64;
    item.width = 1;
    item.height = 1;
    snapshot.items.push_back(item);
    item.slot = 7;
    item.stack_size = 1;
    snapshot.items.push_back(item);
    item.slot = 14;
    item.item_id = 9;
    item.stack_size = 2;
    snapshot.items.push_back(item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.restore_snapshot(snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.craft_item(inventory, 51, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_snapshot(snapshot));
    FT_ASSERT_EQ(4U, snapshot.items.size());
    FT_ASSERT_EQ(400, snapshot.items[0].slot);
    FT_ASSERT_EQ(1, snapshot.items[0].stack_size);
    FT_ASSERT_EQ(7, snapshot.items[1].slot);
    FT_ASSERT_EQ(1, snapshot.items[1].stack_size);
    FT_ASSERT_EQ(14, snapshot.items[2].slot);
    FT_ASSERT_EQ(512, snapshot.items[3].slot);
    FT_ASSERT_EQ(5, snapshot.current_weight);
    FT_ASSERT_EQ(38U, snapshot.revision);
    return (1);
}

FT_TEST(test_game_crafting_failed_output_keeps_inputs)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> retained;
    ft_sharedptr<game_item> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_test_recipe(crafting, 42, 7, 1,
        FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 1, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(9, 1, &retained));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(8, 2, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(retained));
    FT_ASSERT_EQ(FT_ERR_FULL, crafting.craft_item(inventory, 42, output));
    FT_ASSERT_EQ(1, inventory.count_item(7));
    FT_ASSERT_EQ(1, inventory.count_item(9));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_game_crafting_duplicate_ingredients_cannot_reuse_units)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient first_ingredient;
    game_crafting_ingredient second_ingredient;
    ft_sharedptr<game_item> input;
    ft_sharedptr<game_item> output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ingredient.initialize(7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_ingredient.initialize(7, 2, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(first_ingredient));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(second_ingredient));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(43,
        static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 3, &input));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(8, 1, &output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(input));
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR,
        crafting.craft_item(inventory, 43, output));
    FT_ASSERT_EQ(3, inventory.count_item(7));
    FT_ASSERT_EQ(0, inventory.count_item(8));
    return (1);
}

FT_TEST(test_game_inventory_failed_add_does_not_partially_merge_stack)
{
    game_inventory inventory;
    ft_sharedptr<game_item> existing;
    ft_sharedptr<game_item> incoming;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 60, &existing));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, make_crafting_test_item(7, 10, &incoming));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(existing));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(incoming));
    FT_ASSERT_EQ(60, inventory.count_item(7));
    FT_ASSERT_EQ(1U, inventory.get_used());
    return (1);
}
