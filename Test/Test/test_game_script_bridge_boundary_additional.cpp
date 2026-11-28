#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_script_bridge_null_language_reports_argument)
{
    game_script_bridge value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_language(ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_unsupported_language_reports_argument)
{
    game_script_bridge value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_language("python");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_negative_operation_limit_reports_argument)
{
    game_script_bridge value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_max_operations(-1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.get_error());
    return (1);
}

FT_TEST(test_game_script_bridge_zero_operation_limit_is_stored)
{
    game_script_bridge value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    value.set_max_operations(0);
    FT_ASSERT_EQ(0, value.get_max_operations());
    return (1);
}

FT_TEST(test_game_script_bridge_empty_callback_name_is_rejected)
{
    game_script_bridge value;
    ft_string name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> callback;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, value.register_function(name, callback));
    return (1);
}

FT_TEST(test_game_script_bridge_remove_missing_callback_is_safe)
{
    game_script_bridge value;
    ft_string name;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name.initialize("missing"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.remove_function(name));
    FT_ASSERT_EQ(0, value.get_callback_count());
    return (1);
}

FT_TEST(test_game_script_bridge_empty_script_executes)
{
    game_script_bridge value;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.execute(script, state));
    return (1);
}

FT_TEST(test_game_script_bridge_empty_sandbox_report_is_empty)
{
    game_script_bridge value;
    ft_string script;
    ft_vector<ft_string> violations;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, violations.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.check_sandbox_capabilities(script, violations));
    FT_ASSERT_EQ(0, violations.size());
    return (1);
}

FT_TEST(test_game_script_bridge_empty_dry_run_report_is_empty)
{
    game_script_bridge value;
    ft_string script;
    ft_vector<ft_string> warnings;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, warnings.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.validate_dry_run(script, warnings));
    FT_ASSERT_EQ(0, warnings.size());
    return (1);
}

FT_TEST(test_game_script_bridge_empty_script_budget_is_zero)
{
    game_script_bridge value;
    ft_string script;
    int32_t required_operations;

    required_operations = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.inspect_bytecode_budget(script, required_operations));
    FT_ASSERT_EQ(0, required_operations);
    return (1);
}
