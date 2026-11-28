#include "../PThread/pthread_internal.hpp"
#include "game_world_region.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"

thread_local int32_t game_world_region::_last_error = FT_ERR_SUCCESS;

game_world_region::game_world_region() noexcept
    : _world_id(0), _region_ids(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_world_region::~game_world_region() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_world_region::set_error(int32_t error_code) noexcept
{
    game_world_region::_last_error = error_code;
    return (error_code);
}

int32_t game_world_region::initialize() noexcept
{
    int32_t region_ids_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_world_region::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_region_ids.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        region_ids_initialize_error = this->_region_ids.initialize();
        if (region_ids_initialize_error != FT_ERR_SUCCESS)
        {
            this->_initialised_state = FT_CLASS_STATE_DESTROYED;
            this->set_error(region_ids_initialize_error);
            return (region_ids_initialize_error);
        }
    }
    this->_world_id = 0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_region::initialize(const game_world_region &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world_region::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t source_error = other.get_error();
    int32_t result = this->destroy();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    result = this->initialize();
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    ft_bool source_locked;
    result = other.lock_internal(&source_locked);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_region_ids.replace_from(other._region_ids);
        if (result == FT_ERR_SUCCESS)
            this->_world_id = other._world_id;
        (void)other.unlock_internal(source_locked);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_region::initialize(game_world_region &&other) noexcept
{
    return (this->move(other));
}

int32_t game_world_region::move(game_world_region &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world_region::move",
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
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    if (other._mutex != ft_nullptr && other._mutex->lockState())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    result = this->_region_ids.move(other._region_ids);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    this->_world_id = other._world_id;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    result = other.destroy();
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (this->set_error(result));
    }
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_region::initialize(int32_t world_id,
    const ft_vector<int32_t> &region_ids) noexcept
{
    if (&region_ids == &this->_region_ids
        && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        this->set_world_id(world_id);
        return (this->get_error());
    }
    int32_t result = this->destroy();
    if (result == FT_ERR_SUCCESS)
        result = this->initialize();
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_region_ids.replace_from(region_ids);
        if (result == FT_ERR_SUCCESS)
            this->_world_id = world_id;
        else
            (void)this->destroy();
    }
    return (this->set_error(result));
}


int32_t game_world_region::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_world_id = 0;
    const int32_t ids_error = this->_region_ids.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (this->set_error(disable_error));
    return (this->set_error(ids_error));
}

int32_t game_world_region::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::enable_thread_safety");
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

int32_t game_world_region::disable_thread_safety() noexcept
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

ft_bool game_world_region::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_world_region::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_world_region::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_region::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_world_region::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_world_region::get_world_id() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t world_id;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::get_world_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    world_id = this->_world_id;
    (void)this->unlock_internal(lock_acquired);
    return (world_id);
}

void game_world_region::set_world_id(int32_t world_id) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::set_world_id");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    this->_world_id = world_id;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const ft_vector<int32_t> &game_world_region::get_region_ids() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::get_region_ids const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_region_ids);
}

ft_vector<int32_t> &game_world_region::get_region_ids() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::get_region_ids");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_region_ids);
}

void game_world_region::set_region_ids(const ft_vector<int32_t> &region_ids) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_region::set_region_ids");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    const int32_t result = this->_region_ids.replace_from(region_ids);
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return ;
}


int32_t game_world_region::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_region::get_error");
    return (game_world_region::_last_error);
}

const char *game_world_region::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_region::get_error_str");
    return (ft_strerror(this->get_error()));
}
