#include "../PThread/pthread_internal.hpp"
#include "game_map3d.hpp"
#include "game_pathfinding.hpp"
#include "../CMA/CMA.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <atomic>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

thread_local int32_t game_map3d::_last_error = FT_ERR_SUCCESS;
static std::atomic<uint64_t> g_next_map_route_identity{1U};

game_map3d::game_map3d()
    : _data(ft_nullptr), _width(0), _height(0), _depth(0),
      _route_identity(0U), _route_revision(0U),
      _initial_value(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_map3d::~game_map3d() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return ;
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t game_map3d::set_error(int32_t error_code) noexcept
{
    game_map3d::_last_error = error_code;
    return (error_code);
}

int32_t game_map3d::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_map3d::get_error");
    return (game_map3d::_last_error);
}

const char *game_map3d::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_map3d::get_error_str");
    return (ft_strerror(game_map3d::_last_error));
}

int32_t game_map3d::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_map3d::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    return (this->initialize(this->_width, this->_height, this->_depth,
        this->_initial_value));
}

int32_t game_map3d::initialize(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value) noexcept
{
    int32_t allocation_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_map3d::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    allocation_error = this->allocate(width, height, depth, value);
    if (allocation_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(allocation_error);
        return (allocation_error);
    }
    this->_route_identity = g_next_map_route_identity.fetch_add(1U, std::memory_order_relaxed);
    this->_route_revision = 0U;
    this->_initial_value = value;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_map3d::move(game_map3d &other) noexcept
{
    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_map3d::move",
            "source object is uninitialised");
        return (this->set_error(FT_ERR_INVALID_STATE));
    }
    const int32_t source_error = other.get_error();
    const int32_t destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    this->_data = other._data;
    this->_width = other._width;
    this->_height = other._height;
    this->_depth = other._depth;
    this->_initial_value = other._initial_value;
    this->_mutex = other._mutex;
    this->_route_identity = g_next_map_route_identity.fetch_add(1U, std::memory_order_relaxed);
    this->_route_revision = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._data = ft_nullptr;
    other._width = 0U;
    other._height = 0U;
    other._depth = 0U;
    other._initial_value = 0;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_map3d::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (this->set_error(FT_ERR_SUCCESS));
    const int32_t disable_error = this->disable_thread_safety();
    this->deallocate();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (this->set_error(disable_error));
}

int32_t game_map3d::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::enable_thread_safety");
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

int32_t game_map3d::disable_thread_safety() noexcept
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

ft_bool game_map3d::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_map3d::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_map3d::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == FT_FALSE)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }

    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_map3d::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::lock");
    const int32_t lock_result = this->lock_internal(lock_acquired);
    this->set_error(lock_result);
    return (lock_result);
}

void game_map3d::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_map3d::read_route_version(uint64_t &identity, uint64_t &revision) const noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t result = this->lock(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (result);
    identity = this->_route_identity;
    revision = this->_route_revision;
    this->unlock(lock_acquired);
    return (FT_ERR_SUCCESS);
}

void game_map3d::resize(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t allocation_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::resize");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    allocation_error = this->allocate(width, height, depth, value);
    if (allocation_error == FT_ERR_SUCCESS)
        ++this->_route_revision;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(allocation_error);
    return ;
}

int32_t game_map3d::get(ft_size_t x, ft_size_t y, ft_size_t z) const
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::get");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    if (this->_data == ft_nullptr || x >= this->_width || y >= this->_height
        || z >= this->_depth)
    {

        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return (0);
    }
    int32_t value;

    value = this->_data[(z * this->_height + y) * this->_width + x];
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (value);
}

void game_map3d::set(ft_size_t x, ft_size_t y, ft_size_t z, int32_t value)
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::set");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (this->_data != ft_nullptr && x < this->_width && y < this->_height
        && z < this->_depth)
    {
        int32_t &cell = this->_data[(z * this->_height + y) * this->_width + x];
        if ((cell == 0) != (value == 0))
            ++this->_route_revision;
        cell = value;
    }
    else
    {

        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return ;
    }
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return ;
}

ft_bool game_map3d::is_obstacle(ft_size_t x, ft_size_t y, ft_size_t z) const
{
    int32_t value;

    value = this->get(x, y, z);
    if (value != 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

void game_map3d::toggle_obstacle(ft_size_t x, ft_size_t y, ft_size_t z,
    game_pathfinding *listener)
{
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t new_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::toggle_obstacle");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (this->_data == ft_nullptr || x >= this->_width || y >= this->_height
        || z >= this->_depth)
    {

        (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_OUT_OF_RANGE);
        return ;
    }
    int32_t &cell = this->_data[(z * this->_height + y) * this->_width + x];
    new_value = cell == 0;
    cell = new_value;
    ++this->_route_revision;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    if (listener != ft_nullptr)
        listener->update_obstacle(x, y, z, new_value);
    return ;
}

ft_size_t game_map3d::get_width() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t width_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::get_width");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    width_value = this->_width;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (width_value);
}

ft_size_t game_map3d::get_height() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t height_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::get_height");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    height_value = this->_height;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (height_value);
}

ft_size_t game_map3d::get_depth() const
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t depth_value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_map3d::get_depth");
    lock_acquired = FT_FALSE;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (0);
    }
    depth_value = this->_depth;
    (void)this->unlock_internal(lock_acquired);
        this->set_error(FT_ERR_SUCCESS);
    return (depth_value);
}

int32_t game_map3d::allocate(ft_size_t width, ft_size_t height, ft_size_t depth, int32_t value)
{
    constexpr ft_size_t MAX_CELLS = SIZE_MAX / sizeof(int32_t);
    ft_size_t cell_count = 0U;
    if (width != 0U && height != 0U && depth != 0U)
    {
        if (width > MAX_CELLS / height || width * height > MAX_CELLS / depth)
            return (FT_ERR_OUT_OF_RANGE);
        cell_count = width * height * depth;
    }
    ft_size_t old_cell_count = 0U;
    if (this->_data != ft_nullptr)
        old_cell_count = this->_width * this->_height * this->_depth;

    int32_t *cells = this->_data;
    if (cell_count != old_cell_count)
    {
        cells = ft_nullptr;
        if (cell_count != 0U)
        {
            cells = static_cast<int32_t *>(cma_malloc(cell_count * sizeof(int32_t)));
            if (cells == ft_nullptr)
                return (FT_ERR_NO_MEMORY);
        }
    }
    ft_size_t index = 0U;
    while (index < cell_count)
    {
        cells[index] = value;
        ++index;
    }
    if (cells != this->_data && this->_data != ft_nullptr)
        cma_free(this->_data);
    this->_data = cells;
    this->_width = width;
    this->_height = height;
    this->_depth = depth;
    return (FT_ERR_SUCCESS);
}

void game_map3d::deallocate()
{
    if (this->_data != ft_nullptr)
        cma_free(this->_data);
    this->_data = ft_nullptr;
    this->_width = 0U;
    this->_height = 0U;
    this->_depth = 0U;
    return ;
}
