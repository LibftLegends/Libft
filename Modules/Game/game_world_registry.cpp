#include "../PThread/pthread_internal.hpp"
#include "game_world_registry.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/map.hpp"
#include "../Template/move.hpp"
#include "../Template/pair.hpp"
#include "game_region_definition.hpp"
#include "game_world_region.hpp"

thread_local int32_t game_world_registry::_last_error = FT_ERR_SUCCESS;

game_world_registry::game_world_registry() noexcept
    : _regions(), _world_regions(), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_world_registry::~game_world_registry() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_world_registry::set_error(int32_t error_code) noexcept
{
    game_world_registry::_last_error = error_code;
    return (error_code);
}

int32_t game_world_registry::initialize() noexcept
{
    int32_t regions_error;
    int32_t worlds_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_world_registry::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    regions_error = this->_regions.initialize();
    if (regions_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(regions_error);
        return (regions_error);
    }
    worlds_error = this->_world_regions.initialize();
    if (worlds_error != FT_ERR_SUCCESS)
    {
        (void)this->_regions.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(worlds_error);
        return (worlds_error);
    }
    this->_regions.clear();
    this->_world_regions.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_registry::initialize(const game_world_registry &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world_registry::initialize(copy)",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (this->has_active_lock())
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
    ft_bool source_lock_acquired;
    result = other.lock_internal(&source_lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_regions.copy_from(other._regions);
        if (result == FT_ERR_SUCCESS)
            result = this->_world_regions.copy_from(other._world_regions);
        (void)other.unlock_internal(source_lock_acquired);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_regions.destroy();
        (void)this->_world_regions.destroy();
        return (this->set_error(result));
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (this->set_error(FT_ERR_SUCCESS));
}


int32_t game_world_registry::initialize(game_world_registry &&other) noexcept
{
    return (this->move(other));
}

int32_t game_world_registry::move(game_world_registry &other) noexcept
{
    if (&other == this)
        return (this->set_error(FT_ERR_SUCCESS));
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_world_registry::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    if (this->has_active_lock())
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
    if (other.has_active_lock())
        return (this->set_error(FT_ERR_THREAD_BUSY));
    result = this->_regions.move(other._regions);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_world_regions.move(other._world_regions);
        if (result != FT_ERR_SUCCESS)
            (void)other._regions.move(this->_regions);
    }
    if (result != FT_ERR_SUCCESS)
    {
        (void)this->_regions.destroy();
        (void)this->_world_regions.destroy();
        return (this->set_error(result));
    }
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

ft_bool game_world_registry::has_active_lock() const noexcept
{
    return ((this->_mutex != ft_nullptr && this->_mutex->lockState())
        || this->_regions.has_active_lock() || this->_world_regions.has_active_lock());
}

int32_t game_world_registry::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    int32_t result = this->disable_thread_safety();
    const int32_t region_error = this->_regions.destroy();
    const int32_t world_error = this->_world_regions.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (result == FT_ERR_SUCCESS)
        result = region_error;
    if (result == FT_ERR_SUCCESS)
        result = world_error;
    return (this->set_error(result));
}

int32_t game_world_registry::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::enable_thread_safety");
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

int32_t game_world_registry::disable_thread_safety() noexcept
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

ft_bool game_world_registry::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_world_registry::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_world_registry::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t game_world_registry::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_world_registry::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_world_registry::register_region(const game_region_definition &region) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::register_region");
    game_region_definition staged;
    int32_t result = staged.initialize(region);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    const int32_t identifier = staged.get_region_id();
    ft_bool lock_acquired;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_regions.insert(identifier, ft_move(staged));
        result = this->_regions.get_error();
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_world_registry::register_world(const game_world_region &world_region) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::register_world");
    game_world_region staged;
    int32_t result = staged.initialize(world_region);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    const int32_t identifier = staged.get_world_id();
    ft_bool lock_acquired;
    result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        this->_world_regions.insert(identifier, ft_move(staged));
        result = this->_world_regions.get_error();
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_world_registry::fetch_region(int32_t region_id,
    game_region_definition &out_region) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::fetch_region");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_regions.visit_value(region_id, [&out_region](const game_region_definition &value) noexcept
        {
            return (out_region.initialize(value));
        });
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}

int32_t game_world_registry::fetch_world(int32_t world_id,
    game_world_region &out_world) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::fetch_world");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_world_regions.visit_value(world_id, [&out_world](const game_world_region &value) noexcept
        {
            return (out_world.initialize(value));
        });
        (void)this->unlock_internal(lock_acquired);
    }
    return (this->set_error(result));
}


ft_map<int32_t, game_region_definition> &game_world_registry::get_regions() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::get_regions");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_regions);
}

const ft_map<int32_t, game_region_definition> &game_world_registry::get_regions() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::get_regions const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_regions);
}

void game_world_registry::set_regions(
    const ft_map<int32_t, game_region_definition> &regions) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::set_regions");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_regions.copy_from(regions);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


ft_map<int32_t, game_world_region> &game_world_registry::get_world_regions() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::get_world_regions");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_regions);
}

const ft_map<int32_t, game_world_region> &game_world_registry::get_world_regions() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::get_world_regions const");
    this->set_error(FT_ERR_SUCCESS);
    return (this->_world_regions);
}

void game_world_registry::set_world_regions(
    const ft_map<int32_t, game_world_region> &world_regions) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_world_registry::set_world_regions");
    ft_bool lock_acquired;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
    {
        result = this->_world_regions.copy_from(world_regions);
        (void)this->unlock_internal(lock_acquired);
    }
    this->set_error(result);
    return ;
}


int32_t game_world_registry::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_registry::get_error");
    return (game_world_registry::_last_error);
}

const char *game_world_registry::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_world_registry::get_error_str");
    return (ft_strerror(game_world_registry::_last_error));
}
