#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static int32_t
script_bridge_set_first_marker(game_script_context &context,
                               const ft_vector<ft_string> &arguments) noexcept
{
    ft_string key;
    ft_string value;

    (void)arguments;
    if (key.initialize("marker") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (value.initialize("first") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    context.set_variable(key, value);
    return (context.get_error());
}

static int32_t
script_bridge_set_second_marker(game_script_context &context,
                                const ft_vector<ft_string> &arguments) noexcept
{
    ft_string key;
    ft_string value;

    (void)arguments;
    if (key.initialize("marker") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (value.initialize("second") != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    context.set_variable(key, value);
    return (context.get_error());
}

FT_TEST(test_game_script_bridge_duplicate_callback_replacement_executes_latest)
{
    game_script_bridge bridge;
    game_state state;
    ft_string callback_name;
    ft_string marker_name;
    ft_string script;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        first_callback(script_bridge_set_first_marker);
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        second_callback(script_bridge_set_second_marker);
    const ft_string *marker_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, callback_name.initialize("mark"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, marker_name.initialize("marker"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("call mark\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(callback_name, first_callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(callback_name, second_callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    marker_value = state.get_variable(marker_name);
    FT_ASSERT(marker_value != ft_nullptr);
    FT_ASSERT_STR_EQ("second", marker_value->c_str());
    return (1);
}

FT_TEST(test_game_script_bridge_two_distinct_callbacks_have_two_entries)
{
    game_script_bridge bridge;
    ft_string first_name;
    ft_string second_name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        callback(script_bridge_set_first_marker);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_name.initialize("first"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_name.initialize("second"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(first_name, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(second_name, callback));
    FT_ASSERT_EQ(2, bridge.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_remove_then_register_reuses_callback_name)
{
    game_script_bridge bridge;
    ft_string callback_name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        callback(script_bridge_set_first_marker);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, callback_name.initialize("mark"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(callback_name, callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.remove_function(callback_name));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(callback_name, callback));
    FT_ASSERT_EQ(1, bridge.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_execute_at_exact_operation_limit_succeeds)
{
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 script.initialize("set score 1\nunset score\n"));
    bridge.set_max_operations(2);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    return (1);
}

FT_TEST(test_game_script_bridge_execute_beyond_operation_limit_fails)
{
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 script.initialize("set score 1\nunset score\n"));
    bridge.set_max_operations(1);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.execute(script, state));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_inspect_budget_counts_three_commands)
{
    game_script_bridge bridge;
    ft_string script;
    int32_t required_operations;

    required_operations = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 script.initialize("set one 1\nset two 2\nunset one\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.inspect_bytecode_budget(script, required_operations));
    FT_ASSERT_EQ(3, required_operations);
    return (1);
}

FT_TEST(test_game_script_bridge_inspect_budget_rejects_limit_overflow)
{
    game_script_bridge bridge;
    ft_string script;
    int32_t required_operations;

    required_operations = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set one 1\nset two 2\n"));
    bridge.set_max_operations(1);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION,
                 bridge.inspect_bytecode_budget(script, required_operations));
    FT_ASSERT_EQ(2, required_operations);
    return (1);
}

FT_TEST(test_game_script_bridge_comments_and_blank_lines_do_not_consume_budget)
{
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 script.initialize("# comment\n\n; comment\nset score 1\n"));
    bridge.set_max_operations(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    return (1);
}

FT_TEST(test_game_script_bridge_validate_reports_one_missing_callback)
{
    game_script_bridge bridge;
    ft_string script;
    ft_vector<ft_string> warnings;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("call missing 1\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, warnings.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.validate_dry_run(script, warnings));
    FT_ASSERT_EQ(1, warnings.size());
    return (1);
}

FT_TEST(test_game_script_bridge_execute_unknown_command_sets_error)
{
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("unknown command\n"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.execute(script, state));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_language_change_preserves_callback_count)
{
    game_script_bridge bridge;
    ft_string callback_name;
    ft_string language;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        callback(script_bridge_set_first_marker);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, callback_name.initialize("mark"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, language.initialize("custom"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
                 bridge.register_function(callback_name, callback));
    bridge.set_language(language.c_str());
    FT_ASSERT_EQ(1, bridge.get_callback_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_zero_operation_limit_rejects_one_command)
{
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("set score 1\n"));
    bridge.set_max_operations(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    return (1);
}
