#include "../test_internal.hpp"
#include "../../Modules/Game/game_character_snapshot.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_world.hpp"
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
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.save_to_buffer(legacy, character, character.get_inventory()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_character_save_snapshot(snapshot,
        character, character.get_inventory()));
    FT_ASSERT_EQ(0, ft_strcmp(legacy.c_str(), snapshot.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.load_from_buffer(snapshot.c_str(),
        restored_character, restored_character.get_inventory()));
    FT_ASSERT_EQ(57, restored_character.get_hit_points());
    FT_ASSERT_EQ(-23, restored_character.get_x());
    FT_ASSERT_EQ(FT_DAMAGE_RULE_BUFFER, restored_character.get_damage_rule());
    const auto &restored_skills = restored_character.get_skills();
    const auto *restored_skill = restored_skills.find(9);
    FT_ASSERT(restored_skill != restored_skills.end());
    FT_ASSERT_EQ(-14, restored_skill->value.get_modifier4());
    FT_ASSERT_EQ(3, restored_character.get_inventory().count_item(9));
    FT_ASSERT_EQ(1, restored_character.get_inventory().count_item(10));
    return (1);
}
