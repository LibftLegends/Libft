#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_equipment_invalid_negative_slot_is_rejected)
{
    game_equipment value;
    ft_sharedptr<game_item> item_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(-1, item_pointer));
    return (1);
}

FT_TEST(test_game_equipment_invalid_large_slot_is_rejected)
{
    game_equipment value;
    ft_sharedptr<game_item> item_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(3, item_pointer));
    return (1);
}

FT_TEST(test_game_equipment_invalid_negative_get_slot_returns_null)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(ft_nullptr, value.get_item(-1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_invalid_large_get_slot_returns_null)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(ft_nullptr, value.get_item(3));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_unequip_negative_slot_reports_error)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.unequip(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_unequip_large_slot_reports_error)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.unequip(3);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_empty_head_can_be_unequipped)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.unequip(EQUIP_HEAD);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_empty_chest_can_be_unequipped)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.unequip(EQUIP_CHEST);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_empty_weapon_can_be_unequipped)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.unequip(EQUIP_WEAPON);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_all_slots_are_retrievable)
{
    game_equipment value;
    ft_sharedptr<game_item> *head_item;
    ft_sharedptr<game_item> *chest_item;
    ft_sharedptr<game_item> *weapon_item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    head_item = value.get_item(EQUIP_HEAD);
    chest_item = value.get_item(EQUIP_CHEST);
    weapon_item = value.get_item(EQUIP_WEAPON);
    FT_ASSERT_NEQ(ft_nullptr, head_item);
    FT_ASSERT_NEQ(ft_nullptr, chest_item);
    FT_ASSERT_NEQ(ft_nullptr, weapon_item);
    (void)head_item->destroy();
    (void)chest_item->destroy();
    (void)weapon_item->destroy();
    delete head_item;
    delete chest_item;
    delete weapon_item;
    return (1);
}
