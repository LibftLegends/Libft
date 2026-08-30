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
    : _current_path(), _needs_replan(FT_FALSE), _mutex(ft_nullptr),
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
    ft_size_t index;
    ft_size_t count;

    if (&other == this)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state, "game_pathfinding::move", "source object is uninitialised");
        this->set_error(FT_ERR_INVALID_STATE);
        return (FT_ERR_INVALID_STATE);
    }
    destroy_error = this->destroy();
    if (destroy_error != FT_ERR_SUCCESS)
    {
        this->set_error(destroy_error);
        return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->set_error(other.get_error());
        return (FT_ERR_SUCCESS);
    }
    this->_current_path.clear();
    index = 0;
    count = other._current_path.size();
    while (index < count)
    {
        this->_current_path.push_back(other._current_path[index]);
        index += 1;
    }
    this->_needs_replan = other._needs_replan;
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._current_path.clear();
    other._needs_replan = FT_FALSE;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->set_error(other.get_error());
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

void game_pathfinding::update_obstacle(ft_size_t x, ft_size_t y, ft_size_t z,
    int32_t value) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;
    ft_size_t index;

    (void)value;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::update_obstacle");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return ;
    }
    index = 0;
    while (index < this->_current_path.size())
    {
        if (this->_current_path[index].get_x() == x
            && this->_current_path[index].get_y() == y
            && this->_current_path[index].get_z() == z)
        {
            this->_needs_replan = FT_TRUE;
            this->set_error(FT_ERR_SUCCESS);
    (void)this->unlock_internal(lock_acquired);
            return ;
        }
        index += 1;
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
    ft_bool lock_acquired;
    int32_t lock_error;
    int32_t result;
    ft_size_t index;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "game_pathfinding::recalculate_path");
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->set_error(lock_error);
        return (lock_error);
    }
    if (this->_needs_replan == FT_FALSE && this->_current_path.size() > 0)
    {
        out_path.clear();
        index = 0;
        while (index < this->_current_path.size())
        {
            out_path.push_back(this->_current_path[index]);
            index += 1;
        }
        this->set_error(FT_ERR_SUCCESS);

        (void)this->unlock_internal(lock_acquired);
        return (FT_ERR_SUCCESS);
    }
    result = this->astar_grid(grid, start_x, start_y, start_z,
            goal_x, goal_y, goal_z, out_path);
    if (result == FT_ERR_SUCCESS)
    {
        this->_current_path.clear();
        index = 0;
        while (index < out_path.size())
        {
            this->_current_path.push_back(out_path[index]);
            index += 1;
        }
        this->_needs_replan = FT_FALSE;
    }
    else
        this->_needs_replan = FT_TRUE;
    (void)this->unlock_internal(lock_acquired);
    this->set_error(result);
    return (result);
}

int32_t game_pathfinding::astar_grid(const game_map3d &grid,
    ft_size_t start_x, ft_size_t start_y, ft_size_t start_z,
    ft_size_t goal_x, ft_size_t goal_y, ft_size_t goal_z,
    ft_vector<game_path_step> &out_path) const noexcept
{
    struct node
    {
        ft_size_t  x;
        ft_size_t  y;
        ft_size_t  z;
        int32_t     g;
        int32_t     f;
        int32_t     parent;
    };

    ft_vector<node> nodes;
    ft_vector<ft_size_t> open_set;
    int32_t initialize_error;

    if (start_x >= grid.get_width() || start_y >= grid.get_height()
        || start_z >= grid.get_depth() || goal_x >= grid.get_width()
        || goal_y >= grid.get_height() || goal_z >= grid.get_depth())
        return (FT_ERR_GAME_INVALID_MOVE);
    if (grid.get(start_x, start_y, start_z) != 0
        || grid.get(goal_x, goal_y, goal_z) != 0)
        return (FT_ERR_GAME_INVALID_MOVE);

    initialize_error = nodes.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    initialize_error = open_set.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)nodes.destroy();
        return (initialize_error);
    }
    out_path.clear();

    node start_node;
    start_node.x = start_x;
    start_node.y = start_y;
    start_node.z = start_z;
    start_node.g = 0;
    start_node.f = static_cast<int32_t>(distance_component(start_x, goal_x)
            + distance_component(start_y, goal_y)
            + distance_component(start_z, goal_z));
    start_node.parent = -1;
    nodes.push_back(start_node);
    open_set.push_back(0);

    while (open_set.size() > 0)
    {
        ft_size_t best_open;
        ft_size_t open_index;
        ft_size_t current_index;

        best_open = 0;
        open_index = 0;
        while (open_index < open_set.size())
        {
            if (nodes[open_set[open_index]].f < nodes[open_set[best_open]].f)
                best_open = open_index;
            open_index += 1;
        }
        current_index = open_set[best_open];

        if (nodes[current_index].x == goal_x && nodes[current_index].y == goal_y
            && nodes[current_index].z == goal_z)
        {
            ft_vector<game_path_step> reverse_path;
            ft_size_t trace_index;
            ft_size_t reverse_index;

            initialize_error = reverse_path.initialize();
            if (initialize_error != FT_ERR_SUCCESS)
            {
                (void)open_set.destroy();
                (void)nodes.destroy();
                return (initialize_error);
            }
            trace_index = current_index;
            while (FT_TRUE)
            {
                game_path_step step;

                (void)step.initialize();
                (void)step.set_coordinates(nodes[trace_index].x,
                    nodes[trace_index].y, nodes[trace_index].z);
                reverse_path.push_back(step);
                if (nodes[trace_index].parent == -1)
                    break ;
                trace_index = static_cast<ft_size_t>(nodes[trace_index].parent);
            }
            reverse_index = reverse_path.size();
            while (reverse_index > 0)
            {
                reverse_index -= 1;
                out_path.push_back(reverse_path[reverse_index]);
            }
            (void)reverse_path.destroy();
            (void)open_set.destroy();
            (void)nodes.destroy();
            return (FT_ERR_SUCCESS);
        }

        open_set.erase(open_set.begin() + best_open);

        int32_t direction_x[6] = {1, -1, 0, 0, 0, 0};
        int32_t direction_y[6] = {0, 0, 1, -1, 0, 0};
        int32_t direction_z[6] = {0, 0, 0, 0, 1, -1};
        ft_size_t neighbor_index;

        neighbor_index = 0;
        while (neighbor_index < 6)
        {
            ft_size_t neighbor_x;
            ft_size_t neighbor_y;
            ft_size_t neighbor_z;

            neighbor_x = nodes[current_index].x;
            neighbor_y = nodes[current_index].y;
            neighbor_z = nodes[current_index].z;
            if (direction_x[neighbor_index] > 0)
                neighbor_x += 1;
            else if (direction_x[neighbor_index] < 0)
                neighbor_x -= 1;
            if (direction_y[neighbor_index] > 0)
                neighbor_y += 1;
            else if (direction_y[neighbor_index] < 0)
                neighbor_y -= 1;
            if (direction_z[neighbor_index] > 0)
                neighbor_z += 1;
            else if (direction_z[neighbor_index] < 0)
                neighbor_z -= 1;
            if (neighbor_x < grid.get_width() && neighbor_y < grid.get_height()
                && neighbor_z < grid.get_depth()
                && grid.get(neighbor_x, neighbor_y, neighbor_z) == 0)
            {
                ft_bool found;
                ft_size_t search_index;
                int32_t tentative_g;

                found = FT_FALSE;
                search_index = 0;
                while (search_index < nodes.size())
                {
                    if (nodes[search_index].x == neighbor_x
                        && nodes[search_index].y == neighbor_y
                        && nodes[search_index].z == neighbor_z)
                    {
                        found = FT_TRUE;
                        break ;
                    }
                    search_index += 1;
                }
                tentative_g = nodes[current_index].g + 1;
                if (found)
                {
                    if (tentative_g < nodes[search_index].g)
                    {
                        nodes[search_index].g = tentative_g;
                        nodes[search_index].f = tentative_g + static_cast<int32_t>(
                                distance_component(neighbor_x, goal_x)
                                + distance_component(neighbor_y, goal_y)
                                + distance_component(neighbor_z, goal_z));
                        nodes[search_index].parent = static_cast<int32_t>(current_index);
                    }
                }
                else
                {
                    node new_node;

                    new_node.x = neighbor_x;
                    new_node.y = neighbor_y;
                    new_node.z = neighbor_z;
                    new_node.g = tentative_g;
                    new_node.f = tentative_g + static_cast<int32_t>(
                            distance_component(neighbor_x, goal_x)
                            + distance_component(neighbor_y, goal_y)
                            + distance_component(neighbor_z, goal_z));
                    new_node.parent = static_cast<int32_t>(current_index);
                    nodes.push_back(new_node);
                    open_set.push_back(nodes.size() - 1);
                }
            }
            neighbor_index += 1;
        }
    }
    (void)open_set.destroy();
    (void)nodes.destroy();
    return (FT_ERR_GAME_INVALID_MOVE);
}

int32_t game_pathfinding::dijkstra_graph(const ft_graph<int32_t> &graph,
    ft_size_t start_vertex, ft_size_t goal_vertex,
    ft_vector<ft_size_t> &out_path) const noexcept
{
    ft_vector<int32_t> distance;
    ft_vector<int32_t> previous;
    ft_vector<ft_size_t> queue;
    int32_t initialize_error;

    initialize_error = distance.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    initialize_error = previous.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)distance.destroy();
        return (initialize_error);
    }
    initialize_error = queue.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)previous.destroy();
        (void)distance.destroy();
        return (initialize_error);
    }
    out_path.clear();
    if (start_vertex >= graph.size() || goal_vertex >= graph.size())
    {
        (void)queue.destroy();
        (void)previous.destroy();
        (void)distance.destroy();
        return (FT_ERR_GAME_INVALID_MOVE);
    }

    distance.resize(graph.size(), -1);
    previous.resize(graph.size(), -1);
    distance[start_vertex] = 0;
    queue.push_back(start_vertex);

    while (queue.size() > 0)
    {
        ft_size_t best_queue;
        ft_size_t queue_index;
        ft_size_t current;

        best_queue = 0;
        queue_index = 0;
        while (queue_index < queue.size())
        {
            if (distance[queue[queue_index]] < distance[queue[best_queue]])
                best_queue = queue_index;
            queue_index += 1;
        }
        current = queue[best_queue];
        queue.erase(queue.begin() + best_queue);
        if (current == goal_vertex)
            break ;

        ft_vector<ft_size_t> neighbors;
        ft_size_t neighbor_index;

        initialize_error = neighbors.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
        {
            (void)queue.destroy();
            (void)previous.destroy();
            (void)distance.destroy();
            return (initialize_error);
        }
        graph.neighbors(current, neighbors);
        neighbor_index = 0;
        while (neighbor_index < neighbors.size())
        {
            ft_size_t neighbor;
            int32_t alternative_distance;

            neighbor = neighbors[neighbor_index];
            alternative_distance = distance[current] + 1;
            if (distance[neighbor] == -1 || alternative_distance < distance[neighbor])
            {
                distance[neighbor] = alternative_distance;
                previous[neighbor] = static_cast<int32_t>(current);
                queue.push_back(neighbor);
            }
            neighbor_index += 1;
        }
        (void)neighbors.destroy();
    }

    if (distance[goal_vertex] == -1)
    {
        (void)queue.destroy();
        (void)previous.destroy();
        (void)distance.destroy();
        return (FT_ERR_GAME_INVALID_MOVE);
    }

    ft_vector<ft_size_t> reverse_path;
    ft_size_t vertex;

    initialize_error = reverse_path.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)queue.destroy();
        (void)previous.destroy();
        (void)distance.destroy();
        return (initialize_error);
    }
    vertex = goal_vertex;
    while (FT_TRUE)
    {
        reverse_path.push_back(vertex);
        if (vertex == start_vertex)
            break ;
        vertex = static_cast<ft_size_t>(previous[vertex]);
    }
    while (reverse_path.size() > 0)
    {
        out_path.push_back(reverse_path[reverse_path.size() - 1]);
        reverse_path.pop_back();
    }
    (void)reverse_path.destroy();
    (void)queue.destroy();
    (void)previous.destroy();
    (void)distance.destroy();
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
