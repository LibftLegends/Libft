#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void world_registry_copy_uninitialised(game_world_registry &value)
{
    game_world_registry source;

    (void)value.initialize(source);
    return ;
}

static void
world_registry_move_initialize_uninitialised(game_world_registry &value)
{
    game_world_registry source;

    (void)value.initialize(static_cast<game_world_registry &&>(source));
    return ;
}

static void world_registry_move_uninitialised(game_world_registry &value)
{
    game_world_registry source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_world_registry_copy_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_registry>(
                        world_registry_copy_uninitialised));
    return (1);
}

FT_TEST(test_game_world_registry_move_initialize_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_registry>(
                        world_registry_move_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_world_registry_move_uninitialised_aborts_again)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_world_registry>(
                        world_registry_move_uninitialised));
    return (1);
}

FT_TEST(test_game_world_registry_copy_destroyed_source_propagates)
{
    game_world_registry source;
    game_world_registry destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_world_registry_move_destroyed_source_propagates)
{
    game_world_registry source;
    game_world_registry destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_world_registry &&>(source)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_world_registry_copy_into_initialized_destination_commits)
{
    game_world_registry source;
    game_world_registry destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(0, destination.get_regions().size());
    return (1);
}

FT_TEST(test_game_world_registry_move_into_initialized_destination_commits)
{
    game_world_registry source;
    game_world_registry destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_world_registry &&>(source)));
    FT_ASSERT_EQ(0, destination.get_world_regions().size());
    return (1);
}

FT_TEST(test_game_world_registry_self_copy_is_safe)
{
    game_world_registry value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(value));
    return (1);
}

FT_TEST(test_game_world_registry_self_move_is_safe_again)
{
    game_world_registry value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    return (1);
}

FT_TEST(test_game_world_registry_destructor_after_destroy_is_safe)
{
    game_world_registry *value;

    value = new game_world_registry();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}

FT_TEST(test_game_world_registry_bulk_copy_and_set_preserve_live_entries)
{
    game_world_registry registry;
    game_world_registry replacement;
    game_world_registry copied;
    game_region_definition region;
    game_world_region world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    region.set_region_id(1);
    world.set_world_id(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_region(region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_world(world));
    region.set_region_id(2);
    world.set_world_id(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.register_region(region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement.register_world(world));

    registry.set_regions(registry.get_regions());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.get_error());
    registry.set_world_regions(registry.get_world_regions());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.initialize(registry));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.fetch_region(1, region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied.fetch_world(10, world));

    const auto &replacement_regions = replacement.get_regions();
    const auto &replacement_worlds = replacement.get_world_regions();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    registry.set_regions(replacement_regions);
    const int32_t region_error = registry.get_error();
    registry.set_world_regions(replacement_worlds);
    const int32_t world_error = registry.get_error();
    const int32_t copy_error = copied.initialize(replacement);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, region_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, world_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, copy_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, copied._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_region(1, region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_world(10, world));

    registry.set_regions(replacement_regions);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.get_error());
    registry.set_world_regions(replacement_worlds);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_region(2, region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_world(20, world));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, registry.fetch_region(1, region));
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, registry.fetch_world(10, world));
    return (1);
}

FT_TEST(test_game_world_registry_failed_registration_preserves_existing_records)
{
    game_world_registry registry;
    game_region_definition region;
    game_world_region world;
    ft_string name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("replacement"));
    region.set_region_id(1);
    world.set_world_id(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_region(region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_world(world));
    region.set_recommended_level(4);
    region.set_name(name);
    int32_t identifier = 0;
    while (identifier < 10)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, world.get_region_ids().push_back(identifier));
        ++identifier;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t failed_region = registry.register_region(region);
    const int32_t failed_world = registry.register_world(world);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_region);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_world);
    game_region_definition original_region;
    game_world_region original_world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_region(1, original_region));
    FT_ASSERT_EQ(0, original_region.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_world(10, original_world));
    FT_ASSERT_EQ(0U, original_world.get_region_ids().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_region(region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_world(world));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_region(1, original_region));
    FT_ASSERT_EQ(4, original_region.get_recommended_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_world(10, original_world));
    FT_ASSERT_EQ(10U, original_world.get_region_ids().size());
    return (1);
}

FT_TEST(test_game_world_registry_move_transfers_maps_without_allocating)
{
    game_world_registry source;
    game_world_registry destination;
    game_region_definition region;
    game_world_region world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    region.set_region_id(1);
    world.set_world_id(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_region(region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.register_world(world));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    auto &source_regions = source.get_regions();
    auto &destination_worlds = destination.get_world_regions();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_regions.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_worlds.enable_thread_safety());

    ft_bool acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_worlds.lock(&acquired));
    const int32_t locked_destination = destination.move(source);
    const int32_t locked_copy_destination = destination.initialize(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_worlds.unlock(acquired));
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, locked_destination);
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, locked_copy_destination);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, destination._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_regions.lock(&acquired));
    const int32_t locked_source = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_regions.unlock(acquired));
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, locked_source);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_region(1, region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.fetch_world(10, world));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0U, source.get_regions().size());
    return (1);
}
