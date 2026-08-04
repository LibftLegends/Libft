#include "../test_internal.hpp"
#include "../../Modules/Game/game_scripting_bridge.hpp"
#include "../../Modules/Game/game_world.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t game_lua_callback_count = 0;
static int32_t game_lua_callback_value = 0;

static int32_t game_lua_capture_value(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    (void)context;
    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    game_lua_callback_value = ft_atoi(arguments[0].c_str());
    game_lua_callback_count += 1;
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
