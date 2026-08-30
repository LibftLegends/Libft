#include "../PThread/pthread_internal.hpp"
#include "game_scripting_bridge.hpp"
#include "../Template/pair.hpp"
#include "../Template/move.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstdio>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/shared_ptr.hpp"
#include "../Template/vector.hpp"
#include "game_achievement.hpp"
#include "game_buff.hpp"
#include "game_crafting.hpp"
#include "game_currency_rate.hpp"
#include "game_debuff.hpp"
#include "game_dialogue_line.hpp"
#include "game_dialogue_script.hpp"
#include "game_dialogue_table.hpp"
#include "game_economy_table.hpp"
#include "game_pathfinding.hpp"
#include "game_price_definition.hpp"
#include "game_quest.hpp"
#include "game_rarity_band.hpp"
#include "game_region_definition.hpp"
#include "game_skill.hpp"
#include "game_state.hpp"
#include "game_upgrade.hpp"
#include "game_vendor_profile.hpp"
#include "game_world_region.hpp"
#include "game_world_registry.hpp"
#include "game_world_replay.hpp"

static void trim_whitespace(ft_string &target) noexcept
{
    ft_size_t length;
    ft_size_t end;
    ft_size_t index;
    const char *data;

    length = target.size();
    data = target.c_str();
    index = 0;
    while (index < length
        && (data[index] == ' ' || data[index] == '\t' || data[index] == '\r'))
        index++;
    if (index > 0)
        target.erase(0, index);
    length = target.size();
    if (length == 0)
        return ;
    data = target.c_str();
    end = length;
    while (end > 0
        && (data[end - 1] == ' ' || data[end - 1] == '\t' || data[end - 1] == '\r'))
        end--;
    if (end < length)
        target.erase(end, length - end);
    return ;
}

static void game_script_delete_string(ft_string *string) noexcept
{
    if (string == ft_nullptr)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

thread_local int32_t game_script_context::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_script_bridge::_last_error = FT_ERR_SUCCESS;

static ft_bool game_script_uses_legacy_commands(const ft_string &script)
    noexcept
{
    const char *data;

    data = script.c_str();
    if (data == ft_nullptr)
        return (FT_FALSE);
    if (script.size() == 0U)
        return (FT_FALSE);
    if (ft_strstr(data, "set ") != ft_nullptr
        || ft_strstr(data, "unset ") != ft_nullptr
        || ft_strstr(data, "call ") != ft_nullptr)
        return (FT_TRUE);
    if (ft_strstr(data, "\n") != ft_nullptr
        || ft_strstr(data, "\r") != ft_nullptr)
        return (FT_TRUE);
    if (ft_strstr(data, "(") == ft_nullptr
        && ft_strstr(data, ";") == ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

int32_t game_script_context::set_error(int32_t error_code) noexcept
{
    game_script_context::_last_error = error_code;
    return (error_code);
}

game_script_context::game_script_context() noexcept
    : _state(ft_nullptr), _user_data(ft_nullptr), _world(), _variables(),
      _result_integer(0), _result_integer_set(FT_FALSE),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_script_context::~game_script_context() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    else
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return ;
}

int32_t game_script_context::initialize() noexcept
{
    int32_t variable_error;
    int32_t world_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_script_context::initialize", "already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    variable_error = this->_variables.initialize();
    if (variable_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(variable_error);
        return (variable_error);
    }
    world_error = this->_world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        (void)this->_variables.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    this->_state = ft_nullptr;
    this->_user_data = ft_nullptr;
    this->clear_result();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_context::initialize(game_state *state,
    const ft_sharedptr<game_world> &world) noexcept
{
    int32_t variable_error;
    int32_t world_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_script_context::initialize", "already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    variable_error = this->_variables.initialize();
    if (variable_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(variable_error);
        return (variable_error);
    }
    world_error = this->_world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        (void)this->_variables.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    this->_state = state;
    this->_user_data = ft_nullptr;
    this->clear_result();
    this->_world = world;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_context::initialize(const game_script_context &other) noexcept
{
    int32_t destroy_error;
    int32_t variable_error;
    int32_t world_error;
    int32_t copy_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_script_context::initialize(copy)",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            this->set_error(destroy_error);
            return (destroy_error);
        }
    }
    variable_error = this->_variables.initialize();
    if (variable_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(variable_error);
        return (variable_error);
    }
    world_error = this->_world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        (void)this->_variables.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    copy_error = this->_variables.copy_from(other._variables);
    if (copy_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        (void)this->_variables.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(copy_error);
        return (copy_error);
    }
    this->_state = other._state;
    this->_user_data = other._user_data;
    this->_result_integer = other._result_integer;
    this->_result_integer_set = other._result_integer_set;
    this->_world = other._world;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

int32_t game_script_context::move(game_script_context &other) noexcept
{
    int32_t initialize_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_script_context::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_script_context &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t game_script_context::destroy() noexcept
{
    int32_t destroy_error;
    int32_t world_destroy_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_state = ft_nullptr;
    this->_user_data = ft_nullptr;
    this->clear_result();
    world_destroy_error = this->_world.destroy();
    destroy_error = this->_variables.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (world_destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(world_destroy_error);
        return (world_destroy_error);
    }
    this->set_error(destroy_error);
    return (destroy_error);
}

game_state *game_script_context::get_state() const noexcept
{
    return (this->_state);
}

void *game_script_context::get_user_data() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_script_context::get_user_data");
    return (this->_user_data);
}

const ft_sharedptr<game_world> &game_script_context::get_world() const noexcept
{
    return (this->_world);
}

void game_script_context::set_state(game_state *state) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::set_state");
    this->_state = state;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_script_context::set_user_data(void *user_data) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_script_context::set_user_data");
    this->_user_data = user_data;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_script_context::set_world(const ft_sharedptr<game_world> &world) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::set_world");
    this->_world = world;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_script_context::set_variable(const ft_string &key, const ft_string &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::set_variable");
    if (this->_state)
    {
        this->_state->set_variable(key, value);
        this->set_error(this->_state->get_error());
        return ;
    }
    Pair<ft_string, ft_string> *entry;

    entry = this->_variables.find(key);
    if (entry != this->_variables.end())
    {
        entry->value = value;
        this->set_error(FT_ERR_SUCCESS);
        return ;
    }
    this->_variables.insert(key, value);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

const ft_string *game_script_context::get_variable(const ft_string &key) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::get_variable");
    if (this->_state)
    {
        const ft_string *value;

        value = this->_state->get_variable(key);
        this->set_error(this->_state->get_error());
        return (value);
    }
    const Pair<ft_string, ft_string> *entry;

    entry = this->_variables.find(key);
    if (entry == this->_variables.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (ft_nullptr);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (&entry->value);
}

void game_script_context::remove_variable(const ft_string &key) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::remove_variable");
    if (this->_state)
    {
        this->_state->remove_variable(key);
        this->set_error(this->_state->get_error());
        return ;
    }
    this->_variables.remove(key);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_script_context::clear_variables() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_script_context::clear_variables");
    if (this->_state)
    {
        this->_state->clear_variables();
        this->set_error(this->_state->get_error());
        return ;
    }
    this->_variables.clear();
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_script_context::clear_result() noexcept
{
    this->_result_integer = 0;
    this->_result_integer_set = FT_FALSE;
    return ;
}

void game_script_context::set_result_integer(int64_t value) noexcept
{
    this->_result_integer = value;
    this->_result_integer_set = FT_TRUE;
    return ;
}

ft_bool game_script_context::has_result_integer() const noexcept
{
    return (this->_result_integer_set);
}

int64_t game_script_context::get_result_integer() const noexcept
{
    return (this->_result_integer);
}

int32_t game_script_context::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_script_context::get_error");
    return (game_script_context::_last_error);
}

const char *game_script_context::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_script_context::get_error_str");
    return (ft_strerror(game_script_context::_last_error));
}

int32_t game_script_bridge::set_error(int32_t error_code) noexcept
{
    game_script_bridge::_last_error = error_code;
    return (error_code);
}

ft_bool game_script_bridge::is_supported_language(const ft_string &language) noexcept
{
    ft_string normalized;
    char *data;

    normalized = language;
    data = normalized.data();
    if (data)
        ft_to_lower(data);
    if (normalized == "lua")
        return (FT_TRUE);
    if (normalized == "python")
        return (FT_TRUE);
    if (normalized == "custom")
        return (FT_TRUE);
    return (FT_FALSE);
}

game_script_bridge::game_script_bridge() noexcept
    : _world(), _callbacks(), _language(), _max_operations(32),
      _lua_instruction_limit(100000), _lua_instruction_count(0),
      _lua_callback_error(FT_ERR_SUCCESS),
      _lua_memory_limit(16U * 1024U * 1024U), _lua_memory_used(0U),
      _lua_state(ft_nullptr), _lua_context(ft_nullptr),
      _custom_engine(), _custom_context(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED),
      _mutex(ft_nullptr)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_script_bridge::initialize() noexcept
{
    ft_sharedptr<game_world> world;
    int32_t world_error;

    world_error = world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    return (this->initialize(world, "custom"));
}

int32_t game_script_bridge::initialize(const ft_sharedptr<game_world> &world,
    const char *language) noexcept
{
    int32_t map_error;
    int32_t world_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_script_bridge::initialize",
            "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_max_operations = 32;
    this->_lua_instruction_limit = 100000;
    this->_lua_instruction_count = 0;
    this->_lua_callback_error = FT_ERR_SUCCESS;
    this->_lua_memory_limit = 16U * 1024U * 1024U;
    this->_lua_memory_used = 0U;
    this->_lua_state = ft_nullptr;
    this->_lua_context = ft_nullptr;
    this->_custom_context = ft_nullptr;
    map_error = this->_callbacks.initialize();
    if (map_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(map_error);
        return (map_error);
    }
    world_error = this->_world.initialize();
    if (world_error != FT_ERR_SUCCESS)
    {
        (void)this->_callbacks.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(world_error);
        return (world_error);
    }
    this->_world = world;
    if (language)
        this->_language = language;
    else
        this->_language = "custom";
    ft_to_lower(this->_language.data());
    if (game_script_bridge::is_supported_language(this->_language) == FT_FALSE)
    {
        (void)this->_world.destroy();
        (void)this->_callbacks.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    map_error = this->_custom_engine.initialize();
    if (map_error != FT_ERR_SUCCESS)
    {
        (void)this->_custom_engine.destroy();
        (void)this->_world.destroy();
        (void)this->_callbacks.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(map_error);
        return (map_error);
    }
    map_error = this->_custom_engine.set_operation_limit(
        FT_SCRIPTING_MAX_OPERATIONS);
    if (map_error != FT_ERR_SUCCESS)
    {
        (void)this->_custom_engine.destroy();
        (void)this->_world.destroy();
        (void)this->_callbacks.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(map_error);
        return (map_error);
    }
    if (this->_language == "custom")
    {
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    map_error = this->initialize_lua_runtime();
    if (map_error != FT_ERR_SUCCESS)
    {
        (void)this->_world.destroy();
        (void)this->_callbacks.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(map_error);
        return (map_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

game_script_bridge::~game_script_bridge() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_script_bridge::destroy() noexcept
{
    int32_t first_error;
    int32_t current_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    first_error = FT_ERR_SUCCESS;
    current_error = this->disable_thread_safety();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    this->destroy_lua_runtime();
    current_error = this->_custom_engine.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_callbacks.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    current_error = this->_world.destroy();
    if (first_error == FT_ERR_SUCCESS && current_error != FT_ERR_SUCCESS)
        first_error = current_error;
    this->_language.clear();
    this->_custom_context = ft_nullptr;
    this->_max_operations = 32;
    this->_lua_instruction_limit = 100000;
    this->_lua_instruction_count = 0;
    this->_lua_callback_error = FT_ERR_SUCCESS;
    this->_lua_memory_limit = 16U * 1024U * 1024U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(first_error);
    return (first_error);
}

int32_t game_script_bridge::move(game_script_bridge &other) noexcept
{
    int32_t destroy_error;
    int32_t initialize_error;
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry;
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry_end;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_script_bridge::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize(other._world, other._language.c_str());
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_max_operations = other._max_operations;
    entry = other._callbacks.end() - other._callbacks.size();
    entry_end = other._callbacks.end();
    while (entry != entry_end)
    {
        initialize_error = this->register_function(entry->key, entry->value);
        if (initialize_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            this->set_error(initialize_error);
            return (initialize_error);
        }
        entry++;
    }
    (void)other.destroy();
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void game_script_bridge::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

void game_script_bridge::set_language(const char *language) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }

    if (!language)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        this->unlock_internal(lock_acquired);
        return ;
    }
    ft_string candidate;
    if (candidate.initialize(language) != FT_ERR_SUCCESS)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        this->unlock_internal(lock_acquired);
        return ;
    }
    if (game_script_bridge::is_supported_language(candidate) == FT_FALSE)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->_language = candidate;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

const ft_string &game_script_bridge::get_language() const noexcept
{
    return (this->_language);
}

void game_script_bridge::set_max_operations(int32_t limit) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t engine_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }

    if (limit < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        this->unlock_internal(lock_acquired);
        return ;
    }
    this->_max_operations = limit;
    if (limit > 0)
        engine_error = this->_custom_engine.set_operation_limit(
            static_cast<uint32_t>(limit));
    else
        engine_error = this->_custom_engine.set_operation_limit(
            FT_SCRIPTING_MAX_OPERATIONS);
    this->set_error(engine_error);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_script_bridge::get_max_operations() const noexcept
{
    return (this->_max_operations);
}

ft_size_t game_script_bridge::get_callback_count() const noexcept
{
    return (this->_callbacks.size());
}

int32_t game_script_bridge::register_function(const ft_string &name, const ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> &callback) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t native_error;
    uint32_t native_id;
    Pair<ft_string, ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> > *entry;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }

    if (name.empty() || !callback)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    entry = this->_callbacks.find(name);
    if (entry != this->_callbacks.end())
    {
        entry->value = callback;
        this->set_error(FT_ERR_SUCCESS);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_SUCCESS);
    }
    this->_callbacks.insert(name, callback);
    if (this->_custom_engine.find_native(name.c_str(),
        static_cast<uint32_t>(name.size()),
        &native_id) != FT_ERR_SUCCESS)
    {
        Pair<ft_string, ft_function<int32_t(game_script_context &,
            const ft_vector<ft_string> &)> > *native_entry;

        native_entry = this->_callbacks.find(name);
        if (native_entry == this->_callbacks.end())
        {
            this->set_error(FT_ERR_INVALID_STATE);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_INVALID_STATE);
        }
        native_error = this->_custom_engine.register_native(
            native_entry->key.c_str(),
            &game_script_bridge::custom_callback_dispatch, this, &native_id);
        if (native_error != FT_ERR_SUCCESS
            && native_error != FT_ERR_ALREADY_EXISTS)
        {
            this->set_error(native_error);
            this->unlock_internal(lock_acquired);
            return (native_error);
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::remove_function(const ft_string &name) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }

    this->_callbacks.remove(name);
    this->remove_lua_callback(name);
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_script_bridge::tokenize_line(const ft_string &line, ft_vector<ft_string> &tokens) const noexcept
{
    const char *data;
    ft_size_t length;
    ft_size_t index;

    tokens.clear();
    data = line.c_str();
    length = line.size();
    index = 0;
    while (index < length)
    {
        while (index < length
            && (data[index] == ' ' || data[index] == '\t'))
            index++;
        if (index >= length)
            break ;
        ft_size_t start;
        ft_size_t end;

        start = index;
        while (index < length
            && data[index] != ' '
            && data[index] != '\t')
            index++;
        end = index;
        ft_string *token;

        token = line.substr(start, end - start);
        if (token == ft_nullptr)
            return ;
        if (tokens.push_back(ft_move(*token)) != FT_ERR_SUCCESS)
        {
            game_script_delete_string(token);
            return ;
        }
        game_script_delete_string(token);
    }
    return ;
}

int32_t game_script_bridge::handle_set(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept
{
    ft_string value;
    ft_size_t index;
    ft_size_t count;

    if (tokens.size() < 3)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    value = tokens[2];
    count = tokens.size();
    index = 3;
    while (index < count)
    {
        value.append(" ");
        value.append(tokens[index]);
        index++;
    }
    context.set_variable(tokens[1], value);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::handle_unset(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept
{
    if (tokens.size() < 2)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    context.remove_variable(tokens[1]);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::handle_call(game_script_context &context, const ft_vector<ft_string> &tokens) noexcept
{
    ft_vector<ft_string> arguments;
    Pair<ft_string, ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> > *entry;
    ft_size_t count;
    ft_size_t index;
    int32_t initialize_error;
    int32_t result;

    if (tokens.size() < 2)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    entry = this->_callbacks.find(tokens[1]);
    if (entry == this->_callbacks.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    if (!entry->value)
    {
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = arguments.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(arguments.get_error());
        return (arguments.get_error());
    }
    count = tokens.size();
    index = 2;
    while (index < count)
    {
        initialize_error = arguments.push_back(tokens[index]);
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->set_error(initialize_error);
            return (initialize_error);
        }
        index += 1;
    }
    result = entry->value(context, arguments);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (result);
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::execute_line(game_script_context &context, const ft_string &line) noexcept
{
    ft_vector<ft_string> tokens;
    ft_string command;
    char *command_data;
    int32_t initialize_error;

    initialize_error = tokens.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(tokens.get_error());
        return (tokens.get_error());
    }

    this->tokenize_line(line, tokens);
    if (tokens.empty())
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    command = tokens[0];
    command_data = command.data();
    if (command_data)
        ft_to_lower(command_data);
    if (command == "call")
        return (this->handle_call(context, tokens));
    if (command == "set")
        return (this->handle_set(context, tokens));
    if (command == "unset")
        return (this->handle_unset(context, tokens));
    this->set_error(FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_INVALID_ARGUMENT);
}

int32_t game_script_bridge::custom_callback_dispatch(
    const scripting_call_context *call_context,
    const scripting_value *arguments, uint32_t argument_count,
    scripting_value *result, void *user_data) noexcept
{
    game_script_bridge *bridge;
    const char *native_name;
    ft_string callback_name;
    ft_vector<ft_string> callback_arguments;
    Pair<ft_string, ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> > *entry;
    uint32_t argument_index;
    int32_t error_code;
    char number_buffer[64];

    bridge = static_cast<game_script_bridge *>(user_data);
    if (bridge == ft_nullptr || call_context == ft_nullptr
        || call_context->engine == ft_nullptr || result == ft_nullptr
        || bridge->_custom_context == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (argument_count > 0U && arguments == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = call_context->engine->get_native_name(
        call_context->native_id, &native_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = callback_name.initialize(native_name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    entry = bridge->_callbacks.find(callback_name);
    if (entry == bridge->_callbacks.end() || !entry->value)
        return (FT_ERR_NOT_FOUND);
    error_code = callback_arguments.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    argument_index = 0U;
    while (argument_index < argument_count)
    {
        ft_string argument;

        if (arguments[argument_index].type == SCRIPTING_VALUE_INTEGER)
        {
            if (pf_snprintf(number_buffer, sizeof(number_buffer),
                FT_INT64_DECIMAL_FORMAT,
                arguments[argument_index].integer_value) < 0)
                return (FT_ERR_INVALID_ARGUMENT);
            error_code = argument.initialize(number_buffer);
        }
        else if (arguments[argument_index].type == SCRIPTING_VALUE_STRING)
        {
            error_code = argument.initialize();
            if (error_code == FT_ERR_SUCCESS)
                error_code = argument.assign(
                    arguments[argument_index].string_value,
                    arguments[argument_index].string_length);
        }
        else if (arguments[argument_index].type == SCRIPTING_VALUE_BOOLEAN)
        {
            if (arguments[argument_index].boolean_value == FT_FALSE)
                error_code = argument.initialize("0");
            else
                error_code = argument.initialize("1");
        }
        else
            return (FT_ERR_INVALID_ARGUMENT);
        if (error_code == FT_ERR_SUCCESS)
            error_code = callback_arguments.push_back(ft_move(argument));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        argument_index += 1U;
    }
    bridge->_custom_context->clear_result();
    error_code = entry->value(*bridge->_custom_context,
        callback_arguments);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (bridge->_custom_context->has_result_integer() == FT_TRUE)
        return (scripting_value_set_integer(result,
            bridge->_custom_context->get_result_integer()));
    return (scripting_value_set_null(result));
}

int32_t game_script_bridge::execute_custom_with_user_data(
    const ft_string &script, game_state *state, void *user_data) noexcept
{
    ft_bool lock_acquired;
    int32_t error_code;
    int32_t destroy_error;
    scripting_program program;
    scripting_value result;
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
    if (script.empty())
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(FT_ERR_SUCCESS));
    }
    error_code = context.initialize(state, this->_world);
    if (error_code != FT_ERR_SUCCESS)
    {
        this->unlock_internal(lock_acquired);
        return (this->set_error(error_code));
    }
    context.set_user_data(user_data);
    this->_custom_context = &context;
    error_code = this->_custom_engine.compile(script.c_str(), &program);
    if (error_code == FT_ERR_SUCCESS)
        error_code = this->_custom_engine.execute_program(program, &result);
    this->_custom_context = ft_nullptr;
    destroy_error = context.destroy();
    if (error_code == FT_ERR_SUCCESS)
        error_code = destroy_error;
    this->unlock_internal(lock_acquired);
    return (this->set_error(error_code));
}

int32_t game_script_bridge::execute(const ft_string &script, game_state &state) noexcept
{
    return (this->execute_with_user_data(script, &state, ft_nullptr));
}

int32_t game_script_bridge::execute_with_user_data(const ft_string &script,
    game_state *state, void *user_data) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    game_script_context context;
    int32_t context_init_error;
    const char *data;
    ft_size_t length;
    ft_size_t start;
    int32_t operations;

    if (this->_language == "custom"
        && game_script_uses_legacy_commands(script) == FT_FALSE)
        return (this->execute_custom_with_user_data(script, state, user_data));
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    context_init_error = context.initialize(state, this->_world);
    if (context_init_error != FT_ERR_SUCCESS)
    {
        this->set_error(context_init_error);
        this->unlock_internal(lock_acquired);
        return (context_init_error);
    }
    context.set_user_data(user_data);
    if (game_script_bridge::is_supported_language(this->_language) == FT_FALSE)
    {
        this->set_error(FT_ERR_CONFIGURATION);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_CONFIGURATION);
    }
    data = script.c_str();
    length = script.size();
    start = 0;
    operations = 0;
    while (start <= length)
    {
        ft_size_t index;
        ft_size_t count;
        ft_string line;
        const char *line_data;

        if (start >= length)
            break ;
        index = start;
        while (index < length
            && data[index] != '\n'
            && data[index] != '\r')
            index++;
        count = index - start;
        ft_string *line_substring;

        line_substring = script.substr(start, count);
        if (line_substring == ft_nullptr)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        if (line.initialize(*line_substring) != FT_ERR_SUCCESS)
        {
            this->set_error(line.get_error());
            game_script_delete_string(line_substring);
            this->unlock_internal(lock_acquired);
            return (line.get_error());
        }
        game_script_delete_string(line_substring);
        trim_whitespace(line);
        if (!line.empty())
        {
            line_data = line.c_str();
            if (!(line.size() >= 2 && line_data[0] == '-' && line_data[1] == '-')
                && !(line_data[0] == '#')
                && !(line_data[0] == ';'))
            {
                operations++;
                if (this->_max_operations > 0 && operations > this->_max_operations)
                {
                    this->set_error(FT_ERR_INVALID_OPERATION);
                    this->unlock_internal(lock_acquired);
                    return (FT_ERR_INVALID_OPERATION);
                }
                int32_t result = this->execute_line(context, line);
                if (result != FT_ERR_SUCCESS)
                {
                    this->set_error(result);
                    this->unlock_internal(lock_acquired);
                    return (result);
                }
            }
        }
        if (index >= length)
            break ;
        while (index < length
            && (data[index] == '\n' || data[index] == '\r'))
            index++;
        start = index;
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::check_sandbox_capabilities(const ft_string &script, ft_vector<ft_string> &violations) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const char *data;
    ft_size_t length;
    ft_size_t start;
    int32_t operations;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    violations.clear();
    data = script.c_str();
    length = script.size();
    start = 0;
    operations = 0;
    while (start <= length)
    {
        ft_size_t index;
        ft_size_t count;
        ft_string line;
        const char *line_data;

        if (start >= length)
            break ;
        index = start;
        while (index < length
            && data[index] != '\n'
            && data[index] != '\r')
            index++;
        count = index - start;
        ft_string *line_substring;

        line_substring = script.substr(start, count);
        if (line_substring == ft_nullptr)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        if (line.initialize(*line_substring) != FT_ERR_SUCCESS)
        {
            this->set_error(line.get_error());
            game_script_delete_string(line_substring);
            this->unlock_internal(lock_acquired);
            return (line.get_error());
        }
        game_script_delete_string(line_substring);
        trim_whitespace(line);
        if (!line.empty())
        {
            line_data = line.c_str();
            if (!(line.size() >= 2 && line_data[0] == '-' && line_data[1] == '-')
                && !(line_data[0] == '#')
                && !(line_data[0] == ';'))
            {
                ft_vector<ft_string> tokens;
                ft_string command_original;
                ft_string command_normalized;
                char *command_data;
                int32_t initialize_error;

                operations++;
                initialize_error = tokens.initialize();
                if (initialize_error != FT_ERR_SUCCESS)
                {
                    this->set_error(tokens.get_error());
                    this->unlock_internal(lock_acquired);
                    return (tokens.get_error());
                }
                this->tokenize_line(line, tokens);
                if (!tokens.empty())
                {
                    command_original = tokens[0];
                    command_normalized = command_original;
                    command_data = command_normalized.data();
                    if (command_data)
                        ft_to_lower(command_data);
                    if (!(command_normalized == "call"
                        || command_normalized == "set"
                        || command_normalized == "unset"))
                    {
                        ft_string violation;
                        if (violation.initialize("unsupported command: ") != FT_ERR_SUCCESS)
                        {
                            this->set_error(FT_ERR_NO_MEMORY);
                            return (FT_ERR_NO_MEMORY);
                        }
                        violation.append(command_original);
                        violations.push_back(ft_move(violation));
                    }
                }
            }
        }
        if (index >= length)
            break ;
        while (index < length
            && (data[index] == '\n' || data[index] == '\r'))
            index++;
        start = index;
    }
    if (this->_max_operations > 0 && operations > this->_max_operations)
    {
        ft_string violation;
        if (violation.initialize("operation budget exceeded: ") != FT_ERR_SUCCESS)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        ft_string operation_text;
        ft_string limit_text;
        {
            char operation_buffer[64];

            std::snprintf(operation_buffer, sizeof(operation_buffer), "%d", operations);
            operation_text = operation_buffer;
        }
        violation.append(operation_text);
        violation.append(" > ");
        {
            char limit_buffer[64];

            std::snprintf(limit_buffer, sizeof(limit_buffer), "%d", this->_max_operations);
            limit_text = limit_buffer;
        }
        violation.append(limit_text);
        violations.push_back(ft_move(violation));
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::validate_dry_run(const ft_string &script, ft_vector<ft_string> &warnings) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const char *data;
    ft_size_t length;
    ft_size_t start;
    int32_t operations;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    warnings.clear();
    data = script.c_str();
    length = script.size();
    start = 0;
    operations = 0;
    while (start <= length)
    {
        ft_size_t index;
        ft_size_t count;
        ft_string line;
        const char *line_data;

        if (start >= length)
            break ;
        index = start;
        while (index < length
            && data[index] != '\n'
            && data[index] != '\r')
            index++;
        count = index - start;
        ft_string *line_substring;

        line_substring = script.substr(start, count);
        if (line_substring == ft_nullptr)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        if (line.initialize(*line_substring) != FT_ERR_SUCCESS)
        {
            this->set_error(line.get_error());
            game_script_delete_string(line_substring);
            this->unlock_internal(lock_acquired);
            return (line.get_error());
        }
        game_script_delete_string(line_substring);
        trim_whitespace(line);
        if (!line.empty())
        {
            line_data = line.c_str();
            if (!(line.size() >= 2 && line_data[0] == '-' && line_data[1] == '-')
                && !(line_data[0] == '#')
                && !(line_data[0] == ';'))
            {
                ft_vector<ft_string> tokens;
                ft_string command_original;
                ft_string command_normalized;
                char *command_data;
                int32_t initialize_error;

                operations++;
                initialize_error = tokens.initialize();
                if (initialize_error != FT_ERR_SUCCESS)
                {
                    this->set_error(tokens.get_error());
                    this->unlock_internal(lock_acquired);
                    return (tokens.get_error());
                }
                this->tokenize_line(line, tokens);
                if (!tokens.empty())
                {
                    command_original = tokens[0];
                    command_normalized = command_original;
                    command_data = command_normalized.data();
                    if (command_data)
                        ft_to_lower(command_data);
                    if (command_normalized == "call")
                    {
                        if (tokens.size() < 2)
                        {
                            ft_string warning;
                            if (warning.initialize("call missing target") != FT_ERR_SUCCESS)
                                return (FT_ERR_NO_MEMORY);
                            warnings.push_back(ft_move(warning));
                        }
                        else
                        {
                            ft_string callback_name;
                            Pair<ft_string, ft_function<int32_t(game_script_context &, const ft_vector<ft_string> &)> > *entry;

                            callback_name = tokens[1];
                            entry = this->_callbacks.find(callback_name);
                            if (entry == this->_callbacks.end())
                            {
                                ft_string warning;
                                if (warning.initialize("unregistered callback: ") != FT_ERR_SUCCESS)
                                    return (FT_ERR_NO_MEMORY);
                                warning.append(callback_name);
                                warnings.push_back(ft_move(warning));
                            }
                            else if (!entry->value)
                            {
                                ft_string warning;
                                if (warning.initialize("callback missing target: ") != FT_ERR_SUCCESS)
                                    return (FT_ERR_NO_MEMORY);
                                warning.append(callback_name);
                                warnings.push_back(ft_move(warning));
                            }
                            else if (tokens.size() < 3)
                            {
                                ft_string warning;
                                if (warning.initialize("call missing arguments: ")
                                        != FT_ERR_SUCCESS)
                                    return (FT_ERR_NO_MEMORY);
                                warning.append(callback_name);
                                warnings.push_back(ft_move(warning));
                            }
                        }
                    }
                    else if (command_normalized == "set")
                    {
                        if (tokens.size() < 2)
                        {
                            ft_string warning;
                            if (warning.initialize("set missing key") != FT_ERR_SUCCESS)
                                return (FT_ERR_NO_MEMORY);
                            warnings.push_back(ft_move(warning));
                        }
                        else if (tokens.size() < 3)
                        {
                            ft_string warning;
                            if (warning.initialize("set missing value for key: ") != FT_ERR_SUCCESS)
                                return (FT_ERR_NO_MEMORY);
                            ft_string missing_key;
                            missing_key = tokens[1];
                            warning.append(missing_key);
                            warnings.push_back(ft_move(warning));
                        }
                    }
                    else if (command_normalized == "unset")
                    {
                        if (tokens.size() < 2)
                        {
                            ft_string warning;
                            if (warning.initialize("unset missing key") != FT_ERR_SUCCESS)
                                return (FT_ERR_NO_MEMORY);
                            warnings.push_back(ft_move(warning));
                        }
                    }
                }
            }
        }
        if (index >= length)
            break ;
        while (index < length
            && (data[index] == '\n' || data[index] == '\r'))
            index++;
        start = index;
    }
    if (this->_max_operations > 0 && operations > this->_max_operations)
    {
        ft_string warning;
        if (warning.initialize("operation budget exceeded: ") != FT_ERR_SUCCESS)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        ft_string operation_text;
        ft_string limit_text;
        {
            char operation_buffer[64];

            std::snprintf(operation_buffer, sizeof(operation_buffer), "%d", operations);
            operation_text = operation_buffer;
        }
        warning.append(operation_text);
        warning.append(" > ");
        {
            char limit_buffer[64];

            std::snprintf(limit_buffer, sizeof(limit_buffer), "%d", this->_max_operations);
            limit_text = limit_buffer;
        }
        warning.append(limit_text);
        warnings.push_back(ft_move(warning));
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::inspect_bytecode_budget(const ft_string &script, int32_t &required_operations) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    const char *data;
    ft_size_t length;
    ft_size_t start;
    int32_t operations;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    data = script.c_str();
    length = script.size();
    start = 0;
    operations = 0;
    while (start <= length)
    {
        ft_size_t index;
        ft_size_t count;
        ft_string line;
        const char *line_data;

        if (start >= length)
            break ;
        index = start;
        while (index < length
            && data[index] != '\n'
            && data[index] != '\r')
            index++;
        count = index - start;
        ft_string *line_substring;

        line_substring = script.substr(start, count);
        if (line_substring == ft_nullptr)
        {
            this->set_error(FT_ERR_NO_MEMORY);
            this->unlock_internal(lock_acquired);
            return (FT_ERR_NO_MEMORY);
        }
        if (line.initialize(*line_substring) != FT_ERR_SUCCESS)
        {
            this->set_error(line.get_error());
            game_script_delete_string(line_substring);
            this->unlock_internal(lock_acquired);
            return (line.get_error());
        }
        game_script_delete_string(line_substring);
        trim_whitespace(line);
        if (!line.empty())
        {
            line_data = line.c_str();
            if (!(line.size() >= 2 && line_data[0] == '-' && line_data[1] == '-')
                && !(line_data[0] == '#')
                && !(line_data[0] == ';'))
            {
                ft_vector<ft_string> tokens;
                ft_string command_original;
                ft_string command_normalized;
                char *command_data;
                int32_t initialize_error;

                initialize_error = tokens.initialize();
                if (initialize_error != FT_ERR_SUCCESS)
                {
                    this->set_error(tokens.get_error());
                    this->unlock_internal(lock_acquired);
                    return (tokens.get_error());
                }
                this->tokenize_line(line, tokens);
                if (!tokens.empty())
                {
                    command_original = tokens[0];
                    command_normalized = command_original;
                    command_data = command_normalized.data();
                    if (command_data)
                        ft_to_lower(command_data);
                    if (command_normalized == "call"
                        || command_normalized == "set"
                        || command_normalized == "unset")
                    {
                        operations++;
                    }
                    else
                    {
                        required_operations = operations;
                        this->set_error(FT_ERR_INVALID_ARGUMENT);
                        this->unlock_internal(lock_acquired);
                        return (FT_ERR_INVALID_ARGUMENT);
                    }
                }
            }
        }
        if (index >= length)
            break ;
        while (index < length
            && (data[index] == '\n' || data[index] == '\r'))
            index++;
        start = index;
    }
    required_operations = operations;
    if (this->_max_operations > 0 && operations > this->_max_operations)
    {
        this->set_error(FT_ERR_INVALID_OPERATION);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_INVALID_OPERATION);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
    {
        this->set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_script_bridge::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_script_bridge::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_script_bridge::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_script_bridge::get_error");
    return (game_script_bridge::_last_error);
}

const char *game_script_bridge::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_script_bridge::get_error_str");
    return (ft_strerror(game_script_bridge::_last_error));
}
