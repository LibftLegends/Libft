#include "../PThread/pthread_internal.hpp"
#include "game_rarity_band.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_rarity_band::_last_error = FT_ERR_SUCCESS;

game_rarity_band::game_rarity_band() noexcept
    : _rarity(0), _value_multiplier(1.0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_rarity_band::~game_rarity_band() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_rarity_band::set_error(int32_t error_code) noexcept
{
    game_rarity_band::_last_error = error_code;
    return (error_code);
}

int32_t game_rarity_band::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_rarity_band::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_rarity = 0;
    this->_value_multiplier = 1.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::initialize(const game_rarity_band &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_rarity_band::initialize(copy)",
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
        result = other.read_values(this->_rarity, this->_value_multiplier);
        if (result != FT_ERR_SUCCESS)
            return (this->set_error(result));
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::initialize(game_rarity_band &&other) noexcept
{
    return (this->move(other));
}

int32_t game_rarity_band::move(game_rarity_band &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_rarity_band::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (&other == this)
        return (FT_ERR_SUCCESS);
    const int32_t result = this->initialize(static_cast<const game_rarity_band &>(other));
    if (result != FT_ERR_SUCCESS)
        return (result);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t source_error = this->get_error();
        const int32_t destroy_error = other.destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            (void)this->destroy();
            return (this->set_error(destroy_error));
        }
        this->set_error(source_error);
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::initialize(int32_t rarity, double value_multiplier) noexcept
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
    this->_rarity = rarity;
    this->_value_multiplier = value_multiplier;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_rarity = 0;
    this->_value_multiplier = 1.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_rarity_band::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::enable_thread_safety");
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

int32_t game_rarity_band::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::disable_thread_safety");
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

ft_bool game_rarity_band::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_rarity_band::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_rarity_band::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_rarity_band::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_rarity_band::read_values(int32_t &rarity, double &value_multiplier) const noexcept
{
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    rarity = this->_rarity;
    value_multiplier = this->_value_multiplier;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::copy_values(const game_rarity_band &other) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_rarity_band::copy_values destination");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "game_rarity_band::copy_values source");
    if (this == &other)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t rarity;
    double value_multiplier;
    int32_t result = other.read_values(rarity, value_multiplier);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool lock_acquired;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_rarity = rarity;
        this->_value_multiplier = value_multiplier;
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_rarity_band::get_rarity() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::get_rarity");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0);
    }
    const int32_t rarity = this->_rarity;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (rarity);
}

void game_rarity_band::set_rarity(int32_t rarity) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::set_rarity");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_rarity = rarity;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}

double game_rarity_band::get_value_multiplier() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::get_value_multiplier");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
    {
        this->set_error(result);
        return (0.0);
    }
    const double value_multiplier = this->_value_multiplier;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (value_multiplier);
}

void game_rarity_band::set_value_multiplier(double value_multiplier) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::set_value_multiplier");
    ft_bool lock_acquired;
    const int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_value_multiplier = value_multiplier;
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


int32_t game_rarity_band::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_rarity_band::get_error");
    return (game_rarity_band::_last_error);
}

const char *game_rarity_band::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state,
        "game_rarity_band::get_error_str");
    return (ft_strerror(game_rarity_band::_last_error));
}
