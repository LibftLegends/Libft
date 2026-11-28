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
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_behavior_profile::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t result = this->_actions.initialize();
    if (result != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->_profile_id = 0;
    this->_aggression_weight = 0.0;
    this->_caution_weight = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::initialize(const game_behavior_profile &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_profile::initialize(copy)",
            "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);

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
    result = this->initialize();
    if (result == FT_ERR_SUCCESS)
        result = this->replace_contents_from(other);
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::replace_actions_from(
    const ft_vector<game_behavior_action> &actions) noexcept
{
    if (actions.is_initialised() == FT_CLASS_STATE_INITIALISED)
        return (this->_actions.replace_from(actions));
    this->_actions.clear();
    return (this->_actions.get_error());
}

int32_t game_behavior_profile::replace_contents_from(
    const game_behavior_profile &other, ft_bool reuse_action_storage) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);

    const game_behavior_profile *first = this;
    const game_behavior_profile *second = &other;
    if (reinterpret_cast<uintptr_t>(first) > reinterpret_cast<uintptr_t>(second))
    {
        first = &other;
        second = this;
    }
    ft_bool first_locked = FT_FALSE;
    ft_bool second_locked = FT_FALSE;
    int32_t result = first->lock_internal(&first_locked);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = second->lock_internal(&second_locked);
    if (result == FT_ERR_SUCCESS && reuse_action_storage)
    {
        this->_actions.clear();
        result = this->_actions.get_error();
    }
    if (result == FT_ERR_SUCCESS)
        result = this->replace_actions_from(other._actions);
    if (result == FT_ERR_SUCCESS)
    {
        this->_profile_id = other._profile_id;
        this->_aggression_weight = other._aggression_weight;
        this->_caution_weight = other._caution_weight;
    }
    (void)second->unlock_internal(second_locked);
    (void)first->unlock_internal(first_locked);
    return (result);
}

int32_t game_behavior_profile::initialize(game_behavior_profile &&other) noexcept
{
    return (this->move(other));
}

int32_t game_behavior_profile::move(game_behavior_profile &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_behavior_profile::move",
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
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    if (other._actions.is_initialised() == FT_CLASS_STATE_INITIALISED)
        result = this->_actions.move(other._actions);
    else
        result = this->_actions.initialize();
    if (result == FT_ERR_SUCCESS)
    {
        this->_profile_id = other._profile_id;
        this->_aggression_weight = other._aggression_weight;
        this->_caution_weight = other._caution_weight;
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    (void)other.unlock_internal(source_locked);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));

    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::initialize(int32_t profile_id, double aggression_weight,
    double caution_weight,
    const ft_vector<game_behavior_action> &actions) noexcept
{
    int32_t initialize_error;
    int32_t copy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    copy_error = this->replace_actions_from(actions);
    if (copy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(copy_error));
    }
    this->_profile_id = profile_id;
    this->_aggression_weight = aggression_weight;
    this->_caution_weight = caution_weight;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_profile::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    this->_profile_id = 0;
    this->_aggression_weight = 0.0;
    this->_caution_weight = 0.0;
    const int32_t actions_error = this->_actions.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(actions_error));
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

template <typename Value>
Value game_behavior_profile::read_scalar(const Value &value) const noexcept
{
    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (value);
    }
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (Value{});
    const Value snapshot = value;
    (void)this->unlock_internal(lock_acquired);
    return (snapshot);
}

int32_t game_behavior_profile::get_profile_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_behavior_profile::get_profile_id");
    return (this->read_scalar(this->_profile_id));
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
    return (this->read_scalar(this->_aggression_weight));
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
    return (this->read_scalar(this->_caution_weight));
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
    const int32_t result = this->replace_actions_from(actions);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
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
