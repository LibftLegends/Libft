#include "../PThread/pthread_internal.hpp"
#include "game_skill.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_skill::_last_error = FT_ERR_SUCCESS;

int32_t game_skill::set_error(int32_t error_code) noexcept
{
    game_skill::_last_error = error_code;
    return (error_code);
}

int32_t game_skill::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_skill::get_error");
    return (game_skill::_last_error);
}

const char *game_skill::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_skill::get_error_str");
    return (ft_strerror(this->get_error()));
}

game_skill::game_skill() noexcept
    : _id(0), _level(0), _cooldown(0), _modifier1(0), _modifier2(0),
      _modifier3(0), _modifier4(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_skill::~game_skill() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_skill::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_skill::initialize", "called while object is already initialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    this->_id = 0;
    this->_level = 0;
    this->_cooldown = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_skill::initialize(const game_skill &other) noexcept
{
    int32_t destroy_error;

    if (&other == this)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_skill::initialize(copy)",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_id = other._id;
    this->_level = other._level;
    this->_cooldown = other._cooldown;
    this->_modifier1 = other._modifier1;
    this->_modifier2 = other._modifier2;
    this->_modifier3 = other._modifier3;
    this->_modifier4 = other._modifier4;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(other.get_error());
    return (FT_ERR_SUCCESS);
}

int32_t game_skill::initialize(game_skill &&other) noexcept
{
    return (this->move(other));
}

int32_t game_skill::move(game_skill &other) noexcept
{
    int32_t destroy_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_skill::move",
            "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    this->_id = other._id;
    this->_level = other._level;
    this->_cooldown = other._cooldown;
    this->_modifier1 = other._modifier1;
    this->_modifier2 = other._modifier2;
    this->_modifier3 = other._modifier3;
    this->_modifier4 = other._modifier4;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(other.get_error());
    other._id = 0;
    other._level = 0;
    other._cooldown = 0;
    other._modifier1 = 0;
    other._modifier2 = 0;
    other._modifier3 = 0;
    other._modifier4 = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    other.set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_skill::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    disable_error = this->disable_thread_safety();
    this->_id = 0;
    this->_level = 0;
    this->_cooldown = 0;
    this->_modifier1 = 0;
    this->_modifier2 = 0;
    this->_modifier3 = 0;
    this->_modifier4 = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_skill::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::enable_thread_safety");
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

int32_t game_skill::disable_thread_safety() noexcept
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

ft_bool game_skill::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_skill::lock_internal(ft_bool *lock_acquired) const noexcept
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
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_skill::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_skill::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::lock");
    return (this->lock_internal(lock_acquired));
}

void game_skill::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_skill::get_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t identifier;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_id");
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

void game_skill::set_id(int32_t id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_id");
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

int32_t game_skill::get_level() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t level_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_level");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    level_value = this->_level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (level_value);
}

void game_skill::set_level(int32_t level) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_level");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_level = level;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_skill::get_cooldown() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t cooldown_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_cooldown");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    cooldown_value = this->_cooldown;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return (cooldown_value);
}

void game_skill::set_cooldown(int32_t cooldown) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_cooldown");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_cooldown = cooldown;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::add_cooldown(int32_t cooldown) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::add_cooldown");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_cooldown += cooldown;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::sub_cooldown(int32_t cooldown) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::sub_cooldown");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_cooldown -= cooldown;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_skill::get_modifier1() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_modifier1");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_modifier1);
}

void game_skill::set_modifier1(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_modifier1");
    this->_modifier1 = mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::add_modifier1(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::add_modifier1");
    this->_modifier1 += mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::sub_modifier1(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::sub_modifier1");
    this->_modifier1 -= mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_skill::get_modifier2() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_modifier2");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_modifier2);
}

void game_skill::set_modifier2(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_modifier2");
    this->_modifier2 = mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::add_modifier2(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::add_modifier2");
    this->_modifier2 += mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::sub_modifier2(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::sub_modifier2");
    this->_modifier2 -= mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_skill::get_modifier3() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_modifier3");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_modifier3);
}

void game_skill::set_modifier3(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_modifier3");
    this->_modifier3 = mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::add_modifier3(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::add_modifier3");
    this->_modifier3 += mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::sub_modifier3(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::sub_modifier3");
    this->_modifier3 -= mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

int32_t game_skill::get_modifier4() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::get_modifier4");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_modifier4);
}

void game_skill::set_modifier4(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::set_modifier4");
    this->_modifier4 = mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::add_modifier4(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::add_modifier4");
    this->_modifier4 += mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

void game_skill::sub_modifier4(int32_t mod) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_skill::sub_modifier4");
    this->_modifier4 -= mod;
    this->set_error(FT_ERR_SUCCESS);
    return ;
}
