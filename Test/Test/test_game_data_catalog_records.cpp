#include "../test_internal.hpp"
#include "../../Modules/Game/game_data_catalog.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_item_definition_copy_move)
{
    game_item_definition original;
    game_item_definition copy;
    game_item_definition moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(5, 3, 10, 2, 4, 6, 7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(original));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(ft_move(original)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());

    FT_ASSERT_EQ(5, copy.get_item_id());
    FT_ASSERT_EQ(3, copy.get_rarity());
    FT_ASSERT_EQ(10, copy.get_max_stack());
    FT_ASSERT_EQ(2, copy.get_width());
    FT_ASSERT_EQ(4, copy.get_height());
    FT_ASSERT_EQ(6, copy.get_weight());
    FT_ASSERT_EQ(7, copy.get_slot_requirement());

    FT_ASSERT_EQ(5, moved.get_item_id());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, original._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.get_error());
    return (1);
}

FT_TEST(test_recipe_blueprint_copy_move)
{
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient ingredient;
    game_recipe_blueprint blueprint;
    game_recipe_blueprint copy;
    game_recipe_blueprint moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(9, 2, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.get_error());
    ingredient.set_item_id(9);
    ingredient.set_count(2);
    ingredient.set_rarity(4);
    ingredients.push_back(ingredient);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.initialize(12, 30, ingredients));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(blueprint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_error());
    FT_ASSERT_EQ(12, copy.get_recipe_id());
    FT_ASSERT_EQ(30, copy.get_result_item_id());
    FT_ASSERT_EQ(1u, copy.get_ingredients().size());
    FT_ASSERT_EQ(9, copy.get_ingredients()[0].get_item_id());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(ft_move(blueprint)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(12, moved.get_recipe_id());
    FT_ASSERT_EQ(30, moved.get_result_item_id());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, blueprint._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.get_error());
    return (1);
}

FT_TEST(test_loadout_entry_copy_move)
{
    game_loadout_entry entry;
    game_loadout_entry copied;
    game_loadout_entry moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize(2, 15, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(ft_move(entry)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());

    FT_ASSERT_EQ(2, copied.get_slot());
    FT_ASSERT_EQ(15, copied.get_item_id());
    FT_ASSERT_EQ(3, copied.get_quantity());

    FT_ASSERT_EQ(2, moved.get_slot());
    FT_ASSERT_EQ(15, moved.get_item_id());
    FT_ASSERT_EQ(3, moved.get_quantity());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, entry._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    return (1);
}

FT_TEST(test_loadout_blueprint_copy_move)
{
    ft_vector<game_loadout_entry> entries;
    game_loadout_entry entry;
    game_loadout_blueprint blueprint;
    game_loadout_blueprint copy;
    game_loadout_blueprint moved;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize(0, 7, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    entry.set_slot(0);
    entry.set_item_id(7);
    entry.set_quantity(1);
    entries.push_back(entry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    entry.set_slot(1);
    entry.set_item_id(8);
    entry.set_quantity(2);
    entries.push_back(entry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.initialize(20, entries));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(blueprint));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.get_error());
    FT_ASSERT_EQ(20, copy.get_loadout_id());
    FT_ASSERT_EQ(2u, copy.get_entries().size());
    FT_ASSERT_EQ(0, copy.get_entries()[0].get_slot());
    FT_ASSERT_EQ(7, copy.get_entries()[0].get_item_id());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.initialize(ft_move(blueprint)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.get_error());
    FT_ASSERT_EQ(20, moved.get_loadout_id());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, blueprint._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, blueprint.get_error());
    FT_ASSERT_EQ(2u, moved.get_entries().size());
    return (1);
}
FT_TEST(test_catalog_loadout_entry_move_releases_source_and_destination_mutexes)
{
    game_loadout_entry source;
    game_loadout_entry destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(1, 2, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(1, destination.get_slot());
    FT_ASSERT_EQ(2, destination.get_item_id());
    FT_ASSERT_EQ(3, destination.get_quantity());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    return (1);
}
