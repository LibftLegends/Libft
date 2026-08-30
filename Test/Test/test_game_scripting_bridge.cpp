#include "../test_internal.hpp"
#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Template/shared_ptr.hpp"
#include <string>
#include <cstring>

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

static int g_script_callback_invocations = 0;
static int g_script_last_score = 0;
static int64_t g_custom_script_result = 0;

static int game_script_custom_add(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    int64_t first_value;
    int64_t second_value;
    char *end_pointer;

    if (arguments.size() != 2U)
        return (FT_ERR_INVALID_ARGUMENT);
    end_pointer = ft_nullptr;
    if (ft_parse_int64(arguments[0].c_str(), &end_pointer, &first_value)
        != FT_ERR_SUCCESS || end_pointer == ft_nullptr || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    end_pointer = ft_nullptr;
    if (ft_parse_int64(arguments[1].c_str(), &end_pointer, &second_value)
        != FT_ERR_SUCCESS || end_pointer == ft_nullptr || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    context.set_result_integer(first_value + second_value);
    g_custom_script_result = first_value + second_value;
    return (context.get_error());
}

static int game_script_adjust_score(game_script_context &context, const ft_vector<ft_string> &arguments) noexcept
{
    const ft_string *score_value;
    int base_score;
    int delta;
    ft_string updated_score;

    ft_string score_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, score_key.initialize("score"));
    score_value = context.get_variable(score_key);
    if (score_value == ft_nullptr)
        return (context.get_error());
    base_score = ft_atoi(score_value->c_str());
    if (context.get_error() != FT_ERR_SUCCESS)
        return (context.get_error());
    if (arguments.size() > 0)
        delta = ft_atoi(arguments[0].c_str());
    else
        delta = 0;
    {
        std::string numeric_string = std::to_string(static_cast<long>(base_score + delta));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, updated_score.initialize(numeric_string.c_str()));
    }
    context.set_variable(score_key, updated_score);
    if (context.get_error() != FT_ERR_SUCCESS)
        return (context.get_error());
    g_script_last_score = base_score + delta;
    g_script_callback_invocations += 1;
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_game_script_context_variable_controls)
{
    game_script_context context;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.initialize());
    const ft_string *value_pointer;

    ft_string quest_stage_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_key.initialize("quest_stage"));
    ft_string quest_stage_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_value.initialize("3"));
    context.set_variable(quest_stage_key, quest_stage_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());

    value_pointer = context.get_variable(quest_stage_key);
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());
    FT_ASSERT_STR_EQ("3", value_pointer->c_str());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_value.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_value.append("4"));
    context.set_variable(quest_stage_key, quest_stage_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());

    value_pointer = context.get_variable(quest_stage_key);
    FT_ASSERT(value_pointer != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());
    FT_ASSERT_STR_EQ("4", value_pointer->c_str());

    context.remove_variable(quest_stage_key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());

    value_pointer = context.get_variable(quest_stage_key);
    FT_ASSERT(value_pointer == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, context.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_value.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, quest_stage_value.append("7"));
    context.set_variable(quest_stage_key, quest_stage_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());

    context.clear_variables();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, context.get_error());

    value_pointer = context.get_variable(quest_stage_key);
    FT_ASSERT(value_pointer == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, context.get_error());

    return (1);
}

FT_TEST(test_game_script_bridge_executes_callbacks)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> adjust_function(game_script_adjust_score);
    ft_string script;
    int register_result;
    int execute_result;

    FT_ASSERT(world_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    ft_string adjust_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, adjust_key.initialize("adjust_score"));
    register_result = bridge.register_function(adjust_key, adjust_function);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    g_script_callback_invocations = 0;
    g_script_last_score = 0;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set score 10\ncall adjust_score 5\ncall adjust_score 3\n"));

    execute_result = bridge.execute(script, state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, execute_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(2, g_script_callback_invocations);
    FT_ASSERT_EQ(18, g_script_last_score);

    return (1);
}

FT_TEST(test_game_script_bridge_executes_custom_vm_callbacks)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    game_state state;
    game_script_bridge bridge;
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)>
        add_function(game_script_custom_add);
    ft_string function_name;
    ft_string script;

    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer, "CUSTOM"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, function_name.initialize("add"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_function(function_name,
        add_function));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("add(7, 5)"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    FT_ASSERT_EQ(static_cast<int64_t>(12), g_custom_script_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_game_script_bridge_operation_limit)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_state state;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_function<int(game_script_context &, const ft_vector<ft_string> &)> adjust_function(game_script_adjust_score);
    ft_string script;
    int register_result;
    int execute_result;

    FT_ASSERT(world_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    ft_string adjust_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, adjust_key.initialize("adjust_score"));
    register_result = bridge.register_function(adjust_key, adjust_function);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, register_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    bridge.set_max_operations(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    g_script_callback_invocations = 0;
    g_script_last_score = 0;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set score 1\ncall adjust_score 1\n"));

    execute_result = bridge.execute(script, state);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, execute_result);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.get_error());
    FT_ASSERT_EQ(0, g_script_callback_invocations);
    FT_ASSERT_EQ(0, g_script_last_score);

    return (1);
}

FT_TEST(test_game_script_bridge_sandbox_helper)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_vector<ft_string> violations;
    ft_string script;
    int inspection_result;

    FT_ASSERT(world_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, violations.initialize());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set score 4\nteleport player base\ncall adjust_score 1\n"));

    inspection_result = bridge.check_sandbox_capabilities(script, violations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inspection_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT(violations.size() == 1);
    FT_ASSERT(violations[0] == "unsupported command: teleport");

    return (1);
}

FT_TEST(test_game_script_bridge_dry_run_helper)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_vector<ft_string> warnings;
    ft_string script;
    int validation_result;

    FT_ASSERT(world_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, warnings.initialize());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("call missing\nset score\nunset\n"));

    validation_result = bridge.validate_dry_run(script, warnings);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, validation_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT(warnings.size() == 3);
    FT_ASSERT(warnings[0] == "unregistered callback: missing");
    FT_ASSERT(warnings[1] == "set missing value for key: score");
    FT_ASSERT(warnings[2] == "unset missing key");

    return (1);
}

FT_TEST(test_game_script_bridge_bytecode_budget_helper)
{
    ft_sharedptr<game_world> world_pointer(new game_world());
    FT_ASSERT(world_pointer.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, world_pointer->initialize());
    game_script_bridge bridge;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    ft_string script;
    int required_operations;
    int inspection_result;

    FT_ASSERT(world_pointer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set score 10\ncall adjust_score 2\nunset score\n"));

    required_operations = 0;
    inspection_result = bridge.inspect_bytecode_budget(script, required_operations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, inspection_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    FT_ASSERT_EQ(3, required_operations);

    bridge.set_max_operations(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());

    inspection_result = bridge.inspect_bytecode_budget(script, required_operations);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, inspection_result);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.get_error());
    FT_ASSERT_EQ(3, required_operations);

    return (1);
}
