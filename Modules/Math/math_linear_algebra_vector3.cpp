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

static void vector3_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

#if defined(__SSE2__)
static double vector3_compute_dot(double first_x, double first_y, double first_z,
    double second_x, double second_y, double second_z)
{
    __m128d first_xy;
    __m128d second_xy;
    __m128d product_xy;
    __m128d swapped;
    __m128d sum_xy;
    __m128d first_z_vector;
    __m128d second_z_vector;
    __m128d product_z;
    __m128d total;

    first_xy = _mm_set_pd(first_y, first_x);
    second_xy = _mm_set_pd(second_y, second_x);
    product_xy = _mm_mul_pd(first_xy, second_xy);
    swapped = _mm_shuffle_pd(product_xy, product_xy, 0x1);
    sum_xy = _mm_add_sd(product_xy, swapped);
    first_z_vector = _mm_set_sd(first_z);
    second_z_vector = _mm_set_sd(second_z);
    product_z = _mm_mul_sd(first_z_vector, second_z_vector);
    total = _mm_add_sd(sum_xy, product_z);
    return (_mm_cvtsd_f64(total));
}
#else
static double vector3_compute_dot(double first_x, double first_y, double first_z,
    double second_x, double second_y, double second_z)
{
    return (first_x * second_x + first_y * second_y + first_z * second_z);
}
#endif

int32_t vector3::lock_pair(const vector3 &first, const vector3 &second,
    const vector3 *&lower, const vector3 *&upper)
{
    const vector3 *ordered_first;
    const vector3 *ordered_second;

    if (&first == &second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    ordered_first = &first;
    ordered_second = &second;
    if (std::less<const vector3 *>{}(ordered_second, ordered_first))
    {
        const vector3 *temporary;

        temporary = ordered_first;
        ordered_first = ordered_second;
        ordered_second = temporary;
    }
    lower = ordered_first;
    upper = ordered_second;
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
        vector3_sleep_backoff();
    }
}

void vector3::unlock_pair(const vector3 *lower, const vector3 *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

int32_t vector3::get_components(double &x, double &y, double &z) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector3::get_components");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (vector3::set_error(lock_error));
    x = this->_x_component;
    y = this->_y_component;
    z = this->_z_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (vector3::set_error(FT_ERR_SUCCESS));
}

double vector3::get_x() const
{
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::get_x");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    value = this->_x_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector3::set_error(FT_ERR_SUCCESS);
    return (value);
}

double vector3::get_y() const
{
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::get_y");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    value = this->_y_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector3::set_error(FT_ERR_SUCCESS);
    return (value);
}

double vector3::get_z() const
{
    int32_t lock_error;
    double value;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::get_z");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    value = this->_z_component;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector3::set_error(FT_ERR_SUCCESS);
    return (value);
}

vector3 vector3::add(const vector3 &other) const
{
    const vector3 *lower;
    const vector3 *upper;
    int32_t lock_error;
    double result_x;
    double result_y;
    double result_z;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::add");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "vector3::add source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (vector3());
    }
    result_x = this->_x_component + other._x_component;
    result_y = this->_y_component + other._y_component;
    result_z = this->_z_component + other._z_component;
    this->unlock_pair(lower, upper);
    vector3::set_error(FT_ERR_SUCCESS);
    return (vector3(result_x, result_y, result_z));
}

vector3 vector3::subtract(const vector3 &other) const
{
    const vector3 *lower;
    const vector3 *upper;
    int32_t lock_error;
    double result_x;
    double result_y;
    double result_z;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::subtract");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "vector3::subtract source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (vector3());
    }
    result_x = this->_x_component - other._x_component;
    result_y = this->_y_component - other._y_component;
    result_z = this->_z_component - other._z_component;
    this->unlock_pair(lower, upper);
    vector3::set_error(FT_ERR_SUCCESS);
    return (vector3(result_x, result_y, result_z));
}

double vector3::dot(const vector3 &other) const
{
    const vector3 *lower;
    const vector3 *upper;
    int32_t lock_error;
    double result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::dot");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state, "vector3::dot source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (0.0);
    }
    result = vector3_compute_dot(this->_x_component, this->_y_component, this->_z_component,
            other._x_component, other._y_component, other._z_component);
    this->unlock_pair(lower, upper);
    vector3::set_error(FT_ERR_SUCCESS);
    return (result);
}

vector3 vector3::cross(const vector3 &other) const
{
    const vector3 *lower;
    const vector3 *upper;
    int32_t lock_error;
    double result_x;
    double result_y;
    double result_z;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::cross");
    errno_abort_if_uninitialised_or_destroyed(other._initialised_state,
        "vector3::cross source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(static_cast<uint32_t>(lock_error));
        return (vector3());
    }
    result_x = this->_y_component * other._z_component - this->_z_component * other._y_component;
    result_y = this->_z_component * other._x_component - this->_x_component * other._z_component;
    result_z = this->_x_component * other._y_component - this->_y_component * other._x_component;
    this->unlock_pair(lower, upper);
    vector3::set_error(FT_ERR_SUCCESS);
    return (vector3(result_x, result_y, result_z));
}

double vector3::length() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::length");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(lock_error);
        return (0.0);
    }
    const double components[] = {this->_x_component, this->_y_component, this->_z_component};
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    const double result = math_vector_length(components);
    vector3::set_error(FT_ERR_SUCCESS);
    return (result);
}

vector3 vector3::normalize() const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "vector3::normalize");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector3::set_error(lock_error);
        return (vector3());
    }
    double components[] = {this->_x_component, this->_y_component, this->_z_component};
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    vector3::set_error(FT_ERR_SUCCESS);
    if (!math_vector_normalize(components))
        return (vector3());
    return (vector3(components[0], components[1], components[2]));
}

uint32_t vector3::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector3::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (vector3::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (vector3::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (vector3::set_error(static_cast<uint32_t>(mutex_error)));
    }
    this->_mutex = mutex_pointer;
    return (vector3::set_error(FT_ERR_SUCCESS));
}

uint32_t vector3::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector3::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (vector3::set_error(static_cast<uint32_t>(mutex_error)));
    }
    return (vector3::set_error(FT_ERR_SUCCESS));
}

ft_bool vector3::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "vector3::is_thread_safe");
    vector3::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}
