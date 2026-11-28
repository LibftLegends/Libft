#include "../test_internal.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_equipment.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

json_group *serialize_inventory(const game_inventory &inventory);
json_group *serialize_equipment(const game_character &character);
json_group *serialize_quest(const game_quest &quest);
json_group *serialize_character(const game_character &character);
int deserialize_inventory(game_inventory &inventory, json_group *group);
int32_t deserialize_equipment(game_character &character, json_group *group);

FT_TEST(test_game_world_round_trip_restores_live_and_applied_equipment_modifiers)
{
    game_world world;
    game_character source;
    game_character restored;
    ft_string buffer;
    ft_sharedptr<game_item> helmet(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helmet->initialize());
    source.set_physical_armor(10);
    source.set_might(6);
    helmet->set_modifier1_id(1);
    helmet->set_modifier1_value(4);
    helmet->set_modifier2_id(2);
    helmet->set_modifier2_value(-2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip_item(EQUIP_HEAD, helmet));
    source.set_current_physical_armor(11);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.save_to_buffer(buffer, source, source.get_inventory()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        world.load_from_buffer(buffer.c_str(), restored, restored.get_inventory()));
    FT_ASSERT_EQ(14, restored.get_physical_armor());
    FT_ASSERT_EQ(11, restored.get_current_physical_armor());

    helmet->set_modifier1_value(8);
    helmet->set_modifier2_value(9);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.save_to_buffer(buffer, source, source.get_inventory()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        world.load_from_buffer(buffer.c_str(), restored, restored.get_inventory()));
    FT_ASSERT_EQ(14, restored.get_physical_armor());
    FT_ASSERT_EQ(11, restored.get_current_physical_armor());
    FT_ASSERT_EQ(4, restored.get_might());
    auto *item = restored.get_equipped_item(EQUIP_HEAD);
    FT_ASSERT(item != ft_nullptr && item->get() != ft_nullptr);
    FT_ASSERT_EQ(8, (*item)->get_modifier1_value());
    FT_ASSERT_EQ(9, (*item)->get_modifier2_value());
    delete item;
    restored.unequip_item(EQUIP_HEAD);
    FT_ASSERT_EQ(10, restored.get_physical_armor());
    FT_ASSERT_EQ(6, restored.get_might());
    return (1);
}

FT_TEST(test_deserialize_equipment_rejects_truncated_applied_modifiers)
{
    game_character source;
    game_character restored;
    ft_sharedptr<game_item> helmet(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helmet->initialize());
    helmet->set_modifier1_id(1);
    helmet->set_modifier1_value(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip_item(EQUIP_HEAD, helmet));
    helmet->set_modifier1_value(8);
    json_group *group = serialize_equipment(source);
    FT_ASSERT(group != ft_nullptr);
    json_item *applied = json_find_item(group, "head_applied_modifiers");
    FT_ASSERT(applied != ft_nullptr);
    json_remove_item(group, "head_applied_modifiers");
    applied = json_create_item("head_applied_modifiers", "1,4,0,0,0,0,0,0");
    FT_ASSERT(applied != ft_nullptr);
    json_add_item_to_group(group, applied);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_equipment(restored, group));
    FT_ASSERT_EQ(4, restored.get_physical_armor());
    json_remove_item(group, "head_applied_modifiers");
    applied = json_create_item("head_applied_modifiers", "1,4,0");
    FT_ASSERT(applied != ft_nullptr);
    json_add_item_to_group(group, applied);
    const int32_t result = deserialize_equipment(restored, group);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(4, restored.get_physical_armor());
    return (1);
}
int32_t deserialize_quest(game_quest &quest, json_group *group);

FT_TEST(test_serialize_inventory_allocation_failure_sets_errno)
{
    game_inventory inventory;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    cma_set_alloc_limit(1);
    json_group *group = serialize_inventory(inventory);
    cma_set_alloc_limit(0);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

FT_TEST(test_serialize_inventory_item_field_failure_allows_retry)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(2U, 0));
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT(item.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(17);
    item->set_max_stack(1);
    item->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(sizeof("current_weight")));
    json_group *failed = serialize_inventory(inventory);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    const ft_bool failed_as_expected = failed == ft_nullptr;
    if (failed != ft_nullptr)
        json_free_groups(failed);
    FT_ASSERT_EQ(FT_TRUE, failed_as_expected);

    json_group *group = serialize_inventory(inventory);
    FT_ASSERT(group != ft_nullptr);
    const json_item *first_item_field = json_find_item(group, "item_0_instance_id");
    const json_item *last_item_field = json_find_item(group, "item_0_rarity");
    const ft_bool has_expected_fields = first_item_field != ft_nullptr
        && last_item_field != ft_nullptr;
    json_free_groups(group);
    FT_ASSERT_EQ(FT_TRUE, has_expected_fields);
    return (1);
}

FT_TEST(test_serialize_inventory_null_item_sets_errno)
{
    game_inventory inventory;
    ft_sharedptr<game_item> null_item(ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());

    inventory.get_items().insert(0, null_item);
    json_group *group = serialize_inventory(inventory);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, inventory.get_error());
    return (1);
}

FT_TEST(test_serialize_inventory_success_clears_errno)
{
    game_inventory inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT(item.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());

    item->set_item_id(42);
    item->set_max_stack(5);
    item->set_stack_size(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    json_group *group = serialize_inventory(inventory);
    FT_ASSERT(group != ft_nullptr);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    return (1);
}

FT_TEST(test_serialize_equipment_allocation_failure_sets_errno)
{
    game_character character;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    cma_set_alloc_limit(1);
    json_group *group = serialize_equipment(character);
    cma_set_alloc_limit(0);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    return (1);
}

FT_TEST(test_serialize_equipment_success_clears_errno)
{
    game_character character;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    ft_sharedptr<game_item> weapon(new game_item());
    FT_ASSERT(weapon.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, weapon->initialize());

    weapon->set_item_id(7);
    weapon->set_max_stack(1);
    weapon->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.equip_item(EQUIP_WEAPON, weapon));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    json_group *group = serialize_equipment(character);
    FT_ASSERT(group != ft_nullptr);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    return (1);
}

FT_TEST(test_serialize_quest_allocation_failure_sets_errno)
{
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());

    cma_set_alloc_limit(1);
    json_group *group = serialize_quest(quest);
    cma_set_alloc_limit(0);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    return (1);
}

FT_TEST(test_serialize_quest_null_reward_sets_errno)
{
    game_quest quest;
    ft_sharedptr<game_item> null_reward(ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());

    quest.get_reward_items().push_back(null_reward);
    json_group *group = serialize_quest(quest);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    return (1);
}

FT_TEST(test_serialize_quest_success_clears_errno)
{
    game_quest quest;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    ft_sharedptr<game_item> reward(new game_item());
    FT_ASSERT(reward.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reward->initialize());

    reward->set_item_id(9);
    reward->set_max_stack(3);
    reward->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_reward_items().push_back(reward));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    json_group *group = serialize_quest(quest);
    FT_ASSERT(group != ft_nullptr);
    const json_item *item_id = json_find_item(group, "reward_item_0_id");
    FT_ASSERT(item_id != ft_nullptr && item_id->next != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(item_id->next->key, "reward_item_0_rarity"));
    FT_ASSERT(item_id->next->next != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(item_id->next->next->key, "reward_item_0_width"));
    game_quest restored;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_quest(restored, group));
    FT_ASSERT_EQ(1U, restored.get_reward_items().size());
    FT_ASSERT_EQ(9, restored.get_reward_items()[0]->get_item_id());
    json_remove_item(group, "reward_item_0_rarity");
    json_remove_item(group, "reward_item_0_id");
    json_item *reordered = json_create_item("reward_item_0_id", 12);
    FT_ASSERT(reordered != ft_nullptr);
    json_add_item_to_group(group, reordered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_quest(restored, group));
    FT_ASSERT_EQ(1U, restored.get_reward_items().size());
    FT_ASSERT_EQ(12, restored.get_reward_items()[0]->get_item_id());
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
    return (1);
}

FT_TEST(test_deserialize_equipment_accepts_reordered_slot_and_fields)
{
    game_character source;
    game_character restored;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT(item.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(17);
    item->set_instance_id(UINT64_MAX);
    item->set_max_stack(1);
    item->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.equip_item(EQUIP_OFF_HAND, item));
    json_group *group = serialize_equipment(source);
    FT_ASSERT(group != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, json_find_item(group, "off_hand_applied_modifiers"));
    json_remove_item(group, "off_hand_present");
    json_item *flag = json_create_item("off_hand_present", 1);
    FT_ASSERT(flag != ft_nullptr);
    json_add_item_to_group(group, flag);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_equipment(restored, group));
    json_free_groups(group);
    ft_sharedptr<game_item> *handle = restored.get_equipped_item(EQUIP_OFF_HAND);
    FT_ASSERT(handle != ft_nullptr);
    const ft_bool matches = handle->get() != ft_nullptr
        && (*handle)->get_instance_id() == UINT64_MAX
        && (*handle)->get_item_id() == 17;
    delete handle;
    FT_ASSERT_EQ(FT_TRUE, matches);
    return (1);
}

FT_TEST(test_serialize_character_allocation_failure_sets_errno)
{
    game_character character;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    cma_set_alloc_limit(1);
    json_group *group = serialize_character(character);
    cma_set_alloc_limit(0);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    return (1);
}

FT_TEST(test_serialize_character_skill_error_sets_errno)
{
    game_character broken_character;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, broken_character.initialize());
    cma_set_alloc_limit(1);
    json_group *group = serialize_character(broken_character);
    FT_ASSERT(group == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, broken_character.get_error());
    return (1);
}

FT_TEST(test_serialize_character_success_clears_errno)
{
    game_character character;
    game_skill skill;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(3);
    skill.set_level(2);
    skill.set_cooldown(5);
    skill.set_modifier1(1);
    skill.set_modifier2(2);
    skill.set_modifier3(3);
    skill.set_modifier4(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    json_group *group = serialize_character(character);
    FT_ASSERT(group != ft_nullptr);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    return (1);
}

FT_TEST(test_character_skill_save_round_trips_multi_digit_indexes)
{
    game_character source;
    game_character restored;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    int32_t index = 0;
    while (index < 13)
    {
        skill.set_id(100 + index);
        skill.set_level(1 + index);
        skill.set_cooldown(2 + index);
        skill.set_modifier1(-1);
        skill.set_modifier2(-2);
        skill.set_modifier3(-3);
        skill.set_modifier4(-4 - index);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_skill(skill));
        ++index;
    }
    json_group *group = serialize_character(source);
    FT_ASSERT(group != ft_nullptr);
    FT_ASSERT(json_find_item(group, "skill_12_cooldown") != ft_nullptr);
    const int32_t result = deserialize_character(restored, group);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(13U, restored.get_skills().size());
    index = 0;
    while (index < 13)
    {
        const game_skill *loaded = restored.get_skill(100 + index);
        FT_ASSERT(loaded != ft_nullptr);
        FT_ASSERT_EQ(1 + index, loaded->get_level());
        FT_ASSERT_EQ(2 + index, loaded->get_cooldown());
        FT_ASSERT_EQ(-1, loaded->get_modifier1());
        FT_ASSERT_EQ(-2, loaded->get_modifier2());
        FT_ASSERT_EQ(-3, loaded->get_modifier3());
        FT_ASSERT_EQ(-4 - index, loaded->get_modifier4());
        ++index;
    }
    return (1);
}

FT_TEST(test_character_skill_load_preserves_first_duplicate_and_partial_progress)
{
    const char *save = "{\"character\":{\"skill_count\":3,"
        "\"skill_0_id\":20,\"skill_0_level\":4,\"skill_0_cooldown\":5,"
        "\"skill_0_mod1\":1,\"skill_0_mod2\":2,\"skill_0_mod3\":3,\"skill_0_mod4\":4,"
        "\"skill_1_mod4\":11,\"skill_1_level\":7,\"skill_1_id\":21,"
        "\"skill_1_mod3\":10,\"skill_1_mod1\":8,\"skill_1_cooldown\":9,\"skill_1_mod2\":9,"
        "\"skill_2_id\":22}}";
    game_character restored;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.initialize());
    json_group *group = json_read_from_string(save);
    FT_ASSERT(group != ft_nullptr);
    json_item *duplicate = json_create_item("skill_1_level", 99);
    FT_ASSERT(duplicate != ft_nullptr);
    json_add_item_to_group(group, duplicate);
    const int32_t result = deserialize_character(restored, group);
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, result);
    FT_ASSERT_EQ(2U, restored.get_skills().size());
    const game_skill *ordered = restored.get_skill(20);
    const game_skill *reordered = restored.get_skill(21);
    FT_ASSERT(ordered != ft_nullptr && reordered != ft_nullptr);
    FT_ASSERT_EQ(4, ordered->get_level());
    FT_ASSERT_EQ(7, reordered->get_level());
    FT_ASSERT_EQ(11, reordered->get_modifier4());
    FT_ASSERT(restored.get_skill(22) == ft_nullptr);
    return (1);
}

FT_TEST(test_deserialize_inventory_accepts_reordered_fields_and_first_duplicate)
{
    game_inventory source;
    game_inventory restored;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2U, 0));
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT(item.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(17);
    item->set_instance_id(UINT64_MAX);
    item->set_max_stack(1);
    item->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
    json_group *group = serialize_inventory(source);
    FT_ASSERT(group != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_inventory(restored, group));
    game_inventory_snapshot snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_snapshot(snapshot));
    FT_ASSERT_EQ(1U, snapshot.items.size());
    FT_ASSERT_EQ(UINT64_MAX, snapshot.items[0].instance_id);
    FT_ASSERT_EQ(17, snapshot.items[0].item_id);

    json_remove_item(group, "item_0_id");
    json_item *first = json_create_item("item_0_id", 18);
    json_item *duplicate = json_create_item("item_0_id", 19);
    FT_ASSERT(first != ft_nullptr && duplicate != ft_nullptr);
    json_add_item_to_group(group, first);
    json_add_item_to_group(group, duplicate);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_inventory(restored, group));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored.get_snapshot(snapshot));
    FT_ASSERT_EQ(1U, snapshot.items.size());
    FT_ASSERT_EQ(UINT64_MAX, snapshot.items[0].instance_id);
    FT_ASSERT_EQ(18, snapshot.items[0].item_id);
    json_free_groups(group);
    return (1);
}

FT_TEST(test_deserialize_inventory_keeps_prior_stack_when_next_row_cannot_fit)
{
    game_inventory source;
    game_inventory restored;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(2U, 0));
    int32_t item_id = 21;
    while (item_id < 23)
    {
        ft_sharedptr<game_item> item(new game_item());
        FT_ASSERT(item.get() != ft_nullptr);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
        item->set_item_id(item_id);
        item->set_width(1);
        item->set_height(1);
        item->set_max_stack(3);
        item->set_stack_size(2);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, source.add_item(item));
        ++item_id;
    }
    json_group *group = serialize_inventory(source);
    FT_ASSERT(group != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_inventory(restored, group));
    FT_ASSERT_EQ(2, restored.count_item(21));
    FT_ASSERT_EQ(2, restored.count_item(22));

    json_update_item(group, "capacity", 1);
    json_update_item(group, "item_1_id", 21);
    FT_ASSERT_EQ(FT_ERR_FULL, deserialize_inventory(restored, group));
    FT_ASSERT_EQ(2, restored.count_item(21));
    FT_ASSERT_EQ(0, restored.count_item(22));

    json_update_item(group, "item_1_max_stack", 0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, deserialize_inventory(restored, group));
    FT_ASSERT_EQ(2, restored.count_item(21));
    json_free_groups(group);
    return (1);
}

FT_TEST(test_deserialize_inventory_failure_then_success_updates_errno)
{
    game_inventory inventory;
    json_group *group = json_create_json_group("inventory");

    FT_ASSERT(group != ft_nullptr);
    json_item *item = json_create_item("capacity", 2);

    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("weight_limit", 50);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("current_weight", 10);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("used_slots", 1);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_count", 1);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_stack_size", 1);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_id", 7);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_width", 1);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_height", 1);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod1_id", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod1_value", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod2_id", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod2_value", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod3_id", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod3_value", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod4_id", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    item = json_create_item("item_0_mod4_value", 0);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, deserialize_inventory(inventory, group));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    item = json_create_item("item_0_max_stack", 5);
    FT_ASSERT(item != ft_nullptr);
    json_add_item_to_group(group, item);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, deserialize_inventory(inventory, group));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_error());
    json_free_groups(group);
    return (1);
}
