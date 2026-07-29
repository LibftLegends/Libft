#include "../PThread/pthread_internal.hpp"
#include "game_behavior_profile.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "game_behavior_action.hpp"

thread_local int32_t game_behavior_profile::_last_error = FT_ERR_SUCCESS;
static int32_t game_behavior_copy_action_vector(
    const ft_vector<game_behavior_action> &source,
    ft_vector<game_behavior_action> &destination)
{
    const game_behavior_action *entry;
    const game_behavior_action *entry_end;

    destination.clear();
    entry = source.begin();
    entry_end = source.end();
    while (entry != entry_end)
    {
        destination.push_back(*entry);
        if (destination.get_error() != FT_ERR_SUCCESS)
            return (destination.get_error());
        ++entry;
    }
    return (FT_ERR_SUCCESS);
}

game_behavior_profile::game_behavior_profile() noexcept
    : _profile_id(0), _aggression_weight(0.0), _caution_weight(0.0),
      _actions(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_behavior_profile::~game_behavior_profile() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_behavior_profile::initialize() noexcept
{
    int32_t actions_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_behavior_profile::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_actions.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        actions_initialize_error = this->_actions.initialize();
        if (actions_initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            this->set_error(actions_initialize_error);
            return (actions_initialize_error);
        }
    }
    else
        this->_actions.clear();
    this->_profile_id = 0;
    this->_aggression_weight = 0.0;
    this->_caution_weight = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::initialize(const game_behavior_profile &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    int32_t copy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_profile::initialize(copy)",
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
    this->_profile_id = other._profile_id;
    this->_aggression_weight = other._aggression_weight;
    this->_caution_weight = other._caution_weight;
    copy_error = game_behavior_copy_action_vector(other._actions, this->_actions);
    if (copy_error != FT_ERR_SUCCESS)
        return (copy_error);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::initialize(game_behavior_profile &&other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    int32_t source_error;

    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_profile::initialize(move)",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
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
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        source_error = other.get_error();
        this->set_error(static_cast<uint32_t>(source_error));
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize(static_cast<const game_behavior_profile &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(initialize_error));
        return (initialize_error);
    }
    (void)other.destroy();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::move(game_behavior_profile &other) noexcept
{
    return (this->initialize(static_cast<game_behavior_profile &&>(other)));
}

int32_t game_behavior_profile::initialize(int32_t profile_id, double aggression_weight,
    double caution_weight,
    const ft_vector<game_behavior_action> &actions) noexcept
{
    int32_t initialize_error;
    int32_t copy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_profile_id = profile_id;
    this->_aggression_weight = aggression_weight;
    this->_caution_weight = caution_weight;
    copy_error = game_behavior_copy_action_vector(actions, this->_actions);
    if (copy_error != FT_ERR_SUCCESS)
        return (copy_error);
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    this->_profile_id = 0;
    this->_aggression_weight = 0.0;
    this->_caution_weight = 0.0;
    this->_actions.clear();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_behavior_profile::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::enable_thread_safety");
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

int32_t game_behavior_profile::disable_thread_safety() noexcept
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

ft_bool game_behavior_profile::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_behavior_profile::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_behavior_profile::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::lock");
    int32_t lock_error = this->lock_internal(lock_acquired);
    this->set_error(lock_error);
    return (lock_error);
}

void game_behavior_profile::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::unlock");

    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_behavior_profile::get_profile_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_profile_id");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_profile_id);
}

void game_behavior_profile::set_profile_id(int32_t profile_id) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::set_profile_id");
    int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_profile_id = profile_id;
    this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

double game_behavior_profile::get_aggression_weight() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_aggression_weight");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_aggression_weight);
}

void game_behavior_profile::set_aggression_weight(double aggression_weight) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::set_aggression_weight");
    int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_aggression_weight = aggression_weight;
    this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

double game_behavior_profile::get_caution_weight() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_caution_weight");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_caution_weight);
}

void game_behavior_profile::set_caution_weight(double caution_weight) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::set_caution_weight");
    int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_caution_weight = caution_weight;
    this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_vector<game_behavior_action> &game_behavior_profile::get_actions() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_actions");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_actions);
}

const ft_vector<game_behavior_action> &game_behavior_profile::get_actions() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_actions const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_actions);
}

void game_behavior_profile::set_actions(
    const ft_vector<game_behavior_action> &actions) noexcept
{
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::set_actions");
    int32_t lock_error;

    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    game_behavior_copy_action_vector(actions, this->_actions);
    this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}


int32_t game_behavior_profile::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_profile::get_error");
    return (game_behavior_profile::_last_error);
}

const char *game_behavior_profile::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_profile::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_behavior_profile::set_error(int32_t error_code) noexcept
{
    game_behavior_profile::_last_error = error_code;
    return (error_code);
}
