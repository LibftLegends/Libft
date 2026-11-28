#include "../PThread/pthread_internal.hpp"
#include "game_state.hpp"
#include "game_hooks.hpp"
#include "../Errno/errno.hpp"
#include "../Template/move.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
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
#include "game_upgrade.hpp"
#include "game_vendor_profile.hpp"
#include "game_world_region.hpp"
#include "game_world_registry.hpp"
#include "game_world_replay.hpp"

thread_local int32_t game_state::_last_error = FT_ERR_SUCCESS;

game_state::game_state() noexcept
    : _worlds(), _characters(), _variables(), _hooks(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_state::~game_state() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_state::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_state::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    int32_t result = this->_worlds.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_characters.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_variables.initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->_hooks.initialize();
    if (result == FT_ERR_SUCCESS)
    {
        auto *world = new (std::nothrow) game_world();
        if (world == ft_nullptr)
            result = FT_ERR_NO_MEMORY;
        else
        {
            ft_sharedptr<game_world> owner;
            result = owner.initialize(world);
            if (result != FT_ERR_SUCCESS)
                delete world;
            else
            {
                result = world->initialize();
                if (result == FT_ERR_SUCCESS)
                    result = this->_worlds.push_back(ft_move(owner));
            }
        }
    }
    if (result != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (this->set_error(result));
}


template <typename ElementType>
static int32_t game_state_stage_vector(ft_vector<ElementType> &destination,
    const ft_vector<ElementType> &source, ft_bool &transfer_storage) noexcept
{
    const int32_t initialize_error = destination.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    const ft_size_t source_capacity = source.capacity();
    const int32_t capacity_error = source.get_error();
    if (capacity_error != FT_ERR_SUCCESS)
        return (capacity_error);
    transfer_storage = source_capacity > destination.capacity();
    if (transfer_storage)
        return (FT_ERR_SUCCESS);
    return (destination.replace_from(source));
}

int32_t game_state::move(game_state &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_state::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->has_active_lock())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    if (other.has_active_lock())
        return (this->set_error(FT_ERR_THREAD_BUSY));

    // Copy inline handles before any storage changes owners. Copying a handle
    // can fail when it has an independent mutex.
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_bool transfer_worlds = FT_FALSE;
    ft_bool transfer_characters = FT_FALSE;
    result = game_state_stage_vector(this->_worlds, other._worlds, transfer_worlds);
    if (result == FT_ERR_SUCCESS)
        result = game_state_stage_vector(this->_characters, other._characters, transfer_characters);
    if (result == FT_ERR_SUCCESS)
        result = this->_hooks.initialize(other._hooks);
    if (result == FT_ERR_SUCCESS)
        result = this->transfer_storage(other, transfer_worlds, transfer_characters);
    if (result == FT_ERR_SUCCESS)
        result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_state::transfer_storage(game_state &other,
    ft_bool transfer_worlds, ft_bool transfer_characters) noexcept
{
    if (transfer_worlds)
    {
        const int32_t result = this->_worlds.move(other._worlds);
        if (result != FT_ERR_SUCCESS)
            return (result);
    }
    if (transfer_characters)
    {
        const int32_t result = this->_characters.move(other._characters);
        if (result != FT_ERR_SUCCESS)
        {
            if (transfer_worlds)
                (void)other._worlds.move(this->_worlds);
            return (result);
        }
    }
    const int32_t result = this->_variables.move(other._variables);
    if (result != FT_ERR_SUCCESS)
    {
        if (transfer_characters)
            (void)other._characters.move(this->_characters);
        if (transfer_worlds)
            (void)other._worlds.move(this->_worlds);
    }
    return (result);
}

ft_bool game_state::has_active_lock() const noexcept
{
    return ((this->_mutex != ft_nullptr && this->_mutex->lockState())
        || this->_worlds.has_active_lock() || this->_characters.has_active_lock()
        || this->_variables.has_active_lock());
}


int32_t game_state::destroy() noexcept
{
    int32_t worlds_error;
    int32_t characters_error;
    int32_t variables_error;
    int32_t hooks_error;
    int32_t disable_error;
    int32_t final_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    worlds_error = this->_worlds.destroy();
    characters_error = this->_characters.destroy();
    variables_error = this->_variables.destroy();
    hooks_error = this->_hooks.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    final_error = disable_error;
    if (final_error == FT_ERR_SUCCESS && worlds_error != FT_ERR_SUCCESS)
        final_error = worlds_error;
    if (final_error == FT_ERR_SUCCESS && characters_error != FT_ERR_SUCCESS)
        final_error = characters_error;
    if (final_error == FT_ERR_SUCCESS && variables_error != FT_ERR_SUCCESS)
        final_error = variables_error;
    if (final_error == FT_ERR_SUCCESS && hooks_error != FT_ERR_SUCCESS)
        final_error = hooks_error;
    this->set_error(final_error);
    return (final_error);
}

int32_t game_state::lock_internal(ft_bool *lock_acquired) const noexcept
{
    const int32_t result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = result == FT_ERR_SUCCESS && this->_mutex != ft_nullptr;
    return (result);
}

void game_state::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

ft_vector<ft_sharedptr<game_world> > &game_state::get_worlds() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::get_worlds");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_worlds);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_worlds);
}

ft_vector<ft_sharedptr<game_character> > &game_state::get_characters() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::get_characters");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_characters);
    }
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (this->_characters);
}

void game_state::set_variable(const ft_string &key, const ft_string &value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::set_variable");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        ft_bool found = FT_FALSE;
        result = this->_variables.visit_value(key, [&value, &found](ft_string &stored) noexcept
        {
            found = FT_TRUE;
            if (stored.is_initialised() && value.is_initialised())
                return (stored.assign(value));
            stored = value;
            return (stored.get_error());
        });
        if (!found && result == FT_ERR_NOT_FOUND)
        {
            this->_variables.insert(key, value);
            result = this->_variables.get_error();
        }
        this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

const ft_string *game_state::get_variable(const ft_string &key) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::get_variable");
    const ft_string *value = ft_nullptr;
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_variables.visit_value(key, [&value](const ft_string &stored) noexcept
        {
            value = &stored;
            return (FT_ERR_SUCCESS);
        });
        this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return (value);
}

void game_state::remove_variable(const ft_string &key) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::remove_variable");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_variables.erase_value(key, [](const ft_string &) noexcept
        {
            return (FT_ERR_SUCCESS);
        }, [](Pair<ft_string, ft_string> &destination, Pair<ft_string, ft_string> &source) noexcept
        {
            // Stored strings have no independent mutex. Relocation transfers buffers.
            (void)destination.key.move(source.key);
            if (source.value.is_initialised())
                (void)destination.value.move(source.value);
            else
                (void)destination.value.initialize(ft_move(source.value));
            return ;
        });
        if (result == FT_ERR_NOT_FOUND)
            result = FT_ERR_SUCCESS;
        this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


void game_state::clear_variables() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::clear_variables");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_variables.clear();
    this->set_error(this->_variables.get_error());
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_state::add_character(const ft_sharedptr<game_character> &character) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::add_character");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        if (character)
            result = this->_characters.push_back(character);
        else
            result = FT_ERR_GAME_GENERAL_ERROR;
        this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

void game_state::remove_character(ft_size_t index) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::remove_character");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_characters.erase_at(index);
        if (result == FT_ERR_OUT_OF_RANGE)
            result = FT_ERR_GAME_GENERAL_ERROR;
        this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


void game_state::set_hooks(const ft_sharedptr<game_hooks> &hooks) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::set_hooks");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_hooks = hooks;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

static void game_state_delete_hooks_handle(ft_sharedptr<game_hooks> *hooks) noexcept
{
    if (hooks == ft_nullptr)
        return ;
    (void)hooks->destroy();
    delete hooks;
    return ;
}

ft_sharedptr<game_hooks> *game_state::get_hooks() const noexcept
{
    ft_sharedptr<game_hooks> *hooks_copy;
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t hooks_initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::get_hooks");
    hooks_copy = new (std::nothrow) ft_sharedptr<game_hooks>();
    if (hooks_copy == ft_nullptr)
    {
        const_cast<game_state *>(this)->set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    hooks_initialize_error = hooks_copy->initialize();
    if (hooks_initialize_error != FT_ERR_SUCCESS)
    {
        const_cast<game_state *>(this)->set_error(hooks_initialize_error);
        delete hooks_copy;
        return (ft_nullptr);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_state *>(this)->set_error(lock_error);
        game_state_delete_hooks_handle(hooks_copy);
        return (ft_nullptr);
    }
    *hooks_copy = this->_hooks;
    if (hooks_copy->get_error() != FT_ERR_SUCCESS)
    {
        const_cast<game_state *>(this)->set_error(hooks_copy->get_error());
        this->unlock_internal(lock_acquired);
        game_state_delete_hooks_handle(hooks_copy);
        return (ft_nullptr);
    }
    const_cast<game_state *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (hooks_copy);
}

template <typename State, typename Visit>
void game_state::visit_hooks(State &state, Visit visit) noexcept
{
    ft_bool lock_acquired;
    const int32_t lock_error = state.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        state.set_error(lock_error);
        return ;
    }
    ft_bool visited = FT_FALSE;
    const int32_t result = state._hooks.visit_retained([&](auto &hooks) noexcept
    {
        visited = FT_TRUE;
        state.unlock_internal(lock_acquired);
        state.set_error(FT_ERR_SUCCESS);
        visit(hooks);
        return ;
    });
    if (!visited)
    {
        state.unlock_internal(lock_acquired);
        if (result == FT_ERR_INVALID_POINTER)
            state.set_error(FT_ERR_SUCCESS);
        else
            state.set_error(result);
    }
    return ;
}

void game_state::reset_hooks() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::reset_hooks");
    this->visit_hooks(*this, [](game_hooks &hooks) noexcept
    {
        hooks.reset();
        return ;
    });
    return ;
}

void game_state::dispatch_item_crafted(game_character &character, game_item &item) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::dispatch_item_crafted");
    this->visit_hooks(*this, [&character, &item](const game_hooks &hooks) noexcept
    {
        hooks.invoke_on_item_crafted(character, item);
        return ;
    });
    return ;
}

void game_state::dispatch_character_damaged(game_character &character, int32_t damage, uint8_t type) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::dispatch_character_damaged");
    this->visit_hooks(*this, [&character, damage, type](const game_hooks &hooks) noexcept
    {
        hooks.invoke_on_character_damaged(character, damage, type);
        return ;
    });
    return ;
}

void game_state::dispatch_event_triggered(game_world &world, game_event &event) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::dispatch_event_triggered");
    this->visit_hooks(*this, [&world, &event](const game_hooks &hooks) noexcept
    {
        hooks.invoke_on_event_triggered(world, event);
        return ;
    });
    return ;
}


int32_t game_state::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_state::get_error");
    return (game_state::_last_error);
}

const char *game_state::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_state::get_error_str");
    return (ft_strerror(game_state::_last_error));
}

int32_t game_state::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::enable_thread_safety");
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

int32_t game_state::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_state::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_state::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

int32_t game_state::set_error(int32_t error_code) noexcept
{
    game_state::_last_error = error_code;
    return (error_code);
}
