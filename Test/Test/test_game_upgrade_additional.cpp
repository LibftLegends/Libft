#include "../test_internal.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <thread>

FT_TEST(test_game_upgrade_default_fields)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    FT_ASSERT_EQ(0, upgrade.get_id());
    FT_ASSERT_EQ(static_cast<uint16_t>(0), upgrade.get_current_level());
    FT_ASSERT_EQ(static_cast<uint16_t>(0), upgrade.get_max_level());
    return (1);
}

FT_TEST(test_game_upgrade_set_id_negative)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_id(-5);
    FT_ASSERT_EQ(-5, upgrade.get_id());
    return (1);
}

FT_TEST(test_game_upgrade_set_max_level)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_max_level(10);
    FT_ASSERT_EQ(static_cast<uint16_t>(10), upgrade.get_max_level());
    return (1);
}

FT_TEST(test_game_upgrade_level_addition_wraps)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_current_level(65530);
    upgrade.add_level(10);
    FT_ASSERT_EQ(static_cast<uint16_t>(4), upgrade.get_current_level());
    return (1);
}

FT_TEST(test_game_upgrade_level_subtraction_wraps)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_current_level(3);
    upgrade.sub_level(7);
    FT_ASSERT_EQ(static_cast<uint16_t>(65532), upgrade.get_current_level());
    return (1);
}

FT_TEST(test_game_upgrade_modifier1_set)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier1(-10);
    FT_ASSERT_EQ(-10, upgrade.get_modifier1());
    return (1);
}

FT_TEST(test_game_upgrade_modifier1_add)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier1(4);
    upgrade.add_modifier1(6);
    FT_ASSERT_EQ(10, upgrade.get_modifier1());
    return (1);
}

FT_TEST(test_game_upgrade_modifier1_sub)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier1(4);
    upgrade.sub_modifier1(6);
    FT_ASSERT_EQ(-2, upgrade.get_modifier1());
    return (1);
}

FT_TEST(test_game_upgrade_modifier2_arithmetic)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier2(4);
    upgrade.add_modifier2(6);
    upgrade.sub_modifier2(3);
    FT_ASSERT_EQ(7, upgrade.get_modifier2());
    return (1);
}

FT_TEST(test_game_upgrade_modifier3_arithmetic)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier3(-4);
    upgrade.add_modifier3(6);
    upgrade.sub_modifier3(3);
    FT_ASSERT_EQ(-1, upgrade.get_modifier3());
    return (1);
}

FT_TEST(test_game_upgrade_modifier4_arithmetic)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier4(4);
    upgrade.add_modifier4(6);
    upgrade.sub_modifier4(3);
    FT_ASSERT_EQ(7, upgrade.get_modifier4());
    return (1);
}

FT_TEST(test_game_upgrade_copy_preserves_all_modifiers)
{
    game_upgrade source;
    game_upgrade copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_id(8);
    source.set_modifier1(1);
    source.set_modifier2(2);
    source.set_modifier3(3);
    source.set_modifier4(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(8, copy.get_id());
    FT_ASSERT_EQ(1, copy.get_modifier1());
    FT_ASSERT_EQ(2, copy.get_modifier2());
    FT_ASSERT_EQ(3, copy.get_modifier3());
    FT_ASSERT_EQ(4, copy.get_modifier4());
    return (1);
}

FT_TEST(test_game_upgrade_move_clears_source)
{
    game_upgrade source;
    game_upgrade destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_id(9);
    source.set_modifier1(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(9, destination.get_id());
    FT_ASSERT_EQ(5, destination.get_modifier1());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source._initialised_state);
    return (1);
}

FT_TEST(test_game_upgrade_self_move)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_id(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.move(upgrade));
    FT_ASSERT_EQ(2, upgrade.get_id());
    return (1);
}

FT_TEST(test_game_upgrade_thread_safety)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, upgrade.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, upgrade.is_thread_safe());
    return (1);
}

FT_TEST(test_game_upgrade_lock_without_mutex)
{
    game_upgrade upgrade;
    ft_bool acquired;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    acquired = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.lock(&acquired));
    FT_ASSERT_EQ(FT_TRUE, acquired);
    upgrade.unlock(acquired);
    return (1);
}

FT_TEST(test_game_upgrade_modifier_overflow_preserves_value)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier1(INT32_MAX);
    upgrade.add_modifier1(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, upgrade.get_error());
    FT_ASSERT_EQ(INT32_MAX, upgrade.get_modifier1());
    upgrade.set_modifier2(INT32_MIN);
    upgrade.sub_modifier2(1);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, upgrade.get_error());
    FT_ASSERT_EQ(INT32_MIN, upgrade.get_modifier2());
    upgrade.set_modifier3(-1);
    upgrade.sub_modifier3(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.get_error());
    FT_ASSERT_EQ(INT32_MAX, upgrade.get_modifier3());
    upgrade.add_modifier4(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.get_error());
    FT_ASSERT_EQ(INT32_MIN, upgrade.get_modifier4());
    return (1);
}

FT_TEST(test_game_upgrade_copy_reads_consistent_values_during_updates)
{
    game_upgrade source;
    game_upgrade copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    int32_t writer_result = FT_ERR_SUCCESS;
    std::thread writer([&source, &writer_result]()
    {
        int32_t identifier = 1;
        while (identifier <= 2000)
        {
            ft_bool locked;
            writer_result = source.lock(&locked);
            if (writer_result != FT_ERR_SUCCESS)
                return ;
            source.set_id(identifier);
            source.set_modifier1(-identifier);
            source.unlock(locked);
            ++identifier;
        }
    });
    ft_bool coherent = FT_TRUE;
    uint32_t iteration = 0U;
    while (iteration < 2000U && coherent)
    {
        coherent = copy.initialize(source) == FT_ERR_SUCCESS
            && copy.get_id() == -copy.get_modifier1();
        ++iteration;
    }
    writer.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, writer_result);
    FT_ASSERT_EQ(FT_TRUE, coherent);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.move(source));
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(2000, copy.get_id());
    FT_ASSERT_EQ(-2000, copy.get_modifier1());
    return (1);
}

FT_TEST(test_game_upgrade_destroy_resets_fields)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_id(3);
    upgrade.set_modifier1(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.destroy());
    FT_ASSERT_EQ(0, upgrade._id);
    FT_ASSERT_EQ(0, upgrade._modifier1);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, upgrade._initialised_state);
    return (1);
}

FT_TEST(test_game_upgrade_reinitialize_after_destroy)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_id(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    FT_ASSERT_EQ(0, upgrade.get_id());
    return (1);
}

FT_TEST(test_game_upgrade_extreme_modifiers)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_modifier1(INT32_MIN);
    upgrade.set_modifier2(INT32_MAX);
    FT_ASSERT_EQ(INT32_MIN, upgrade.get_modifier1());
    FT_ASSERT_EQ(INT32_MAX, upgrade.get_modifier2());
    return (1);
}

FT_TEST(test_game_upgrade_error_after_valid_operation)
{
    game_upgrade upgrade;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.initialize());
    upgrade.set_id(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, upgrade.get_error());
    FT_ASSERT_STR_EQ("Success", upgrade.get_error_str());
    return (1);
}

