#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_destroyed_state_helpers.hpp"

static void equipment_destroyed_equip(game_equipment &value)
{
    ft_sharedptr<game_item> item_pointer;

    (void)value.equip(EQUIP_HEAD, item_pointer);
    return ;
}

static void equipment_destroyed_unequip(game_equipment &value)
{
    value.unequip(EQUIP_HEAD);
    return ;
}

static void equipment_destroyed_get_item(game_equipment &value)
{
    (void)value.get_item(EQUIP_HEAD);
    return ;
}

static void equipment_destroyed_enable_thread_safety(game_equipment &value)
{
    (void)value.enable_thread_safety();
    return ;
}

FT_TEST(test_game_equipment_destroyed_equip_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_equipment>(
                        equipment_destroyed_equip));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_unequip_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_equipment>(
                        equipment_destroyed_unequip));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_get_item_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_equipment>(
                        equipment_destroyed_get_item));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_enable_thread_safety_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_equipment>(
                        equipment_destroyed_enable_thread_safety));
    return (1);
}

FT_TEST(test_game_equipment_destroyed_get_error_aborts)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_get_error_str_is_valid)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_NEQ(ft_nullptr, value.get_error_str());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_is_thread_safe_is_safe)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_disable_thread_safety_is_safe)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    return (1);
}

FT_TEST(test_game_equipment_destroyed_destructor_is_non_aborting)
{
    game_equipment *value;

    value = new game_equipment();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->destroy());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}

FT_TEST(test_game_equipment_destroyed_state_is_reusable)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}
