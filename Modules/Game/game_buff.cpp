#include "../PThread/pthread_internal.hpp"
#include "game_buff.hpp"
#include <new>
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_buff::_last_error = FT_ERR_SUCCESS;

game_buff::game_buff() noexcept
    : _id(0), _duration(0), _modifier1(0), _modifier2(0), _modifier3(0),
      _modifier4(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_buff::~game_buff() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t previous_error = this->get_error();
        (void)this->destroy();
        this->set_error(previous_error);
    }
    return ;
}

inline void game_buff::reset_fields() noexcept
{
    this->_id = 0;
    this->_duration = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    return ;
}

inline void game_buff::copy_fields_from(const game_buff &other) noexcept
{
    this->_id = other._id;
    this->_duration = other._duration;
    this->_modifier1 = other._modifier1;
    this->_modifier2 = other._modifier2;
    this->_modifier3 = other._modifier3;
    this->_modifier4 = other._modifier4;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return ;
}

int32_t game_buff::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_buff::initialize",
            "called while object is already initialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_buff::initialize(const game_buff &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_buff::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED
        && other._initialised_state == FT_CLASS_STATE_INITIALISED && other._mutex == ft_nullptr)
    {
        // Both instances already share the caller's thread-local error.
        this->copy_fields_from(other);
        return (FT_ERR_SUCCESS);
    }
    return (this->copy_with_lifecycle(other));
}

int32_t game_buff::copy_with_lifecycle(const game_buff &other) noexcept
{
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
    this->copy_fields_from(other);
    (void)other.unlock_internal(source_locked);
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_buff::initialize(game_buff &&other) noexcept
{
    return (this->move(other));
}

int32_t game_buff::move(game_buff &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_buff::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
        return (this->copy_with_lifecycle(other));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;

    ft_bool source_locked = FT_FALSE;
    const int32_t lock_error = other.lock_internal(&source_locked);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->set_error(lock_error));
    this->copy_fields_from(other);
    other.reset_fields();
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    (void)other.unlock_internal(source_locked);
    this->_mutex = other._mutex;
    other._mutex = ft_nullptr;
    return (this->set_error(FT_ERR_SUCCESS));
}

int32_t game_buff::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    this->reset_fields();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t game_buff::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::enable_thread_safety");
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

int32_t game_buff::disable_thread_safety() noexcept
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

ft_bool game_buff::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_buff::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_buff::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_buff::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::lock");
    return (this->set_error(this->lock_internal(lock_acquired)));
}

void game_buff::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_buff::set_error(int32_t error_code) noexcept
{
    game_buff::_last_error = error_code;
    return (error_code);
}

int32_t game_buff::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_buff::get_error");
    return (game_buff::_last_error);
}

const char *game_buff::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_buff::get_error_str");
    return (ft_strerror(game_buff::_last_error));
}

inline int32_t game_buff::read_field(const int32_t &field) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    this->set_error(result);
    if (result != FT_ERR_SUCCESS)
        return (0);
    const int32_t value = field;
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

inline void game_buff::write_field(int32_t &field, int32_t value) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        field = value;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

inline void game_buff::adjust_field(int32_t &field, int64_t delta) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        const int64_t next_value = static_cast<int64_t>(field) + delta;
        if (next_value < INT32_MIN || next_value > INT32_MAX)
            result = FT_ERR_OUT_OF_RANGE;
        else
            field = static_cast<int32_t>(next_value);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_buff::get_id() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_id");
    return (this->read_field(this->_id));
}

void game_buff::set_id(int32_t id) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_id");
    if (id < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    this->write_field(this->_id, id);
    return ;
}

int32_t game_buff::get_duration() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_duration");
    return (this->read_field(this->_duration));
}

void game_buff::set_duration(int32_t duration) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_duration");
    if (duration < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    this->write_field(this->_duration, duration);
    return ;
}

void game_buff::add_duration(int32_t duration) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::add_duration");
    if (duration < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    this->adjust_field(this->_duration, duration);
    return ;
}

void game_buff::sub_duration(int32_t duration) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::sub_duration");
    if (duration < 0)
    {
        this->set_error(FT_ERR_INVALID_ARGUMENT);
        return ;
    }
    this->adjust_field(this->_duration, -static_cast<int64_t>(duration));
    return ;
}

int32_t game_buff::get_modifier1() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_modifier1");
    return (this->read_field(this->_modifier1));
}

void game_buff::set_modifier1(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_modifier1");
    this->write_field(this->_modifier1, modifier);
    return ;
}

void game_buff::add_modifier1(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::add_modifier1");
    this->adjust_field(this->_modifier1, modifier);
    return ;
}

void game_buff::sub_modifier1(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::sub_modifier1");
    this->adjust_field(this->_modifier1, -static_cast<int64_t>(modifier));
    return ;
}

int32_t game_buff::get_modifier2() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_modifier2");
    return (this->read_field(this->_modifier2));
}

void game_buff::set_modifier2(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_modifier2");
    this->write_field(this->_modifier2, modifier);
    return ;
}

void game_buff::add_modifier2(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::add_modifier2");
    this->adjust_field(this->_modifier2, modifier);
    return ;
}

void game_buff::sub_modifier2(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::sub_modifier2");
    this->adjust_field(this->_modifier2, -static_cast<int64_t>(modifier));
    return ;
}

int32_t game_buff::get_modifier3() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_modifier3");
    return (this->read_field(this->_modifier3));
}

void game_buff::set_modifier3(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_modifier3");
    this->write_field(this->_modifier3, modifier);
    return ;
}

void game_buff::add_modifier3(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::add_modifier3");
    this->adjust_field(this->_modifier3, modifier);
    return ;
}

void game_buff::sub_modifier3(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::sub_modifier3");
    this->adjust_field(this->_modifier3, -static_cast<int64_t>(modifier));
    return ;
}

int32_t game_buff::get_modifier4() const noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::get_modifier4");
    return (this->read_field(this->_modifier4));
}

void game_buff::set_modifier4(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::set_modifier4");
    this->write_field(this->_modifier4, modifier);
    return ;
}

void game_buff::add_modifier4(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::add_modifier4");
    this->adjust_field(this->_modifier4, modifier);
    return ;
}

void game_buff::sub_modifier4(int32_t modifier) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_buff::sub_modifier4");
    this->adjust_field(this->_modifier4, -static_cast<int64_t>(modifier));
    return ;
}
