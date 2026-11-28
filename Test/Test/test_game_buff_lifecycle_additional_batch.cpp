#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

static void buff_initialize_twice(game_buff &value)
{
    (void)value.initialize();
    (void)value.initialize();
    return ;
}

FT_TEST(test_game_buff_initialize_twice_aborts)
{
    FT_ASSERT_EQ(
        1, expect_game_lifecycle_sigabrt<game_buff>(buff_initialize_twice));
    return (1);
}

FT_TEST(test_game_buff_initialize_succeeds_with_zero_values)
{
    game_buff value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0, value.get_id());
    FT_ASSERT_EQ(0, value.get_duration());
    return (1);
}

FT_TEST(test_game_buff_destroy_twice_is_safe)
{
    game_buff value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_game_buff_reinitialize_after_destroy)
{
    game_buff value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_id(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0, value.get_id());
    return (1);
}

FT_TEST(test_game_buff_self_move_is_safe)
{
    game_buff value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_modifier4(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.move(value));
    FT_ASSERT_EQ(9, value.get_modifier4());
    return (1);
}

FT_TEST(test_game_buff_thread_safety_cycle)
{
    game_buff value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}
template <typename Effect>
static int32_t check_effect_arithmetic()
{
    Effect effect;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, effect.initialize());
    effect.set_duration(INT32_MAX);
    effect.add_duration(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, effect.get_error());
    FT_ASSERT_EQ(INT32_MAX, effect.get_duration());
    effect.set_duration(0);
    effect.sub_duration(INT32_MAX);
    effect.sub_duration(1);
    FT_ASSERT_EQ(INT32_MIN, effect.get_duration());
    effect.sub_duration(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, effect.get_error());
    FT_ASSERT_EQ(INT32_MIN, effect.get_duration());
    effect.set_duration(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, effect.get_error());
    FT_ASSERT_EQ(INT32_MIN, effect.get_duration());

    effect.set_modifier1(INT32_MAX);
    effect.add_modifier1(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, effect.get_error());
    FT_ASSERT_EQ(INT32_MAX, effect.get_modifier1());
    effect.set_modifier4(0);
    effect.sub_modifier4(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, effect.get_error());
    FT_ASSERT_EQ(0, effect.get_modifier4());
    effect.set_modifier4(-1);
    effect.sub_modifier4(INT32_MIN);
    FT_ASSERT_EQ(INT32_MAX, effect.get_modifier4());
    return (1);
}

FT_TEST(test_game_buff_and_debuff_reject_unrepresentable_field_arithmetic)
{
    FT_ASSERT_EQ(1, check_effect_arithmetic<game_buff>());
    FT_ASSERT_EQ(1, check_effect_arithmetic<game_debuff>());
    return (1);
}

FT_TEST(test_game_debuff_modifiers_honor_optional_mutex)
{
    game_debuff effect;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, effect.initialize());
    effect.set_modifier3(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, effect.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    effect.add_modifier3(5);
    const int32_t write_error = effect.get_error();
    const int32_t read_value = effect.get_modifier3();
    const int32_t read_error = effect.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, write_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, read_error);
    FT_ASSERT_EQ(0, read_value);
    FT_ASSERT_EQ(7, effect.get_modifier3());
    return (1);
}

template <typename Effect>
static int32_t check_effect_lifecycle_transfer()
{
    Effect source;
    Effect destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_id(17);
    source.set_duration(9);
    source.set_modifier1(-4);
    source.set_modifier2(13);
    source.set_modifier3(21);
    source.set_modifier4(26);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    const auto *mutex = source._mutex;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t copy_result = destination.initialize(source);
    const int32_t move_result = destination.move(source);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, copy_result);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, move_result);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT(source._mutex == mutex);
    FT_ASSERT_EQ(9, source.get_duration());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    FT_ASSERT_EQ(17, destination.get_id());
    FT_ASSERT_EQ(-4, destination.get_modifier1());
    FT_ASSERT_EQ(13, destination.get_modifier2());
    FT_ASSERT_EQ(21, destination.get_modifier3());
    FT_ASSERT_EQ(26, destination.get_modifier4());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT(destination._mutex == mutex);
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    FT_ASSERT_EQ(17, destination.get_id());
    FT_ASSERT_EQ(9, destination.get_duration());
    FT_ASSERT_EQ(26, destination.get_modifier4());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(0, source.get_id());
    FT_ASSERT_EQ(0, source.get_duration());
    FT_ASSERT_EQ(0, source.get_modifier4());
    return (1);
}

FT_TEST(test_game_buff_and_debuff_lifecycle_transfer_honors_source_mutex)
{
    FT_ASSERT_EQ(1, check_effect_lifecycle_transfer<game_buff>());
    FT_ASSERT_EQ(1, check_effect_lifecycle_transfer<game_debuff>());
    return (1);
}
