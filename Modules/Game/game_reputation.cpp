#include "../PThread/pthread_internal.hpp"
#include "game_reputation.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/pair.hpp"

thread_local int32_t game_reputation::_last_error = FT_ERR_SUCCESS;

int32_t game_reputation::set_error(int32_t error_code) noexcept
{
    game_reputation::_last_error = error_code;
    return (error_code);
}

int32_t game_reputation::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_reputation::get_error");
    return (game_reputation::_last_error);
}

const char *game_reputation::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_reputation::get_error_str");
    return (ft_strerror(game_reputation::_last_error));
}

game_reputation::game_reputation() noexcept
    : _milestones(), _reps(), _total_rep(0), _current_rep(0),
      _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_reputation::~game_reputation() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_reputation::initialize() noexcept
{
    int32_t milestones_error;
    int32_t reps_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_reputation::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    milestones_error = this->_milestones.initialize();
    if (milestones_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(milestones_error);
        return (milestones_error);
    }
    reps_error = this->_reps.initialize();
    if (reps_error != FT_ERR_SUCCESS)
    {
        (void)this->_milestones.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(reps_error);
        return (reps_error);
    }
    this->_total_rep = 0;
    this->_current_rep = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_reputation::initialize(const ft_map<int32_t, int32_t> &milestones,
    int32_t total) noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    initialize_error = this->_milestones.copy_from(milestones);
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(initialize_error));
    }
    this->_total_rep = total;
    this->_current_rep = 0;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_reputation::initialize(const game_reputation &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_reputation::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    result = other.lock_internal();
    if (result == FT_ERR_SUCCESS)
    {
        result = this->initialize();
        if (result == FT_ERR_SUCCESS)
            result = this->_milestones.copy_from(other._milestones);
        if (result == FT_ERR_SUCCESS)
            result = this->_reps.copy_from(other._reps);
        if (result == FT_ERR_SUCCESS)
        {
            this->_total_rep = other._total_rep;
            this->_current_rep = other._current_rep;
        }
        other.unlock_internal();
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_reputation::initialize(game_reputation &&other) noexcept
{
    return (this->move(other));
}

int32_t game_reputation::move(game_reputation &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_reputation::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    ft_bool milestones_locked = FT_FALSE;
    ft_bool reps_locked = FT_FALSE;
    result = other.lock_internal();
    const ft_bool source_locked = result == FT_ERR_SUCCESS;
    if (result == FT_ERR_SUCCESS)
        result = other._milestones.lock(&milestones_locked);
    if (result == FT_ERR_SUCCESS)
        result = other._reps.lock(&reps_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_milestones.move(other._milestones);
        if (result == FT_ERR_SUCCESS)
        {
            result = this->_reps.move(other._reps);
            if (result != FT_ERR_SUCCESS)
                (void)other._milestones.move(this->_milestones);
        }
        if (result == FT_ERR_SUCCESS)
        {
            this->_total_rep = other._total_rep;
            this->_current_rep = other._current_rep;
        }
    }
    if (reps_locked)
        (void)other._reps.unlock(reps_locked);
    if (milestones_locked)
        (void)other._milestones.unlock(milestones_locked);
    if (source_locked)
        other.unlock_internal();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
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

int32_t game_reputation::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t first_error = this->disable_thread_safety();
    const int32_t milestones_error = this->_milestones.destroy();
    if (first_error == FT_ERR_SUCCESS && milestones_error != FT_ERR_SUCCESS)
        first_error = milestones_error;
    const int32_t reps_error = this->_reps.destroy();
    if (first_error == FT_ERR_SUCCESS && reps_error != FT_ERR_SUCCESS)
        first_error = reps_error;
    this->_total_rep = 0;
    this->_current_rep = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(first_error));
}

int32_t game_reputation::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::enable_thread_safety");
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

int32_t game_reputation::disable_thread_safety() noexcept
{
    pt_recursive_mutex *old_mutex;
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    old_mutex = this->_mutex;
    this->_mutex = ft_nullptr;
    destroy_error = old_mutex->destroy();
    delete old_mutex;
    this->set_error(destroy_error);
    return (destroy_error);
}

ft_bool game_reputation::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_reputation::lock_internal() const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (pt_recursive_mutex_lock_if_not_null(this->_mutex));
}

void game_reputation::unlock_internal() const noexcept
{
    if (this->_mutex != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_reputation::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::lock");
    const int32_t result = this->lock_internal();
    if (lock_acquired != ft_nullptr)
        *lock_acquired = (result == FT_ERR_SUCCESS);
    return (this->set_error(result));
}

void game_reputation::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::unlock");
    if (lock_acquired)
        this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_reputation::read_counter(const int32_t &counter) const noexcept
{
    const int32_t error_code = this->lock_internal();
    if (error_code != FT_ERR_SUCCESS)
    {
        this->set_error(error_code);
        return (0);
    }
    const int32_t value = counter;
    this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_reputation::set_counter(int32_t &counter, int32_t value) noexcept
{
    int32_t result = this->lock_internal();
    if (result == FT_ERR_SUCCESS)
    {
        if (value >= 0)
            counter = value;
        else
            result = FT_ERR_INVALID_ARGUMENT;
        this->unlock_internal();
    }
    this->set_error(result);
    return ;
}

void game_reputation::adjust_counter(int32_t &counter, int64_t delta) noexcept
{
    int32_t result = this->lock_internal();
    if (result == FT_ERR_SUCCESS)
    {
        const int64_t adjusted = static_cast<int64_t>(counter) + delta;
        if (adjusted < INT32_MIN || adjusted > INT32_MAX)
            result = FT_ERR_OUT_OF_RANGE;
        else
            counter = static_cast<int32_t>(adjusted);
        this->unlock_internal();
    }
    this->set_error(result);
    return ;
}

int32_t game_reputation::get_total_rep() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_total_rep");
    return (this->read_counter(this->_total_rep));
}

void game_reputation::set_total_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_total_rep");
    this->set_counter(this->_total_rep, rep);
    return ;
}

void game_reputation::add_total_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::add_total_rep");
    this->adjust_counter(this->_total_rep, rep);
    return ;
}

void game_reputation::sub_total_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::sub_total_rep");
    this->adjust_counter(this->_total_rep, -static_cast<int64_t>(rep));
    return ;
}

int32_t game_reputation::get_current_rep() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_current_rep");
    return (this->read_counter(this->_current_rep));
}

void game_reputation::set_current_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_current_rep");
    this->set_counter(this->_current_rep, rep);
    return ;
}

void game_reputation::add_current_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::add_current_rep");
    this->adjust_counter(this->_current_rep, rep);
    return ;
}

void game_reputation::sub_current_rep(int32_t rep) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::sub_current_rep");
    this->adjust_counter(this->_current_rep, -static_cast<int64_t>(rep));
    return ;
}

ft_map<int32_t, int32_t> &game_reputation::get_milestones() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_milestones");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_milestones);
    }
    this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return (this->_milestones);
}

const ft_map<int32_t, int32_t> &game_reputation::get_milestones() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_milestones const");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_milestones);
    }
    this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return (this->_milestones);
}

void game_reputation::set_milestones(const ft_map<int32_t, int32_t> &milestones) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_milestones");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const int32_t result = this->_milestones.copy_from(milestones);
    this->unlock_internal();
    this->set_error(result);
    return ;
}

int32_t game_reputation::get_milestone(int32_t id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_milestone");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    int32_t value = 0;
    const int32_t result = this->_milestones.visit_value(id, [&value](const int32_t &stored) noexcept
    {
        value = stored;
        return (FT_ERR_SUCCESS);
    });
    this->unlock_internal();
    this->set_error(result);
    return (value);
}

void game_reputation::set_milestone(int32_t id, int32_t value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_milestone");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_milestones.insert(id, value);
    const int32_t result = this->_milestones.get_error();
    this->unlock_internal();
    this->set_error(result);
    return ;
}

ft_map<int32_t, int32_t> &game_reputation::get_reps() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_reps");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_reps);
    }
    this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return (this->_reps);
}

const ft_map<int32_t, int32_t> &game_reputation::get_reps() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_reps const");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (this->_reps);
    }
    this->unlock_internal();
    this->set_error(FT_ERR_SUCCESS);
    return (this->_reps);
}

void game_reputation::set_reps(const ft_map<int32_t, int32_t> &reps) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_reps");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const int32_t result = this->_reps.copy_from(reps);
    this->unlock_internal();
    this->set_error(result);
    return ;
}

int32_t game_reputation::get_rep(int32_t id) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::get_rep");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    int32_t value = 0;
    const int32_t result = this->_reps.visit_value(id, [&value](const int32_t &stored) noexcept
    {
        value = stored;
        return (FT_ERR_SUCCESS);
    });
    this->unlock_internal();
    this->set_error(result);
    return (value);
}

void game_reputation::set_rep(int32_t id, int32_t value) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_reputation::set_rep");
    const int32_t lock_error = this->lock_internal();
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_reps.insert(id, value);
    const int32_t result = this->_reps.get_error();
    this->unlock_internal();
    this->set_error(result);
    return ;
}
