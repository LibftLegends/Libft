#include "../PThread/pthread_internal.hpp"
#include "game_achievement.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"

thread_local int32_t game_goal::_last_error = FT_ERR_SUCCESS;
thread_local int32_t game_achievement::_last_error = FT_ERR_SUCCESS;

int32_t game_goal::set_error(int32_t error_code) noexcept
{
    game_goal::_last_error = error_code;
    return (error_code);
}

int32_t game_goal::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_goal::get_error");
    return (game_goal::_last_error);
}

const char *game_goal::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_goal::get_error_str");
    return (ft_strerror(this->get_error()));
}

int32_t game_achievement::set_error(int32_t error_code) noexcept
{
    game_achievement::_last_error = error_code;
    return (error_code);
}

int32_t game_achievement::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_achievement::get_error");
    return (game_achievement::_last_error);
}

const char *game_achievement::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_achievement::get_error_str");
    return (ft_strerror(this->get_error()));
}

game_goal::game_goal() noexcept
    : _target(0), _progress(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_goal::~game_goal() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_goal::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_goal::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_target = 0;
    this->_progress = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_goal::initialize(const game_goal &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_goal::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
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
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_target = other._target;
    this->_progress = other._progress;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_goal::initialize(game_goal &&other) noexcept
{
    int32_t destroy_error;
    int32_t source_error;

    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_goal::initialize(move)",
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
    this->_target = other._target;
    this->_progress = other._progress;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._target = 0;
    other._progress = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_goal::move(game_goal &other) noexcept
{
    return (this->initialize(static_cast<game_goal &&>(other)));
}

int32_t game_goal::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_target = 0;
    this->_progress = 0;
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_goal::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
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

int32_t game_goal::disable_thread_safety() noexcept
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

ft_bool game_goal::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_goal::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_goal::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_goal::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_goal::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_goal::get_target() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t target_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::get_target");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    target_value = this->_target;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (target_value);
}

void game_goal::set_target(int32_t target) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::set_target");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_target = target;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_goal::get_progress() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t progress_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::get_progress");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    progress_value = this->_progress;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (progress_value);
}

void game_goal::set_progress(int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::set_progress");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_progress = value;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_goal::add_progress(int32_t delta) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_goal::add_progress");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_progress += delta;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}


int32_t game_goal::read_completion(ft_bool &completed) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    completed = this->_progress >= this->_target;
    return (this->unlock_internal(lock_acquired));
}

game_achievement::game_achievement() noexcept
    : _id(0), _goals(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_achievement::~game_achievement() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_achievement::initialize() noexcept
{
    int32_t goals_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_achievement::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_id = 0;
    goals_initialize_error = this->_goals.initialize();
    if (goals_initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(goals_initialize_error);
        return (goals_initialize_error);
    }
    this->_goals.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_achievement::initialize(const game_achievement &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;
    const Pair<int32_t, game_goal> *goal_entry;
    const Pair<int32_t, game_goal> *goals_end;
    game_goal                   new_goal;
    ft_size_t                    goals_count;
    ft_size_t                    index;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_achievement::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
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
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_id = other._id;
    goals_count = other._goals.size();
    goals_end = other._goals.end();
    goal_entry = goals_end - goals_count;
    index = 0;
    while (index < goals_count)
    {
        if (new_goal.initialize() != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            this->set_error(FT_ERR_INVALID_STATE);
            return (FT_ERR_INVALID_STATE);
        }
        new_goal.set_target(goal_entry->value.get_target());
        new_goal.set_progress(goal_entry->value.get_progress());
        this->_goals.insert(goal_entry->key, new_goal);
        (void)new_goal.destroy();
        goal_entry++;
        index += 1;
    }
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_achievement::initialize(game_achievement &&other) noexcept
{
    int32_t initialize_error;
    int32_t source_error;
    int32_t destroy_error;

    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_achievement::initialize(move)",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        {
            destroy_error = this->destroy();
            if (destroy_error != FT_ERR_SUCCESS)
            {
                this->set_error(destroy_error);
                return (destroy_error);
            }
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        source_error = other.get_error();
        this->set_error(static_cast<uint32_t>(source_error));
        return (FT_ERR_SUCCESS);
    }
    initialize_error = this->initialize(static_cast<const game_achievement &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    (void)other.destroy();
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_achievement::move(game_achievement &other) noexcept
{
    return (this->initialize(static_cast<game_achievement &&>(other)));
}

int32_t game_achievement::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    this->_id = 0;
    this->_goals.clear();
    (void)this->_goals.destroy();
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_achievement::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
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

int32_t game_achievement::disable_thread_safety() noexcept
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

ft_bool game_achievement::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_achievement::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_achievement::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_achievement::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_achievement::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_achievement::get_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::get_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    identifier = this->_id;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (identifier);
}

void game_achievement::set_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::set_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_id = id;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_map<int32_t, game_goal> &game_achievement::get_goals() noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::get_goals");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_goals);
    }
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (this->_goals);
}

const ft_map<int32_t, game_goal> &game_achievement::get_goals() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::get_goals const");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_goals);
    }
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (this->_goals);
}

void game_achievement::set_goals(const ft_map<int32_t, game_goal> &goals) noexcept
{
    ft_bool lock_acquired;
    const Pair<int32_t, game_goal> *goal_entry;
    const Pair<int32_t, game_goal> *goals_end;
    game_goal                   new_goal;
    ft_size_t                    goals_count;
    ft_size_t                    index;
    int32_t                       lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_achievement::set_goals");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_goals.clear();
    goals_count = goals.size();
    goals_end = goals.end();
    goal_entry = goals_end - goals_count;
    index = 0;
    while (index < goals_count)
    {
        if (new_goal.initialize() != FT_ERR_SUCCESS)
        {
            (void)this->unlock_internal(lock_acquired);
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            this->set_error(FT_ERR_INVALID_STATE);
            return ;
        }
        new_goal.set_target(goal_entry->value.get_target());
        new_goal.set_progress(goal_entry->value.get_progress());
        this->_goals.insert(goal_entry->key, new_goal);
        (void)new_goal.destroy();
        goal_entry++;
        index += 1;
    }
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_achievement::get_goal(int32_t id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::get_goal");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    int32_t target = 0;
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [&target](const game_goal &goal) noexcept
        {
            const int32_t value = goal.get_target();
            const int32_t error = goal.get_error();
            if (error == FT_ERR_SUCCESS)
                target = value;
            return (error);
        });
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (result);
    return (target);
}

static int32_t insert_missing_goal(ft_map<int32_t, game_goal> &goals,
    int32_t id, int32_t target, int32_t progress) noexcept
{
    game_goal goal;
    const int32_t result = goal.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (target != 0)
        goal.set_target(target);
    if (progress != 0)
        goal.set_progress(progress);
    goals.insert(id, goal);
    return (goals.get_error());
}

void game_achievement::set_goal(int32_t id, int32_t goal) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::set_goal");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [goal](game_goal &existing) noexcept
        {
            existing.set_target(goal);
            return (existing.get_error());
        });
        if (result == FT_ERR_NOT_FOUND)
            result = insert_missing_goal(this->_goals, id, goal, 0);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_achievement::get_progress(int32_t id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::get_progress");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (0);
    }
    int32_t progress = 0;
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [&progress](const game_goal &goal) noexcept
        {
            const int32_t value = goal.get_progress();
            const int32_t error = goal.get_error();
            if (error == FT_ERR_SUCCESS)
                progress = value;
            return (error);
        });
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return (progress);
}

void game_achievement::set_progress(int32_t id, int32_t progress) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::set_progress");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [progress](game_goal &existing) noexcept
        {
            existing.set_progress(progress);
            return (existing.get_error());
        });
        if (result == FT_ERR_NOT_FOUND)
            result = insert_missing_goal(this->_goals, id, 0, progress);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

void game_achievement::add_progress(int32_t id, int32_t value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::add_progress");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [value](game_goal &existing) noexcept
        {
            existing.add_progress(value);
            return (existing.get_error());
        });
        if (result == FT_ERR_NOT_FOUND)
            result = insert_missing_goal(this->_goals, id, 0, value);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

ft_bool game_achievement::is_goal_complete(int32_t id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::is_goal_complete");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_FALSE);
    }
    ft_bool completed = FT_FALSE;
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_goals.visit_value(id, [&completed](const game_goal &goal) noexcept
        {
            return (goal.read_completion(completed));
        });
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return (completed);
}

ft_bool game_achievement::is_complete() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_achievement::is_complete");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (FT_FALSE);
    }
    ft_bool completed = FT_FALSE;
    ft_bool goals_locked = FT_FALSE;
    result = this->_goals.lock(&goals_locked);
    if (result == FT_ERR_SUCCESS)
    {
        completed = FT_TRUE;
        ft_size_t remaining = this->_goals.size();
        const Pair<int32_t, game_goal> *entry = this->_goals.end();
        if (remaining > 0U)
            entry -= remaining;
        while (remaining > 0U && completed)
        {
            result = entry->value.read_completion(completed);
            if (result != FT_ERR_SUCCESS)
                completed = FT_FALSE;
            ++entry;
            --remaining;
        }
        (void)this->_goals.unlock(goals_locked);
    }
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return (completed);
}
