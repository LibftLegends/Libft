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
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(initialize_error));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::initialize(const game_behavior_table &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_table::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));

    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_profiles.copy_from(other._profiles);
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::initialize(game_behavior_table &&other) noexcept
{
    return (this->move(other));
}

int32_t game_behavior_table::move(game_behavior_table &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_table::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_profiles.move(other._profiles);
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_profiles.destroy();
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_table::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    const int32_t map_error = this->_profiles.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(map_error));
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
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::set_profiles");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_profiles.copy_from(profiles);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_behavior_table::register_profile(const game_behavior_profile &profile) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::register_profile");
    errno_abort_if_uninitialised_or_destroyed(profile._initialised_state,
        "game_behavior_table::register_profile source");
    if (profile.is_thread_safe())
    {
        game_behavior_profile snapshot;
        const int32_t result = snapshot.initialize(profile);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        return (this->register_profile_snapshot(snapshot));
    }
    return (this->register_profile_snapshot(profile));
}

int32_t game_behavior_table::register_profile_snapshot(
    const game_behavior_profile &profile) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        ft_bool profiles_locked = FT_FALSE;
        result = this->_profiles.lock(&profiles_locked);
        if (result == FT_ERR_SUCCESS)
        {
            const int32_t profile_id = profile.get_profile_id();
            ft_bool found = FT_FALSE;
            result = this->_profiles.visit_value(profile_id,
                [&profile, &found](game_behavior_profile &stored_profile) noexcept
                {
                    found = FT_TRUE;
                    if (stored_profile._initialised_state != FT_CLASS_STATE_INITIALISED)
                        return (stored_profile.initialize(profile));
                    return (stored_profile.replace_contents_from(profile));
                });
            if (!found && result == FT_ERR_NOT_FOUND)
            {
                this->_profiles.insert(profile_id, profile);
                result = this->_profiles.get_error();
            }
            (void)this->_profiles.unlock(profiles_locked);
        }
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_behavior_table::fetch_profile(int32_t profile_id,
    game_behavior_profile &profile) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_table::fetch_profile");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_profiles.visit_value(profile_id,
            [&profile](const game_behavior_profile &stored_profile) noexcept
            {
                if (profile._initialised_state != FT_CLASS_STATE_INITIALISED
                    || stored_profile._initialised_state != FT_CLASS_STATE_INITIALISED)
                    return (profile.initialize(stored_profile));
                const int32_t copy_result = profile.replace_contents_from(stored_profile, FT_TRUE);
                if (copy_result != FT_ERR_SUCCESS)
                    (void)profile.destroy();
                return (profile.set_error(copy_result));
            });
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
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
