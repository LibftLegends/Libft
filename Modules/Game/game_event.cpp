#include "../PThread/pthread_internal.hpp"
#include "game_event.hpp"
#include "../Template/move.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <climits>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_event::_last_error = FT_ERR_SUCCESS;

game_event::game_event() noexcept
    : _id(0), _duration(0), _modifier1(0), _modifier2(0), _modifier3(0),
      _modifier4(0), _callback(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_event::~game_event() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t previous_error = game_event::_last_error;
        (void)this->destroy();
        this->set_error(previous_error);
    }
    return ;
}

int32_t game_event::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_event::initialize", "already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_id = 0;
    this->_duration = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_callback = ft_function<void(game_world&, game_event&)>();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_event::move(game_event &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_event::move", "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = other.get_error();
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    ft_bool source_locked = FT_FALSE;
    const int32_t lock_error = other.lock_internal(&source_locked);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->_id = other._id;
    this->_duration = other._duration;
    this->_modifier1 = other._modifier1;
    this->_modifier2 = other._modifier2;
    this->_modifier3 = other._modifier3;
    this->_modifier4 = other._modifier4;
    this->_callback = ft_move(other._callback);
    other._id = 0;
    other._duration = 0;
    other._modifier1 = 0;
    other._modifier2 = 0;
    other._modifier3 = 0;
    other._modifier4 = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.unlock_internal(source_locked);
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_event::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_id = 0;
    this->_duration = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_callback = ft_function<void(game_world&, game_event&)>();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_event::lock_internal(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event::lock_internal");
    pt_recursive_mutex *mutex_pointer = this->_mutex;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = mutex_pointer != ft_nullptr && lock_error == FT_ERR_SUCCESS;
    return (lock_error);
}

void game_event::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t game_event::get_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    value = this->_id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (value);
}

void game_event::set_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_id = id;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_event::get_duration() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    value = this->_duration;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (value);
}

int32_t game_event::get_schedule_values(int32_t &id, int32_t &duration) const noexcept
{
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    id = this->_id;
    duration = this->_duration;
    this->unlock_internal(lock_acquired);
    return (this->set_error(FT_ERR_SUCCESS));
}

void game_event::set_duration(int32_t duration) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (duration < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_duration = duration;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_event::add_duration(int32_t duration) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    if (duration < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (this->_duration > FT_INT32_MAX - duration)
    {
        this->set_error(FT_ERR_OUT_OF_RANGE);
        this->unlock_internal(lock_acquired);
        return (FT_ERR_OUT_OF_RANGE);
    }
    this->_duration += duration;
    this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_event::sub_duration(int32_t duration) noexcept
{
    this->subtract_duration(duration, ft_nullptr);
    return ;
}

void game_event::subtract_duration(int32_t duration, int32_t *remaining_duration) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const ft_bool valid = duration >= 0 && duration <= this->_duration;
    if (valid)
        this->_duration -= duration;
    if (remaining_duration != ft_nullptr)
        *remaining_duration = this->_duration;
    if (remaining_duration == ft_nullptr && !valid)
        this->set_error(FT_ERR_INVALID_ARGUMENT);
    else
        this->set_error(FT_ERR_SUCCESS);
    this->unlock_internal(lock_acquired);
    return ;
}

inline int32_t game_event::read_modifier(const int32_t &field) const noexcept
{
    const pt_recursive_mutex *mutex_pointer = this->_mutex;
    const int32_t result = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    const int32_t value = field;
    (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    return (value);
}

inline void game_event::write_modifier(int32_t &field, int32_t value) noexcept
{
    const pt_recursive_mutex *mutex_pointer = this->_mutex;
    const int32_t result = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (result == FT_ERR_SUCCESS)
    {
        field = value;
        (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    }
    this->set_error(result);
    return ;
}

inline void game_event::adjust_modifier(int32_t &field, int64_t adjustment) noexcept
{
    const pt_recursive_mutex *mutex_pointer = this->_mutex;
    int32_t result = pt_recursive_mutex_lock_if_not_null(mutex_pointer);
    if (result == FT_ERR_SUCCESS)
    {
        const int64_t updated = field + adjustment;
        if (updated < FT_INT32_MIN || updated > FT_INT32_MAX)
            result = FT_ERR_OUT_OF_RANGE;
        else
            field = static_cast<int32_t>(updated);
        (void)pt_recursive_mutex_unlock_if_not_null(mutex_pointer);
    }
    this->set_error(result);
    return ;
}

int32_t game_event::get_modifier1() const noexcept
{
    return (this->read_modifier(this->_modifier1));
}

void game_event::set_modifier1(int32_t mod) noexcept
{
    this->write_modifier(this->_modifier1, mod);
    return ;
}

void game_event::add_modifier1(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier1, mod);
    return ;
}

void game_event::sub_modifier1(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier1, -static_cast<int64_t>(mod));
    return ;
}

int32_t game_event::get_modifier2() const noexcept
{
    return (this->read_modifier(this->_modifier2));
}

void game_event::set_modifier2(int32_t mod) noexcept
{
    this->write_modifier(this->_modifier2, mod);
    return ;
}

void game_event::add_modifier2(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier2, mod);
    return ;
}

void game_event::sub_modifier2(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier2, -static_cast<int64_t>(mod));
    return ;
}

int32_t game_event::get_modifier3() const noexcept
{
    return (this->read_modifier(this->_modifier3));
}

void game_event::set_modifier3(int32_t mod) noexcept
{
    this->write_modifier(this->_modifier3, mod);
    return ;
}

void game_event::add_modifier3(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier3, mod);
    return ;
}

void game_event::sub_modifier3(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier3, -static_cast<int64_t>(mod));
    return ;
}

int32_t game_event::get_modifier4() const noexcept
{
    return (this->read_modifier(this->_modifier4));
}

void game_event::set_modifier4(int32_t mod) noexcept
{
    this->write_modifier(this->_modifier4, mod);
    return ;
}

void game_event::add_modifier4(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier4, mod);
    return ;
}

void game_event::sub_modifier4(int32_t mod) noexcept
{
    this->adjust_modifier(this->_modifier4, -static_cast<int64_t>(mod));
    return ;
}

const ft_function<void(game_world&, game_event&)> &game_event::get_callback() const noexcept
{
    this->set_error(FT_ERR_SUCCESS);
    return (this->_callback);
}

void game_event::set_callback(ft_function<void(game_world&, game_event&)> &&callback) noexcept
{
    this->_callback = ft_move(callback);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_event::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (this->set_error(FT_ERR_SUCCESS));
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

int32_t game_event::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event::disable_thread_safety");
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

ft_bool game_event::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_event::is_thread_safe");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

int32_t game_event::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_event::get_error");
    return (game_event::_last_error);
}

const char *game_event::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_event::get_error_str");
    return (ft_strerror(game_event::_last_error));
}

int32_t game_event::set_error(int32_t error_code) noexcept
{
    game_event::_last_error = error_code;
    return (error_code);
}
