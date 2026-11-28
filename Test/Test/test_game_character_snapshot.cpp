#include "../test_internal.hpp"
#include "../../Modules/Game/game_character_snapshot.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_character_snapshot_matches_world_codec)
{
    game_character character;
    game_character restored_character;
    game_world world;
    game_skill skill;
    ft_string legacy;
    ft_string snapshot;
    ft_sharedptr<game_item> stack(new game_item());
    ft_sharedptr<game_item> helmet(new game_item());
    ft_sharedptr<game_item> physical_item(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, snapshot.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helmet->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, physical_item->initialize());
    character.set_hit_points(57);
    character.set_x(-23);
    character.set_damage_rule(FT_DAMAGE_RULE_BUFFER);
    skill.set_id(9);
    skill.set_modifier4(-14);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    skill.set_id(3);
    skill.set_level(5);
    skill.set_cooldown(-7);
    skill.set_modifier1(11);
    skill.set_modifier2(-12);
    skill.set_modifier3(13);
    skill.set_modifier4(INT32_MIN);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    game_skill *stored_skill = character.get_skill(3);
    FT_ASSERT(stored_skill != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored_skill->enable_thread_safety());
    stack->set_item_id(9);
    stack->set_max_stack(8);
    stack->set_stack_size(3);
    stack->set_width(1);
    stack->set_height(1);
    stack->set_rarity(2);
    stack->set_modifier2_id(8);
    stack->set_modifier2_value(-6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_inventory().add_item(stack));
    physical_item->set_item_id(10);
    physical_item->set_instance_id(UINT64_MAX - 1U);
    physical_item->set_max_stack(1);
    physical_item->set_stack_size(1);
    physical_item->set_width(1);
    physical_item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_inventory().add_item(physical_item));
    helmet->set_item_id(12);
    helmet->set_instance_id(7U);
    helmet->set_max_stack(1);
    helmet->set_stack_size(1);
    helmet->set_width(1);
    helmet->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.equip_item(EQUIP_HEAD, helmet));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, helmet->enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.save_to_buffer(legacy, character, character.get_inventory()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_character_save_snapshot(snapshot,
        character, character.get_inventory()));
    FT_ASSERT_EQ(0, ft_strcmp(legacy.c_str(), snapshot.c_str()));
    ft_string adopted;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        world.save_to_buffer(adopted, character, character.get_inventory()));
    FT_ASSERT_EQ(snapshot.size(), adopted.size());
    FT_ASSERT_EQ(0, ft_strcmp(snapshot.c_str(), adopted.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t limited_save = world.save_to_buffer(adopted, character, character.get_inventory());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_GAME_GENERAL_ERROR, limited_save);
    FT_ASSERT_EQ(snapshot.size(), adopted.size());
    FT_ASSERT_EQ(0, ft_strcmp(snapshot.c_str(), adopted.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.load_from_buffer(snapshot.c_str(),
        restored_character, restored_character.get_inventory()));
    FT_ASSERT_EQ(57, restored_character.get_hit_points());
    FT_ASSERT_EQ(-23, restored_character.get_x());
    FT_ASSERT_EQ(FT_DAMAGE_RULE_BUFFER, restored_character.get_damage_rule());
    const auto &restored_skills = restored_character.get_skills();
    const auto *restored_skill = restored_skills.find(9);
    FT_ASSERT(restored_skill != restored_skills.end());
    FT_ASSERT_EQ(-14, restored_skill->value.get_modifier4());
    const auto *second_skill = restored_skills.find(3);
    FT_ASSERT(second_skill != restored_skills.end());
    FT_ASSERT_EQ(5, second_skill->value.get_level());
    FT_ASSERT_EQ(-7, second_skill->value.get_cooldown());
    FT_ASSERT_EQ(11, second_skill->value.get_modifier1());
    FT_ASSERT_EQ(-12, second_skill->value.get_modifier2());
    FT_ASSERT_EQ(13, second_skill->value.get_modifier3());
    FT_ASSERT_EQ(INT32_MIN, second_skill->value.get_modifier4());
    FT_ASSERT_EQ(3, restored_character.get_inventory().count_item(9));
    FT_ASSERT_EQ(1, restored_character.get_inventory().count_item(10));
    return (1);
}

FT_TEST(test_game_character_checkpoint_item_keeps_output_on_item_lock_failure)
{
    game_character character;
    ft_sharedptr<game_item> item(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->initialize());
    item->set_item_id(42);
    item->set_max_stack(1);
    item->set_stack_size(1);
    item->set_width(1);
    item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.equip_item(EQUIP_WEAPON, item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item->enable_thread_safety());
    game_inventory_item_snapshot captured = {};
    ft_bool present = FT_FALSE;
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t status = character.capture_checkpoint_item(EQUIP_WEAPON, captured, present);
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, status);
    FT_ASSERT_EQ(FT_FALSE, present);
    FT_ASSERT_EQ(0, captured.item_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        character.capture_checkpoint_item(EQUIP_WEAPON, captured, present));
    FT_ASSERT_EQ(FT_TRUE, present);
    FT_ASSERT_EQ(42, captured.item_id);
    return (1);
}

FT_TEST(test_game_character_skill_replacement_preserves_live_value_on_source_failure)
{
    game_character character;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(7);
    skill.set_level(19);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    game_skill *stored = character.get_skill(7);
    FT_ASSERT(stored != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(*stored));
    FT_ASSERT_EQ(19, character.get_skill(7)->get_level());

    skill.set_level(30);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.enable_thread_safety());
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const int32_t failed_add = character.add_skill(skill);
    const int32_t add_error = character.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, failed_add);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, add_error);
    FT_ASSERT_EQ(19, character.get_skill(7)->get_level());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    FT_ASSERT_EQ(30, character.get_skill(7)->get_level());
    return (1);
}

FT_TEST(test_game_character_skill_operations_preserve_map_errors)
{
    game_character character;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    skill.set_id(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_skills().enable_thread_safety());

    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SYS_MUTEX_LOCK_FAILED,
        std::memory_order_release);
    const game_skill *found = static_cast<const game_character &>(character).get_skill(4);
    const int32_t lookup_error = character.get_error();
    const int32_t add_error = character.add_skill(skill);
    character.remove_skill(4);
    const int32_t remove_error = character.get_error();
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);

    FT_ASSERT(found == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, lookup_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, add_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, remove_error);
    FT_ASSERT(character.get_skill(4) != ft_nullptr);
    character.remove_skill(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    FT_ASSERT(character.get_skill(4) == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, character.get_error());
    return (1);
}

FT_TEST(test_game_character_skill_removal_preserves_retained_mutexes)
{
    game_character character;
    game_skill skill;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.initialize());
    int32_t id = 1;
    while (id <= 3)
    {
        skill.set_id(id);
        skill.set_level(id * 11);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
        ++id;
    }
    game_skill *first = character.get_skill(1);
    game_skill *last = character.get_skill(3);
    FT_ASSERT(first != ft_nullptr && last != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, last->enable_thread_safety());
    const auto *first_mutex = first->_mutex;
    const auto *last_mutex = last->_mutex;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    character.remove_skill(2);
    const int32_t allocation_error = character.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, allocation_error);
    FT_ASSERT(character.get_skill(1) == first);
    FT_ASSERT(character.get_skill(2) != ft_nullptr);
    FT_ASSERT(character.get_skill(3) == last);

    character.remove_skill(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.get_error());
    FT_ASSERT(character.get_skill(2) == ft_nullptr);
    first = character.get_skill(1);
    last = character.get_skill(3);
    FT_ASSERT(first != ft_nullptr && last != ft_nullptr);
    FT_ASSERT(first->_mutex == first_mutex);
    FT_ASSERT(last->_mutex == last_mutex);
    FT_ASSERT_EQ(11, first->get_level());
    FT_ASSERT_EQ(33, last->get_level());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, last->disable_thread_safety());
    skill.set_id(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, character.add_skill(skill));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    character.remove_skill(3);
    const int32_t in_place_error = character.get_error();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, in_place_error);
    FT_ASSERT(character.get_skill(1) == first);
    FT_ASSERT(character.get_skill(4) != ft_nullptr);
    return (1);
}
