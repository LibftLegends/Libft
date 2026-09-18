#include "../test_internal.hpp"
#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"

static int32_t g_custom_game_callback_value = 0;
static int32_t g_custom_game_callback_count = 0;
static int32_t g_custom_game_argument_count = 0;

static int32_t custom_game_capture_value(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    (void)context;
    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    g_custom_game_callback_value = ft_atoi(arguments[0].c_str());
    g_custom_game_callback_count += 1;
    return (FT_ERR_SUCCESS);
}

static int32_t custom_game_capture_arguments(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    (void)context;
    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (arguments[0] != "hello" || arguments[1] != "42"
        || arguments[2] != "1")
        return (FT_ERR_INVALID_ARGUMENT);
    g_custom_game_argument_count = 3;
    return (FT_ERR_SUCCESS);
}

static int32_t custom_game_add(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    int32_t first;
    int32_t second;

    (void)context;
    if (arguments.size() != 2U)
        return (FT_ERR_INVALID_ARGUMENT);
    first = ft_atoi(arguments[0].c_str());
    second = ft_atoi(arguments[1].c_str());
    context.set_result_integer(static_cast<int64_t>(first + second));
    return (FT_ERR_SUCCESS);
}

static int32_t custom_game_register(game_script_bridge &bridge,
    const char *name, int32_t (*callback)(game_script_context &,
        const ft_vector<ft_string> &)) noexcept
{
    ft_string function_name;
    ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> function(callback);
    int32_t error_code;

    error_code = function_name.initialize(name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = bridge.register_function(function_name, function);
    return (error_code);
}

static int32_t custom_game_prepare(game_script_bridge &bridge,
    ft_sharedptr<game_world> &world_pointer) noexcept
{
    int32_t error_code;

    error_code = world_pointer.initialize(new game_world());
    if (error_code == FT_ERR_SUCCESS)
        error_code = world_pointer->initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = bridge.initialize(world_pointer);
    return (error_code);
}

FT_TEST(test_game_custom_scripting_executes_control_flow_and_callback)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_prepare(bridge, world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_register(bridge,
        "capture_value", custom_game_capture_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize(
        "let total = 0; let index = 1;"
        "while (index <= 5) { total = total + index; index = index + 1; };"
        "capture_value(total);"));
    g_custom_game_callback_count = 0;
    g_custom_game_callback_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    FT_ASSERT_EQ(1, g_custom_game_callback_count);
    FT_ASSERT_EQ(15, g_custom_game_callback_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_game_custom_scripting_converts_typed_callback_arguments)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_prepare(bridge, world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_register(bridge,
        "capture_arguments", custom_game_capture_arguments));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize(
        "capture_arguments(\"hello\", 42, true);"));
    g_custom_game_argument_count = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    FT_ASSERT_EQ(3, g_custom_game_argument_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_game_custom_scripting_reports_callback_failures_and_recovers)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string invalid_script;
    ft_string valid_script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_prepare(bridge, world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_register(bridge,
        "capture_value", custom_game_capture_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, invalid_script.initialize(
        "capture_value(1, 2);"));
    valid_script.initialize("capture_value(12);");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        bridge.execute(invalid_script, state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(valid_script, state));
    FT_ASSERT_EQ(12, g_custom_game_callback_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}

FT_TEST(test_game_custom_scripting_returns_native_result)
{
    ft_sharedptr<game_world> world_pointer;
    game_script_bridge bridge;
    game_state state;
    ft_string script;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_prepare(bridge, world_pointer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, state.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, custom_game_register(bridge, "add",
        custom_game_add));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, script.initialize("add(7, 5);"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.execute(script, state));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bridge.destroy());
    return (1);
}
