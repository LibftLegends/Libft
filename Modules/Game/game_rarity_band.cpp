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
    int32_t initialize_error;
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_rarity_band::initialize(copy)",
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
    this->_rarity = other._rarity;
    this->_value_multiplier = other._value_multiplier;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_rarity_band::initialize(game_rarity_band &&other) noexcept
{
    return (this->move(other));
}

int32_t game_rarity_band::move(game_rarity_band &other) noexcept
{
    int32_t initialize_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_rarity_band::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->initialize(static_cast<const game_rarity_band &>(other));
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
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
    int32_t lock_error;

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

int32_t game_rarity_band::get_rarity() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::get_rarity");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_rarity);
}

void game_rarity_band::set_rarity(int32_t rarity) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::set_rarity");
    this->_rarity = rarity;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

double game_rarity_band::get_value_multiplier() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::get_value_multiplier");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_value_multiplier);
}

void game_rarity_band::set_value_multiplier(double value_multiplier) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_rarity_band::set_value_multiplier");
    this->_value_multiplier = value_multiplier;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}


int32_t game_rarity_band::get_error() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_rarity_band::get_error");
    return (game_rarity_band::_last_error);
}

const char *game_rarity_band::get_error_str() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "game_rarity_band::get_error_str");
    return (ft_strerror(this->get_error()));
}
