#include "../test_internal.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_reputation.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/Game/game_event_scheduler.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/JSon/json.hpp"
#include <climits>
#include <cstdio>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

int test_game_simulation(void)
{
    game_character hero;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    hero.set_hit_points(50);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    hero.set_might(10);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    hero.set_physical_armor(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());

    game_map3d grid;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid.initialize(3, 3, 1, 0));
    grid.set(1, 1, 0, 1);
    hero.set_x(1);
    hero.set_y(1);
    hero.set_z(0);

    game_buff strength;
    strength.set_id(1);
    strength.set_modifier1(5);
    hero.get_buffs().insert(strength.get_id(), strength);
    hero.set_might(hero.get_might() + strength.get_modifier1());
    if (hero.get_might() != 15)
        return (0);

    game_debuff weakness;
    weakness.set_id(2);
    weakness.set_modifier1(-2);
    hero.get_debuffs().insert(weakness.get_id(), weakness);
    hero.set_physical_armor(hero.get_physical_armor() + weakness.get_modifier1());
    if (hero.get_physical_armor() != 3)
        return (0);

    game_upgrade upgrade;
    upgrade.set_id(1);
    upgrade.set_modifier1(3);
    hero.get_upgrades().insert(upgrade.get_id(), upgrade);
    Pair<int, game_upgrade>* uentry = hero.get_upgrades().find(1);
    if (uentry == hero.get_upgrades().end())
        return (0);
    hero.set_might(hero.get_might() + uentry->value.get_modifier1());
    if (hero.get_might() != 18)
        return (0);

    game_quest quest;
    quest.set_id(1);
    quest.set_phases(2);
    hero.get_quests().insert(quest.get_id(), quest);
    Pair<int, game_quest>* qentry = hero.get_quests().find(1);
    if (qentry == hero.get_quests().end())
        return (0);
    qentry->value.set_current_phase(1);
    if (qentry->value.get_current_phase() != 1)
        return (0);

    hero.get_reputation().set_milestone(1, 10);
    hero.get_reputation().add_current_rep(4);
    if (hero.get_reputation().get_current_rep() != 4 ||
        hero.get_reputation().get_total_rep() != 4)
        return (0);

    ft_sharedptr<game_world> overworld(new game_world());
    FT_ASSERT(overworld.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, overworld->initialize());
    ft_sharedptr<game_event> meeting(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, meeting->initialize());
    meeting->set_id(1);
    meeting->set_duration(5);
    overworld->schedule_event(meeting);
    overworld->update_events(overworld, 1);
    ft_vector<ft_sharedptr<game_event> > events;
    overworld->get_event_scheduler()->dump_events(events);
    if (events.size() != 1 || events[0]->get_duration() != 4)
        return (0);

    game_inventory pack;
    ft_sharedptr<game_item> potion(new game_item());
    potion->set_item_id(1);
    potion->set_max_stack(10);
    potion->set_stack_size(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.add_item(potion));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.get_error());
    ft_sharedptr<game_item> more(new game_item());
    more->set_item_id(1);
    more->set_max_stack(10);
    more->set_stack_size(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.add_item(more));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, pack.get_error());
    Pair<int, ft_sharedptr<game_item> > *ientry = pack.get_items().find(0);
    if (ientry == pack.get_items().end() || ientry->value->get_stack_size() != 8)
        return (0);

    if (grid.get(hero.get_x(), hero.get_y(), hero.get_z()) != 1)
        return (0);

    return (1);
}

int test_item_basic(void)
{
    game_item item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());

    item.set_item_id(1);
    item.set_max_stack(10);
    item.set_stack_size(3);
    item.set_modifier1_id(5);
    item.set_modifier1_value(2);
    item.set_width(2);
    item.set_height(3);
    if (item.get_item_id() != 1 || item.get_max_stack() != 10 ||
        item.get_stack_size() != 3 || item.get_modifier1_id() != 5 ||
        item.get_modifier1_value() != 2 || item.get_width() != 2 ||
        item.get_height() != 3)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    return (1);
}

int test_character_valor(void)
{
    game_character hero;
    hero.set_valor(42);
    return (hero.get_valor() == 42);
}

int test_character_add_sub_coins(void)
{
    game_character hero;
    hero.add_coins(10);
    hero.sub_coins(3);
    return (hero.get_coins() == 7);
}

int test_character_add_sub_valor(void)
{
    game_character hero;
    hero.add_valor(5);
    hero.sub_valor(2);
    return (hero.get_valor() == 3);
}

int test_game_save_load(void)
{
    game_character hero;
    hero.set_hit_points(42);
    game_world world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    ft_sharedptr<game_event> event(new game_event());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event->initialize());
    event->set_id(1);
    event->set_duration(5);
    world.schedule_event(event);
    game_inventory inventory;
    ft_sharedptr<game_item> saved_item(new game_item());
    uint64_t inventory_revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4U, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, saved_item->initialize());
    saved_item->set_item_id(91);
    saved_item->set_stack_size(2);
    saved_item->set_max_stack(8);
    saved_item->set_width(1);
    saved_item->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(saved_item));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.get_revision(&inventory_revision));
    if (world.save_to_file("test_save.json", hero, inventory) != FT_ERR_SUCCESS)
        return (0);
    game_character loaded_hero;
    game_world loaded_world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_world.initialize());
    game_inventory loaded_inventory;
    if (loaded_world.load_from_file("test_save.json", loaded_hero, loaded_inventory) != FT_ERR_SUCCESS)
        return (0);
    uint64_t loaded_inventory_revision = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        loaded_inventory.get_revision(&loaded_inventory_revision));
    ft_vector<ft_sharedptr<game_event> > loaded_events;
    loaded_world.get_event_scheduler()->dump_events(loaded_events);
    remove("test_save.json");
    if (loaded_events.size() != 1)
        return (0);
    if (loaded_events[0]->get_duration() != 5)
        return (0);
    if (loaded_hero.get_hit_points() != 42)
        return (0);
    FT_ASSERT_EQ(inventory_revision, loaded_inventory_revision);
    FT_ASSERT_EQ(2, loaded_inventory.count_item(91));
    return (1);
}

int test_buff_subtracters(void)
{
    game_buff buff;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buff.initialize());
    buff.set_duration(10);
    buff.sub_duration(3);
    buff.set_modifier1(5);
    buff.sub_modifier1(2);
    buff.set_modifier2(4);
    buff.sub_modifier2(1);
    buff.set_modifier3(6);
    buff.sub_modifier3(6);
    buff.set_modifier4(8);
    buff.sub_modifier4(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buff.get_error());
    return (buff.get_duration() == 7 && buff.get_modifier1() == 3 &&
            buff.get_modifier2() == 3 && buff.get_modifier3() == 0 &&
            buff.get_modifier4() == 5);
}

int test_debuff_subtracters(void)
{
    game_debuff debuff;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, debuff.initialize());
    debuff.set_duration(8);
    debuff.sub_duration(2);
    debuff.set_modifier1(-1);
    debuff.sub_modifier1(-1);
    debuff.set_modifier2(3);
    debuff.sub_modifier2(1);
    debuff.set_modifier3(2);
    debuff.sub_modifier3(2);
    debuff.set_modifier4(0);
    debuff.sub_modifier4(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, debuff.get_error());
    return (debuff.get_duration() == 6 && debuff.get_modifier1() == 0 &&
            debuff.get_modifier2() == 2 && debuff.get_modifier3() == 0 &&
            debuff.get_modifier4() == 0);
}

int test_event_subtracters(void)
{
    game_event ev;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ev.initialize());
    ev.set_duration(5);
    ev.sub_duration(1);
    ev.set_modifier1(4);
    ev.sub_modifier1(2);
    ev.set_modifier2(3);
    ev.sub_modifier2(3);
    ev.set_modifier3(0);
    ev.sub_modifier3(0);
    ev.set_modifier4(-2);
    ev.sub_modifier4(-1);
    return (ev.get_duration() == 4 && ev.get_modifier1() == 2 &&
            ev.get_modifier2() == 0 && ev.get_modifier3() == 0 &&
            ev.get_modifier4() == -1);
}

FT_TEST(test_game_event_sub_duration_prevents_underflow)
{
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());

    event.set_duration(3);
    event.sub_duration(5);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event.get_error());
    FT_ASSERT_EQ(3, event.get_duration());

    event.set_duration(3);
    event.sub_duration(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(0, event.get_duration());
    return (1);
}

FT_TEST(test_game_event_add_duration_detects_overflow)
{
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());

    event.set_duration(INT_MAX - 2);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.add_duration(5));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, event.get_error());
    FT_ASSERT_EQ(INT_MAX - 2, event.get_duration());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.add_duration(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(INT_MAX, event.get_duration());
    return (1);
}

FT_TEST(test_game_event_add_duration_rejects_negative)
{
    game_event event;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.initialize());

    event.set_duration(4);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event.add_duration(-1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, event.get_error());
    FT_ASSERT_EQ(4, event.get_duration());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.add_duration(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, event.get_error());
    FT_ASSERT_EQ(6, event.get_duration());
    return (1);
}

FT_TEST(test_inventory_remove_item_releases_usage)
{
    game_inventory inventory;
    ft_sharedptr<game_item> stack(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(3, 20));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack->get_error());

    stack->set_item_id(7);
    stack->set_max_stack(10);
    stack->set_stack_size(3);
    stack->set_width(2);
    stack->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(stack));
    FT_ASSERT_EQ(2u, inventory.get_used());
    FT_ASSERT_EQ(3, inventory.get_current_weight());
    inventory.remove_item(0);
    FT_ASSERT_EQ(0u, inventory.get_used());
    FT_ASSERT_EQ(0, inventory.get_current_weight());
    FT_ASSERT_EQ(false, inventory.has_item(7));
    return (1);
}

FT_TEST(test_inventory_count_rarity_sums_stacks)
{
    game_inventory inventory;
    ft_sharedptr<game_item> first(new game_item());
    ft_sharedptr<game_item> second(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(4, 20));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->get_error());

    first->set_item_id(1);
    first->set_rarity(2);
    first->set_max_stack(10);
    first->set_stack_size(2);
    second->set_item_id(2);
    second->set_rarity(2);
    second->set_max_stack(10);
    second->set_stack_size(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(second));
    FT_ASSERT_EQ(7, inventory.count_rarity(2));
    FT_ASSERT_EQ(true, inventory.has_rarity(2));
    FT_ASSERT_EQ(false, inventory.has_rarity(3));
    return (1);
}

FT_TEST(test_character_move_updates_coordinates)
{
    game_character hero;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    hero.set_x(1);
    hero.set_y(2);
    hero.set_z(3);
    hero.move(2, -1, 1);
    FT_ASSERT_EQ(3, hero.get_x());
    FT_ASSERT_EQ(1, hero.get_y());
    FT_ASSERT_EQ(4, hero.get_z());
    return (1);
}

FT_TEST(test_character_restore_armor_recovers_all)
{
    game_character hero;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.get_error());
    hero.set_physical_armor(12);
    hero.set_magic_armor(9);
    hero.set_damage_rule(FT_DAMAGE_RULE_BUFFER);
    hero.take_damage(5, FT_DAMAGE_PHYSICAL);
    hero.take_damage(3, FT_DAMAGE_MAGICAL);
    FT_ASSERT_EQ(7, hero.get_current_physical_armor());
    FT_ASSERT_EQ(6, hero.get_current_magic_armor());
    hero.restore_armor();
    FT_ASSERT_EQ(12, hero.get_current_physical_armor());
    FT_ASSERT_EQ(9, hero.get_current_magic_armor());
    return (1);
}

FT_TEST(test_inventory_copy_preserves_stacks)
{
    game_inventory original;
    game_inventory duplicate;
    ft_sharedptr<game_item> stack(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.initialize(5, 15));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.initialize(5, 15));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack->get_error());

    stack->set_item_id(3);
    stack->set_max_stack(10);
    stack->set_stack_size(4);
    stack->set_width(1);
    stack->set_height(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original.add_item(stack));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, duplicate.initialize(original));
    original.remove_item(0);
    FT_ASSERT_EQ(0, original.count_item(3));
    FT_ASSERT_EQ(4, duplicate.count_item(3));
    FT_ASSERT_EQ(0, original.get_current_weight());
    FT_ASSERT_EQ(4, duplicate.get_current_weight());
    return (1);
}

int test_upgrade_subtracters(void)
{
    game_upgrade up;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, up.initialize());
    up.set_current_level(5);
    up.sub_level(2);
    up.set_modifier1(3);
    up.sub_modifier1(1);
    up.set_modifier2(4);
    up.sub_modifier2(4);
    up.set_modifier3(-2);
    up.sub_modifier3(-2);
    up.set_modifier4(7);
    up.sub_modifier4(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, up.get_error());
    return (up.get_current_level() == 3 && up.get_modifier1() == 2 &&
            up.get_modifier2() == 0 && up.get_modifier3() == 0 &&
            up.get_modifier4() == 4);
}

int test_item_stack_subtract(void)
{
    game_item item;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.initialize());
    item.set_max_stack(10);
    item.set_stack_size(7);
    item.sub_from_stack(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, item.get_error());
    return (item.get_stack_size() == 4);
}

int test_character_level(void)
{
    game_character hero;
    int levels[] = {0, 100, 300};
    hero.get_experience_table().set_levels(levels, 3);
    hero.set_experience(150);
    return (hero.get_level() == 2);
}

int test_quest_progress(void)
{
    game_quest q;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, q.initialize());
    q.set_phases(3);
    if (q.is_complete())
        return (0);
    q.advance_phase();
    if (q.get_current_phase() != 1 || q.is_complete())
        return (0);
    q.advance_phase();
    q.advance_phase();
    if (!q.is_complete() || q.get_current_phase() != 3)
        return (0);
    q.advance_phase();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, q.get_error());
    return (q.get_current_phase() == 3);
}

int test_restore_individual_armor(void)
{
    game_character hero;
    hero.set_physical_armor(10);
    hero.set_magic_armor(8);
    hero.set_damage_rule(FT_DAMAGE_RULE_BUFFER);
    hero.take_damage(5, FT_DAMAGE_PHYSICAL);
    hero.take_damage(3, FT_DAMAGE_MAGICAL);
    if (hero.get_current_physical_armor() != 5 || hero.get_current_magic_armor() != 5)
        return (0);
    hero.restore_physical_armor();
    if (hero.get_current_physical_armor() != 10 || hero.get_current_magic_armor() != 5)
        return (0);
    hero.restore_magic_armor();
    if (hero.get_current_magic_armor() != 8)
        return (0);
    return (1);
}

int test_character_serialization_damage(void)
{
    game_character hero;
    hero.set_hit_points(50);
    hero.set_physical_armor(10);
    hero.set_magic_armor(8);
    hero.set_damage_rule(FT_DAMAGE_RULE_BUFFER);
    hero.take_damage(4, FT_DAMAGE_PHYSICAL);
    hero.take_damage(2, FT_DAMAGE_MAGICAL);
    json_group *group = serialize_character(hero);
    if (!group)
        return (0);
    game_character clone;
    deserialize_character(clone, group);
    json_free_groups(group);
    if (clone.get_current_physical_armor() != 6)
        return (0);
    if (clone.get_current_magic_armor() != 6)
        return (0);
    if (clone.get_damage_rule() != FT_DAMAGE_RULE_BUFFER)
        return (0);
    return (1);
}

FT_TEST(test_world_region_setters_replace_ids)
{
    game_world_region world_region;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_region.initialize());
    ft_vector<int> first_ids;
    ft_vector<int> second_ids;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_ids.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.push_back(3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.push_back(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.push_back(9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_ids.get_error());
    world_region.set_world_id(12);
    world_region.set_region_ids(first_ids);
    FT_ASSERT_EQ(12, world_region.get_world_id());
    FT_ASSERT_EQ(3u, world_region.get_region_ids().size());
    FT_ASSERT_EQ(3, world_region.get_region_ids()[0]);
    FT_ASSERT_EQ(7, world_region.get_region_ids()[1]);
    FT_ASSERT_EQ(9, world_region.get_region_ids()[2]);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_ids.push_back(42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_ids.get_error());
    world_region.set_region_ids(second_ids);
    FT_ASSERT_EQ(1u, world_region.get_region_ids().size());
    FT_ASSERT_EQ(42, world_region.get_region_ids()[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_region.get_error());
    return (1);
}

FT_TEST(test_world_region_move_assignment_clears_source)
{
    ft_vector<int> source_ids;
    game_world_region source_region;
    game_world_region target_region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target_region.initialize());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_ids.push_back(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_ids.push_back(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_ids.get_error());
    source_region.set_world_id(4);
    source_region.set_region_ids(source_ids);
    target_region.set_world_id(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target_region.initialize(ft_move(source_region)));
    FT_ASSERT_EQ(4, target_region.get_world_id());
    FT_ASSERT_EQ(2u, target_region.get_region_ids().size());
    FT_ASSERT_EQ(5, target_region.get_region_ids()[0]);
    FT_ASSERT_EQ(8, target_region.get_region_ids()[1]);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_region._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target_region.get_error());
    return (1);
}

FT_TEST(test_world_region_copy_assignment_preserves_content)
{
    ft_vector<int> original_ids;
    game_world_region original_region;
    game_world_region copied_region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_ids.push_back(15));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_ids.push_back(27));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, original_ids.get_error());
    original_region.set_world_id(6);
    original_region.set_region_ids(original_ids);
    copied_region.set_world_id(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_region.initialize(original_region));
    FT_ASSERT_EQ(6, copied_region.get_world_id());
    FT_ASSERT_EQ(2u, copied_region.get_region_ids().size());
    FT_ASSERT_EQ(15, copied_region.get_region_ids()[0]);
    FT_ASSERT_EQ(27, copied_region.get_region_ids()[1]);
    FT_ASSERT_EQ(2u, original_region.get_region_ids().size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copied_region.get_error());
    return (1);
}

FT_TEST(test_world_default_region_pointer_available)
{
    game_world world;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world.initialize());
    ft_sharedptr<game_world_region> &region_pointer = world.get_world_region();

    FT_ASSERT_EQ(true, static_cast<bool>(region_pointer));
    FT_ASSERT_EQ(0, region_pointer->get_world_id());
    FT_ASSERT_EQ(0u, region_pointer->get_region_ids().size());
    return (1);
}

FT_TEST(test_world_registry_registers_and_fetches_regions)
{
    game_world_registry registry;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.initialize());
    game_world_region stored_region;
    game_world_region fetched_region;
    ft_vector<int> region_ids;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stored_region.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fetched_region.initialize());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(101));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(202));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.get_error());
    stored_region.set_world_id(3);
    stored_region.set_region_ids(region_ids);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.register_world(stored_region));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.fetch_world(3, fetched_region));
    FT_ASSERT_EQ(3, fetched_region.get_world_id());
    FT_ASSERT_EQ(2u, fetched_region.get_region_ids().size());
    FT_ASSERT_EQ(101, fetched_region.get_region_ids()[0]);
    FT_ASSERT_EQ(202, fetched_region.get_region_ids()[1]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, registry.get_error());
    return (1);
}

FT_TEST(test_inventory_add_item_rejects_weight_overflow)
{
    game_inventory inventory;
    ft_sharedptr<game_item> heavy(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(0, 5));

    heavy->set_item_id(11);
    heavy->set_width(2);
    heavy->set_height(2);
    heavy->set_max_stack(5);
    heavy->set_stack_size(2);
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(heavy));
    FT_ASSERT_EQ(0, inventory.count_item(11));
    FT_ASSERT_EQ(0, inventory.get_current_weight());
    return (1);
}

FT_TEST(test_inventory_is_full_checks_capacity)
{
    game_inventory inventory;
    ft_sharedptr<game_item> first(new game_item());
    ft_sharedptr<game_item> second(new game_item());
    ft_sharedptr<game_item> third(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(2, 50));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, third->get_error());

    first->set_item_id(1);
    first->set_width(1);
    first->set_height(1);
    first->set_max_stack(5);
    first->set_stack_size(1);
    second->set_item_id(2);
    second->set_width(1);
    second->set_height(1);
    second->set_max_stack(5);
    second->set_stack_size(1);
    third->set_item_id(3);
    third->set_width(1);
    third->set_height(1);
    third->set_max_stack(5);
    third->set_stack_size(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(first));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(second));
    FT_ASSERT_EQ(true, inventory.is_full());
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(third));
    FT_ASSERT_EQ(2u, inventory.get_used());
    FT_ASSERT_EQ(true, inventory.has_item(1));
    FT_ASSERT_EQ(true, inventory.has_item(2));
    FT_ASSERT_EQ(false, inventory.has_item(3));
    return (1);
}

FT_TEST(test_inventory_failed_partial_stack_merge_preserves_state)
{
    game_inventory inventory;
    ft_sharedptr<game_item> base(new game_item());
    ft_sharedptr<game_item> extra(new game_item());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.initialize(1, 50));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base->get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extra->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, extra->get_error());

    base->set_item_id(5);
    base->set_width(1);
    base->set_height(1);
    base->set_max_stack(5);
    base->set_stack_size(3);
    extra->set_item_id(5);
    extra->set_width(1);
    extra->set_height(1);
    extra->set_max_stack(5);
    extra->set_stack_size(4);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.add_item(base));
    FT_ASSERT_EQ(FT_ERR_FULL, inventory.add_item(extra));
    FT_ASSERT_EQ(3, inventory.count_item(5));
    FT_ASSERT_EQ(3, inventory.get_current_weight());
    FT_ASSERT_EQ(1u, inventory.get_used());
    return (1);
}

FT_TEST(test_world_region_parameterized_constructor_copies_values)
{
    ft_vector<int> region_ids;
    game_world_region region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(12, region_ids));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region_ids.push_back(10));
    FT_ASSERT_EQ(12, region.get_world_id());
    FT_ASSERT_EQ(3u, region.get_region_ids().size());
    FT_ASSERT_EQ(4, region.get_region_ids()[0]);
    FT_ASSERT_EQ(6, region.get_region_ids()[1]);
    FT_ASSERT_EQ(8, region.get_region_ids()[2]);
    return (1);
}

FT_TEST(test_world_region_mutable_ids_reference_updates_state)
{
    game_world_region region;
    ft_vector<int> *ids;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize());
    ids = &region.get_region_ids();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ids->push_back(14));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ids->push_back(16));
    FT_ASSERT_EQ(2u, region.get_region_ids().size());
    FT_ASSERT_EQ(14, region.get_region_ids()[0]);
    FT_ASSERT_EQ(16, region.get_region_ids()[1]);
    return (1);
}
