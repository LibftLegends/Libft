#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_destroyed_state_helpers.hpp"

static void bridge_destroyed_set_language(game_script_bridge &value)
{
    value.set_language("custom");
    return ;
}

static void bridge_destroyed_set_max_operations(game_script_bridge &value)
{
    value.set_max_operations(1);
    return ;
}

static void bridge_destroyed_get_callback_count(game_script_bridge &value)
{
    (void)value.get_callback_count();
    return ;
}

static void bridge_destroyed_register_function(game_script_bridge &value)
{
    ft_string name;
    ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)>
        callback;

    (void)name.initialize("callback");
    (void)value.register_function(name, callback);
    return ;
}

static void bridge_destroyed_remove_function(game_script_bridge &value)
{
    ft_string name;

    (void)name.initialize("callback");
    (void)value.remove_function(name);
    return ;
}

static void bridge_destroyed_execute(game_script_bridge &value)
{
    game_state state;
    ft_string script;

    (void)state.initialize();
    (void)script.initialize();
    (void)value.execute(script, state);
    return ;
}

static void bridge_destroyed_check_sandbox(game_script_bridge &value)
{
    ft_string script;
    ft_vector<ft_string> violations;

    (void)script.initialize();
    (void)violations.initialize();
    (void)value.check_sandbox_capabilities(script, violations);
    return ;
}

static void bridge_destroyed_validate_dry_run(game_script_bridge &value)
{
    ft_string script;
    ft_vector<ft_string> warnings;

    (void)script.initialize();
    (void)warnings.initialize();
    (void)value.validate_dry_run(script, warnings);
    return ;
}

static void bridge_destroyed_inspect_budget(game_script_bridge &value)
{
    ft_string script;
    int32_t required_operations;

    required_operations = 0;
    (void)script.initialize();
    (void)value.inspect_bytecode_budget(script, required_operations);
    return ;
}

static void bridge_destroyed_enable_thread_safety(game_script_bridge &value)
{
    (void)value.enable_thread_safety();
    return ;
}

FT_TEST(test_game_script_bridge_destroyed_set_language_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_set_language));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_set_max_operations_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_set_max_operations));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_get_callback_count_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_get_callback_count));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_register_function_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_register_function));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_remove_function_aborts)
{
    FT_ASSERT_EQ(1, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_remove_function));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_execute_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_execute));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_check_sandbox_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_check_sandbox));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_validate_dry_run_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_validate_dry_run));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_inspect_budget_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_inspect_budget));
    return (1);
}

FT_TEST(test_game_script_bridge_destroyed_enable_thread_safety_aborts)
{
    FT_ASSERT_EQ(0, expect_game_destroyed_sigabrt<game_script_bridge>(
                        bridge_destroyed_enable_thread_safety));
    return (1);
}
