#include "../test_internal.hpp"
#include "../../Modules/Game/game_data_catalog.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int populate_ingredient(game_crafting_ingredient &ingredient, int item_id, int count, int rarity)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.initialize(item_id, count, rarity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredient.get_error());
    ingredient.set_item_id(item_id);
    ingredient.set_count(count);
    ingredient.set_rarity(rarity);
    return (1);
}

static int populate_loadout_entry(game_loadout_entry &entry, int slot, int item_id, int quantity)
{
    if (entry._initialised_state == FT_CLASS_STATE_INITIALISED)
        FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.initialize(slot, item_id, quantity));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    return (1);
}

static int assert_item_definition_equals(const game_item_definition &definition, int item_id, int rarity,
        int max_stack, int width, int height, int weight, int slot_requirement)
{
    FT_ASSERT_EQ(item_id, definition.get_item_id());
    FT_ASSERT_EQ(rarity, definition.get_rarity());
    FT_ASSERT_EQ(max_stack, definition.get_max_stack());
    FT_ASSERT_EQ(width, definition.get_width());
    FT_ASSERT_EQ(height, definition.get_height());
    FT_ASSERT_EQ(weight, definition.get_weight());
    FT_ASSERT_EQ(slot_requirement, definition.get_slot_requirement());
    return (1);
}

static int assert_single_ingredient(const game_recipe_blueprint &recipe, int item_id, int count, int rarity)
{
    const ft_vector<game_crafting_ingredient> &ingredients = recipe.get_ingredients();
    FT_ASSERT_EQ(1u, ingredients.size());
    FT_ASSERT_EQ(item_id, ingredients[0].get_item_id());
    FT_ASSERT_EQ(count, ingredients[0].get_count());
    FT_ASSERT_EQ(rarity, ingredients[0].get_rarity());
    return (1);
}

static int assert_loadout_entry_values(const game_loadout_blueprint &loadout, size_t index,
        int slot, int item_id, int quantity)
{
    const ft_vector<game_loadout_entry> &entries = loadout.get_entries();
    FT_ASSERT_EQ(slot, entries[index].get_slot());
    FT_ASSERT_EQ(item_id, entries[index].get_item_id());
    FT_ASSERT_EQ(quantity, entries[index].get_quantity());
    return (1);
}

FT_TEST(test_catalog_register_and_fetch_item)
{
    game_data_catalog catalog;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    game_item_definition definition;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.initialize(1, 2, 5, 3, 4, 6, 7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.get_error());
    game_item_definition fetched;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_item_definition(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_item_definition(1, fetched));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    assert_item_definition_equals(fetched, 1, 2, 5, 3, 4, 6, 7);
    return (1);
}

FT_TEST(test_catalog_fetch_missing_item)
{
    game_data_catalog catalog;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    game_item_definition definition;

    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, catalog.fetch_item_definition(99, definition));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, catalog.get_error());
    return (1);
}

FT_TEST(test_catalog_item_fetch_keeps_destination_mutex_and_values_on_failure)
{
    game_data_catalog catalog;
    game_item_definition definition;
    game_item_definition destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.initialize(1, 2, 5, 3, 4, 6, 7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(8, 0, 1, 1, 1, 0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_item_definition(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    definition.set_max_stack(11);

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t register_error = catalog.register_item_definition(definition);
    const int32_t fetch_error = catalog.fetch_item_definition(1, destination);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, register_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_error);
    FT_ASSERT_EQ(8, destination.get_item_id());
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_item_definition(1, destination));
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(1, assert_item_definition_equals(destination, 1, 2, 5, 3, 4, 6, 7));
    return (1);
}

FT_TEST(test_catalog_reports_map_lock_failures_for_all_record_types)
{
    game_data_catalog catalog;
    game_item_definition definition;
    game_recipe_blueprint recipe;
    game_loadout_blueprint loadout;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.initialize(1, 2, 5, 3, 4, 6, 7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.initialize());
    recipe.set_recipe_id(5);
    loadout.set_loadout_id(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_item_definition(definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_recipe(recipe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_loadout(loadout));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_item_definitions().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_recipes().enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_loadouts().enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t register_item_error = catalog.register_item_definition(definition);
    const int32_t register_recipe_error = catalog.register_recipe(recipe);
    const int32_t register_loadout_error = catalog.register_loadout(loadout);
    const int32_t fetch_item_error = catalog.fetch_item_definition(1, definition);
    const int32_t fetch_recipe_error = catalog.fetch_recipe(5, recipe);
    const int32_t fetch_loadout_error = catalog.fetch_loadout(7, loadout);
    const int32_t catalog_error = catalog.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, register_item_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, register_recipe_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, register_loadout_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_item_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_recipe_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, fetch_loadout_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, catalog_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_item_definition(1, definition));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_recipe(5, recipe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_loadout(7, loadout));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, catalog.fetch_loadout(8, loadout));
    FT_ASSERT_EQ(7, loadout.get_loadout_id());
    return (1);
}

FT_TEST(test_catalog_recipe_copy_isolated)
{
    game_data_catalog catalog;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    ft_vector<game_crafting_ingredient> ingredients;
    game_crafting_ingredient ingredient;
    game_recipe_blueprint recipe;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe.initialize(5, 42, ingredients));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe.get_error());
    game_recipe_blueprint first_fetch;
    game_recipe_blueprint second_fetch;

    FT_ASSERT_EQ(1, populate_ingredient(ingredient, 2, 3, 1));
    ingredients.push_back(ingredient);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.get_error());
    recipe.set_ingredients(ingredients);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_recipe(recipe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_recipe(5, first_fetch));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    assert_single_ingredient(first_fetch, 2, 3, 1);

    ft_vector<game_crafting_ingredient> &first_ingredients = first_fetch.get_ingredients();
    first_ingredients[0].set_count(9);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_recipe(5, second_fetch));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    assert_single_ingredient(second_fetch, 2, 3, 1);
    return (1);
}

FT_TEST(test_catalog_loadout_copy_isolated)
{
    game_data_catalog catalog;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    ft_vector<game_loadout_entry> entries;
    game_loadout_blueprint loadout;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.initialize(7, entries));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.get_error());
    game_loadout_blueprint first_fetch;
    game_loadout_blueprint second_fetch;

    game_loadout_entry entry;
    FT_ASSERT_EQ(1, populate_loadout_entry(entry, 0, 10, 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    entries.push_back(entry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.destroy());
    FT_ASSERT_EQ(1, populate_loadout_entry(entry, 1, 11, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entry.get_error());
    entries.push_back(entry);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.get_error());
    loadout.set_entries(entries);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_loadout(loadout));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_loadout(7, first_fetch));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    const ft_vector<game_loadout_entry> &first_entries = first_fetch.get_entries();
    FT_ASSERT_EQ(2u, first_entries.size());
    assert_loadout_entry_values(first_fetch, 0, 0, 10, 2);
    assert_loadout_entry_values(first_fetch, 1, 1, 11, 1);

    ft_vector<game_loadout_entry> &first_mutable_entries = first_fetch.get_entries();
    first_mutable_entries[0].set_quantity(5);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_loadout(7, second_fetch));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.get_error());
    assert_loadout_entry_values(second_fetch, 0, 0, 10, 2);
    assert_loadout_entry_values(second_fetch, 1, 1, 11, 1);
    return (1);
}

FT_TEST(test_catalog_blueprints_keep_previous_values_on_copy_failure)
{
    game_data_catalog catalog;
    game_recipe_blueprint recipe;
    game_loadout_blueprint loadout;
    game_recipe_blueprint changed_recipe;
    game_loadout_blueprint changed_loadout;
    ft_vector<game_crafting_ingredient> ingredients;
    ft_vector<game_loadout_entry> entries;
    game_crafting_ingredient ingredient;
    game_loadout_entry entry;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.initialize());
    FT_ASSERT_EQ(1, populate_ingredient(ingredient, 2, 3, 1));
    FT_ASSERT_EQ(1, populate_loadout_entry(entry, 0, 2, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(ingredient));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(entry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe.initialize(5, 42, ingredients));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.initialize(7, entries));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_recipe(recipe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.register_loadout(loadout));
    while (ingredients.size() < 16U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, ingredients.push_back(ingredient));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, entries.push_back(entry));
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_recipe.initialize(5, 99, ingredients));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_loadout.initialize(7, entries));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    recipe.set_ingredients(ingredients);
    const int32_t ingredient_error = recipe.get_error();
    loadout.set_entries(entries);
    const int32_t entry_error = loadout.get_error();
    const int32_t register_recipe_error = catalog.register_recipe(changed_recipe);
    const int32_t register_loadout_error = catalog.register_loadout(changed_loadout);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, ingredient_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, entry_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, register_recipe_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, register_loadout_error);
    FT_ASSERT_EQ(1U, recipe.get_ingredients().size());
    FT_ASSERT_EQ(1U, loadout.get_entries().size());
    recipe.set_ingredients(recipe.get_ingredients());
    loadout.set_entries(loadout.get_entries());
    FT_ASSERT_EQ(1U, recipe.get_ingredients().size());
    FT_ASSERT_EQ(1U, loadout.get_entries().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_recipe(5, recipe));
    FT_ASSERT_EQ(42, recipe.get_result_item_id());
    FT_ASSERT_EQ(1U, recipe.get_ingredients().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, catalog.fetch_loadout(7, loadout));
    FT_ASSERT_EQ(1U, loadout.get_entries().size());

    const auto *ingredient_storage = changed_recipe.get_ingredients().begin();
    const auto *entry_storage = changed_loadout.get_entries().begin();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t recipe_move_error = recipe.move(changed_recipe);
    const int32_t loadout_move_error = loadout.move(changed_loadout);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe_move_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout_move_error);
    FT_ASSERT(ingredient_storage == recipe.get_ingredients().begin());
    FT_ASSERT(entry_storage == loadout.get_entries().begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_recipe.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, changed_loadout.initialize());
    return (1);
}
