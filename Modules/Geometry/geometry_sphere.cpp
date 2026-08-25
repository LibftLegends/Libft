#include "sphere.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include <functional>
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

sphere::sphere() noexcept
    : _center_x(0.0)
    , _center_y(0.0)
    , _center_z(0.0)
    , _radius(0.0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t sphere::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "sphere::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_center_z = 0.0;
    this->_radius = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t sphere::initialize(double center_x, double center_y, double center_z,
    double radius) noexcept
{
    uint32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    this->_center_x = center_x;
    this->_center_y = center_y;
    this->_center_z = center_z;
    this->_radius = radius;
    return (FT_ERR_SUCCESS);
}

int32_t sphere::initialize(const sphere &other) noexcept
{
    uint32_t destroy_error;
    uint32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "sphere::initialize(const sphere &) source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_center_z = 0.0;
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
    this->_center_z = other._center_z;
    this->_radius = other._radius;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t sphere::move(sphere &other) noexcept
{
    const sphere *lower;
    const sphere *upper;
    ft_bool source_uninitialised;
    uint32_t lock_error;

    if (this == &other)
    {
        if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        {
            errno_abort_lifecycle(this->_initialised_state,
                "sphere::move source", "called with uninitialised source object");
            return (FT_ERR_INVALID_STATE);
        }
        return (FT_ERR_SUCCESS);
    }
    lock_error = this->lock_pair(other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (lock_error);
    }
    source_uninitialised = FT_FALSE;
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        source_uninitialised = FT_TRUE;
    if (source_uninitialised != FT_FALSE)
    {
        this->unlock_pair(lower, upper);
        errno_abort_lifecycle(other._initialised_state, "sphere::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->unlock_pair(lower, upper);
        return (FT_ERR_SUCCESS);
    }
    this->_center_x = other._center_x;
    this->_center_y = other._center_y;
    this->_center_z = other._center_z;
    this->_radius = other._radius;
    other._center_x = 0.0;
    other._center_y = 0.0;
    other._center_z = 0.0;
    other._radius = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->unlock_pair(lower, upper);
    return (FT_ERR_SUCCESS);
}

int32_t sphere::initialize(sphere &&other) noexcept
{
    return (this->move(other));
}

uint32_t sphere::destroy() noexcept
{
    uint32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS
        && this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->_center_x = 0.0;
    this->_center_y = 0.0;
    this->_center_z = 0.0;
    this->_radius = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (FT_ERR_SUCCESS);
}

sphere::~sphere() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

uint32_t sphere::set_center(double center_x, double center_y, double center_z)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::set_center");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_x = center_x;
    this->_center_y = center_y;
    this->_center_z = center_z;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t sphere::set_center_x(double center_x)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::set_center_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_x = center_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t sphere::set_center_y(double center_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::set_center_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_y = center_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t sphere::set_center_z(double center_z)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::set_center_z");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_center_z = center_z;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t sphere::set_radius(double radius)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::set_radius");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_radius = radius;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

double sphere::get_center_x() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::get_center_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_center_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double sphere::get_center_y() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::get_center_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_center_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double sphere::get_center_z() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::get_center_z");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_center_z;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double sphere::get_radius() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::get_radius");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_radius;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

uint32_t sphere::lock_pair(const sphere &other, const sphere *&lower,
    const sphere *&upper) const
{
    const sphere *ordered_first;
    const sphere *ordered_second;
    uint32_t lower_error;
    uint32_t upper_error;
    std::less<const sphere *> pointer_order;
    std::less<const pt_recursive_mutex *> mutex_order;

    ordered_first = this;
    ordered_second = &other;
    if (ordered_first == ordered_second)
    {
        lower = this;
        upper = this;
        return (pt_recursive_mutex_lock_if_not_null(this->_mutex));
    }
    if (ordered_first->_mutex == ft_nullptr
        && ordered_second->_mutex != ft_nullptr)
    {
        ordered_first = &other;
        ordered_second = this;
    }
    else if (ordered_first->_mutex != ft_nullptr
        && ordered_second->_mutex != ft_nullptr
        && mutex_order(ordered_second->_mutex, ordered_first->_mutex))
    {
        ordered_first = &other;
        ordered_second = this;
    }
    else if (ordered_first->_mutex == ft_nullptr
        && ordered_second->_mutex == ft_nullptr
        && pointer_order(ordered_second, ordered_first))
    {
        ordered_first = &other;
        ordered_second = this;
    }
    lower = ordered_first;
    upper = ordered_second;
    upper_error = FT_ERR_MUTEX_ALREADY_LOCKED;
    while (true)
    {
        lower_error = pt_recursive_mutex_lock_if_not_null(lower->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
        {
            if (lower_error != FT_ERR_MUTEX_ALREADY_LOCKED)
                return (lower_error);
            (void)pt_thread_yield();
            continue ;
        }
        upper_error = pt_recursive_mutex_lock_if_not_null(upper->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
            return (upper_error);
        (void)pt_thread_yield();
    }
}

void sphere::unlock_pair(const sphere *lower, const sphere *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

uint32_t sphere::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    uint32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::enable_thread_safety");
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

uint32_t sphere::disable_thread_safety() noexcept
{
    uint32_t mutex_error;
    pt_recursive_mutex *mutex_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "sphere::disable_thread_safety");
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

ft_bool sphere::is_thread_safe() const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}
