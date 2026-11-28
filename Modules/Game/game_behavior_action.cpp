#include "../PThread/pthread_internal.hpp"
#include "game_behavior_action.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_behavior_action::_last_error = FT_ERR_SUCCESS;
game_behavior_action::game_behavior_action() noexcept
    : _action_id(0), _weight(0.0), _cooldown_seconds(0.0),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED), _mutex(ft_nullptr)
{
    return ;
}

int32_t game_behavior_action::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_behavior_action::initialize",
            "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_action_id = 0;
    this->_weight = 0.0;
    this->_cooldown_seconds = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_behavior_action::initialize(int32_t action_id, double weight,
    double cooldown_seconds) noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state,
            "game_behavior_action::initialize",
            "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_action_id = action_id;
    this->_weight = weight;
    this->_cooldown_seconds = cooldown_seconds;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

game_behavior_action::~game_behavior_action() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t game_behavior_action::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void game_behavior_action::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_behavior_action::initialize(const game_behavior_action &other) noexcept
{
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_behavior_action::initialize(copy)", "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        const int32_t source_error = other.get_error();
        const int32_t result = this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        if (result != FT_ERR_SUCCESS)
            return (result);
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    if (this->_mutex != ft_nullptr || other._mutex != ft_nullptr)
        return (this->copy_locked(other));
    this->copy_values_from(other);
    return (this->set_error(FT_ERR_SUCCESS));
}

void game_behavior_action::copy_values_from(const game_behavior_action &other) noexcept
{
    this->_action_id = other._action_id;
    this->_weight = other._weight;
    this->_cooldown_seconds = other._cooldown_seconds;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return ;
}

int32_t game_behavior_action::copy_locked(const game_behavior_action &other) noexcept
{
    const game_behavior_action *first = &other;
    const game_behavior_action *second = this;
    if (reinterpret_cast<uintptr_t>(this) < reinterpret_cast<uintptr_t>(&other))
    {
        first = this;
        second = &other;
    }
    ft_bool first_locked = FT_FALSE;
    ft_bool second_locked = FT_FALSE;
    int32_t result = first->lock_internal(&first_locked);
    if (result == FT_ERR_SUCCESS)
        result = second->lock_internal(&second_locked);
    if (result == FT_ERR_SUCCESS)
        this->copy_values_from(other);
    else if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    second->unlock_internal(second_locked);
    first->unlock_internal(first_locked);
    return (this->set_error(result));
}

int32_t game_behavior_action::initialize(game_behavior_action &&other) noexcept
{
    return (this->move(other));
}

int32_t game_behavior_action::move(game_behavior_action &other) noexcept
{
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "game_behavior_action::move", "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = other.get_error();
    const int32_t destroy_error = this->destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    const int32_t lock_error = other.lock_internal(&source_locked);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->_action_id = other._action_id;
    this->_weight = other._weight;
    this->_cooldown_seconds = other._cooldown_seconds;
    other._action_id = 0;
    other._weight = 0.0;
    other._cooldown_seconds = 0.0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(source_locked);
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_behavior_action::destroy() noexcept
{
    const int32_t disable_error = this->disable_thread_safety();
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(disable_error));
    this->_action_id = 0;
    this->_weight = 0.0;
    this->_cooldown_seconds = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(disable_error));
}

int32_t game_behavior_action::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (this->_mutex != ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (this->set_error(FT_ERR_NO_MEMORY));
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (this->set_error(initialize_error));
    }
    this->_mutex = mutex_pointer;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_behavior_action::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (this->set_error(destroy_error));
}

ft_bool game_behavior_action::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_behavior_action::get_action_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t action_id;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_behavior_action *>(this)->set_error(lock_error);
        return (0);
    }
    action_id = this->_action_id;
    const_cast<game_behavior_action *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (action_id);
}

void game_behavior_action::set_action_id(int32_t action_id) noexcept
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
    this->_action_id = action_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

double game_behavior_action::get_weight() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    double weight;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_behavior_action *>(this)->set_error(lock_error);
        return (0.0);
    }
    weight = this->_weight;
    const_cast<game_behavior_action *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (weight);
}

void game_behavior_action::set_weight(double weight) noexcept
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
    this->_weight = weight;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

double game_behavior_action::get_cooldown_seconds() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    double cooldown_seconds;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        const_cast<game_behavior_action *>(this)->set_error(lock_error);
        return (0.0);
    }
    cooldown_seconds = this->_cooldown_seconds;
    const_cast<game_behavior_action *>(this)->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (cooldown_seconds);
}

void game_behavior_action::set_cooldown_seconds(double cooldown_seconds) noexcept
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
    this->_cooldown_seconds = cooldown_seconds;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_behavior_action::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_action::get_error");
    return (game_behavior_action::_last_error);
}

const char *game_behavior_action::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_behavior_action::get_error_str");
    return (ft_strerror(game_behavior_action::_last_error));
}

int32_t game_behavior_action::set_error(int32_t error_code) noexcept
{
    game_behavior_action::_last_error = error_code;
    return (error_code);
}
