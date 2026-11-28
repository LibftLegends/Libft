#include "../test_internal.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/function.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <new>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_game_world_replay_capture_restore_replay)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_world_replay_session session;
    game_character hero_character;
    game_inventory hero_inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.initialize(4, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    ft_sharedptr<game_item> healing_potion(new (std::nothrow) game_item());
    ft_sharedptr<game_event> damage_event(new (std::nothrow) game_event());
    int damage_invocations;
    int capture_result;
    int restore_result;
    int replay_result;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT(healing_potion.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, healing_potion->initialize());
    FT_ASSERT(damage_event.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, damage_event->initialize());

    hero_character.set_hit_points(20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    hero_character.set_damage_rule(FT_DAMAGE_RULE_FLAT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    healing_potion->set_item_id(301);
    healing_potion->set_max_stack(5);
    healing_potion->set_stack_size(1);
    FT_ASSERT_EQ(hero_inventory.add_item(healing_potion), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());

    damage_invocations = 0;
    damage_event->set_id(55);
    damage_event->set_duration(1);
    damage_event->set_callback(ft_function<void(game_world&, game_event&)>([&hero_character, &damage_invocations](game_world &world_ref, game_event &event_ref)
    {
        (void)world_ref;
        (void)event_ref;
        hero_character.take_damage(5, FT_DAMAGE_PHYSICAL);
        damage_invocations = damage_invocations + 1;
        return ;
    }));
    (void)damage_event;

    world_pointer->schedule_event(damage_event);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->get_error());
    FT_ASSERT_EQ(world_pointer->get_event_scheduler()->size(), static_cast<size_t>(1));

    capture_result = session.capture_snapshot(*world_pointer, hero_character, hero_inventory);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, capture_result);
    game_world_replay_session moved_session;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_session.move(session));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        session.restore_snapshot(world_pointer, hero_character, hero_inventory));

    world_pointer->update_events(world_pointer, 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->get_error());
    FT_ASSERT_EQ(15, hero_character.get_hit_points());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(damage_invocations, 1);
    FT_ASSERT_EQ(world_pointer->get_event_scheduler()->size(), static_cast<size_t>(0));

    restore_result = moved_session.restore_snapshot(world_pointer, hero_character, hero_inventory);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restore_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    FT_ASSERT_EQ(hero_character.get_hit_points(), 20);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(hero_inventory.count_item(301), 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    FT_ASSERT_EQ(world_pointer->get_event_scheduler()->size(), static_cast<size_t>(1));

    hero_character.set_hit_points(25);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    replay_result = moved_session.replay_ticks(world_pointer, hero_character, hero_inventory, 1,
        ft_nullptr, ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    FT_ASSERT_EQ(hero_character.get_hit_points(), 15);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(hero_inventory.count_item(301), 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    FT_ASSERT_EQ(world_pointer->get_event_scheduler()->size(), static_cast<size_t>(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->get_error());
    FT_ASSERT_EQ(damage_invocations, 2);

    return (1);
}

FT_TEST(test_game_world_replay_import_export_clear)
{
    ft_sharedptr<game_world> world_pointer(new (std::nothrow) game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_world_replay_session session;
    game_world_replay_session imported_session;
    game_character hero_character;
    game_inventory hero_inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.initialize(2, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());
    ft_string exported_snapshot;
    ft_string cleared_snapshot;
    int restore_result;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, imported_session.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exported_snapshot.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleared_snapshot.initialize());

    restore_result = session.restore_snapshot(world_pointer, hero_character, hero_inventory);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, restore_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->get_error());
    hero_character.set_hit_points(33);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());

    FT_ASSERT_EQ(session.capture_snapshot(*world_pointer, hero_character, hero_inventory), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_character.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero_inventory.get_error());


    FT_ASSERT_EQ(session.export_snapshot(exported_snapshot), FT_ERR_SUCCESS);
    FT_ASSERT(exported_snapshot.empty() == false);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exported_snapshot.get_error());

    session.clear_snapshot();

    FT_ASSERT_EQ(session.export_snapshot(cleared_snapshot), FT_ERR_SUCCESS);
    FT_ASSERT(cleared_snapshot.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cleared_snapshot.get_error());

    FT_ASSERT_EQ(imported_session.import_snapshot(exported_snapshot), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exported_snapshot.get_error());

    hero_character.set_hit_points(10);

    FT_ASSERT_EQ(imported_session.restore_snapshot(world_pointer, hero_character, hero_inventory), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->get_error());
    FT_ASSERT_EQ(hero_character.get_hit_points(), 33);

    return (1);
}
