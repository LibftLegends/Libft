#include "../../Modules/Game/game_data_catalog.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void data_catalog_copy_uninitialised(game_data_catalog &value)
{
    game_data_catalog source;

    (void)value.initialize(source);
    return ;
}

static void data_catalog_move_initialize_uninitialised(game_data_catalog &value)
{
    game_data_catalog source;

    (void)value.initialize(static_cast<game_data_catalog &&>(source));
    return ;
}

static void data_catalog_move_uninitialised(game_data_catalog &value)
{
    game_data_catalog source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_data_catalog_copy_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_data_catalog>(
                        data_catalog_copy_uninitialised));
    return (1);
}

FT_TEST(test_game_data_catalog_move_initialize_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_data_catalog>(
                        data_catalog_move_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_data_catalog_move_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_data_catalog>(
                        data_catalog_move_uninitialised));
    return (1);
}

FT_TEST(test_game_data_catalog_copy_destroyed_source_propagates)
{
    game_data_catalog source;
    game_data_catalog destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_data_catalog_move_destroyed_source_propagates)
{
    game_data_catalog source;
    game_data_catalog destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_data_catalog &&>(source)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_data_catalog_copy_into_initialized_destination_commits)
{
    game_data_catalog source;
    game_data_catalog destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(0, destination.get_recipes().size());
    return (1);
}

FT_TEST(test_game_data_catalog_move_into_initialized_destination_commits)
{
    game_data_catalog source;
    game_data_catalog destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_data_catalog &&>(source)));
    FT_ASSERT_EQ(0, destination.get_loadouts().size());
    return (1);
}

FT_TEST(test_game_data_catalog_self_copy_is_safe)
{
    game_data_catalog value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(value));
    return (1);
}

FT_TEST(test_game_data_catalog_move_transfers_all_maps_without_allocation)
{
    game_data_catalog source;
    game_data_catalog destination;
    game_item_definition item;
    game_recipe_blueprint recipe;
    game_loadout_blueprint loadout;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize(1, 2, 64, 1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recipe.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.initialize());
    recipe.set_recipe_id(2);
    loadout.set_loadout_id(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_item_definition(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_recipe(recipe));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_loadout(loadout));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_recipes().enable_thread_safety());
    const auto *stored_item = source.get_item_definitions().find(1);
    const auto *stored_recipe = source.get_recipes().find(2);
    const auto *stored_loadout = source.get_loadouts().find(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT(stored_item == destination.get_item_definitions().find(1));
    FT_ASSERT(stored_recipe == destination.get_recipes().find(2));
    FT_ASSERT(stored_loadout == destination.get_loadouts().find(3));
    FT_ASSERT_EQ(FT_FALSE, destination.get_recipes().is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_recipes().size());
    return (1);
}

FT_TEST(test_game_data_catalog_preserves_source_after_failed_copy_and_move)
{
    game_data_catalog source;
    game_data_catalog copied;
    game_data_catalog moved;
    game_item_definition item;
    game_loadout_blueprint loadout;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize(1, 2, 64, 1, 1, 1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loadout.initialize());
    loadout.set_loadout_id(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_item_definition(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_loadout(loadout));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_loadouts().enable_thread_safety());
    const auto *stored_item = source.get_item_definitions().find(1);
    const ft_size_t item_map_bytes = source.get_item_definitions().capacity()
        * sizeof(Pair<int32_t, game_item_definition>);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(item_map_bytes));
    const int32_t copy_result = copied.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t move_result = moved.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_result);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, move_result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, moved._initialised_state);
    FT_ASSERT(stored_item == source.get_item_definitions().find(1));
    FT_ASSERT_EQ(1U, source.get_loadouts().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved.move(source));
    FT_ASSERT(stored_item == moved.get_item_definitions().find(1));
    FT_ASSERT_EQ(1U, copied.get_loadouts().size());
    return (1);
}

FT_TEST(test_game_data_catalog_self_move_is_safe_again)
{
    game_data_catalog value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    return (1);
}

FT_TEST(test_game_data_catalog_destructor_after_destroy_is_safe)
{
    game_data_catalog *value;

    value = new game_data_catalog();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}
