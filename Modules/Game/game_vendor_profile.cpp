#include "../PThread/pthread_internal.hpp"
#include "game_vendor_profile.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_vendor_profile::_last_error = FT_ERR_SUCCESS;

game_vendor_profile::game_vendor_profile() noexcept
    : _vendor_id(0), _buy_markup(1.0), _sell_multiplier(1.0), _tax_rate(0.0),
      _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_vendor_profile::~game_vendor_profile() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_vendor_profile::set_error(int32_t error_code) noexcept
{
    game_vendor_profile::_last_error = error_code;
    return (error_code);
}

int32_t game_vendor_profile::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_vendor_profile::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_vendor_id = 0;
    this->_buy_markup = 1.0;
    this->_sell_multiplier = 1.0;
    this->_tax_rate = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::initialize(const game_vendor_profile &other) noexcept
{
    int32_t initialize_error;
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_vendor_profile::initialize(copy)",
            "source object is uninitialised");
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
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->set_error(initialize_error);
            return (initialize_error);
        }
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_vendor_id = other._vendor_id;
    this->_buy_markup = other._buy_markup;
    this->_sell_multiplier = other._sell_multiplier;
    this->_tax_rate = other._tax_rate;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::initialize(game_vendor_profile &&other) noexcept
{
    return (this->move(other));
}

int32_t game_vendor_profile::move(game_vendor_profile &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_vendor_profile::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_vendor_profile &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::initialize(int32_t vendor_id, double buy_markup,
    double sell_multiplier, double tax_rate) noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        initialize_error = this->destroy();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            this->set_error(initialize_error);
            return (initialize_error);
        }
    }
    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->set_error(initialize_error);
        return (initialize_error);
    }
    this->_vendor_id = vendor_id;
    this->_buy_markup = buy_markup;
    this->_sell_multiplier = sell_multiplier;
    this->_tax_rate = tax_rate;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_vendor_id = 0;
    this->_buy_markup = 1.0;
    this->_sell_multiplier = 1.0;
    this->_tax_rate = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_vendor_profile::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::enable_thread_safety");
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

int32_t game_vendor_profile::disable_thread_safety() noexcept
{
    pt_recursive_mutex *old_mutex;
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::disable_thread_safety");
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

ft_bool game_vendor_profile::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_vendor_profile::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::lock_internal");
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (lock_acquired != ft_nullptr && this->_mutex != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::unlock_internal(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::unlock_internal");
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_vendor_profile::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_vendor_profile::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_vendor_profile::get_vendor_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::get_vendor_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    value = this->_vendor_id;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_vendor_profile::set_vendor_id(int32_t vendor_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::set_vendor_id");
    if (vendor_id < 0)
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
    this->_vendor_id = vendor_id;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

double game_vendor_profile::get_buy_markup() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::get_buy_markup");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0.0);
    }
    value = this->_buy_markup;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_vendor_profile::set_buy_markup(double buy_markup) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::set_buy_markup");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_buy_markup = buy_markup;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

double game_vendor_profile::get_sell_multiplier() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::get_sell_multiplier");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0.0);
    }
    value = this->_sell_multiplier;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_vendor_profile::set_sell_multiplier(double sell_multiplier) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::set_sell_multiplier");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_sell_multiplier = sell_multiplier;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

double game_vendor_profile::get_tax_rate() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::get_tax_rate");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0.0);
    }
    value = this->_tax_rate;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_vendor_profile::set_tax_rate(double tax_rate) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_vendor_profile::set_tax_rate");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_tax_rate = tax_rate;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}


int32_t game_vendor_profile::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_vendor_profile::get_error");
    return (game_vendor_profile::_last_error);
}

const char *game_vendor_profile::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_vendor_profile::get_error_str");
    return (ft_strerror(game_vendor_profile::_last_error));
}
