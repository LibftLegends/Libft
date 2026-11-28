#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void world_region_copy_uninitialised(game_world_region &value)
{
    game_world_region source;

    (void)value.initialize(source);
    return ;
}

static void world_region_move_initialize_uninitialised(game_world_region &value)
{
    game_world_region source;

    (void)value.initialize(static_cast<game_world_region &&>(source));
    return ;
}

static void world_region_move_uninitialised(game_world_region &value)
{
    game_world_region source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_world_region_copy_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_region>(
                        world_region_copy_uninitialised));
    return (1);
}

FT_TEST(test_game_world_region_move_initialize_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_region>(
                        world_region_move_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_world_region_move_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_region>(
                        world_region_move_uninitialised));
    return (1);
}

FT_TEST(test_game_world_region_copy_destroyed_source_propagates)
{
    game_world_region source;
    game_world_region destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_world_region_move_destroyed_source_propagates)
{
    game_world_region source;
    game_world_region destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_world_region &&>(source)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_world_region_copy_into_initialized_destination_commits)
{
    game_world_region source;
    game_world_region destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_world_id(22);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    destination.set_world_id(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(22, destination.get_world_id());
    return (1);
}

FT_TEST(test_game_world_region_move_into_initialized_destination_commits)
{
    game_world_region source;
    game_world_region destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_world_id(23);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_world_region &&>(source)));
    FT_ASSERT_EQ(23, destination.get_world_id());
    return (1);
}

FT_TEST(test_game_world_region_self_copy_is_safe)
{
    game_world_region value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(value));
    return (1);
}

FT_TEST(test_game_world_region_self_move_is_safe_again)
{
    game_world_region value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    return (1);
}

FT_TEST(test_game_world_region_destructor_after_destroy_is_safe)
{
    game_world_region *value;

    value = new game_world_region();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}

FT_TEST(test_game_world_region_bulk_copy_and_move_preserve_region_ids)
{
    game_world_region source;
    game_world_region copy;
    game_world_region moved;
    ft_vector<int32_t> replacement;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.initialize());
    int32_t region_id = 1;
    while (region_id <= 20)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_region_ids().push_back(region_id));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.push_back(region_id + 20));
        ++region_id;
    }
    source.set_region_ids(source.get_region_ids());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(9, source.get_region_ids()));
    FT_ASSERT_EQ(9, source.get_world_id());
    FT_ASSERT_EQ(20U, source.get_region_ids().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(20, copy.get_region_ids()[19]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.push_back(41));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    copy.set_region_ids(replacement);
    const int32_t replace_error = copy.get_error();
    const ft_size_t preserved_count = copy.get_region_ids().size();
    const int32_t preserved_end = copy.get_region_ids()[19];
    const int32_t move_error = moved.move(source);
    const int32_t copy_error = copy.initialize(moved);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, replace_error);
    FT_ASSERT_EQ(20U, preserved_count);
    FT_ASSERT_EQ(20, preserved_end);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, move_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copy._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(9, moved.get_world_id());
    FT_ASSERT_EQ(20U, moved.get_region_ids().size());
    FT_ASSERT_EQ(20, moved.get_region_ids()[19]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_region_ids().size());
    return (1);
}
