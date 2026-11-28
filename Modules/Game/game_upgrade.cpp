#include "../PThread/pthread_internal.hpp"
#include "game_upgrade.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_upgrade::_last_error = FT_ERR_SUCCESS;

int32_t game_upgrade::set_error(int32_t error_code) noexcept
{
    game_upgrade::_last_error = error_code;
    return (error_code);
}

int32_t game_upgrade::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_upgrade::get_error");
    return (game_upgrade::_last_error);
}

const char *game_upgrade::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_upgrade::get_error_str");
    return (ft_strerror(this->get_error()));
}

game_upgrade::game_upgrade() noexcept
    : _id(0), _current_level(0), _max_level(0), _modifier1(0), _modifier2(0),
      _modifier3(0), _modifier4(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_upgrade::~game_upgrade() noexcept
{
    int32_t destroy_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            this->set_error(destroy_error);
    }
    return ;
}

int32_t game_upgrade::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_upgrade::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_id = 0;
    this->_current_level = 0;
    this->_max_level = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_upgrade::initialize(const game_upgrade &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_upgrade::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->_mutex != ft_nullptr && this->_mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        ft_bool source_locked;
        result = other.lock_internal(&source_locked);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        this->_id = other._id;
        this->_current_level = other._current_level;
        this->_max_level = other._max_level;
        this->_modifier1 = other._modifier1;
        this->_modifier2 = other._modifier2;
        this->_modifier3 = other._modifier3;
        this->_modifier4 = other._modifier4;
        (void)other.unlock_internal(source_locked);
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_upgrade::initialize(game_upgrade &&other) noexcept
{
    return (this->move(other));
}

int32_t game_upgrade::move(game_upgrade &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_upgrade::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._mutex != ft_nullptr && other._mutex->lockState())
    {
        if (this->_mutex != ft_nullptr && this->_mutex->lockState())
            return (this->set_error(FT_ERR_THREAD_BUSY));
        const int32_t result = this->destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        return (this->set_error(FT_ERR_THREAD_BUSY));
    }
    const int32_t source_error = other.get_error();
    int32_t result = this->initialize(other);
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


int32_t game_upgrade::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_id = 0;
    this->_current_level = 0;
    this->_max_level = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_upgrade::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::enable_thread_safety");
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

int32_t game_upgrade::disable_thread_safety() noexcept
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

ft_bool game_upgrade::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_upgrade::lock_internal(ft_bool *lock_acquired) const noexcept
{
    const int32_t result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = result == FT_ERR_SUCCESS && this->_mutex != ft_nullptr;
    return (result);
}

int32_t game_upgrade::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    return (pt_recursive_mutex_unlock_if_not_null(this->_mutex));
}

int32_t game_upgrade::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::lock");
    const int32_t result = this->lock_internal(lock_acquired);
    if (result == FT_ERR_SUCCESS && lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (this->set_error(result));
}

void game_upgrade::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::unlock");
    this->set_error(this->unlock_internal(lock_acquired));
    return ;
}

void game_upgrade::change_modifier(int32_t &modifier, int64_t delta,
    const char *operation) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, operation);
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        const int64_t updated = static_cast<int64_t>(modifier) + delta;
        if (updated < INT32_MIN || updated > INT32_MAX)
            result = FT_ERR_OUT_OF_RANGE;
        else
            modifier = static_cast<int32_t>(updated);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_upgrade::get_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_id");
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

void game_upgrade::set_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_id");
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

uint16_t game_upgrade::get_current_level() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    uint16_t level_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_current_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    level_value = this->_current_level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (level_value);
}

void game_upgrade::set_current_level(uint16_t level) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_current_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_current_level = level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::add_level(uint16_t level) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::add_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_current_level = static_cast<uint16_t>(this->_current_level + level);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::sub_level(uint16_t level) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::sub_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_current_level = static_cast<uint16_t>(this->_current_level - level);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

uint16_t game_upgrade::get_max_level() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    uint16_t max_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_max_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    max_value = this->_max_level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (max_value);
}

void game_upgrade::set_max_level(uint16_t level) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_max_level");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_max_level = level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_upgrade::get_modifier1() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_modifier1");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    value = this->_modifier1;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_upgrade::set_modifier1(int32_t mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_modifier1");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier1 = mod;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::add_modifier1(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier1, static_cast<int64_t>(mod), "game_upgrade::add_modifier1");
    return ;
}

void game_upgrade::sub_modifier1(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier1, -static_cast<int64_t>(mod), "game_upgrade::sub_modifier1");
    return ;
}

int32_t game_upgrade::get_modifier2() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_modifier2");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    value = this->_modifier2;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_upgrade::set_modifier2(int32_t mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_modifier2");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier2 = mod;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::add_modifier2(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier2, static_cast<int64_t>(mod), "game_upgrade::add_modifier2");
    return ;
}

void game_upgrade::sub_modifier2(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier2, -static_cast<int64_t>(mod), "game_upgrade::sub_modifier2");
    return ;
}

int32_t game_upgrade::get_modifier3() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_modifier3");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    value = this->_modifier3;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_upgrade::set_modifier3(int32_t mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_modifier3");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier3 = mod;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::add_modifier3(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier3, static_cast<int64_t>(mod), "game_upgrade::add_modifier3");
    return ;
}

void game_upgrade::sub_modifier3(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier3, -static_cast<int64_t>(mod), "game_upgrade::sub_modifier3");
    return ;
}

int32_t game_upgrade::get_modifier4() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::get_modifier4");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    value = this->_modifier4;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_upgrade::set_modifier4(int32_t mod) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_upgrade::set_modifier4");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_modifier4 = mod;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_upgrade::add_modifier4(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier4, static_cast<int64_t>(mod), "game_upgrade::add_modifier4");
    return ;
}

void game_upgrade::sub_modifier4(int32_t mod) noexcept
{
    this->change_modifier(this->_modifier4, -static_cast<int64_t>(mod), "game_upgrade::sub_modifier4");
    return ;
}
