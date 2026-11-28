#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static void equipment_slot_cleanup(ft_sharedptr<game_item> *item_pointer)
{
    if (item_pointer == ft_nullptr)
        return ;
    (void)item_pointer->destroy();
    delete item_pointer;
    return ;
}

static ft_bool equipment_slot_contains_item(game_equipment &value, int32_t slot)
{
    ft_sharedptr<game_item> *item_pointer;
    ft_bool contains_item;

    item_pointer = value.get_item(slot);
    if (item_pointer == ft_nullptr)
        return (FT_FALSE);
    contains_item = item_pointer->get() != ft_nullptr;
    equipment_slot_cleanup(item_pointer);
    return (contains_item);
}

FT_TEST(test_game_equipment_move_preserves_distinct_slots_and_reference_counts)
{
    game_equipment source;
    game_equipment destination;
    ft_sharedptr<game_item> items[6];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    int32_t slot = EQUIP_HEAD;
    while (slot <= EQUIP_OFF_HAND)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, items[slot].initialize(new game_item()));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, items[slot]->initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip(slot, items[slot]));
        FT_ASSERT_EQ(2, items[slot].use_count());
        ++slot;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    slot = EQUIP_HEAD;
    while (slot <= EQUIP_OFF_HAND)
    {
        FT_ASSERT_EQ(2, items[slot].use_count());
        FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(source, slot));
        ft_sharedptr<game_item> *retained = destination.get_item(slot);
        FT_ASSERT(retained != ft_nullptr);
        FT_ASSERT(retained->get() == items[slot].get());
        destination.unequip(slot);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
        FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(destination, slot));
        FT_ASSERT_EQ(2, items[slot].use_count());
        equipment_slot_cleanup(retained);
        FT_ASSERT_EQ(1, items[slot].use_count());
        ++slot;
    }
    return (1);
}

FT_TEST(test_game_equipment_equip_head_stores_initialized_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_HEAD, item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_HEAD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_equip_chest_stores_initialized_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_CHEST, item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_CHEST));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_equip_weapon_stores_initialized_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_WEAPON, item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_WEAPON));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_replacing_head_item_keeps_one_item)
{
    game_equipment value;
    ft_sharedptr<game_item> first_item(new game_item());
    ft_sharedptr<game_item> second_item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_HEAD, first_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_HEAD, second_item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_HEAD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_replacing_chest_item_keeps_one_item)
{
    game_equipment value;
    ft_sharedptr<game_item> first_item(new game_item());
    ft_sharedptr<game_item> second_item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_CHEST, first_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_CHEST, second_item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_CHEST));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_replacing_weapon_item_keeps_one_item)
{
    game_equipment value;
    ft_sharedptr<game_item> first_item(new game_item());
    ft_sharedptr<game_item> second_item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_WEAPON, first_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_WEAPON, second_item));
    FT_ASSERT_EQ(FT_TRUE, equipment_slot_contains_item(value, EQUIP_WEAPON));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_item.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_null_head_item_is_rejected)
{
    game_equipment value;
    ft_sharedptr<game_item> item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(EQUIP_HEAD, item));
    return (1);
}

FT_TEST(test_game_equipment_null_chest_item_is_rejected)
{
    game_equipment value;
    ft_sharedptr<game_item> item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(EQUIP_CHEST, item));
    return (1);
}

FT_TEST(test_game_equipment_null_weapon_item_is_rejected)
{
    game_equipment value;
    ft_sharedptr<game_item> item;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(EQUIP_WEAPON, item));
    return (1);
}

FT_TEST(test_game_equipment_unequip_head_clears_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_HEAD, item));
    value.unequip(EQUIP_HEAD);
    FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(value, EQUIP_HEAD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_unequip_chest_clears_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_CHEST, item));
    value.unequip(EQUIP_CHEST);
    FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(value, EQUIP_CHEST));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_unequip_weapon_clears_item)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_WEAPON, item));
    value.unequip(EQUIP_WEAPON);
    FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(value, EQUIP_WEAPON));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_invalid_slot_does_not_change_head)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.equip(99, item));
    FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(value, EQUIP_HEAD));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_thread_safety_enable_is_idempotent)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_thread_safety_disable_is_idempotent)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_thread_safety_can_restart_after_disable)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_thread_safety_destroy_disables_mutex)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_game_equipment_reinitialize_restores_empty_slots)
{
    game_equipment value;
    ft_sharedptr<game_item> item(new game_item());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.equip(EQUIP_WEAPON, item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_FALSE, equipment_slot_contains_item(value, EQUIP_WEAPON));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    return (1);
}

FT_TEST(test_game_equipment_repeated_cleanup_preserves_success)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_error());
    return (1);
}

FT_TEST(test_game_equipment_destructor_after_thread_safety_is_safe)
{
    game_equipment *value;

    value = new game_equipment();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value->enable_thread_safety());
    delete value;
    FT_ASSERT_EQ(1, 1);
    return (1);
}

FT_TEST(test_game_equipment_error_string_is_available_after_destroy)
{
    game_equipment value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.destroy());
    FT_ASSERT_NEQ(ft_nullptr, value.get_error_str());
    return (1);
}
