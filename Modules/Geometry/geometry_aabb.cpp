#include "aabb.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Errno/errno_internal.hpp"
#include <functional>
#include <new>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

uint32_t aabb::lock_pair(const aabb &other, const aabb *&lower,
    const aabb *&upper) const
{
    const aabb *ordered_first;
    const aabb *ordered_second;
    uint32_t lower_error;
    uint32_t upper_error;
    std::less<const aabb *> pointer_order;
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

void aabb::unlock_pair(const aabb *lower, const aabb *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

aabb::aabb() noexcept
    : _minimum_x(0.0)
    , _minimum_y(0.0)
    , _maximum_x(0.0)
    , _maximum_y(0.0)
    , _mutex(ft_nullptr)
    , _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

int32_t aabb::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "aabb::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_minimum_x = 0.0;
    this->_minimum_y = 0.0;
    this->_maximum_x = 0.0;
    this->_maximum_y = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t aabb::initialize(double minimum_x, double minimum_y,
    double maximum_x, double maximum_y) noexcept
{
    uint32_t initialize_error;

    initialize_error = this->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    if (minimum_x > maximum_x || minimum_y > maximum_y)
    {
        (void)this->destroy();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_minimum_x = minimum_x;
    this->_minimum_y = minimum_y;
    this->_maximum_x = maximum_x;
    this->_maximum_y = maximum_y;
    return (FT_ERR_SUCCESS);
}

int32_t aabb::initialize(const aabb &other) noexcept
{
    uint32_t destroy_error;
    uint32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        errno_abort_lifecycle(other._initialised_state,
            "aabb::initialize(const aabb &) source",
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
    this->_minimum_x = 0.0;
    this->_minimum_y = 0.0;
    this->_maximum_x = 0.0;
    this->_maximum_y = 0.0;
    this->_minimum_x = other._minimum_x;
    this->_minimum_y = other._minimum_y;
    this->_maximum_x = other._maximum_x;
    this->_maximum_y = other._maximum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::move(aabb &other) noexcept
{
    const aabb *lower;
    const aabb *upper;
    ft_bool source_uninitialised;
    uint32_t lock_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
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
        errno_abort_lifecycle(other._initialised_state, "aabb::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->unlock_pair(lower, upper);
        return (FT_ERR_SUCCESS);
    }
    this->_minimum_x = other._minimum_x;
    this->_minimum_y = other._minimum_y;
    this->_maximum_x = other._maximum_x;
    this->_maximum_y = other._maximum_y;
    other._minimum_x = 0.0;
    other._minimum_y = 0.0;
    other._maximum_x = 0.0;
    other._maximum_y = 0.0;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->unlock_pair(lower, upper);
    return (FT_ERR_SUCCESS);
}

int32_t aabb::initialize(aabb &&other) noexcept
{
    return (this->move(other));
}

uint32_t aabb::destroy() noexcept
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
    this->_minimum_x = 0.0;
    this->_minimum_y = 0.0;
    this->_maximum_x = 0.0;
    this->_maximum_y = 0.0;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (FT_ERR_SUCCESS);
}

aabb::~aabb() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

uint32_t aabb::set_bounds(double minimum_x, double minimum_y,
    double maximum_x, double maximum_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_bounds");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (minimum_x > maximum_x || minimum_y > maximum_y)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_minimum_x = minimum_x;
    this->_minimum_y = minimum_y;
    this->_maximum_x = maximum_x;
    this->_maximum_y = maximum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_minimum(double minimum_x, double minimum_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_minimum");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (minimum_x > this->_maximum_x || minimum_y > this->_maximum_y)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_minimum_x = minimum_x;
    this->_minimum_y = minimum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_minimum_x(double minimum_x)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_minimum_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (minimum_x > this->_maximum_x)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_minimum_x = minimum_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_minimum_y(double minimum_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_minimum_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (minimum_y > this->_maximum_y)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_minimum_y = minimum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_maximum(double maximum_x, double maximum_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_maximum");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (maximum_x < this->_minimum_x || maximum_y < this->_minimum_y)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_maximum_x = maximum_x;
    this->_maximum_y = maximum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_maximum_x(double maximum_x)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_maximum_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (maximum_x < this->_minimum_x)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_maximum_x = maximum_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t aabb::set_maximum_y(double maximum_y)
{
    uint32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::set_maximum_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (maximum_y < this->_minimum_y)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_maximum_y = maximum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

double aabb::get_minimum_x() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::get_minimum_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_minimum_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double aabb::get_minimum_y() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::get_minimum_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_minimum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double aabb::get_maximum_x() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::get_maximum_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_maximum_x;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

double aabb::get_maximum_y() const
{
    uint32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::get_maximum_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0.0);
    value = this->_maximum_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (value);
}

uint32_t aabb::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    uint32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::enable_thread_safety");
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

uint32_t aabb::disable_thread_safety() noexcept
{
    uint32_t mutex_error;
    pt_recursive_mutex *mutex_pointer;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "aabb::disable_thread_safety");
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

ft_bool aabb::is_thread_safe() const noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_FALSE);
    return (FT_TRUE);
}
