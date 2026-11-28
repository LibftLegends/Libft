#include "../test_internal.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Errno/errno.hpp"

static void make_ingredient(game_crafting_ingredient &ingredient, int32_t id,
    int32_t count, int32_t rarity)
{
    (void)ingredient.initialize(id, count, rarity);
    return ;
}

FT_TEST(test_game_crafting_ingredient_default_rarity)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(1, 1, -1));
    FT_ASSERT_EQ(-1, ingredient.get_rarity());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_values)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(10, 4, 2));
    FT_ASSERT_EQ(10, ingredient.get_item_id());
    FT_ASSERT_EQ(4, ingredient.get_count());
    FT_ASSERT_EQ(2, ingredient.get_rarity());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_set_item_id)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(1, 1, -1));
    ingredient.set_item_id(12);
    FT_ASSERT_EQ(12, ingredient.get_item_id());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_set_count)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(1, 1, -1));
    ingredient.set_count(12);
    FT_ASSERT_EQ(12, ingredient.get_count());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_set_rarity)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(1, 1, -1));
    ingredient.set_rarity(3);
    FT_ASSERT_EQ(3, ingredient.get_rarity());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_copy)
{
    game_crafting_ingredient source;
    game_crafting_ingredient copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 3, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(2, copy.get_item_id());
    FT_ASSERT_EQ(3, copy.get_count());
    FT_ASSERT_EQ(4, copy.get_rarity());
    return (1);
}

FT_TEST(test_game_crafting_ingredient_move)
{
    game_crafting_ingredient source;
    game_crafting_ingredient destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2, 3, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(2, destination.get_item_id());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    return (1);
}

FT_TEST(test_game_crafting_ingredient_destroy_reset)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(2, 3, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.destroy());
    FT_ASSERT_EQ(0, ingredient._item_id);
    FT_ASSERT_EQ(0, ingredient._count);
    FT_ASSERT_EQ(-1, ingredient._rarity);
    return (1);
}

FT_TEST(test_game_crafting_ingredient_thread_safety)
{
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(1, 1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, ingredient.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.disable_thread_safety());
    return (1);
}

FT_TEST(test_game_crafting_recipe_registration)
{
    game_crafting crafting;
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient ingredient;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    make_ingredient(ingredient, 5, 2, -1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(ingredient));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(22, static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients)));
    FT_ASSERT(crafting.get_recipes().find(22) != crafting.get_recipes().end());
    return (1);
}

FT_TEST(test_game_crafting_empty_recipe_registration)
{
    game_crafting crafting;
    ft_vector<game_crafting_ingredient> ingredients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(23, static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients)));
    FT_ASSERT(crafting.get_recipes().find(23) != crafting.get_recipes().end());
    return (1);
}

FT_TEST(test_game_crafting_recipe_replacement)
{
    game_crafting crafting;
    ft_vector<game_crafting_ingredient> first;
    ft_vector<game_crafting_ingredient> second;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(24, static_cast<ft_vector<game_crafting_ingredient> &&>(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.register_recipe(24, static_cast<ft_vector<game_crafting_ingredient> &&>(second)));
    FT_ASSERT(crafting.get_recipes().find(24) != crafting.get_recipes().end());
    return (1);
}

FT_TEST(test_game_crafting_missing_recipe)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_sharedptr<game_item> result(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(10, 10));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, crafting.craft_item(inventory, 999, result));
    return (1);
}

FT_TEST(test_game_crafting_null_result)
{
    game_crafting crafting;
    game_inventory inventory;
    ft_sharedptr<game_item> result;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(10, 10));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crafting.craft_item(inventory, 1, result));
    return (1);
}

FT_TEST(test_game_crafting_thread_safety)
{
    game_crafting crafting;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, crafting.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, crafting.is_thread_safe());
    return (1);
}

FT_TEST(test_game_crafting_recipe_const_getter)
{
    game_crafting crafting;
    const game_crafting &constant_crafting = crafting;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, constant_crafting.get_error());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), constant_crafting.get_recipes().size());
    return (1);
}

FT_TEST(test_game_crafting_move_transfers_recipe_table)
{
    game_crafting source;
    game_crafting destination;
    ft_vector<game_crafting_ingredient> ingredients;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_recipe(25, static_cast<ft_vector<game_crafting_ingredient> &&>(ingredients)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT(destination.get_recipes().find(25) != destination.get_recipes().end());
    return (1);
}

FT_TEST(test_game_crafting_destroy_resets_state)
{
    game_crafting crafting;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.destroy());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, crafting._initialised_state);
    return (1);
}

FT_TEST(test_game_crafting_empty_table_after_initialize)
{
    game_crafting crafting;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.initialize());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), crafting.get_recipes().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crafting.get_error());
    return (1);
}
