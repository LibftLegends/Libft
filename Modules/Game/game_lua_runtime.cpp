#include "game_scripting_bridge.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "../Lua/vendor/lua-5.4.8/lua.hpp"
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif
#include <cstdlib>

static void game_lua_remove_global(lua_State *lua_state,
    const char *name) noexcept
{
    lua_pushnil(lua_state);
    lua_setglobal(lua_state, name);
    return ;
}

static void game_lua_open_library(lua_State *lua_state, const char *name,
    lua_CFunction open_function) noexcept
{
    luaL_requiref(lua_state, name, open_function, 1);
    lua_pop(lua_state, 1);
    return ;
}

void *game_script_bridge::lua_allocate(void *user_data, void *pointer,
    ft_size_t old_size, ft_size_t new_size) noexcept
{
    game_script_bridge *bridge;
    void *new_pointer;
    ft_size_t prospective_size;

    bridge = static_cast<game_script_bridge *>(user_data);
    if (bridge == ft_nullptr)
        return (ft_nullptr);
    if (new_size == 0U)
    {
        std::free(pointer);
        if (old_size <= bridge->_lua_memory_used)
            bridge->_lua_memory_used -= old_size;
        else
            bridge->_lua_memory_used = 0U;
        return (ft_nullptr);
    }
    prospective_size = bridge->_lua_memory_used;
    if (pointer != ft_nullptr && old_size <= prospective_size)
        prospective_size -= old_size;
    if (new_size > bridge->_lua_memory_limit
        || prospective_size > bridge->_lua_memory_limit - new_size)
        return (ft_nullptr);
    new_pointer = std::realloc(pointer, new_size);
    if (new_pointer == ft_nullptr)
        return (ft_nullptr);
    bridge->_lua_memory_used = prospective_size + new_size;
    return (new_pointer);
}

void *game_script_bridge::lua_allocate_native(void *user_data, void *pointer,
    std::size_t old_size, std::size_t new_size) noexcept
{
    return (game_script_bridge::lua_allocate(user_data, pointer,
        FT_NATIVE_SIZE_TO_FT_SIZE_CAST(old_size),
        FT_NATIVE_SIZE_TO_FT_SIZE_CAST(new_size)));
}

void game_script_bridge::lua_instruction_hook(lua_State *lua_state,
    lua_Debug *debug_record) noexcept
{
    game_script_bridge *bridge;

    (void)debug_record;
    lua_getfield(lua_state, LUA_REGISTRYINDEX, "libft.game.bridge");
    bridge = static_cast<game_script_bridge *>(lua_touserdata(lua_state, -1));
    lua_pop(lua_state, 1);
    if (bridge == ft_nullptr)
        return ;
    bridge->_lua_instruction_count += 100;
    if (bridge->_lua_instruction_limit > 0
        && bridge->_lua_instruction_count > bridge->_lua_instruction_limit)
    {
        bridge->_lua_callback_error = FT_ERR_INVALID_OPERATION;
        (void)luaL_error(lua_state, "Lua instruction limit exceeded");
    }
    return ;
}

int32_t game_script_bridge::lua_callback_dispatch(lua_State *lua_state) noexcept
{
    game_script_bridge *bridge;
    const char *function_name;
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry;
    ft_vector<ft_string> arguments;
    int32_t argument_index;
    int32_t argument_count;
    int32_t error_code;
    ft_string callback_name;

    bridge = static_cast<game_script_bridge *>(lua_touserdata(lua_state,
        lua_upvalueindex(1)));
    function_name = lua_tostring(lua_state, lua_upvalueindex(2));
    if (bridge == ft_nullptr || function_name == ft_nullptr
        || bridge->_lua_context == ft_nullptr)
        return (0);
    error_code = callback_name.initialize(function_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        bridge->_lua_callback_error = error_code;
        return (0);
    }
    entry = bridge->_callbacks.find(callback_name);
    if (entry == bridge->_callbacks.end() || !entry->value)
    {
        bridge->_lua_callback_error = FT_ERR_NOT_FOUND;
        return (0);
    }
    error_code = arguments.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        bridge->_lua_callback_error = error_code;
        return (0);
    }
    argument_count = lua_gettop(lua_state);
    argument_index = 1;
    while (argument_index <= argument_count)
    {
        const char *argument_data;
        ft_string argument;

        if (lua_type(lua_state, argument_index) == LUA_TBOOLEAN)
        {
            if (lua_toboolean(lua_state, argument_index) != 0)
                argument_data = "1";
            else
                argument_data = "0";
        }
        else if (lua_type(lua_state, argument_index) == LUA_TNUMBER
            || lua_type(lua_state, argument_index) == LUA_TSTRING)
            argument_data = lua_tostring(lua_state, argument_index);
        else
        {
            bridge->_lua_callback_error = FT_ERR_INVALID_ARGUMENT;
            return (0);
        }
        error_code = argument.initialize(argument_data);
        if (error_code == FT_ERR_SUCCESS)
            error_code = arguments.push_back(argument);
        if (error_code != FT_ERR_SUCCESS)
        {
            bridge->_lua_callback_error = error_code;
            return (0);
        }
        argument_index += 1;
    }
    bridge->_lua_context->clear_result();
    error_code = entry->value(*bridge->_lua_context, arguments);
    if (error_code != FT_ERR_SUCCESS)
    {
        bridge->_lua_callback_error = error_code;
        return (0);
    }
    if (bridge->_lua_context->has_result_integer() == FT_TRUE)
    {
        lua_pushinteger(lua_state,
            bridge->_lua_context->get_result_integer());
        return (1);
    }
    return (0);
}

int32_t game_script_bridge::initialize_lua_runtime() noexcept
{
    this->_lua_memory_used = 0U;
    this->_lua_instruction_count = 0;
    this->_lua_callback_error = FT_ERR_SUCCESS;
    this->_lua_context = ft_nullptr;
    this->_lua_state = lua_newstate(
        &game_script_bridge::lua_allocate_native, this);
    if (this->_lua_state == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    game_lua_open_library(this->_lua_state, LUA_GNAME, luaopen_base);
    game_lua_open_library(this->_lua_state, LUA_TABLIBNAME, luaopen_table);
    game_lua_open_library(this->_lua_state, LUA_STRLIBNAME, luaopen_string);
    game_lua_open_library(this->_lua_state, LUA_MATHLIBNAME, luaopen_math);
    game_lua_open_library(this->_lua_state, LUA_UTF8LIBNAME, luaopen_utf8);
    game_lua_remove_global(this->_lua_state, "dofile");
    game_lua_remove_global(this->_lua_state, "loadfile");
    game_lua_remove_global(this->_lua_state, "load");
    game_lua_remove_global(this->_lua_state, "collectgarbage");
    game_lua_remove_global(this->_lua_state, "require");
    game_lua_remove_global(this->_lua_state, "package");
    game_lua_remove_global(this->_lua_state, "io");
    game_lua_remove_global(this->_lua_state, "os");
    game_lua_remove_global(this->_lua_state, "debug");
    return (FT_ERR_SUCCESS);
}

void game_script_bridge::destroy_lua_runtime() noexcept
{
    lua_State *lua_state;

    lua_state = this->_lua_state;
    this->_lua_state = ft_nullptr;
    this->_lua_context = ft_nullptr;
    if (lua_state != ft_nullptr)
        lua_close(lua_state);
    this->_lua_memory_used = 0U;
    return ;
}

int32_t game_script_bridge::register_lua_callbacks() noexcept
{
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry;
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry_end;

    if (this->_lua_state == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    entry = this->_callbacks.end() - this->_callbacks.size();
    entry_end = this->_callbacks.end();
    while (entry != entry_end)
    {
        lua_pushlightuserdata(this->_lua_state, this);
        lua_pushstring(this->_lua_state, entry->key.c_str());
        lua_pushcclosure(this->_lua_state,
            &game_script_bridge::lua_callback_dispatch, 2);
        lua_setglobal(this->_lua_state, entry->key.c_str());
        entry += 1;
    }
    return (FT_ERR_SUCCESS);
}

void game_script_bridge::remove_lua_callback(const ft_string &name) noexcept
{
    if (this->_lua_state == ft_nullptr)
        return ;
    lua_pushnil(this->_lua_state);
    lua_setglobal(this->_lua_state, name.c_str());
    return ;
}

int32_t game_script_bridge::execute_lua(const ft_string &script,
    game_state &state) noexcept
{
    return (this->execute_lua_with_user_data(script, &state, ft_nullptr));
}

int32_t game_script_bridge::execute_lua_with_user_data(const ft_string &script,
    game_state *state, void *user_data) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;
    int32_t lua_status;
    game_script_context context;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_NOT_INITIALISED));
    }
    if (this->_lua_state == ft_nullptr)
    {
        error_code = this->initialize_lua_runtime();
        if (error_code != FT_ERR_SUCCESS)
        {
            this->unlock_internal(lock_acquired);
            return (this->set_error(error_code));
        }
    }
    error_code = context.initialize(state, this->_world);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(error_code));
    }
    context.set_user_data(user_data);
    this->_lua_context = &context;
    this->_lua_instruction_count = 0;
    this->_lua_callback_error = FT_ERR_SUCCESS;
    lua_settop(this->_lua_state, 0);
    lua_pushlightuserdata(this->_lua_state, this);
    lua_setfield(this->_lua_state, LUA_REGISTRYINDEX, "libft.game.bridge");
    error_code = this->register_lua_callbacks();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->_lua_context = ft_nullptr;
        this->unlock_internal(lock_acquired);
        return (this->set_error(error_code));
    }
    lua_sethook(this->_lua_state, &game_script_bridge::lua_instruction_hook,
        LUA_MASKCOUNT, 100);
    lua_status = luaL_loadbufferx(this->_lua_state, script.c_str(),
        script.size(), "libft-script", "t");
    if (lua_status == LUA_OK)
        lua_status = lua_pcall(this->_lua_state, 0, 0, 0);
    lua_sethook(this->_lua_state, ft_nullptr, 0, 0);
    this->_lua_context = ft_nullptr;
    if (this->_lua_callback_error != FT_ERR_SUCCESS)
        error_code = this->_lua_callback_error;
    else if (lua_status == LUA_ERRMEM)
        error_code = FT_ERR_NO_MEMORY;
    else if (lua_status != LUA_OK)
        error_code = FT_ERR_INVALID_OPERATION;
    else
        error_code = FT_ERR_SUCCESS;
    lua_settop(this->_lua_state, 0);
    this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_script_bridge::get_lua_global_string(const ft_string &name,
    ft_string &value) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;
    const char *string_value;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_lua_state == ft_nullptr)
        error_code = FT_ERR_NOT_INITIALISED;
    else if (name.empty())
        error_code = FT_ERR_INVALID_ARGUMENT;
    else
    {
        lua_getglobal(this->_lua_state, name.c_str());
        if (lua_type(this->_lua_state, -1) == LUA_TNIL)
            error_code = FT_ERR_NOT_FOUND;
        else if (lua_type(this->_lua_state, -1) != LUA_TSTRING)
            error_code = FT_ERR_INVALID_ARGUMENT;
        else
        {
            string_value = lua_tostring(this->_lua_state, -1);
            error_code = value.clear();
            if (error_code == FT_ERR_SUCCESS)
                error_code = value.append(string_value);
        }
        lua_pop(this->_lua_state, 1);
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_script_bridge::get_lua_global_integer(const ft_string &name,
    int64_t &value) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;
    int32_t is_integer;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_lua_state == ft_nullptr)
        error_code = FT_ERR_NOT_INITIALISED;
    else if (name.empty())
        error_code = FT_ERR_INVALID_ARGUMENT;
    else
    {
        lua_getglobal(this->_lua_state, name.c_str());
        is_integer = 0;
        if (lua_type(this->_lua_state, -1) == LUA_TNIL)
            error_code = FT_ERR_NOT_FOUND;
        else if (lua_type(this->_lua_state, -1) != LUA_TNUMBER)
            error_code = FT_ERR_INVALID_ARGUMENT;
        else
        {
            value = lua_tointegerx(this->_lua_state, -1, &is_integer);
            if (is_integer == 0)
                error_code = FT_ERR_INVALID_ARGUMENT;
            else
                error_code = FT_ERR_SUCCESS;
        }
        lua_pop(this->_lua_state, 1);
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_script_bridge::get_lua_global_boolean(const ft_string &name,
    ft_bool &value) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
        return (this->set_error(error_code));
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        || this->_lua_state == ft_nullptr)
        error_code = FT_ERR_NOT_INITIALISED;
    else if (name.empty())
        error_code = FT_ERR_INVALID_ARGUMENT;
    else
    {
        lua_getglobal(this->_lua_state, name.c_str());
        if (lua_type(this->_lua_state, -1) == LUA_TNIL)
            error_code = FT_ERR_NOT_FOUND;
        else if (lua_type(this->_lua_state, -1) != LUA_TBOOLEAN)
            error_code = FT_ERR_INVALID_ARGUMENT;
        else
        {
            value = static_cast<ft_bool>(lua_toboolean(this->_lua_state,
                -1));
            error_code = FT_ERR_SUCCESS;
        }
        lua_pop(this->_lua_state, 1);
    }
    this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

void game_script_bridge::set_lua_instruction_limit(int32_t limit) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->set_error(error_code);
        return ;
    }
    if (limit < 0)
        this->set_error(FT_ERR_INVALID_ARGUMENT);
    else
    {
        this->_lua_instruction_limit = limit;
        this->set_error(FT_ERR_SUCCESS);
    }
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_script_bridge::get_lua_instruction_limit() const noexcept
{
    return (this->_lua_instruction_limit);
}

void game_script_bridge::set_lua_memory_limit(ft_size_t limit) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;

    lock_acquired = FT_FALSE;
    error_code = this->lock_internal(&lock_acquired);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->set_error(error_code);
        return ;
    }
    if (limit < this->_lua_memory_used)
        this->set_error(FT_ERR_INVALID_ARGUMENT);
    else
    {
        this->_lua_memory_limit = limit;
        this->set_error(FT_ERR_SUCCESS);
    }
    this->unlock_internal(lock_acquired);
    return ;
}

ft_size_t game_script_bridge::get_lua_memory_limit() const noexcept
{
    return (this->_lua_memory_limit);
}

ft_size_t game_script_bridge::get_lua_memory_used() const noexcept
{
    return (this->_lua_memory_used);
}
