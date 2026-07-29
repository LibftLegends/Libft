#include "../PThread/pthread_internal.hpp"
#include "game_behavior_table.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"
#include "game_behavior_action.hpp"
#include "game_behavior_profile.hpp"

thread_local int32_t game_behavior_table::_last_error = FT_ERR_SUCCESS;

game_behavior_table::game_behavior_table() noexcept
    : _profiles(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_behavior_table::~game_behavior_table() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_behavior_table::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_behavior_table::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t initialize_error = this->_profiles.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::initialize(const game_behavior_table &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    ft_size_t count;
    ft_size_t index;
    const Pair<int32_t, game_behavior_profile> *entry;
    const Pair<int32_t, game_behavior_profile> *entry_end;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_table::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    count = other._profiles.size();
    entry_end = other._profiles.end();
    entry = entry_end - count;
    index = 0;
    while (index < count)
    {
        this->_profiles.insert(entry->key, entry->value);
        entry++;
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::initialize(game_behavior_table &&other) noexcept
{
    return (this->move(other));
}

int32_t game_behavior_table::move(game_behavior_table &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_table::move",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_behavior_table &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    this->_profiles.clear();
    (void)this->_profiles.destroy();
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_behavior_table::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::enable_thread_safety");
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

int32_t game_behavior_table::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_behavior_table::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_behavior_table::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_behavior_table::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::lock");
    int32_t lock_error = this->lock_internal(lock_acquired);
    this->set_error(lock_error);
    return (lock_error);
}

void game_behavior_table::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_map<int32_t, game_behavior_profile> &game_behavior_table::get_profiles() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::get_profiles");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_profiles);
}

const ft_map<int32_t, game_behavior_profile> &game_behavior_table::get_profiles() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::get_profiles const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_profiles);
}

void game_behavior_table::set_profiles(
    const ft_map<int32_t, game_behavior_profile> &profiles) noexcept
{
    ft_bool lock_acquired;
    ft_size_t count;
    ft_size_t index;
    const Pair<int32_t, game_behavior_profile> *entry;
    const Pair<int32_t, game_behavior_profile> *entry_end;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::set_profiles");
    int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_profiles.clear();
    count = profiles.size();
    entry_end = profiles.end();
    entry = entry_end - count;
    index = 0;
    while (index < count)
    {
        this->_profiles.insert(entry->key, entry->value);
        entry++;
        index += 1;
    }
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_behavior_table::register_profile(const game_behavior_profile &profile) noexcept
{
    ft_bool lock_acquired;
    int32_t profile_identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::register_profile");
    int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    profile_identifier = profile.get_profile_id();
    this->_profiles.insert(profile_identifier, profile);
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::fetch_profile(int32_t profile_id,
    game_behavior_profile &profile) const noexcept
{
    const Pair<int32_t, game_behavior_profile> *entry;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::fetch_profile");
    entry = this->_profiles.find(profile_id);
    if (entry == this->_profiles.end())
    {
        this->set_error(FT_ERR_NOT_FOUND);
        return (FT_ERR_NOT_FOUND);
    }
    int32_t initialize_error = profile.initialize(entry->value);
    this->set_error(initialize_error);
    return (initialize_error);
}


int32_t game_behavior_table::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_table::get_error");
    return (game_behavior_table::_last_error);
}

const char *game_behavior_table::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_table::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_behavior_table::set_error(int32_t error_code) noexcept
{
    game_behavior_table::_last_error = error_code;
    return (error_code);
}
