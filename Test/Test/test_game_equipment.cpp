#include "../test_internal.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_equipment_equip)
{
    game_character hero;
    ft_sharedptr<game_item> helm(new game_item());

    FT_ASSERT(helm.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helm->initialize());
    helm->set_item_id(1);
    helm->set_modifier1_id(1);
    helm->set_modifier1_value(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, helm));
    FT_ASSERT_EQ(5, hero.get_physical_armor());
    ft_sharedptr<game_item> *found = hero.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(found != ft_nullptr);
    FT_ASSERT(found->get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, found->destroy());
    delete found;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helm.destroy());
    return (1);
}

FT_TEST(test_game_equipment_unequip)
{
    game_character hero;
    ft_sharedptr<game_item> helm(new game_item());

    FT_ASSERT(helm.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helm->initialize());
    helm->set_item_id(1);
    helm->set_modifier1_id(1);
    helm->set_modifier1_value(5);
    hero.equip_item(EQUIP_HEAD, helm);
    hero.unequip_item(EQUIP_HEAD);
    FT_ASSERT_EQ(0, hero.get_physical_armor());
    ft_sharedptr<game_item> *found = hero.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(found != ft_nullptr);
    FT_ASSERT(found->get() == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, found->destroy());
    delete found;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helm.destroy());
    return (1);
}

FT_TEST(test_game_equipment_thread_safety_toggle)
{
    game_equipment equipment;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.initialize());
    FT_ASSERT_EQ(false, equipment.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.enable_thread_safety());
    FT_ASSERT_EQ(true, equipment.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, equipment.disable_thread_safety());
    FT_ASSERT_EQ(false, equipment.is_thread_safe());
    return (1);
}

FT_TEST(test_game_character_read_equipped_item_retains_item_and_reports_errors)
{
    game_character character;
    ft_sharedptr<game_item> item(new game_item());
    ft_sharedptr<game_item> retained;
    const game_character &reader = character;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(42);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.equip_item(EQUIP_HEAD, item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.read_equipped_item(EQUIP_HEAD, retained));
    FT_ASSERT(retained.get() == item.get());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, reader.read_equipped_item(-1, retained));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, reader.get_error());
    FT_ASSERT(retained.get() == item.get());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t read_result = reader.read_equipped_item(EQUIP_HEAD, retained);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, read_result);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, reader.get_error());
    FT_ASSERT(retained.get() == item.get());

    character.unequip_item(EQUIP_HEAD);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.destroy());
    FT_ASSERT_EQ(42, retained->get_item_id());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reader.read_equipped_item(EQUIP_HEAD, retained));
    FT_ASSERT(retained.get() == ft_nullptr);
    return (1);
}

FT_TEST(test_game_character_equipment_keeps_slot_and_stats_on_modifier_read_failure)
{
    game_character hero;
    ft_sharedptr<game_item> previous(new game_item());
    ft_sharedptr<game_item> replacement(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, previous->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement->initialize());
    previous->set_modifier1_id(1);
    previous->set_modifier1_value(2);
    previous->set_modifier2_id(1);
    previous->set_modifier2_value(3);
    replacement->set_modifier1_id(1);
    replacement->set_modifier1_value(4);
    replacement->set_modifier2_id(1);
    replacement->set_modifier2_value(5);
    replacement->set_modifier3_id(1);
    replacement->set_modifier3_value(6);
    replacement->set_modifier4_id(1);
    replacement->set_modifier4_value(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, previous));
    FT_ASSERT_EQ(5, hero.get_physical_armor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement->enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t replace_result = hero.equip_item(EQUIP_HEAD, replacement);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, replace_result);
    FT_ASSERT_EQ(5, hero.get_physical_armor());
    ft_sharedptr<game_item> *current = hero.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(current != ft_nullptr);
    FT_ASSERT(current->get() == previous.get());
    delete current;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, replacement));
    FT_ASSERT_EQ(22, hero.get_physical_armor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero._equipment.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    hero.unequip_item(EQUIP_HEAD);
    const int32_t unequip_result = hero.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, unequip_result);
    FT_ASSERT_EQ(22, hero.get_physical_armor());
    current = hero.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(current != ft_nullptr);
    FT_ASSERT(current->get() == replacement.get());
    delete current;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero._equipment.disable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    hero.unequip_item(EQUIP_HEAD);
    const int32_t cached_removal_result = hero.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS,
        std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cached_removal_result);
    FT_ASSERT_EQ(0, hero.get_physical_armor());
    return (1);
}

FT_TEST(test_game_character_equipment_removes_previously_applied_modifier_values)
{
    game_character hero;
    ft_sharedptr<game_item> item(new game_item());
    ft_sharedptr<game_item> replacement(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement->initialize());
    hero.set_physical_armor(10);
    item->set_modifier1_id(1);
    item->set_modifier1_value(4);
    replacement->set_modifier1_id(1);
    replacement->set_modifier1_value(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, item));
    FT_ASSERT_EQ(14, hero.get_physical_armor());
    item->set_modifier1_value(8);
    FT_ASSERT_EQ(14, hero.get_physical_armor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, item));
    FT_ASSERT_EQ(18, hero.get_physical_armor());
    item->set_modifier1_id(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, replacement));
    FT_ASSERT_EQ(13, hero.get_physical_armor());
    FT_ASSERT_EQ(0, hero.get_magic_armor());
    replacement->set_modifier1_value(27);
    hero.unequip_item(EQUIP_HEAD);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    FT_ASSERT_EQ(10, hero.get_physical_armor());
    return (1);
}

FT_TEST(test_game_character_move_preserves_applied_equipment_modifiers)
{
    game_character source;
    game_character destination;
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_modifier1_id(1);
    item->set_modifier1_value(11);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip_item(EQUIP_FEET, item));
    item->set_modifier1_value(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(11, destination.get_physical_armor());
    destination.unequip_item(EQUIP_FEET);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_error());
    FT_ASSERT_EQ(0, destination.get_physical_armor());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    const int32_t armor_after_reinitialize = source.get_physical_armor();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip_item(EQUIP_FEET, item));
    FT_ASSERT_EQ(armor_after_reinitialize + 19, source.get_physical_armor());
    source.unequip_item(EQUIP_FEET);
    FT_ASSERT_EQ(armor_after_reinitialize, source.get_physical_armor());
    return (1);
}

FT_TEST(test_game_character_equipment_overflow_leaves_slot_and_stats_unchanged)
{
    game_character hero;
    ft_sharedptr<game_item> original(new game_item());
    ft_sharedptr<game_item> replacement(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replacement->initialize());
    hero.set_physical_armor(5);
    hero.set_might(INT32_MAX);
    original->set_modifier1_id(1);
    original->set_modifier1_value(2);
    replacement->set_modifier1_id(1);
    replacement->set_modifier1_value(3);
    replacement->set_modifier2_id(2);
    replacement->set_modifier2_value(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_HEAD, original));
    hero.set_current_physical_armor(3);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, hero.equip_item(EQUIP_HEAD, replacement));
    FT_ASSERT_EQ(7, hero.get_physical_armor());
    FT_ASSERT_EQ(3, hero.get_current_physical_armor());
    FT_ASSERT_EQ(INT32_MAX, hero.get_might());
    auto *current = hero.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(current != ft_nullptr && current->get() == original.get());
    delete current;
    hero.unequip_item(EQUIP_HEAD);
    FT_ASSERT_EQ(5, hero.get_physical_armor());
    return (1);
}

FT_TEST(test_game_character_equipment_validates_net_delta_before_narrowing)
{
    game_character hero;
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_modifier1_id(2);
    item->set_modifier1_value(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_CHEST, item));
    FT_ASSERT_EQ(INT32_MIN, hero.get_might());

    hero.set_might(INT32_MAX);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.equip_item(EQUIP_CHEST, item));
    FT_ASSERT_EQ(INT32_MAX, hero.get_might());
    hero.unequip_item(EQUIP_CHEST);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, hero.get_error());
    FT_ASSERT_EQ(INT32_MAX, hero.get_might());
    auto *current = hero.get_equipped_item(EQUIP_CHEST);
    FT_ASSERT(current != ft_nullptr && current->get() == item.get());
    delete current;

    hero.set_might(INT32_MIN);
    hero.unequip_item(EQUIP_CHEST);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    FT_ASSERT_EQ(0, hero.get_might());
    return (1);
}
