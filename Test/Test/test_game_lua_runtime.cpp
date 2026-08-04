#include "../test_internal.hpp"
#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t game_lua_callback_count = 0;
static int32_t game_lua_callback_value = 0;
static int32_t game_lua_argument_count = 0;
static char game_lua_argument_one[32] = {0};
static char game_lua_argument_two[32] = {0};
static char game_lua_argument_three[32] = {0};
static void *game_lua_expected_user_data = ft_nullptr;
static void *game_lua_observed_user_data = ft_nullptr;
static game_state *game_lua_observed_state = ft_nullptr;

static int32_t game_lua_capture_value(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    (void)context;
    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    game_lua_observed_user_data = context.get_user_data();
    game_lua_observed_state = context.get_state();
    game_lua_callback_value = ft_atoi(arguments[0].c_str());
    game_lua_callback_count += 1;
    return (FT_ERR_SUCCESS);
}

static int32_t game_lua_capture_arguments(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    game_lua_observed_user_data = context.get_user_data();
    game_lua_observed_state = context.get_state();
    ft_strlcpy(game_lua_argument_one, arguments[0].c_str(),
        sizeof(game_lua_argument_one));
    ft_strlcpy(game_lua_argument_two, arguments[1].c_str(),
        sizeof(game_lua_argument_two));
    ft_strlcpy(game_lua_argument_three, arguments[2].c_str(),
        sizeof(game_lua_argument_three));
    game_lua_argument_count = 3;
    return (FT_ERR_SUCCESS);
}

static int32_t game_lua_prepare_bridge(game_script_bridge &bridge,
    ft_sharedptr<game_world> &world_pointer) noexcept
{
    ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> callback(game_lua_capture_value);
    ft_string callback_name;
    int32_t error_code;

    error_code = world_pointer.initialize(new game_world());
    if (error_code == FT_ERR_SUCCESS)
        error_code = world_pointer->initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = bridge.initialize(world_pointer);
    if (error_code == FT_ERR_SUCCESS)
        error_code = callback_name.initialize("capture_value");
    if (error_code == FT_ERR_SUCCESS)
        error_code = bridge.register_function(callback_name, callback);
    return (error_code);
}

static int32_t game_lua_register_callback(game_script_bridge &bridge,
    const char *name, int32_t (*callback)(game_script_context &,
        const ft_vector<ft_string> &)) noexcept
{
    ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> function(callback);
    ft_string function_name;
    int32_t error_code;

    error_code = function_name.initialize(name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = bridge.register_function(function_name, function);
    return (error_code);
}

static int32_t game_lua_read_fixture(const char *name,
    ft_string &script) noexcept
{
    ft_string path;
    int32_t error_code;

    error_code = path.initialize("Lua/");
    if (error_code == FT_ERR_SUCCESS)
        error_code = path.append(name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = script.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_read_all(path.c_str(), script);
    if (error_code == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    error_code = script.destroy();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = path.clear();
    if (error_code == FT_ERR_SUCCESS)
        error_code = path.append("Test/Lua/");
    if (error_code == FT_ERR_SUCCESS)
        error_code = path.append(name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = script.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_read_all(path.c_str(), script);
    return (error_code);
}

FT_TEST(test_game_lua_runtime_executes_real_lua_control_flow)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("control_flow.lua",
        script));
    game_lua_callback_count = 0;
    game_lua_callback_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(1, game_lua_callback_count);
    FT_ASSERT_EQ(15, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_sandbox_hides_system_libraries)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("sandbox.lua",
        script));
    game_lua_callback_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(7, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_converts_callback_argument_types)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;
    void *user_data;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_register_callback(bridge,
        "capture_arguments", game_lua_capture_arguments));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture(
        "argument_conversion.lua", script));
    user_data = static_cast<void *>(&bridge);
    game_lua_expected_user_data = user_data;
    game_lua_argument_count = 0;
    game_lua_observed_user_data = ft_nullptr;
    game_lua_observed_state = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua_with_user_data(script,
        &state, user_data));
    FT_ASSERT_EQ(3, game_lua_argument_count);
    FT_ASSERT_STR_EQ("hello", game_lua_argument_one);
    FT_ASSERT_STR_EQ("42", game_lua_argument_two);
    FT_ASSERT_STR_EQ("1", game_lua_argument_three);
    FT_ASSERT_EQ(game_lua_expected_user_data, game_lua_observed_user_data);
    FT_ASSERT_EQ(&state, game_lua_observed_state);
    return (1);
}

FT_TEST(test_game_lua_runtime_preserves_globals_between_executions)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string first_script;
    ft_string second_script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture(
        "persistent_state_first.lua", first_script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture(
        "persistent_state_second.lua", second_script));
    game_lua_callback_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(first_script, state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(second_script, state));
    FT_ASSERT_EQ(15, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_exports_values_to_cpp)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;
    ft_string integer_name;
    ft_string message_name;
    ft_string enabled_name;
    ft_string message;
    int64_t integer_value;
    ft_bool enabled_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("export_values.lua",
        script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, integer_name.initialize("bridge_integer"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message_name.initialize("bridge_message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, enabled_name.initialize("bridge_enabled"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, message.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_lua_global_integer(integer_name,
        integer_value));
    FT_ASSERT_EQ(37, integer_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_lua_global_string(message_name,
        message));
    FT_ASSERT_STR_EQ("value exported from Lua", message.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.get_lua_global_boolean(enabled_name,
        enabled_value));
    FT_ASSERT_EQ(FT_TRUE, enabled_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_reports_callback_argument_errors)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string error_script;
    ft_string recovery_script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("callback_error.lua",
        error_script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovery_script.initialize(
        "capture_value(12)\n"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, bridge.execute_lua(error_script,
        state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(recovery_script, state));
    FT_ASSERT_EQ(12, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_recovers_after_syntax_and_runtime_errors)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string syntax_script;
    ft_string runtime_script;
    ft_string recovery_script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("syntax_error.lua",
        syntax_script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("runtime_error.lua",
        runtime_script));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recovery_script.initialize(
        "capture_value(13)\n"));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.execute_lua(syntax_script,
        state));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.execute_lua(runtime_script,
        state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(recovery_script, state));
    FT_ASSERT_EQ(13, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_remove_and_reregister_callback)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string callback_name;
    ft_string script;
    ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> callback(game_lua_capture_value);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, callback_name.initialize("capture_value"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("capture_value(14)\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.remove_function(callback_name));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.register_function(callback_name,
        callback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(14, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_destroy_and_reinitialize)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("capture_value(16)\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.initialize(world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_register_callback(bridge,
        "capture_value", game_lua_capture_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(16, game_lua_callback_value);
    return (1);
}

FT_TEST(test_game_lua_runtime_instruction_limit_stops_infinite_loop)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    bridge.set_lua_instruction_limit(1000);
    FT_ASSERT_EQ(1000, bridge.get_lua_instruction_limit());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture(
        "instruction_limit.lua", script));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, bridge.execute_lua(script, state));
    return (1);
}

FT_TEST(test_game_lua_runtime_memory_limit_is_retryable)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;
    ft_size_t memory_used;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_prepare_bridge(bridge,
        world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    memory_used = bridge.get_lua_memory_used();
    bridge.set_lua_memory_limit(memory_used + 256U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, game_lua_read_fixture("memory_limit.lua",
        script));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, bridge.execute_lua(script, state));
    bridge.set_lua_memory_limit(16U * 1024U * 1024U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.append("capture_value(9)\n"));
    game_lua_callback_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute_lua(script, state));
    FT_ASSERT_EQ(9, game_lua_callback_value);
    return (1);
}
