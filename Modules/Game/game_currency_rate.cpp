#include "../PThread/pthread_internal.hpp"
#include "game_currency_rate.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_currency_rate::_last_error = FT_ERR_SUCCESS;

game_currency_rate::game_currency_rate() noexcept
    : _currency_id(0), _rate_to_base(1.0), _display_precision(2),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_currency_rate::~game_currency_rate() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_currency_rate::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_currency_rate::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_currency_id = 0;
    this->_rate_to_base = 1.0;
    this->_display_precision = 2;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::initialize(const game_currency_rate &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_currency_rate::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        result = other.read_values(this->_currency_id, this->_rate_to_base,
            this->_display_precision);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::initialize(game_currency_rate &&other) noexcept
{
    return (this->move(other));
}

int32_t game_currency_rate::move(game_currency_rate &other) noexcept
{
    if (&other == this && other._initialised_state != FT_CLASS_STATE_UNINITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t result = this->initialize(static_cast<const game_currency_rate &>(other));
    if (result != FT_ERR_SUCCESS || other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (result);
    const int32_t source_error = this->get_error();
    const int32_t destroy_error = other.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(destroy_error));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::initialize(int32_t currency_id, double rate_to_base,
    int32_t display_precision) noexcept
{
    int32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_currency_id = currency_id;
    this->_rate_to_base = rate_to_base;
    this->_display_precision = display_precision;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_currency_id = 0;
    this->_rate_to_base = 1.0;
    this->_display_precision = 2;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_currency_rate::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::enable_thread_safety");
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

int32_t game_currency_rate::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::disable_thread_safety");
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

ft_bool game_currency_rate::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_currency_rate::lock_internal(ft_bool *lock_acquired) const noexcept
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_currency_rate::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::unlock");
    if (lock_acquired)
        (void)this->unlock_internal(lock_acquired);
    else
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_currency_rate::read_values(int32_t &currency_id, double &rate_to_base,
    int32_t &display_precision) const noexcept
{
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    currency_id = this->_currency_id;
    rate_to_base = this->_rate_to_base;
    display_precision = this->_display_precision;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_currency_rate::copy_values(const game_currency_rate &other) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_currency_rate::copy_values destination");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "game_currency_rate::copy_values source");
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t currency_id;
    double rate_to_base;
    int32_t display_precision;
    int32_t result = other.read_values(currency_id, rate_to_base, display_precision);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool lock_acquired;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_currency_id = currency_id;
        this->_rate_to_base = rate_to_base;
        this->_display_precision = display_precision;
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_currency_rate::get_currency_id() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::get_currency_id");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0);
    }
    const int32_t currency_id = this->_currency_id;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (currency_id);
}

void game_currency_rate::set_currency_id(int32_t currency_id) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::set_currency_id");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_currency_id = currency_id;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

double game_currency_rate::get_rate_to_base() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::get_rate_to_base");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0.0);
    }
    const double rate_to_base = this->_rate_to_base;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (rate_to_base);
}

void game_currency_rate::set_rate_to_base(double rate_to_base) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::set_rate_to_base");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_rate_to_base = rate_to_base;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_currency_rate::get_display_precision() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::get_display_precision");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0);
    }
    const int32_t display_precision = this->_display_precision;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (display_precision);
}

void game_currency_rate::set_display_precision(int32_t display_precision) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_currency_rate::set_display_precision");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_display_precision = display_precision;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

int32_t game_currency_rate::set_error(int32_t error_code) noexcept
{
    game_currency_rate::_last_error = error_code;
    return (error_code);
}

int32_t game_currency_rate::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_currency_rate::get_error");
    return (game_currency_rate::_last_error);
}

const char *game_currency_rate::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_currency_rate::get_error_str");
    return (ft_strerror(game_currency_rate::_last_error));
}
