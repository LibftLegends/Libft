#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_game_lifecycle_helpers.hpp"

typedef ft_function<int32_t(game_script_context &,
                            const ft_vector<ft_string> &)>
    game_script_callback;

static void game_script_bridge_set_language(game_script_bridge &value)
{
    value.set_language("custom");
    return ;
}

static void game_script_bridge_get_language(game_script_bridge &value)
{
    (void)value.get_language();
    return ;
}

static void game_script_bridge_set_max_operations(game_script_bridge &value)
{
    value.set_max_operations(1);
    return ;
}

static void game_script_bridge_get_max_operations(game_script_bridge &value)
{
    (void)value.get_max_operations();
    return ;
}

static void game_script_bridge_get_callback_count(game_script_bridge &value)
{
    (void)value.get_callback_count();
    return ;
}

static void game_script_bridge_register_function(game_script_bridge &value)
{
    ft_string name;
    game_script_callback callback;

    (void)value.register_function(name, callback);
    return ;
}

static void game_script_bridge_remove_function(game_script_bridge &value)
{
    ft_string name;

    (void)value.remove_function(name);
    return ;
}

static void game_script_bridge_execute(game_script_bridge &value)
{
    ft_string script;
    game_state state;

    (void)value.execute(script, state);
    return ;
}

static void
game_script_bridge_check_sandbox_capabilities(game_script_bridge &value)
{
    ft_string script;
    ft_vector<ft_string> violations;

    (void)value.check_sandbox_capabilities(script, violations);
    return ;
}

static void game_script_bridge_validate_dry_run(game_script_bridge &value)
{
    ft_string script;
    ft_vector<ft_string> warnings;

    (void)value.validate_dry_run(script, warnings);
    return ;
}

static void
game_script_bridge_inspect_bytecode_budget(game_script_bridge &value)
{
    ft_string script;
    int32_t required_operations = 0;

    (void)value.inspect_bytecode_budget(script, required_operations);
    return ;
}

static void game_script_bridge_enable_thread_safety(game_script_bridge &value)
{
    (void)value.enable_thread_safety();
    return ;
}

static void game_script_bridge_disable_thread_safety(game_script_bridge &value)
{
    (void)value.disable_thread_safety();
    return ;
}

static void game_script_bridge_is_thread_safe(game_script_bridge &value)
{
    (void)value.is_thread_safe();
    return ;
}

static void game_script_bridge_get_error(game_script_bridge &value)
{
    (void)value.get_error();
    return ;
}

static void game_script_bridge_get_error_str(game_script_bridge &value)
{
    (void)value.get_error_str();
    return ;
}

FT_TEST(test_game_script_bridge_set_language_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_set_language));
    return (1);
}

FT_TEST(test_game_script_bridge_get_language_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_get_language));
    return (1);
}

FT_TEST(test_game_script_bridge_set_max_operations_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_set_max_operations));
    return (1);
}

FT_TEST(test_game_script_bridge_get_max_operations_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_get_max_operations));
    return (1);
}

FT_TEST(test_game_script_bridge_get_callback_count_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_get_callback_count));
    return (1);
}

FT_TEST(test_game_script_bridge_register_function_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_register_function));
    return (1);
}

FT_TEST(test_game_script_bridge_remove_function_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_remove_function));
    return (1);
}

FT_TEST(test_game_script_bridge_execute_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_execute));
    return (1);
}

FT_TEST(
    test_game_script_bridge_check_sandbox_capabilities_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_check_sandbox_capabilities));
    return (1);
}

FT_TEST(test_game_script_bridge_validate_dry_run_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_validate_dry_run));
    return (1);
}

FT_TEST(test_game_script_bridge_inspect_bytecode_budget_uninitialised_is_safe)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_inspect_bytecode_budget));
    return (1);
}

FT_TEST(test_game_script_bridge_enable_thread_safety_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_enable_thread_safety));
    return (1);
}

FT_TEST(test_game_script_bridge_disable_thread_safety_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_disable_thread_safety));
    return (1);
}

FT_TEST(test_game_script_bridge_is_thread_safe_uninitialised_is_safe)
{
    FT_ASSERT_EQ(0, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_is_thread_safe));
    return (1);
}

FT_TEST(test_game_script_bridge_get_error_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_get_error));
    return (1);
}

FT_TEST(test_game_script_bridge_get_error_str_uninitialised_aborts)
{
    FT_ASSERT_EQ(1, expect_game_lifecycle_sigabrt<game_script_bridge>(
                        game_script_bridge_get_error_str));
    return (1);
}
