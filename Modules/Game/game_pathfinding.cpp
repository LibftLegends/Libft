#include "../PThread/pthread_internal.hpp"
#include "game_pathfinding.hpp"
#include "../Printf/printf.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno_internal.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Template/unordered_map.hpp"
#include "../Template/priority_queue.hpp"

static ft_size_t distance_component(ft_size_t left_value, ft_size_t right_value)
{
    if (left_value > right_value)
        return (left_value - right_value);
    return (right_value - left_value);
}

thread_local int32_t game_path_step::_last_error = FT_ERR_SUCCESS;

#ifdef LIBFT_TEST_BUILD

int32_t game_path_step_test_helper::ensure_thread_safe(game_path_step &step) noexcept
{
    if (step._mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (step.enable_thread_safety());
}

int32_t game_path_step_test_helper::lock(game_path_step &step) noexcept
{
    int32_t ensure_error;

    ensure_error = game_path_step_test_helper::ensure_thread_safe(step);
    if (ensure_error != FT_ERR_SUCCESS)
        return (ensure_error);
    return (pt_recursive_mutex_lock_if_not_null(step._mutex));
}

int32_t game_path_step_test_helper::unlock(game_path_step &step) noexcept
{
    (void)pt_recursive_mutex_unlock_if_not_null(step._mutex);
    return (FT_ERR_SUCCESS);
}

ft_bool game_path_step_test_helper::is_locked(const game_path_step &step) noexcept
{
    return (step._mutex != ft_nullptr && step._mutex->lockState());
}

ft_bool game_path_step_test_helper::is_owned_by_thread(const game_path_step &step,
    pt_thread_id_type thread_id) noexcept
{
    return (step._mutex != ft_nullptr
        && step._mutex->is_owned_by_thread(thread_id));
}

int32_t game_path_step_test_helper::get_mutex_error(const game_path_step &step) noexcept
{
    if (step._mutex == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (step._mutex->lock_state(ft_nullptr));
}

#endif

int32_t game_path_step::set_error(int32_t error_code) noexcept
{
    game_path_step::_last_error = error_code;
    return (error_code);
}

int32_t game_path_step::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_path_step::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

game_path_step::game_path_step() noexcept
    : _x(0), _y(0), _z(0), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_path_step::~game_path_step() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_path_step::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_path_step::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::initialize(const game_path_step &other) noexcept
{
    int32_t destroy_error;
    int32_t lock_error;
    ft_bool lock_acquired;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    lock_error = other.lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        (void)other.unlock_internal(lock_acquired);
        errno_abort_lifecycle(other._initialised_state, "game_path_step::initialize(copy)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            (void)other.unlock_internal(lock_acquired);
            return (destroy_error);
        }
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        (void)other.unlock_internal(lock_acquired);
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
        {
            (void)other.unlock_internal(lock_acquired);
            return (destroy_error);
        }
    }
    this->_x = other._x;
    this->_y = other._y;
    this->_z = other._z;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::initialize(game_path_step &&other) noexcept
{
    int32_t destroy_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_path_step::initialize(move)", "source object is uninitialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(static_cast<uint32_t>(other.get_error()));
        return (FT_ERR_SUCCESS);
    }
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_x = other._x;
    this->_y = other._y;
    this->_z = other._z;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._x = 0;
    other._y = 0;
    other._z = 0;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::move(game_path_step &other) noexcept
{
    return (this->initialize(static_cast<game_path_step &&>(other)));
}

int32_t game_path_step::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    disable_error = this->disable_thread_safety();
    this->_x = 0;
    this->_y = 0;
    this->_z = 0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(disable_error);
    return (disable_error);
}

int32_t game_path_step::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::enable_thread_safety");
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

int32_t game_path_step::disable_thread_safety() noexcept
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

ft_bool game_path_step::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_path_step::set_coordinates(ft_size_t x, ft_size_t y, ft_size_t z) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::set_coordinates");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_x = x;
    this->_y = y;
    this->_z = z;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::set_x(ft_size_t x) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::set_x");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_x = x;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::set_y(ft_size_t y) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::set_y");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_y = y;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t game_path_step::set_z(ft_size_t z) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::set_z");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_z = z;
    (void)this->unlock_internal(lock_acquired);
    return (FT_ERR_SUCCESS);
}

ft_size_t game_path_step::get_x() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::get_x");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0U);
    value = this->_x;
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

ft_size_t game_path_step::get_y() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::get_y");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0U);
    value = this->_y;
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

ft_size_t game_path_step::get_z() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_path_step::get_z");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (0U);
    value = this->_z;
    (void)this->unlock_internal(lock_acquired);
    return (value);
}

int32_t game_path_step::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_path_step::get_error");
    return (game_path_step::_last_error);
}

const char *game_path_step::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_path_step::get_error_str");
    return (ft_strerror(game_path_step::_last_error));
}

thread_local int32_t game_pathfinding::_last_error = FT_ERR_SUCCESS;

int32_t game_pathfinding::set_error(int32_t error_code) noexcept
{
    game_pathfinding::_last_error = error_code;
    return (error_code);
}

int32_t game_pathfinding::lock_internal(ft_bool *lock_acquired) const noexcept
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

int32_t game_pathfinding::unlock_internal(ft_bool lock_acquired) const noexcept
{

    if (lock_acquired == FT_FALSE)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

game_pathfinding::game_pathfinding() noexcept
    : _current_path(), _cached_grid_identity(0U), _cached_grid_revision(0U),
      _needs_replan(FT_FALSE), _mutex(ft_nullptr),
      _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    this->set_error(FT_ERR_SUCCESS);
    return ;
}

game_pathfinding::~game_pathfinding() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return ;
    (void)this->destroy();
    return ;
}

int32_t game_pathfinding::initialize() noexcept
{
    int32_t path_initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "game_pathfinding::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_needs_replan = FT_FALSE;
    path_initialize_error = this->_current_path.initialize();
    if (path_initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(path_initialize_error);
        return (path_initialize_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t game_pathfinding::destroy() noexcept
{
    int32_t disable_error;
    int32_t path_destroy_error;
    int32_t first_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
    {
        this->set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    path_destroy_error = this->_current_path.destroy();
    if (path_destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = path_destroy_error;
    this->_needs_replan = FT_FALSE;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(first_error);
    return (first_error);
}

int32_t game_pathfinding::move(game_pathfinding &other) noexcept
{
    int32_t destroy_error;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_pathfinding::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t source_error = other.get_error();
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    const int32_t move_error = this->_current_path.move(other._current_path);
    if (move_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->set_error(move_error));
    }
    this->_cached_grid_identity = other._cached_grid_identity;
    this->_cached_grid_revision = other._cached_grid_revision;
    this->_needs_replan = other._needs_replan;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;

    other._needs_replan = FT_FALSE;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t game_pathfinding::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::enable_thread_safety");
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

int32_t game_pathfinding::disable_thread_safety() noexcept
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

ft_bool game_pathfinding::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t game_pathfinding::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::lock");
    return (this->lock_internal(lock_acquired));
}

void game_pathfinding::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}

static ft_bool path_step_matches(const game_path_step &step, ft_size_t x,
    ft_size_t y, ft_size_t z) noexcept
{
    return (step.get_x() == x && step.get_y() == y && step.get_z() == z);
}

static int32_t copy_path_steps(const ft_vector<game_path_step> &source,
    ft_vector<game_path_step> &destination) noexcept
{
    if (&source == &destination)
        return (FT_ERR_SUCCESS);
    ft_bool source_locked = FT_FALSE;
    int32_t result = source.lock(&source_locked);
    if (result != FT_ERR_SUCCESS)
        return (result);
    const ft_size_t step_count = source.size();
    const game_path_step *step = source.begin();
    destination.reserve(step_count);
    result = destination.get_error();
    if (result == FT_ERR_SUCCESS)
    {
        destination.clear();
        result = destination.get_error();
        ft_size_t index = 0U;
        while (result == FT_ERR_SUCCESS && index < step_count)
        {
            result = destination.push_back(step[index]);
            ++index;
        }
        if (result != FT_ERR_SUCCESS)
            destination.clear();
    }
    source.unlock(source_locked);
    return (result);
}

void game_pathfinding::update_obstacle(ft_size_t x, ft_size_t y, ft_size_t z,
    int32_t value) noexcept
{
    (void)value;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::update_obstacle");
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    if (!this->_needs_replan)
    {
        const ft_size_t path_size = this->_current_path.size();
        const game_path_step *steps = this->_current_path.begin();
        ft_size_t index = 0U;
        while (index < path_size)
        {
            if (path_step_matches(steps[index], x, y, z))
            {
                this->_needs_replan = FT_TRUE;
                break ;
            }
            ++index;
        }
    }
    this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

int32_t game_pathfinding::recalculate_path(const game_map3d &grid,
    ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
    ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
    ft_vector<game_path_step> &out_path) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::recalculate_path");
    ft_bool lock_acquired = FT_FALSE;
    int32_t result = this->lock_internal(&lock_acquired);
    if (result != FT_ERR_SUCCESS)
        return (this->set_error(result));
    uint64_t grid_identity = 0U;
    uint64_t grid_revision = 0U;
    result = grid.read_route_version(grid_identity, grid_revision);
    if (result == FT_ERR_SUCCESS)
    {
        const ft_size_t cached_size = this->_current_path.size();
        if (!this->_needs_replan && cached_size > 0U
            && grid_identity == this->_cached_grid_identity
            && grid_revision == this->_cached_grid_revision
            && path_step_matches(this->_current_path[0], start_x, start_y, start_z)
            && path_step_matches(this->_current_path[cached_size - 1U], goal_x, goal_y, goal_z))
            result = copy_path_steps(this->_current_path, out_path);
        else
        {
            result = this->astar_grid(grid, start_x, start_y, start_z,
                goal_x, goal_y, goal_z, out_path);
            this->_needs_replan = FT_TRUE;
            uint64_t identity_after_search = 0U;
            uint64_t revision_after_search = 0U;
            if (result == FT_ERR_SUCCESS
                && grid.read_route_version(identity_after_search, revision_after_search) == FT_ERR_SUCCESS
                && grid_identity == identity_after_search && grid_revision == revision_after_search
                && copy_path_steps(out_path, this->_current_path) == FT_ERR_SUCCESS)
            {
                this->_cached_grid_identity = grid_identity;
                this->_cached_grid_revision = grid_revision;
                this->_needs_replan = FT_FALSE;
            }
        }
    }
    (void)this->unlock_internal(lock_acquired);
    return (this->set_error(result));
}

int32_t game_pathfinding::astar_grid(const game_map3d &grid,
    ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
    ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
    ft_vector<game_path_step> &out_path) const noexcept
{
    struct node
    {
        ft_size_t x;
        ft_size_t y;
        ft_size_t z;
        ft_size_t g;
        ft_size_t f;
        ft_size_t parent;
        ft_bool open;
    };
    struct frontier_entry
    {
        ft_size_t priority;
        ft_size_t node_index;
    };
    const auto compare_entries = [](const frontier_entry &left,
        const frontier_entry &right) noexcept -> ft_bool
    {
        if (left.priority != right.priority)
            return (left.priority > right.priority);
        return (left.node_index > right.node_index);
    };

    const ft_size_t width = grid.get_width();
    const ft_size_t height = grid.get_height();
    const ft_size_t depth = grid.get_depth();
    if (start_x >= width || start_y >= height || start_z >= depth
        || goal_x >= width || goal_y >= height || goal_z >= depth)
        return (FT_ERR_GAME_INVALID_MOVE);
    if (grid.get(start_x, start_y, start_z) != 0 || grid.get_error() != FT_ERR_SUCCESS
        || grid.get(goal_x, goal_y, goal_z) != 0 || grid.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_GAME_INVALID_MOVE);

    out_path.clear();
    if (out_path.get_error() != FT_ERR_SUCCESS)
        return (out_path.get_error());
    if (start_x == goal_x && start_y == goal_y && start_z == goal_z)
    {
        game_path_step step;
        (void)step.initialize();
        (void)step.set_coordinates(start_x, start_y, start_z);
        return (out_path.push_back(step));
    }
    constexpr ft_size_t MAX_SIZE = FT_SYSTEM_SIZE_MAX;
    if (width > MAX_SIZE / height || width * height > MAX_SIZE / depth)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t layer_stride = width * height;
    ft_vector<node> nodes;
    ft_priority_queue<frontier_entry, decltype(compare_entries)> open_set(0U, compare_entries);
    ft_unordered_map<ft_size_t, ft_size_t> node_indices;
    int32_t result = nodes.initialize();
    if (result == FT_ERR_SUCCESS)
        result = open_set.initialize();
    if (result == FT_ERR_SUCCESS)
        result = node_indices.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);

    const node start_node{start_x, start_y, start_z, 0U,
        distance_component(start_x, goal_x) + distance_component(start_y, goal_y)
        + distance_component(start_z, goal_z), MAX_SIZE, FT_TRUE};
    result = nodes.push_back(start_node);
    if (result != FT_ERR_SUCCESS)
        return (result);
    open_set.push(frontier_entry{start_node.f, 0U});
    if (open_set.get_error() != FT_ERR_SUCCESS)
        return (open_set.get_error());
    node_indices.insert(start_z * layer_stride + start_y * width + start_x, 0U);
    if (node_indices.get_error() != FT_ERR_SUCCESS)
        return (node_indices.get_error());

    while (!open_set.empty())
    {
        const frontier_entry next = open_set.pop();
        const ft_size_t current_index = next.node_index;
        if (!nodes[current_index].open || next.priority != nodes[current_index].f)
            continue ;
        nodes[current_index].open = FT_FALSE;
        const node current_node = nodes[current_index];
        if (current_node.x == goal_x && current_node.y == goal_y && current_node.z == goal_z)
        {
            game_path_step step;
            (void)step.initialize();
            ft_size_t path_index = 0U;
            ft_size_t trace_index = current_index;
            while (trace_index != MAX_SIZE)
            {
                ++path_index;
                trace_index = nodes[trace_index].parent;
            }
            out_path.resize(path_index, step);
            result = out_path.get_error();
            if (result != FT_ERR_SUCCESS)
                return (result);
            trace_index = current_index;
            while (path_index != 0U)
            {
                const node &previous = nodes[trace_index];
                --path_index;
                (void)out_path[path_index].set_coordinates(previous.x, previous.y, previous.z);
                trace_index = previous.parent;
            }
            return (FT_ERR_SUCCESS);
        }

        static constexpr int32_t DIRECTIONS[6][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0},
            {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
        ft_size_t neighbor_index = 0U;
        while (neighbor_index < 6U)
        {
            const int32_t *direction = DIRECTIONS[neighbor_index];
            const ft_size_t neighbor_x = current_node.x + static_cast<ft_size_t>(direction[0]);
            const ft_size_t neighbor_y = current_node.y + static_cast<ft_size_t>(direction[1]);
            const ft_size_t neighbor_z = current_node.z + static_cast<ft_size_t>(direction[2]);
            ++neighbor_index;
            if (neighbor_x >= width || neighbor_y >= height || neighbor_z >= depth
                || grid.get(neighbor_x, neighbor_y, neighbor_z) != 0
                || grid.get_error() != FT_ERR_SUCCESS)
                continue ;

            const ft_size_t key = neighbor_z * layer_stride + neighbor_y * width + neighbor_x;
            const ft_size_t search_index = node_indices.at(key);
            const int32_t lookup_error = node_indices.get_error();
            const ft_size_t tentative_g = current_node.g + 1U;
            if (lookup_error == FT_ERR_SUCCESS && tentative_g >= nodes[search_index].g)
                continue ;
            if (lookup_error != FT_ERR_SUCCESS && lookup_error != FT_ERR_NOT_FOUND)
                return (lookup_error);

            const ft_size_t remaining = distance_component(neighbor_x, goal_x)
                + distance_component(neighbor_y, goal_y) + distance_component(neighbor_z, goal_z);
            ft_size_t priority = MAX_SIZE;
            if (tentative_g <= MAX_SIZE - remaining)
                priority = tentative_g + remaining;
            if (lookup_error == FT_ERR_SUCCESS)
            {
                node &existing = nodes[search_index];
                // Queue the improved priority and discard the old entry when it reaches the top.
                if (existing.open && priority != existing.f)
                {
                    open_set.push(frontier_entry{priority, search_index});
                    if (open_set.get_error() != FT_ERR_SUCCESS)
                        return (open_set.get_error());
                }
                existing.g = tentative_g;
                existing.f = priority;
                existing.parent = current_index;
                continue ;
            }
            const node new_node{neighbor_x, neighbor_y, neighbor_z, tentative_g, priority,
                current_index, FT_TRUE};
            const ft_size_t new_index = nodes.size();
            result = nodes.push_back(new_node);
            if (result != FT_ERR_SUCCESS)
                return (result);
            open_set.push(frontier_entry{priority, new_index});
            if (open_set.get_error() != FT_ERR_SUCCESS)
                return (open_set.get_error());
            node_indices.insert(key, new_index);
            if (node_indices.get_error() != FT_ERR_SUCCESS)
                return (node_indices.get_error());
        }
    }
    return (FT_ERR_GAME_INVALID_MOVE);
}

int32_t game_pathfinding::dijkstra_graph(const ft_graph<int32_t> &graph,
    ft_size_t start_vertex, ft_size_t goal_vertex,
    ft_vector<ft_size_t> &out_path) const noexcept
{
    out_path.clear();
    if (out_path.get_error() != FT_ERR_SUCCESS)
        return (out_path.get_error());
    const ft_size_t vertex_count = graph.size();
    if (graph.get_error() != FT_ERR_SUCCESS)
        return (graph.get_error());
    if (start_vertex >= vertex_count || goal_vertex >= vertex_count)
        return (FT_ERR_GAME_INVALID_MOVE);
    if (start_vertex == goal_vertex)
        return (out_path.push_back(start_vertex));

    ft_vector<ft_size_t> previous;
    ft_vector<ft_size_t> queue;
    ft_vector<ft_size_t> neighbors;
    int32_t result = previous.initialize();
    if (result == FT_ERR_SUCCESS)
        result = queue.initialize();
    if (result == FT_ERR_SUCCESS)
        result = neighbors.initialize();
    if (result != FT_ERR_SUCCESS)
        return (result);
    previous.resize(vertex_count, vertex_count);
    if (previous.get_error() != FT_ERR_SUCCESS)
        return (previous.get_error());
    previous[start_vertex] = start_vertex;
    result = queue.push_back(start_vertex);
    if (result != FT_ERR_SUCCESS)
        return (result);

    ft_size_t queue_index = 0U;
    ft_bool found = FT_FALSE;
    // Each edge costs one, so discovery order is also shortest-distance order.
    while (!found && queue_index < queue.size())
    {
        const ft_size_t current_vertex = queue[queue_index];
        ++queue_index;
        neighbors.clear();
        graph.neighbors(current_vertex, neighbors);
        if (graph.get_error() != FT_ERR_SUCCESS)
            return (graph.get_error());
        const ft_size_t neighbor_count = neighbors.size();
        ft_size_t neighbor_index = 0U;
        while (neighbor_index < neighbor_count)
        {
            const ft_size_t neighbor = neighbors[neighbor_index];
            ++neighbor_index;
            if (neighbor >= vertex_count || previous[neighbor] != vertex_count)
                continue ;
            previous[neighbor] = current_vertex;
            if (neighbor == goal_vertex)
            {
                found = FT_TRUE;
                break ;
            }
            result = queue.push_back(neighbor);
            if (result != FT_ERR_SUCCESS)
                return (result);
        }
    }
    if (!found)
        return (FT_ERR_GAME_INVALID_MOVE);

    ft_size_t path_length = 1U;
    ft_size_t vertex = goal_vertex;
    while (vertex != start_vertex)
    {
        ++path_length;
        vertex = previous[vertex];
    }
    out_path.resize(path_length);
    result = out_path.get_error();
    if (result != FT_ERR_SUCCESS)
        return (result);
    vertex = goal_vertex;
    while (path_length != 0U)
    {
        --path_length;
        out_path[path_length] = vertex;
        vertex = previous[vertex];
    }
    return (FT_ERR_SUCCESS);
}

int32_t game_pathfinding::get_error() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_pathfinding::get_error");
    return (game_pathfinding::_last_error);
}

const char *game_pathfinding::get_error_str() const noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
            "game_pathfinding::get_error_str");
    return (ft_strerror(game_pathfinding::_last_error));
}
