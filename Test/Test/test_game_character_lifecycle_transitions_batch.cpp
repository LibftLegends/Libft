#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void character_move_from_uninitialised(game_character &value)
{
    game_character source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_character_initialization_rolls_back_before_retry)
{
    game_character character;
    character._achievements._initial_capacity = character._achievements.MAXIMUM_CAPACITY + 1U;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, character.initialize());
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, character.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, character._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, character._inventory._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, character._quests._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_UNINITIALISED, character._equipment._initialised_state);

    character._achievements._initial_capacity = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.destroy());
    return (1);
}

FT_TEST(test_game_character_move_copies_initialized_values)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.set_hit_points(41);
    source.set_coins(12);
    source.set_x(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(41, destination.get_hit_points());
    FT_ASSERT_EQ(12, destination.get_coins());
    FT_ASSERT_EQ(7, destination.get_x());
    return (1);
}

FT_TEST(test_game_character_move_replaces_initialized_destination)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    source.set_hit_points(31);
    destination.set_hit_points(99);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(31, destination.get_hit_points());
    return (1);
}

FT_TEST(test_game_character_move_needs_no_empty_table_allocations)
{
    game_character source;
    game_character destination;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(42);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_skill(skill));
    const game_skill *stored_skill = source.get_skill(42);
    FT_ASSERT(stored_skill != ft_nullptr);
    uint64_t before;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_inventory().get_revision(&before));
    const ft_size_t initial_capacity = destination._skills._initial_capacity;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(destination.get_skill(42) == stored_skill);
    uint64_t after;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_inventory().get_revision(&after));
    FT_ASSERT_EQ(before + 4U, after);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(initial_capacity, destination._skills._capacity);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    return (1);
}

FT_TEST(test_game_character_move_keeps_source_on_late_component_lock_failure)
{
    game_character source;
    game_character destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_fire_res(17, 3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source._achievements.enable_thread_safety());
    const auto *slots = source._skills._data;

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._initialised_state);
    FT_ASSERT_EQ(FT_CLASS_STATE_INITIALISED, source._fire_res._initialised_state);
    FT_ASSERT_EQ(17, source.get_fire_res().get_percent());
    FT_ASSERT(source._skills._data == slots);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(17, destination.get_fire_res().get_percent());
    return (1);
}

FT_TEST(test_game_character_move_rejects_active_experience_table_lock)
{
    game_character source;
    game_character destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_fire_res(23, 1);
    game_experience_table &table = source.get_experience_table();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table._mutex->lock());
    const int32_t result = destination.move(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, table._mutex->unlock());
    FT_ASSERT_EQ(FT_ERR_THREAD_BUSY, result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(23, source.get_fire_res().get_percent());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(23, destination.get_fire_res().get_percent());
    return (1);
}

FT_TEST(test_game_character_move_transfers_experience_table_storage)
{
    game_character source;
    game_character destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    game_experience_table &source_table = source.get_experience_table();
    game_experience_table &destination_table = destination.get_experience_table();
    constexpr int32_t LEVELS[] = {0, 25, 50};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_table.set_levels(LEVELS, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_table.resize(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_table.enable_thread_safety());
    const int32_t *storage = source_table._levels;
    const int32_t expected_level = source_table.get_level(27);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_table.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT(destination_table._levels == storage);
    FT_ASSERT_EQ(3, destination_table.get_count());
    FT_ASSERT_EQ(expected_level, destination_table.get_level(27));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_table.get_error());
    FT_ASSERT_EQ(FT_FALSE, destination_table.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0, source_table.get_count());
    return (1);
}

FT_TEST(
    test_game_character_move_from_destroyed_source_leaves_destination_destroyed)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_character_move_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_character>(
                        character_move_from_uninitialised));
    return (1);
}

FT_TEST(test_game_character_move_self_preserves_values)
{
    game_character value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_hit_points(24);
    value.set_y(8);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(24, value.get_hit_points());
    FT_ASSERT_EQ(8, value.get_y());
    return (1);
}

FT_TEST(test_game_character_move_transfers_thread_safe_source_data)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    source.set_valor(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(19, destination.get_valor());
    return (1);
}

FT_TEST(test_game_character_move_source_can_be_reinitialized)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_experience(55);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(55, source.get_experience());
    return (1);
}

FT_TEST(
    test_game_character_move_destination_can_be_reinitialized_after_destroyed_source)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_FALSE, destination.is_alive());
    return (1);
}

FT_TEST(test_game_character_move_preserves_coordinates)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_x(-4);
    source.set_y(5);
    source.set_z(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(-4, destination.get_x());
    FT_ASSERT_EQ(5, destination.get_y());
    FT_ASSERT_EQ(9, destination.get_z());
    return (1);
}

FT_TEST(test_game_character_move_cleanup_is_idempotent)
{
    game_character source;
    game_character destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}
