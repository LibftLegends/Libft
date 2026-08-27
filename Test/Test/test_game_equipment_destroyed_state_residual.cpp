#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_destroyed_state_helpers.hpp"

static void equipment_destroyed_get_item_const(game_equipment &value)
{
    const game_equipment &constant_value = value;

    (void)constant_value.get_item(EQUIP_HEAD);
    return ;
}

FT_TEST(test_game_equipment_destroyed_get_item_const_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_equipment>(
                        equipment_destroyed_get_item_const));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_state_can_reinitialize_again)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_NEQ(ft_nullptr, value.get_item(EQUIP_HEAD));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_get_error_str_is_valid_again)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_NEQ(ft_nullptr, value.get_error_str());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_cleanup_is_idempotent_again)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_thread_safety_restarts)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_disable_after_cleanup_is_safe)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_destructor_after_cleanup_is_safe_again)
{
    game_equipment *value;

    value = new game_equipment();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}

FT_TEST(test_game_equipment_destroyed_empty_slots_remain_empty)
{
    game_equipment value;
    ft_sharedptr<game_item> *head_item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    head_item = value.get_item(EQUIP_HEAD);
    FT_ASSERT(head_item != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, head_item->get());
    (void)head_item->destroy();
    delete head_item;
    return (1);
}

FT_TEST(test_game_equipment_destroyed_error_remains_success)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(
    test_game_equipment_destroyed_cleanup_then_thread_safety_disable_is_safe)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_cleanup_can_repeat)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    return (1);
}
