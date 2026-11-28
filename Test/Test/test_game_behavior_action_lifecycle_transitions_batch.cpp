#include "../../Modules/Game/game_behavior_action.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void behavior_action_copy_from_uninitialised(game_behavior_action &value)
{
    game_behavior_action source;

    (void)value.initialize(source);
    return ;
}

static void behavior_action_move_from_uninitialised(game_behavior_action &value)
{
    game_behavior_action source;

    (void)value.initialize(static_cast<game_behavior_action &&>(source));
    return ;
}

FT_TEST(test_game_behavior_action_copy_initializes_destination_values)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7, 2.5, 4.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(7, destination.get_action_id());
    FT_ASSERT_EQ(2.5, destination.get_weight());
    FT_ASSERT_EQ(4.0, destination.get_cooldown_seconds());
    return (1);
}

FT_TEST(test_game_behavior_action_move_initializes_destination_values)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(8, 3.5, 5.0));
    FT_ASSERT_EQ(
        FT_ERR_SUCCESS,
        destination.initialize(static_cast<game_behavior_action &&>(source)));
    FT_ASSERT_EQ(8, destination.get_action_id());
    FT_ASSERT_EQ(3.5, destination.get_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    return (1);
}

FT_TEST(test_game_behavior_action_copy_replaces_initialized_destination)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(4, 1.5, 2.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(9, 9.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(4, destination.get_action_id());
    FT_ASSERT_EQ(1.5, destination.get_weight());
    return (1);
}

FT_TEST(test_game_behavior_action_move_replaces_initialized_destination)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(5, 2.0, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(9, 9.0, 9.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(5, destination.get_action_id());
    FT_ASSERT_EQ(2.0, destination.get_weight());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    return (1);
}

FT_TEST(test_game_behavior_action_copy_from_destroyed_source_propagates_state)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_behavior_action_move_from_destroyed_source_propagates_state)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    return (1);
}

FT_TEST(test_game_behavior_action_copy_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_behavior_action>(
                        behavior_action_copy_from_uninitialised));
    return (1);
}

FT_TEST(test_game_behavior_action_move_from_uninitialised_source_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_behavior_action>(
                        behavior_action_move_from_uninitialised));
    return (1);
}

FT_TEST(test_game_behavior_action_copy_self_is_successful)
{
    game_behavior_action value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(6, 2.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(value));
    FT_ASSERT_EQ(6, value.get_action_id());
    return (1);
}

FT_TEST(test_game_behavior_action_move_self_is_successful)
{
    game_behavior_action value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(6, 2.0, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(6, value.get_action_id());
    return (1);
}

FT_TEST(test_game_behavior_action_copy_preserves_thread_safety_destination)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    return (1);
}

FT_TEST(test_game_behavior_action_move_cleans_source_state)
{
    game_behavior_action source;
    game_behavior_action destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(10, 3.0, 6.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(10, destination.get_action_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0, source.get_action_id());
    return (1);
}

FT_TEST(test_game_behavior_action_copy_honors_destination_lock)
{
    game_behavior_action source;
    game_behavior_action destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7, 2.5, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 0.5, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t result = destination.initialize(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);

    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(4, destination.get_action_id());
    FT_ASSERT_EQ(0.5, destination.get_weight());
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(7, destination.get_action_id());
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    return (1);
}

FT_TEST(test_game_behavior_action_move_transfers_mutex_ownership)
{
    game_behavior_action source;
    game_behavior_action destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(7, 2.5, 3.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    const auto *mutex = source._mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(4, 0.5, 1.0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT(destination._mutex == mutex);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(7, destination.get_action_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    return (1);
}
