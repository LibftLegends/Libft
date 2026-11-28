#include "circle.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include "geometry_pair_lock.hpp"
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

circle::circle() noexcept
    : _center_x(0.0)
    , _center_y(0.0)
    , _radius(0.0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t circle::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "circle::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    if (this->_mutex != ft_nullptr)
    {
        uint32_t cleanup_error = this->release_mutex();
        if (cleanup_error != FT_ERR_SUCCESS)
            return (cleanup_error);
    }
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_radius = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t circle::initialize(double center_x, double center_y, double radius) noexcept
{
    uint32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_center_x = center_x;
    this->_center_y = center_y;
    this->_radius = radius;
    return (FT_ERR_SUCCESS);
}

int32_t circle::initialize(const circle &other) noexcept
{
    uint32_t destroy_error;
    uint32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "circle::initialize(const circle &) source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED
        || this->_mutex != ft_nullptr)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_radius = 0.0;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    this->_center_x = other._center_x;
    this->_center_y = other._center_y;
    this->_radius = other._radius;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t circle::move(circle &other) noexcept
{
    const pt_recursive_mutex *lower;
    const pt_recursive_mutex *upper;
    uint32_t lock_error;

    if (this == &other)
    {
        if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        {
            errno_abort_lifecycle(this->_initialised_state,
                "circle::move source", "called with uninitialised source object");
            return (FT_ERR_INVALID_STATE);
        }
        return (FT_ERR_SUCCESS);
    }
    lower = this->_mutex;
    upper = other._mutex;
    lock_error = geometry_lock_pair(lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        geometry_unlock_pair(lower, upper);
        errno_abort_lifecycle(other._initialised_state, "circle::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        geometry_unlock_pair(lower, upper);
        return (FT_ERR_SUCCESS);
    }
    this->_center_x = other._center_x;
    this->_center_y = other._center_y;
    this->_radius = other._radius;
    other._center_x = 0.0;
    other._center_y = 0.0;
    other._radius = 0.0;
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    geometry_unlock_pair(lower, upper);
    return (FT_ERR_SUCCESS);
}

int32_t circle::initialize(circle &&other) noexcept
{
    return (this->move(other));
}

uint32_t circle::destroy() noexcept
{
    uint32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (this->release_mutex());
    disable_error = this->disable_thread_safety();
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_radius = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (FT_ERR_SUCCESS);
}

circle::~circle() noexcept
{
    (void)this->destroy();
    return ;
}

uint32_t circle::set_center(double center_x, double center_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::set_center");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_x = center_x;
    this->_center_y = center_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t circle::set_center_x(double center_x)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::set_center_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_x = center_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t circle::set_center_y(double center_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::set_center_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_y = center_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t circle::set_radius(double radius)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::set_radius");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_radius = radius;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

double circle::get_center_x() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::get_center_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_center_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double circle::get_center_y() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::get_center_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_center_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double circle::get_radius() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::get_radius");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_radius;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

uint32_t circle::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    uint32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

uint32_t circle::disable_thread_safety() noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "circle::disable_thread_safety");
    return (this->release_mutex());
}

uint32_t circle::release_mutex() noexcept
{
    uint32_t mutex_error;
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (mutex_error != FT_ERR_SUCCESS)
        return (mutex_error);
    return (FT_ERR_SUCCESS);
}

ft_bool circle::is_thread_safe() const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}
