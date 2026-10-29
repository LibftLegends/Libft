#include "../test_internal.hpp"
#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <cstring>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
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
#include "../../Modules/Game/game_state.hpp"
#include "../../Modules/Game/game_upgrade.hpp"
#include "../../Modules/Game/game_vendor_profile.hpp"
#include "../../Modules/Game/game_world_region.hpp"
#include "../../Modules/Game/game_world_registry.hpp"
#include "../../Modules/Game/game_world_replay.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int script_noop(game_script_context &context, const ft_vector<ft_string> &arguments) noexcept
{
    (void)arguments;
    ft_string flag_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, flag_key.initialize("flag"));
    ft_string flag_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, flag_value.initialize("1"));
    context.set_variable(flag_key, flag_value);
    return (context.get_error());
}

static int script_set_score(game_script_context &context, const ft_vector<ft_string> &arguments) noexcept
{
    if (arguments.size() == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string score_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, score_key.initialize("score"));
    context.set_variable(score_key, arguments[0]);
    return (context.get_error());
}

FT_TEST(test_game_script_bridge_defaults_to_custom_language)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer, ft_nullptr));

    FT_ASSERT(world_pointer);
    FT_ASSERT_STR_EQ("custom", bridge.get_language().c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(32, bridge.get_max_operations());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_rejects_unsupported_language)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.initialize(world_pointer, "ruby"));

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    FT_ASSERT_STR_EQ("ruby", bridge.get_language().c_str());
    return (1);
}

FT_TEST(test_game_script_bridge_language_update_accepts_custom_value)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));

    {
        ft_string custom_lang;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_lang.initialize("custom"));
        bridge.set_language(custom_lang);
        FT_ASSERT_STR_EQ("custom", bridge.get_language().c_str());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_language_update_rejects_null)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));

    {
        ft_string custom_lang;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_lang.initialize("custom"));
        bridge.set_language(custom_lang);
        FT_ASSERT_STR_EQ("custom", bridge.get_language().c_str());
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    bridge.set_language(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    FT_ASSERT_STR_EQ("custom", bridge.get_language().c_str());
    return (1);
}

FT_TEST(test_game_script_bridge_callback_count_tracks_overwrite)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> first_callback(script_noop);
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> second_callback(script_set_score);

    FT_ASSERT_EQ(0u, bridge.get_callback_count());
    ft_string do_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, do_name.initialize("do"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_function(do_name, first_callback));
    FT_ASSERT_EQ(1u, bridge.get_callback_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_function(do_name, second_callback));
    FT_ASSERT_EQ(1u, bridge.get_callback_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_register_function_rejects_empty_name)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> callback(script_noop);
    int result;

    ft_string empty_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_name.initialize(""));
    result = bridge.register_function(empty_name, callback);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    FT_ASSERT_EQ(0u, bridge.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_register_function_rejects_null_callback)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> callback;
    int result;

    ft_string noop_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, noop_name.initialize("noop"));
    result = bridge.register_function(noop_name, callback);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    FT_ASSERT_EQ(0u, bridge.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_remove_function_clears_existing_entry)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> callback(script_noop);

    ft_string noop_name_local;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, noop_name_local.initialize("noop"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_function(noop_name_local, callback));
    FT_ASSERT_EQ(1u, bridge.get_callback_count());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.remove_function(noop_name_local));
    FT_ASSERT_EQ(0u, bridge.get_callback_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_set_max_operations_updates_limit)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));

    bridge.set_max_operations(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(5, bridge.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_set_max_operations_rejects_negative)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));

    bridge.set_max_operations(7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(7, bridge.get_max_operations());

    bridge.set_max_operations(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    FT_ASSERT_EQ(7, bridge.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_thread_safety_toggle)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));

    FT_ASSERT_EQ(false, bridge.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(true, bridge.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(false, bridge.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_execute_reports_unknown_commands)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_state state;
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_string script;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("unknown jump\n"));
    int result;

    result = bridge.execute(script, state);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_execute_with_registered_callback_runs_successfully)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_state state;
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> callback(script_set_score);
    const ft_string *score_value;
    ft_string script;

    script = "call score 42\n";
    ft_string score_name;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, score_name.initialize("score"));
    bridge.register_function(score_name, callback);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));

    score_value = state.get_variable(score_name);
    FT_ASSERT(score_value != ft_nullptr);
    FT_ASSERT_STR_EQ("42", score_value->c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_validate_dry_run_counts_operations)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_vector<ft_string> warnings;
    ft_string script;
    int result;

    script = "# comment\nset score 1\n;another\ncall missing\n";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, warnings.initialize());
    result = bridge.validate_dry_run(script, warnings);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT(warnings.size() == 1);
    return (1);
}
