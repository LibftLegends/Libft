#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void skill_initialize_twice(game_skill &value)
{
    (void)value.initialize();
    (void)value.initialize();
    return ;
}

static void skill_copy_initialize_uninitialised(game_skill &value)
{
    game_skill source;

    (void)value.initialize(source);
    return ;
}

static void skill_move_initialize_uninitialised(game_skill &value)
{
    game_skill source;

    (void)value.initialize(static_cast<game_skill &&>(source));
    return ;
}

static void skill_move_uninitialised(game_skill &value)
{
    game_skill source;

    (void)value.move(source);
    return ;
}

FT_TEST(test_game_skill_initialize_twice_aborts)
{
    FT_ASSERT_EQ(
        1, expect_game_lifecycle_sigabrt<game_skill>(skill_initialize_twice));
    return (1);
}

FT_TEST(test_game_skill_copy_initialize_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_skill>(
                        skill_copy_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_skill_move_initialize_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_skill>(
                        skill_move_initialize_uninitialised));
    return (1);
}

FT_TEST(test_game_skill_move_uninitialised_aborts)
{
    FT_ASSERT_EQ(
        1, expect_game_lifecycle_sigabrt<game_skill>(skill_move_uninitialised));
    return (1);
}

FT_TEST(test_game_skill_initialize_default_values)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0, value.get_id());
    FT_ASSERT_EQ(0, value.get_level());
    FT_ASSERT_EQ(0, value.get_cooldown());
    FT_ASSERT_EQ(0, value.get_modifier1());
    FT_ASSERT_EQ(0, value.get_modifier4());
    return (1);
}

FT_TEST(test_game_skill_set_identity_and_level)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_id(4);
    value.set_level(3);
    FT_ASSERT_EQ(4, value.get_id());
    FT_ASSERT_EQ(3, value.get_level());
    return (1);
}

FT_TEST(test_game_skill_cooldown_add_and_subtract)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_cooldown(8);
    value.add_cooldown(4);
    value.sub_cooldown(3);
    FT_ASSERT_EQ(9, value.get_cooldown());
    return (1);
}

FT_TEST(test_game_skill_cooldown_and_remaining_modifiers_preserve_values_on_overflow)
{
    game_skill value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_cooldown(INT32_MAX);
    value.add_cooldown(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    FT_ASSERT_EQ(INT32_MAX, value.get_cooldown());
    value.set_modifier3(INT32_MIN);
    value.sub_modifier3(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    FT_ASSERT_EQ(INT32_MIN, value.get_modifier3());
    value.set_modifier4(0);
    value.sub_modifier4(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, value.get_error());
    FT_ASSERT_EQ(0, value.get_modifier4());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    value.set_modifier3(17);
    const int32_t write_error = value.get_error();
    const int32_t read_value = value.get_modifier3();
    const int32_t read_error = value.get_error();
    const int32_t cooldown_result = value.get_cooldown();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, write_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, read_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, cooldown_result);
    FT_ASSERT_EQ(0, read_value);
    FT_ASSERT_EQ(INT32_MIN, value.get_modifier3());
    return (1);
}

FT_TEST(test_game_skill_copy_and_move_honor_source_mutex)
{
    game_skill source;
    game_skill destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_id(7);
    source.set_level(2);
    source.set_cooldown(-3);
    source.set_modifier3(14);
    source.set_modifier4(-9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    const auto *mutex = source._mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t copy_error = destination.initialize(source);
    const int32_t move_error = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, copy_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, move_error);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(source._mutex == mutex);
    FT_ASSERT_EQ(-9, source.get_modifier4());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(2, destination.get_level());
    FT_ASSERT_EQ(-3, destination.get_cooldown());
    FT_ASSERT_EQ(14, destination.get_modifier3());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT(destination._mutex == mutex);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(7, destination.get_id());
    FT_ASSERT_EQ(-9, destination.get_modifier4());
    return (1);
}

FT_TEST(test_game_skill_modifier1_values)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier1(1);
    value.add_modifier1(2);
    value.sub_modifier1(1);
    FT_ASSERT_EQ(2, value.get_modifier1());
    return (1);
}

FT_TEST(test_game_skill_modifier2_values)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier2(2);
    value.add_modifier2(3);
    value.sub_modifier2(1);
    FT_ASSERT_EQ(4, value.get_modifier2());
    return (1);
}

FT_TEST(test_game_skill_modifier3_values)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier3(3);
    value.add_modifier3(4);
    value.sub_modifier3(1);
    FT_ASSERT_EQ(6, value.get_modifier3());
    return (1);
}

FT_TEST(test_game_skill_modifier4_values)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier4(4);
    value.add_modifier4(5);
    value.sub_modifier4(1);
    FT_ASSERT_EQ(8, value.get_modifier4());
    return (1);
}

FT_TEST(test_game_skill_thread_safety_enable_disable_cycle)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_skill_lock_unlock_initialised)
{
    game_skill value;
    ft_bool lock_acquired = FT_FALSE;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.lock(&lock_acquired));
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    value.unlock(lock_acquired);
    return (1);
}

FT_TEST(test_game_skill_error_after_success)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_level(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    FT_ASSERT_NEQ(ft_nullptr, value.get_error_str());
    return (1);
}

FT_TEST(test_game_skill_destroy_twice_is_safe)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_game_skill_reinitialize_after_destroy)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_id(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0, value.get_id());
    return (1);
}

FT_TEST(test_game_skill_self_move_is_safe)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_id(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(10, value.get_id());
    return (1);
}

FT_TEST(test_game_skill_all_modifiers_can_be_updated)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier1(1);
    value.set_modifier2(2);
    value.set_modifier3(3);
    value.set_modifier4(4);
    FT_ASSERT_EQ(1, value.get_modifier1());
    FT_ASSERT_EQ(2, value.get_modifier2());
    FT_ASSERT_EQ(3, value.get_modifier3());
    FT_ASSERT_EQ(4, value.get_modifier4());
    return (1);
}

FT_TEST(test_game_skill_zero_cooldown_subtraction_is_preserved)
{
    game_skill value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_cooldown(0);
    value.sub_cooldown(0);
    FT_ASSERT_EQ(0, value.get_cooldown());
    return (1);
}

FT_TEST(test_game_skill_copy_keeps_values)
{
    game_skill source;
    game_skill destination;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_id(4);
    source.set_level(6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(4, destination.get_id());
    FT_ASSERT_EQ(6, destination.get_level());
    return (1);
}
