#include "../test_internal.hpp"
#include "../../Modules/Game/game_price_definition.hpp"
#include "../../Modules/Game/game_rarity_band.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_currency_rate.hpp"
#include "../../Modules/Game/game_economy_table.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/Game/game_behavior_profile.hpp"
#include "../../Modules/Game/game_behavior_table.hpp"
#include "../../Modules/Game/game_buff.hpp"
#include "../../Modules/Game/game_debuff.hpp"
#include "../../Modules/Game/game_hooks.hpp"
#include "../../Modules/Game/game_inventory.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/Game/game_progress_tracker.hpp"
#include "../../Modules/Game/game_skill.hpp"
#include "../../Modules/Game/game_reputation.hpp"
#include "../../Modules/Game/game_dialogue_script.hpp"
#include "../../Modules/Game/game_data_catalog.hpp"
#include "../../Modules/Game/game_dialogue_table.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_region_definition.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/Game/game_event_scheduler.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/Game/game_quest.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <csetjmp>
#include <cstring>
#include <new>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Game/game_behavior_action.hpp"
#include "../../Modules/Game/game_crafting.hpp"
#include "../../Modules/Game/game_dialogue_line.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static volatile sig_atomic_t g_signal_caught = 0;
static sigjmp_buf g_signal_jump_buffer;

static void uninitialised_destructor_signal_handler(int signal_value)
{
    g_signal_caught = signal_value;
    siglongjmp(g_signal_jump_buffer, 1);
}

template <typename TypeName>
static int expect_no_sigabrt_on_uninitialised_destructor()
{
    struct sigaction old_action_abort;
    struct sigaction new_action_abort;
    struct sigaction old_action_iot;
    struct sigaction new_action_iot;
    bool iot_handler_installed;
    int jump_result;
    int restore_abort_result;
    int restore_iot_result;
    TypeName *object_instance;
    alignas(TypeName) unsigned char object_storage[sizeof(TypeName)];

    std::memset(&old_action_abort, 0, sizeof(old_action_abort));
    std::memset(&new_action_abort, 0, sizeof(new_action_abort));
    std::memset(&old_action_iot, 0, sizeof(old_action_iot));
    std::memset(&new_action_iot, 0, sizeof(new_action_iot));
    new_action_abort.sa_handler = &uninitialised_destructor_signal_handler;
    new_action_iot.sa_handler = &uninitialised_destructor_signal_handler;
    sigemptyset(&new_action_abort.sa_mask);
    sigemptyset(&new_action_iot.sa_mask);
    iot_handler_installed = false;
    if (sigaction(SIGABRT, &new_action_abort, &old_action_abort) != 0)
        return (0);
    if (SIGIOT != SIGABRT && sigaction(SIGIOT, &new_action_iot,
            &old_action_iot) != 0)
    {
        restore_abort_result = sigaction(SIGABRT, &old_action_abort, ft_nullptr);
        if (restore_abort_result != 0)
            return (0);
        return (0);
    }
    if (SIGIOT != SIGABRT)
        iot_handler_installed = true;

    g_signal_caught = 0;
    jump_result = sigsetjmp(g_signal_jump_buffer, 1);
    if (jump_result == 0)
    {
        object_instance = new (object_storage) TypeName();
        object_instance->~TypeName();
    }

    restore_abort_result = sigaction(SIGABRT, &old_action_abort, ft_nullptr);
    restore_iot_result = 0;
    if (iot_handler_installed == true)
        restore_iot_result = sigaction(SIGIOT, &old_action_iot, ft_nullptr);
    if (restore_abort_result != 0)
        return (0);
    if (restore_iot_result != 0)
        return (0);
    return (g_signal_caught == 0);
}

static int is_non_aborting_uninitialised_destroy_result(int error_code)
{
    return (error_code == FT_ERR_SUCCESS);
}

FT_TEST(test_game_price_definition_destroy_uninitialised_returns_invalid_state)
{
    game_price_definition definition;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, definition.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_price_definition>());
    return (1);
}

FT_TEST(test_game_rarity_band_destroy_uninitialised_returns_invalid_state)
{
    game_rarity_band band;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, band.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_rarity_band>());
    return (1);
}

FT_TEST(test_game_vendor_profile_destroy_uninitialised_returns_invalid_state)
{
    game_vendor_profile vendor;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, vendor.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_vendor_profile>());
    return (1);
}

FT_TEST(test_game_currency_rate_destroy_uninitialised_returns_invalid_state)
{
    game_currency_rate rate;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rate.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_currency_rate>());
    return (1);
}

FT_TEST(test_game_economy_table_destroy_uninitialised_returns_invalid_state)
{
    game_economy_table table;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_economy_table>());
    return (1);
}

FT_TEST(test_game_behavior_profile_destroy_uninitialised_returns_invalid_state)
{
    game_behavior_profile profile;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, profile.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_behavior_profile>());
    return (1);
}

FT_TEST(test_game_behavior_table_destroy_uninitialised_returns_invalid_state)
{
    game_behavior_table table;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, table.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_behavior_table>());
    return (1);
}

FT_TEST(test_game_buff_destroy_uninitialised_returns_invalid_state)
{
    game_buff buff;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, buff.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_buff>());
    return (1);
}

FT_TEST(test_game_debuff_destroy_uninitialised_returns_invalid_state)
{
    game_debuff debuff;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, debuff.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_debuff>());
    return (1);
}

FT_TEST(test_game_skill_destroy_uninitialised_returns_invalid_state)
{
    game_skill skill;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, skill.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_skill>());
    return (1);
}

FT_TEST(test_game_reputation_destroy_uninitialised_returns_invalid_state)
{
    game_reputation reputation;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, reputation.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_reputation>());
    return (1);
}

FT_TEST(test_game_dialogue_script_destroy_uninitialised_returns_invalid_state)
{
    game_dialogue_script script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_dialogue_script>());
    return (1);
}

FT_TEST(test_game_progress_tracker_destroy_uninitialised_returns_invalid_state)
{
    game_progress_tracker tracker;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, tracker.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_progress_tracker>());
    return (1);
}

FT_TEST(test_game_inventory_destroy_uninitialised_returns_invalid_state)
{
    game_inventory inventory;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, inventory.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_inventory>());
    return (1);
}

FT_TEST(test_game_map3d_destroy_uninitialised_returns_invalid_state)
{
    game_map3d map_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_map3d>());
    return (1);
}

FT_TEST(test_game_achievement_destroy_uninitialised_returns_invalid_state)
{
    game_achievement achievement;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, achievement.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_achievement>());
    return (1);
}

FT_TEST(test_game_hooks_destroy_uninitialised_returns_invalid_state)
{
    game_hooks hooks;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, hooks.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_hooks>());
    return (1);
}

FT_TEST(test_game_goal_destroy_uninitialised_returns_invalid_state)
{
    game_goal goal;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, goal.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_goal>());
    return (1);
}

FT_TEST(test_game_item_definition_destroy_uninitialised_is_non_aborting)
{
    game_item_definition item_definition;

    const int destroy_error = item_definition.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_item_definition>());
    return (1);
}

FT_TEST(test_game_recipe_blueprint_destroy_uninitialised_is_non_aborting)
{
    game_recipe_blueprint recipe_blueprint;

    const int destroy_error = recipe_blueprint.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_recipe_blueprint>());
    return (1);
}

FT_TEST(test_game_loadout_entry_destroy_uninitialised_is_non_aborting)
{
    game_loadout_entry loadout_entry;

    const int destroy_error = loadout_entry.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_loadout_entry>());
    return (1);
}

FT_TEST(test_game_loadout_blueprint_destroy_uninitialised_is_non_aborting)
{
    game_loadout_blueprint loadout_blueprint;

    const int destroy_error = loadout_blueprint.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_loadout_blueprint>());
    return (1);
}

FT_TEST(test_game_data_catalog_destroy_uninitialised_is_non_aborting)
{
    game_data_catalog data_catalog;

    const int destroy_error = data_catalog.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_data_catalog>());
    return (1);
}

FT_TEST(test_game_dialogue_table_destroy_uninitialised_is_non_aborting)
{
    game_dialogue_table dialogue_table;

    const int destroy_error = dialogue_table.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_dialogue_table>());
    return (1);
}

FT_TEST(test_game_path_step_destroy_uninitialised_is_non_aborting)
{
    game_path_step path_step;

    const int destroy_error = path_step.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_path_step>());
    return (1);
}

FT_TEST(test_game_pathfinding_destroy_uninitialised_is_non_aborting)
{
    game_pathfinding pathfinding;

    const int destroy_error = pathfinding.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_pathfinding>());
    return (1);
}

FT_TEST(test_game_world_region_destroy_uninitialised_is_non_aborting)
{
    game_world_region world_region;

    const int destroy_error = world_region.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_world_region>());
    return (1);
}

FT_TEST(test_game_region_definition_destroy_uninitialised_is_non_aborting)
{
    game_region_definition region_definition;

    const int destroy_error = region_definition.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_region_definition>());
    return (1);
}

FT_TEST(test_game_world_registry_destroy_uninitialised_is_non_aborting)
{
    game_world_registry world_registry;

    const int destroy_error = world_registry.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_world_registry>());
    return (1);
}

FT_TEST(test_game_event_scheduler_destroy_uninitialised_is_non_aborting)
{
    game_event_scheduler scheduler;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scheduler.destroy());
    FT_ASSERT_EQ(1,
        expect_no_sigabrt_on_uninitialised_destructor<game_event_scheduler>());
    return (1);
}

FT_TEST(test_game_world_replay_destroy_uninitialised_is_non_aborting)
{
    game_world_replay_session replay_session;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, replay_session.destroy());
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<
        game_world_replay_session>());
    return (1);
}

FT_TEST(test_game_quest_destroy_uninitialised_is_non_aborting)
{
    game_quest quest;

    const int destroy_error = quest.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_quest>());
    return (1);
}

FT_TEST(test_game_upgrade_destroy_uninitialised_is_non_aborting)
{
    game_upgrade upgrade;

    const int destroy_error = upgrade.destroy();
    FT_ASSERT_EQ(1, is_non_aborting_uninitialised_destroy_result(
        destroy_error));
    FT_ASSERT_EQ(1, expect_no_sigabrt_on_uninitialised_destructor<game_upgrade>());
    return (1);
}
