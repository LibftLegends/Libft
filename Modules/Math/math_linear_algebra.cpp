#include "linear_algebra.hpp"
#include "math.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "math_interval.hpp"
#include "matrix2.hpp"
#include "matrix3.hpp"
#include "matrix4.hpp"

static void matrix2_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

int32_t matrix2::lock_pair(const matrix2 &first, const matrix2 &second,
    const matrix2 *&lower, const matrix2 *&upper)
{
    const matrix2 *ordered_first;
    const matrix2 *ordered_second;

    if (&first == &second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    ordered_first = &first;
    ordered_second = &second;
    if (std::less<const matrix2 *>{}(ordered_second, ordered_first))
    {
        const matrix2 *temporary;

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
        matrix2_sleep_backoff();
    }
}

void matrix2::unlock_pair(const matrix2 *lower, const matrix2 *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

vector2 matrix2::transform(const vector2 &vector) const
{
    this->abort_if_not_initialised("matrix2::transform");
    vector.abort_if_not_initialised("matrix2::transform source");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(vector._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector2::set_error(lock_error);
        matrix2::set_error(lock_error);
        return (vector2());
    }
    const double vector_x = vector._x_component;
    const double vector_y = vector._y_component;
    (void)pt_recursive_mutex_unlock_if_not_null(vector._mutex);
    vector2::set_error(FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix2::set_error(lock_error);
        return (vector2());
    }
    const double result_x = this->_m[0][0] * vector_x + this->_m[0][1] * vector_y;
    const double result_y = this->_m[1][0] * vector_x + this->_m[1][1] * vector_y;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    matrix2::set_error(FT_ERR_SUCCESS);
    return (vector2(result_x, result_y));
}

matrix2 matrix2::multiply(const matrix2 &other) const
{
    const matrix2 *lower;
    const matrix2 *upper;
    int32_t lock_error;
    double result_00;
    double result_01;
    double result_10;
    double result_11;

    this->abort_if_not_initialised("matrix2::multiply");
    other.abort_if_not_initialised("matrix2::multiply source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix2::set_error(static_cast<uint32_t>(lock_error));
        return (matrix2());
    }
    result_00 = this->_m[0][0] * other._m[0][0] + this->_m[0][1] * other._m[1][0];
    result_01 = this->_m[0][0] * other._m[0][1] + this->_m[0][1] * other._m[1][1];
    result_10 = this->_m[1][0] * other._m[0][0] + this->_m[1][1] * other._m[1][0];
    result_11 = this->_m[1][0] * other._m[0][1] + this->_m[1][1] * other._m[1][1];
    this->unlock_pair(lower, upper);
    matrix2::set_error(FT_ERR_SUCCESS);
    return (matrix2(result_00, result_01, result_10, result_11));
}

matrix2 matrix2::invert() const
{
    this->abort_if_not_initialised("matrix2::invert");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix2::set_error(lock_error);
        return (matrix2());
    }
    const double first_diagonal = this->_m[0][0];
    const double upper = this->_m[0][1];
    const double lower = this->_m[1][0];
    const double second_diagonal = this->_m[1][1];
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);

    constexpr double MINIMUM_DETERMINANT = 1e-6;
    const double determinant = first_diagonal * second_diagonal - upper * lower;
    matrix2::set_error(FT_ERR_SUCCESS);
    if (std::fabs(determinant) < MINIMUM_DETERMINANT)
        return (matrix2());
    const double inverse_determinant = 1.0 / determinant;
    return (matrix2(second_diagonal * inverse_determinant, -upper * inverse_determinant,
        -lower * inverse_determinant, first_diagonal * inverse_determinant));
}

uint32_t matrix2::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix2::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (matrix2::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (matrix2::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (matrix2::set_error(static_cast<uint32_t>(mutex_error)));
    }
    this->_mutex = mutex_pointer;
    return (matrix2::set_error(FT_ERR_SUCCESS));
}

uint32_t matrix2::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix2::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (matrix2::set_error(static_cast<uint32_t>(mutex_error)));
    }
    return (matrix2::set_error(FT_ERR_SUCCESS));
}

ft_bool matrix2::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix2::is_thread_safe");
    matrix2::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

static void matrix3_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

int32_t matrix3::lock_pair(const matrix3 &first, const matrix3 &second,
    const matrix3 *&lower, const matrix3 *&upper)
{
    const matrix3 *ordered_first = &first;
    const matrix3 *ordered_second = &second;

    if (ordered_first == ordered_second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    if (std::less<const matrix3 *>{}(ordered_second, ordered_first))
    {
        const matrix3 *temporary = ordered_first;

        ordered_first = ordered_second;
        ordered_second = temporary;
    }
    lower = ordered_first;
    upper = ordered_second;
    while (FT_TRUE)
    {
        int32_t lower_error = pt_recursive_mutex_lock_if_not_null(ordered_first->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
            return (lower_error);
        int32_t upper_error = pt_recursive_mutex_lock_if_not_null(ordered_second->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(ordered_first->_mutex);
            return (upper_error);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(ordered_first->_mutex);
        matrix3_sleep_backoff();
    }
}

void matrix3::unlock_pair(const matrix3 *lower, const matrix3 *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

vector3 matrix3::transform(const vector3 &vector) const
{
    this->abort_if_not_initialised("matrix3::transform");
    double vector_x;
    double vector_y;
    double vector_z;
    int32_t lock_error = vector.get_components(vector_x, vector_y, vector_z);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix3::set_error(lock_error);
        return (vector3());
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix3::set_error(lock_error);
        return (vector3());
    }
    const double result_x = this->_m[0][0] * vector_x + this->_m[0][1] * vector_y
        + this->_m[0][2] * vector_z;
    const double result_y = this->_m[1][0] * vector_x + this->_m[1][1] * vector_y
        + this->_m[1][2] * vector_z;
    const double result_z = this->_m[2][0] * vector_x + this->_m[2][1] * vector_y
        + this->_m[2][2] * vector_z;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    matrix3::set_error(FT_ERR_SUCCESS);
    return (vector3(result_x, result_y, result_z));
}

matrix3 matrix3::multiply(const matrix3 &other) const
{
    const matrix3 *lower;
    const matrix3 *upper;
    int32_t lock_error;
    double result_00;
    double result_01;
    double result_02;
    double result_10;
    double result_11;
    double result_12;
    double result_20;
    double result_21;
    double result_22;

    this->abort_if_not_initialised("matrix3::multiply");
    other.abort_if_not_initialised("matrix3::multiply source");
    lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix3::set_error(static_cast<uint32_t>(lock_error));
        return (matrix3());
    }
    result_00 = this->_m[0][0] * other._m[0][0] + this->_m[0][1] * other._m[1][0] + this->_m[0][2] * other._m[2][0];
    result_01 = this->_m[0][0] * other._m[0][1] + this->_m[0][1] * other._m[1][1] + this->_m[0][2] * other._m[2][1];
    result_02 = this->_m[0][0] * other._m[0][2] + this->_m[0][1] * other._m[1][2] + this->_m[0][2] * other._m[2][2];
    result_10 = this->_m[1][0] * other._m[0][0] + this->_m[1][1] * other._m[1][0] + this->_m[1][2] * other._m[2][0];
    result_11 = this->_m[1][0] * other._m[0][1] + this->_m[1][1] * other._m[1][1] + this->_m[1][2] * other._m[2][1];
    result_12 = this->_m[1][0] * other._m[0][2] + this->_m[1][1] * other._m[1][2] + this->_m[1][2] * other._m[2][2];
    result_20 = this->_m[2][0] * other._m[0][0] + this->_m[2][1] * other._m[1][0] + this->_m[2][2] * other._m[2][0];
    result_21 = this->_m[2][0] * other._m[0][1] + this->_m[2][1] * other._m[1][1] + this->_m[2][2] * other._m[2][1];
    result_22 = this->_m[2][0] * other._m[0][2] + this->_m[2][1] * other._m[1][2] + this->_m[2][2] * other._m[2][2];
    this->unlock_pair(lower, upper);
    matrix3::set_error(FT_ERR_SUCCESS);
    return (matrix3(result_00, result_01, result_02,
            result_10, result_11, result_12,
            result_20, result_21, result_22));
}

matrix3 matrix3::invert() const
{
    this->abort_if_not_initialised("matrix3::invert");
    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix3::set_error(lock_error);
        return (matrix3());
    }
    double values[3][3];
    ft_size_t row = 0U;
    while (row < 3U)
    {
        std::copy_n(this->_m[row], 3U, values[row]);
        row += 1U;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);

    const double minor_00 = values[1][1] * values[2][2] - values[1][2] * values[2][1];
    const double minor_01 = values[1][0] * values[2][2] - values[1][2] * values[2][0];
    const double minor_02 = values[1][0] * values[2][1] - values[1][1] * values[2][0];
    const double determinant = values[0][0] * minor_00 - values[0][1] * minor_01
        + values[0][2] * minor_02;
    constexpr double MINIMUM_DETERMINANT = 1e-6;
    matrix3::set_error(FT_ERR_SUCCESS);
    if (std::fabs(determinant) <= MINIMUM_DETERMINANT)
        return (matrix3());

    const double inverse_determinant = 1.0 / determinant;
    return (matrix3(minor_00 * inverse_determinant,
        (values[0][2] * values[2][1] - values[0][1] * values[2][2]) * inverse_determinant,
        (values[0][1] * values[1][2] - values[0][2] * values[1][1]) * inverse_determinant,
        -minor_01 * inverse_determinant,
        (values[0][0] * values[2][2] - values[0][2] * values[2][0]) * inverse_determinant,
        (values[0][2] * values[1][0] - values[0][0] * values[1][2]) * inverse_determinant,
        minor_02 * inverse_determinant,
        (values[0][1] * values[2][0] - values[0][0] * values[2][1]) * inverse_determinant,
        (values[0][0] * values[1][1] - values[0][1] * values[1][0]) * inverse_determinant));
}

uint32_t matrix3::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix3::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (matrix3::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (matrix3::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (matrix3::set_error(static_cast<uint32_t>(mutex_error)));
    }
    this->_mutex = mutex_pointer;
    return (matrix3::set_error(FT_ERR_SUCCESS));
}

uint32_t matrix3::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix3::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (matrix3::set_error(static_cast<uint32_t>(mutex_error)));
    }
    return (matrix3::set_error(FT_ERR_SUCCESS));
}

ft_bool matrix3::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix3::is_thread_safe");
    matrix3::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}

static void matrix4_sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

int32_t matrix4::lock_pair(const matrix4 &first, const matrix4 &second,
    const matrix4 *&lower, const matrix4 *&upper)
{
    const matrix4 *ordered_first = &first;
    const matrix4 *ordered_second = &second;

    if (ordered_first == ordered_second)
    {
        lower = &first;
        upper = &first;
        return (pt_recursive_mutex_lock_if_not_null(first._mutex));
    }
    if (std::less<const matrix4 *>{}(ordered_second, ordered_first))
    {
        const matrix4 *temporary = ordered_first;

        ordered_first = ordered_second;
        ordered_second = temporary;
    }
    lower = ordered_first;
    upper = ordered_second;
    while (FT_TRUE)
    {
        int32_t lower_error = pt_recursive_mutex_lock_if_not_null(ordered_first->_mutex);
        if (lower_error != FT_ERR_SUCCESS)
            return (lower_error);
        int32_t upper_error = pt_recursive_mutex_lock_if_not_null(ordered_second->_mutex);
        if (upper_error == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
        if (upper_error != FT_ERR_MUTEX_ALREADY_LOCKED)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(ordered_first->_mutex);
            return (upper_error);
        }
        (void)pt_recursive_mutex_unlock_if_not_null(ordered_first->_mutex);
        matrix4_sleep_backoff();
    }
}

void matrix4::unlock_pair(const matrix4 *lower, const matrix4 *upper)
{
    if (upper != ft_nullptr)
        (void)pt_recursive_mutex_unlock_if_not_null(upper->_mutex);
    if (lower != ft_nullptr && lower != upper)
        (void)pt_recursive_mutex_unlock_if_not_null(lower->_mutex);
    return ;
}

vector4 matrix4::transform(const vector4 &vector) const
{
    this->abort_if_not_initialised("matrix4::transform");
    vector.abort_if_not_initialised("matrix4::transform source");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(vector._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        vector4::set_error(lock_error);
        matrix4::set_error(lock_error);
        return (vector4());
    }
    const double vector_x = vector._x_component;
    const double vector_y = vector._y_component;
    const double vector_z = vector._z_component;
    const double vector_w = vector._w_component;
    (void)pt_recursive_mutex_unlock_if_not_null(vector._mutex);
    vector4::set_error(FT_ERR_SUCCESS);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix4::set_error(lock_error);
        return (vector4());
    }
    const double result_x = this->_m[0][0] * vector_x + this->_m[0][1] * vector_y
        + this->_m[0][2] * vector_z + this->_m[0][3] * vector_w;
    const double result_y = this->_m[1][0] * vector_x + this->_m[1][1] * vector_y
        + this->_m[1][2] * vector_z + this->_m[1][3] * vector_w;
    const double result_z = this->_m[2][0] * vector_x + this->_m[2][1] * vector_y
        + this->_m[2][2] * vector_z + this->_m[2][3] * vector_w;
    const double result_w = this->_m[3][0] * vector_x + this->_m[3][1] * vector_y
        + this->_m[3][2] * vector_z + this->_m[3][3] * vector_w;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    matrix4::set_error(FT_ERR_SUCCESS);
    return (vector4(result_x, result_y, result_z, result_w));
}

matrix4 matrix4::multiply(const matrix4 &other) const
{
    this->abort_if_not_initialised("matrix4::multiply");
    other.abort_if_not_initialised("matrix4::multiply source");
    const matrix4 *lower;
    const matrix4 *upper;
    const int32_t lock_error = this->lock_pair(*this, other, lower, upper);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix4::set_error(lock_error);
        return (matrix4());
    }
    double product[4][4];
    ft_size_t row_index = 0U;
    while (row_index < 4U)
    {
        const double *row = this->_m[row_index];
        ft_size_t column_index = 0U;
        while (column_index < 4U)
        {
            product[row_index][column_index] = (row[0] * other._m[0][column_index]
                + row[2] * other._m[2][column_index])
                + (row[1] * other._m[1][column_index] + row[3] * other._m[3][column_index]);
            column_index += 1U;
        }
        row_index += 1U;
    }
    this->unlock_pair(lower, upper);
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(product[0][0], product[0][1], product[0][2], product[0][3],
        product[1][0], product[1][1], product[1][2], product[1][3],
        product[2][0], product[2][1], product[2][2], product[2][3],
        product[3][0], product[3][1], product[3][2], product[3][3]));
}

static ft_bool matrix4_reduce_inverse(double *(&rows)[4]) noexcept
{
    constexpr double MINIMUM_PIVOT = 1e-6;

    ft_size_t pivot_index = 0U;
    while (pivot_index < 4U)
    {
        ft_size_t strongest = pivot_index;
        double pivot_magnitude = std::fabs(rows[strongest][pivot_index]);
        ft_size_t index = pivot_index + 1U;
        while (index < 4U)
        {
            const double candidate = std::fabs(rows[index][pivot_index]);
            if (candidate > pivot_magnitude)
            {
                strongest = index;
                pivot_magnitude = candidate;
            }
            index += 1U;
        }
        if (pivot_magnitude < MINIMUM_PIVOT)
            return (FT_FALSE);
        std::swap(rows[pivot_index], rows[strongest]);

        double *pivot_row = rows[pivot_index];
        const double inverse_pivot = 1.0 / pivot_row[pivot_index];
        ft_size_t column = pivot_index + 1U;
        while (column < 8U)
        {
            pivot_row[column] *= inverse_pivot;
            column += 1U;
        }
        // The pivot column and everything to its left are no longer read.
        index = 0U;
        while (index < 4U)
        {
            if (index != pivot_index)
            {
                double *row = rows[index];
                const double factor = row[pivot_index];
                if (std::fpclassify(factor) != FP_ZERO)
                {
                    column = pivot_index + 1U;
                    while (column < 8U)
                    {
                        row[column] -= factor * pivot_row[column];
                        column += 1U;
                    }
                }
            }
            index += 1U;
        }
        pivot_index += 1U;
    }
    return (FT_TRUE);
}

matrix4 matrix4::invert() const
{
    this->abort_if_not_initialised("matrix4::invert");
    double augmented[4][8]{};

    const int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        matrix4::set_error(lock_error);
        return (matrix4());
    }
    ft_size_t row = 0U;
    while (row < 4U)
    {
        std::copy_n(this->_m[row], 4U, augmented[row]);
        row += 1U;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    row = 0U;
    while (row < 4U)
    {
        augmented[row][row + 4U] = 1.0;
        row += 1U;
    }
    double *rows[] = {augmented[0], augmented[1], augmented[2], augmented[3]};
    matrix4::set_error(FT_ERR_SUCCESS);
    if (!matrix4_reduce_inverse(rows))
        return (matrix4());
    return (matrix4(rows[0][4], rows[0][5], rows[0][6], rows[0][7],
        rows[1][4], rows[1][5], rows[1][6], rows[1][7],
        rows[2][4], rows[2][5], rows[2][6], rows[2][7],
        rows[3][4], rows[3][5], rows[3][6], rows[3][7]));
}

matrix4 matrix4::make_translation(double translation_x, double translation_y, double translation_z)
{
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(1.0, 0.0, 0.0, translation_x,
            0.0, 1.0, 0.0, translation_y,
            0.0, 0.0, 1.0, translation_z,
            0.0, 0.0, 0.0, 1.0));
}

matrix4 matrix4::make_scale(double scale_x, double scale_y, double scale_z)
{
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(scale_x, 0.0, 0.0, 0.0,
            0.0, scale_y, 0.0, 0.0,
            0.0, 0.0, scale_z, 0.0,
            0.0, 0.0, 0.0, 1.0));
}

matrix4 matrix4::make_rotation_x(double angle)
{
    double cos_angle;
    double sin_angle;

    cos_angle = math_cos(angle);
    sin_angle = ft_sin(angle);
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(1.0, 0.0, 0.0, 0.0,
            0.0, cos_angle, -sin_angle, 0.0,
            0.0, sin_angle, cos_angle, 0.0,
            0.0, 0.0, 0.0, 1.0));
}

matrix4 matrix4::make_rotation_y(double angle)
{
    double cos_angle;
    double sin_angle;

    cos_angle = math_cos(angle);
    sin_angle = ft_sin(angle);
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(cos_angle, 0.0, sin_angle, 0.0,
            0.0, 1.0, 0.0, 0.0,
            -sin_angle, 0.0, cos_angle, 0.0,
            0.0, 0.0, 0.0, 1.0));
}

matrix4 matrix4::make_rotation_z(double angle)
{
    double cos_angle;
    double sin_angle;

    cos_angle = math_cos(angle);
    sin_angle = ft_sin(angle);
    matrix4::set_error(FT_ERR_SUCCESS);
    return (matrix4(cos_angle, -sin_angle, 0.0, 0.0,
            sin_angle, cos_angle, 0.0, 0.0,
            0.0, 0.0, 1.0, 0.0,
            0.0, 0.0, 0.0, 1.0));
}


uint32_t matrix4::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix4::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (matrix4::set_error(FT_ERR_SUCCESS));
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (matrix4::set_error(FT_ERR_NO_MEMORY));
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (matrix4::set_error(static_cast<uint32_t>(mutex_error)));
    }
    this->_mutex = mutex_pointer;
    return (matrix4::set_error(FT_ERR_SUCCESS));
}

uint32_t matrix4::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix4::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (matrix4::set_error(static_cast<uint32_t>(mutex_error)));
    }
    return (matrix4::set_error(FT_ERR_SUCCESS));
}

ft_bool matrix4::is_thread_safe() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "matrix4::is_thread_safe");
    matrix4::set_error(FT_ERR_SUCCESS);
    return (this->_mutex != ft_nullptr);
}
