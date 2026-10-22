#include "../test_internal.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_equipment.hpp"
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
    json_free_groups(group);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest.get_error());
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
