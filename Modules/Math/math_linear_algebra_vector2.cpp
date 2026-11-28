#include "linear_algebra.hpp"
#include "math.hpp"
#include "math_vector_norm.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <new>
#include <functional>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "math_interval.hpp"
#if defined(__SSE2__)
#include <immintrin.h>
#endif

static void vector2_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

#if defined(__SSE2__)
static double vector2_compute_dot(double first_x, double first_y,
    double second_x, double second_y)
{
    __m128d first_vector;
    __m128d second_vector;
    __m128d product;
    __m128d swapped;
    __m128d sum;

    first_vector = _mm_set_pd(first_y, first_x);
    second_vector = _mm_set_pd(second_y, second_x);
    product = _mm_mul_pd(first_vector, second_vector);
    swapped = _mm_shuffle_pd(product, product, 0x1);
    sum = _mm_add_sd(product, swapped);
    return (_mm_cvtsd_f64(sum));
}
#else
static double vector2_compute_dot(double first_x, double first_y,
    double second_x, double second_y)
{
    return (first_x * second_x + first_y * second_y);
}
#endif

int32_t vector2::lock_pair(const vector2 &first, const vector2 &second,
    const vector2 *&lower, const vector2 *&upper)
{
    if (&first == &second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    lower = &first;
    upper = &second;
    if (std::less<const vector2 *>{}(upper, lower))
    {
        const vector2 *temporary;

        temporary = lower;
        lower = upper;
        upper = temporary;
    }
    while (FT_TRUE)
    {
        int32_t lower_error = pt_recursive_mutex_lock_if_not_null(lower->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
        {
            return (lower_error);
        }
        int32_t upper_error = pt_recursive_mutex_lock_if_not_null(upper->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
        {
            return (FT_ERR_SUCCESS);
        }
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
            return (upper_error);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
        vector2_sleep_backoff();
    }
}

void vector2::unlock_pair(const vector2 *lower, const vector2 *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

double vector2::get_x() const
{
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::get_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    value = this->_x_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector2::set_error(FT_ERR_SUCCESS);
    return (value);
}

double vector2::get_y() const
{
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::get_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    value = this->_y_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector2::set_error(FT_ERR_SUCCESS);
    return (value);
}

vector2 vector2::add(const vector2 &other) const
{
    const vector2 *lower;
    const vector2 *upper;
    int32_t lock_error;
    double result_x;
    double result_y;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::add");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "vector2::add source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(static_cast<uint32_t>(lock_error));
        return (vector2());
    }
    result_x = this->_x_component + other._x_component;
    result_y = this->_y_component + other._y_component;
    this->unlock_pair(lower, upper);
    vector2::set_error(FT_ERR_SUCCESS);
    return (vector2(result_x, result_y));
}

vector2 vector2::subtract(const vector2 &other) const
{
    const vector2 *lower;
    const vector2 *upper;
    int32_t lock_error;
    double result_x;
    double result_y;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::subtract");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "vector2::subtract source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(static_cast<uint32_t>(lock_error));
        return (vector2());
    }
    result_x = this->_x_component - other._x_component;
    result_y = this->_y_component - other._y_component;
    this->unlock_pair(lower, upper);
    vector2::set_error(FT_ERR_SUCCESS);
    return (vector2(result_x, result_y));
}

double vector2::dot(const vector2 &other) const
{
    const vector2 *lower;
    const vector2 *upper;
    int32_t lock_error;
    double result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::dot");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "vector2::dot source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    result = vector2_compute_dot(this->_x_component, this->_y_component, other._x_component, other._y_component);
    this->unlock_pair(lower, upper);
    vector2::set_error(FT_ERR_SUCCESS);
    return (result);
}

double vector2::length() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::length");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(lock_error);
        return (0.0);
    }
    const double components[] = {this->_x_component, this->_y_component};
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    const double result = math_vector_length(components);
    vector2::set_error(FT_ERR_SUCCESS);
    return (result);
}

vector2 vector2::normalize() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector2::normalize");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(lock_error);
        return (vector2());
    }
    double components[] = {this->_x_component, this->_y_component};
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector2::set_error(FT_ERR_SUCCESS);
    if (!math_vector_normalize(components))
        return (vector2());
    return (vector2(components[0], components[1]));
}

uint32_t vector2::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector2::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (vector2::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (vector2::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (vector2::set_error(static_cast<uint32_t>(mutex_error)));
    }
    this->_mutex = mutex_pointer;
    return (vector2::set_error(FT_ERR_SUCCESS));
}

uint32_t vector2::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector2::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (vector2::set_error(static_cast<uint32_t>(mutex_error)));
    }
    return (vector2::set_error(FT_ERR_SUCCESS));
}

ft_bool vector2::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector2::is_thread_safe");
    vector2::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}
