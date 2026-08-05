#include "../test_internal.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Game/game_character.hpp"
#include "../../Modules/Game/game_event.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_item.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <fcntl.h>
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
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void cleanup_game_persistence_store(void)
{
    int error_code;

    error_code = FT_ERR_SUCCESS;
    cmp_file_delete("world_persistence_store.json", &error_code);
    return ;
}

FT_TEST(test_game_world_persistence_round_trip)
{
    const char *store_path = "world_persistence_store.json";
    int error_code = FT_ERR_SUCCESS;

    cleanup_game_persistence_store();
    su_file *store_file = su_fopen(store_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    FT_ASSERT(store_file != ft_nullptr);
    const char *initial_content = "{\n  \"kv_store\": {\n  }\n}\n";
    size_t initial_length = ft_strlen(initial_content);
    FT_ASSERT_EQ(su_fwrite(initial_content, 1, initial_length, store_file), initial_length);
    FT_ASSERT_EQ(su_fclose(store_file), 0);

    kv_store persistence_store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, persistence_store.initialize(store_path));

    game_world world_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_instance.initialize());
    FT_ASSERT_EQ(world_instance.get_error(), FT_ERR_SUCCESS);

    ft_sharedptr<game_event> scheduled_event(new (std::nothrow) game_event());
    FT_ASSERT(scheduled_event.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduled_event->initialize());
    FT_ASSERT_EQ(scheduled_event->get_error(), FT_ERR_SUCCESS);
    scheduled_event->set_id(7);
    scheduled_event->set_duration(5);
    FT_ASSERT_EQ(scheduled_event->get_error(), FT_ERR_SUCCESS);
    world_instance.schedule_event(scheduled_event);
    FT_ASSERT_EQ(world_instance.get_error(), FT_ERR_SUCCESS);

    game_character hero;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, hero.initialize());
    hero.set_hit_points(64);
    hero.set_damage_rule(FT_DAMAGE_RULE_FLAT);
    FT_ASSERT_EQ(hero.get_error(), FT_ERR_SUCCESS);

    game_inventory backpack;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backpack.initialize(6, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backpack.get_error());
    ft_sharedptr<game_item> potion(new (std::nothrow) game_item());
    FT_ASSERT(potion.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, potion->initialize());
    FT_ASSERT_EQ(potion->get_error(), FT_ERR_SUCCESS);
    potion->set_item_id(200);
    potion->set_max_stack(5);
    potion->set_stack_size(2);
    potion->set_modifier1_id(3);
    potion->set_modifier1_value(25);
    FT_ASSERT_EQ(potion->get_error(), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(backpack.add_item(potion), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backpack.get_error());

    FT_ASSERT_EQ(world_instance.save_to_store(persistence_store, "slot-primary", hero, backpack), FT_ERR_SUCCESS);
    FT_ASSERT_EQ(world_instance.get_error(), FT_ERR_SUCCESS);

    game_character restored_hero;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_hero.initialize());
    game_inventory restored_inventory;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_inventory.initialize(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_inventory.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_instance.load_from_store(persistence_store, "slot-primary", restored_hero, restored_inventory));
    FT_ASSERT_EQ(world_instance.get_error(), FT_ERR_SUCCESS);

    FT_ASSERT_EQ(restored_hero.get_hit_points(), hero.get_hit_points());
    FT_ASSERT_EQ(restored_inventory.get_used(), backpack.get_used());
    FT_ASSERT_EQ(restored_inventory.count_item(200), backpack.count_item(200));

    ft_vector<ft_sharedptr<game_event> > restored_events;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_events.initialize());
    ft_sharedptr<game_event_scheduler> &restored_scheduler = world_instance.get_event_scheduler();
    FT_ASSERT(restored_scheduler.get() != ft_nullptr);
    restored_scheduler->dump_events(restored_events);
    FT_ASSERT_EQ(restored_events.size(), static_cast<size_t>(1));
    FT_ASSERT(restored_events[0].get() != ft_nullptr);
    FT_ASSERT_EQ(restored_events[0]->get_id(), scheduled_event->get_id());

    FT_ASSERT_EQ(cmp_file_delete(store_path, &error_code), 0);
    cleanup_game_persistence_store();

    return (1);
}
