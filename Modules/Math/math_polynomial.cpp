#include "ft_cubic_spline.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <functional>
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <new>
#include "../Template/move.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/vector.hpp"
static void math_polynomial_copy_vector(const ft_vector<double> &source,
    ft_vector<double> &destination, ft_size_t count, int32_t &error) noexcept
{
    ft_bool acquired = FT_FALSE;
    error = source.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
        return ;
    const ft_size_t available = source.size();
    error = source.get_error();
    if (error == FT_ERR_SUCCESS && available != count)
        error = FT_ERR_INVALID_ARGUMENT;
    if (error == FT_ERR_SUCCESS)
    {
        const double *values = source.begin();
        error = source.get_error();
        if (error == FT_ERR_SUCCESS)
            error = destination.append(values, count);
    }
    source.unlock(acquired);
    return ;
}

void ft_cubic_spline::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

void ft_cubic_spline::abort_if_not_initialised(const char *method_name) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, method_name);
    return ;
}

int32_t ft_cubic_spline::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    this->abort_if_not_initialised("ft_cubic_spline::enable_thread_safety");
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

int32_t ft_cubic_spline::disable_thread_safety() noexcept
{
    int32_t mutex_error;

    this->abort_if_not_initialised("ft_cubic_spline::disable_thread_safety");
    if (this->_mutex != ft_nullptr)
    {
        mutex_error = this->_mutex->destroy();
        delete this->_mutex;
        this->_mutex = ft_nullptr;
        if (mutex_error != FT_ERR_SUCCESS)
            return (mutex_error);
    }
    return (FT_ERR_SUCCESS);
}

ft_bool ft_cubic_spline::is_thread_safe() const noexcept
{
    this->abort_if_not_initialised("ft_cubic_spline::is_thread_safe");
    return (this->_mutex != ft_nullptr);
}

ft_cubic_spline::ft_cubic_spline() noexcept
    : _mutex(ft_nullptr)
{
    this->_initialised_state = FT_CLASS_STATE_UNINITIALISED;
    return ;
}

int32_t ft_cubic_spline::initialize() noexcept
{
    int32_t initialize_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        this->abort_lifecycle_error("ft_cubic_spline::initialize",
            "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    initialize_error = this->_x_values.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    initialize_error = this->_a_coefficients.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_x_values.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    initialize_error = this->_b_coefficients.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_a_coefficients.destroy();
        (void)this->_x_values.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    initialize_error = this->_c_coefficients.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_b_coefficients.destroy();
        (void)this->_a_coefficients.destroy();
        (void)this->_x_values.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    initialize_error = this->_d_coefficients.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        (void)this->_c_coefficients.destroy();
        (void)this->_b_coefficients.destroy();
        (void)this->_a_coefficients.destroy();
        (void)this->_x_values.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (initialize_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_cubic_spline::initialize(const ft_cubic_spline &other) noexcept
{
    int32_t copy_error;
    int32_t destroy_error;
    int32_t lock_error;

    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        other.abort_lifecycle_error("ft_cubic_spline::initialize(const ft_cubic_spline &) source",
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
    copy_error = this->initialize();
    if (copy_error != FT_ERR_SUCCESS)
        return (copy_error);
    lock_error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (lock_error);
    }
    copy_error = this->_x_values.copy_from(other._x_values);
    if (copy_error == FT_ERR_SUCCESS)
        copy_error = this->_a_coefficients.copy_from(other._a_coefficients);
    if (copy_error == FT_ERR_SUCCESS)
        copy_error = this->_b_coefficients.copy_from(other._b_coefficients);
    if (copy_error == FT_ERR_SUCCESS)
        copy_error = this->_c_coefficients.copy_from(other._c_coefficients);
    if (copy_error == FT_ERR_SUCCESS)
        copy_error = this->_d_coefficients.copy_from(other._d_coefficients);
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    if (copy_error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (copy_error);
    }
    return (FT_ERR_SUCCESS);
}

int32_t ft_cubic_spline::move(ft_cubic_spline &other) noexcept
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
    {
        other.abort_lifecycle_error("ft_cubic_spline::move source",
            "called with uninitialised source object");
        return (FT_ERR_INVALID_STATE);
    }
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t error = this->destroy();
        if (error != FT_ERR_SUCCESS)
            return (error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    int32_t error = this->initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = pt_recursive_mutex_lock_if_not_null(other._mutex);
    if (error != FT_ERR_SUCCESS)
    {
        (void)this->destroy();
        return (error);
    }

    ft_vector<double> *source[] = {&other._x_values, &other._a_coefficients,
        &other._b_coefficients, &other._c_coefficients, &other._d_coefficients};
    ft_vector<double> *destination[] = {&this->_x_values, &this->_a_coefficients,
        &this->_b_coefficients, &this->_c_coefficients, &this->_d_coefficients};
    constexpr ft_size_t VECTOR_COUNT = sizeof(source) / sizeof(source[0]);
    ft_bool can_transfer = FT_TRUE;
    ft_size_t index = 0U;
    while (index < VECTOR_COUNT)
    {
        if (source[index]->is_initialised() != FT_CLASS_STATE_INITIALISED)
        {
            error = FT_ERR_INVALID_STATE;
            break ;
        }
        if (source[index]->is_thread_safe())
            can_transfer = FT_FALSE;
        index += 1U;
    }
    index = 0U;
    while (error == FT_ERR_SUCCESS && index < VECTOR_COUNT)
    {
        if (can_transfer)
        {
            // For plain doubles with no vector mutex, storage moves and empty initialization do not allocate.
            error = destination[index]->move(*source[index]);
            if (error == FT_ERR_SUCCESS)
                error = source[index]->initialize();
        }
        else
            error = destination[index]->copy_from(*source[index]);
        index += 1U;
    }
    if (error == FT_ERR_SUCCESS && !can_transfer)
    {
        index = 0U;
        while (index < VECTOR_COUNT)
        {
            source[index]->clear();
            index += 1U;
        }
    }
    (void)pt_recursive_mutex_unlock_if_not_null(other._mutex);
    if (error != FT_ERR_SUCCESS)
        (void)this->destroy();
    return (error);
}

int32_t ft_cubic_spline::initialize(ft_cubic_spline &&other) noexcept
{
    return (this->move(other));
}

int32_t ft_cubic_spline::destroy() noexcept
{
    int32_t first_error;
    int32_t destroy_error;
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    first_error = FT_ERR_SUCCESS;
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = disable_error;
    destroy_error = this->_x_values.destroy();
    if (destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = destroy_error;
    destroy_error = this->_a_coefficients.destroy();
    if (destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = destroy_error;
    destroy_error = this->_b_coefficients.destroy();
    if (destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = destroy_error;
    destroy_error = this->_c_coefficients.destroy();
    if (destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = destroy_error;
    destroy_error = this->_d_coefficients.destroy();
    if (destroy_error != FT_ERR_SUCCESS && first_error == FT_ERR_SUCCESS)
        first_error = destroy_error;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (first_error);
}

ft_cubic_spline::~ft_cubic_spline() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


static int32_t math_polynomial_coefficients(const ft_vector<double> &coefficients,
    const double *&values, ft_size_t &count) noexcept
{
    count = coefficients.size();
    if (coefficients.get_error() != FT_ERR_SUCCESS)
        return (coefficients.get_error());
    if (count == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    values = coefficients.begin();
    return (coefficients.get_error());
}

int32_t math_polynomial_evaluate(const ft_vector<double> &coefficients,
    double input_value, double &result) noexcept
{
    double value = 0.0;
    ft_bool acquired = FT_FALSE;
    int32_t error = coefficients.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
    {
        result = 0.0;
        return (error);
    }
    const double *values;
    ft_size_t count;
    error = math_polynomial_coefficients(coefficients, values, count);
    if (error == FT_ERR_SUCCESS)
    {
        ft_size_t index = count - 1U;
        value = values[index];
        while (index != 0U)
        {
            index -= 1U;
            value = value * input_value + values[index];
        }
    }
    result = value;
    coefficients.unlock(acquired);
    return (error);
}

static int32_t math_polynomial_newton(const double *coefficients, ft_size_t count,
    double tolerance, ft_size_t max_iterations, double &root) noexcept
{
    constexpr double DERIVATIVE_EPSILON = 1e-12;

    ft_size_t iteration = 0U;
    while (iteration < max_iterations)
    {
        ft_size_t index = count - 1U;
        double value = coefficients[index];
        double derivative = 0.0;
        while (index != 0U)
        {
            index -= 1U;
            derivative = derivative * root + value;
            value = value * root + coefficients[index];
        }
        const double residual = std::fabs(value);
        if (std::fabs(derivative) < DERIVATIVE_EPSILON)
        {
            if (residual <= tolerance)
                return (FT_ERR_SUCCESS);
            return (FT_ERR_INVALID_OPERATION);
        }
        root -= value / derivative;
        if (residual <= tolerance)
            return (FT_ERR_SUCCESS);
        iteration += 1U;
    }
    return (FT_ERR_INVALID_OPERATION);
}

int32_t math_polynomial_find_root_newton(const ft_vector<double> &coefficients,
    double initial_guess, double tolerance, ft_size_t max_iterations, double &root) noexcept
{
    if (!(tolerance > 0.0) || max_iterations == 0U)
    {
        root = 0.0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    double current = initial_guess;
    ft_bool acquired = FT_FALSE;
    int32_t error = coefficients.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
    {
        root = initial_guess;
        return (error);
    }
    const double *values;
    ft_size_t count;
    error = math_polynomial_coefficients(coefficients, values, count);
    if (error == FT_ERR_SUCCESS)
        error = math_polynomial_newton(values, count, tolerance, max_iterations, current);
    root = current;
    coefficients.unlock(acquired);
    return (error);
}

static int32_t math_quadratic_scaled(double coefficient_a, double coefficient_b,
    double coefficient_c, double &root_one, double &root_two) noexcept
{
    int32_t exponent_a;
    int32_t exponent_b;
    int32_t exponent_c;
    const double mantissa_a = std::frexp(coefficient_a, &exponent_a);
    const double mantissa_b = std::frexp(coefficient_b, &exponent_b);
    const double mantissa_c = std::frexp(coefficient_c, &exponent_c);

    const int32_t product_exponent = exponent_a + exponent_c;
    int32_t root_exponent = product_exponent / 2;
    if (product_exponent > 0 && product_exponent % 2 != 0)
        root_exponent += 1;
    if (std::fpclassify(mantissa_b) != FP_ZERO && exponent_b - 1 > root_exponent)
        root_exponent = exponent_b - 1;

    const double half_b = std::scalbn(mantissa_b, exponent_b - 1 - root_exponent);
    const double scaled_product = std::scalbn(mantissa_a * mantissa_c,
        product_exponent - 2 * root_exponent);
    const double discriminant = half_b * half_b - scaled_product;
    if (discriminant < 0.0)
        return (FT_ERR_INVALID_ARGUMENT);

    double signed_square_root = std::sqrt(discriminant);
    if (coefficient_b < 0.0)
        signed_square_root = -signed_square_root;
    const double numerator = -half_b - signed_square_root;
    const double first = std::scalbn(numerator / mantissa_a, root_exponent - exponent_a);
    const double second = std::scalbn(mantissa_c / numerator, exponent_c - root_exponent);
    root_one = first;
    root_two = second;
    return (FT_ERR_SUCCESS);
}

int32_t math_polynomial_solve_quadratic(double coefficient_a, double coefficient_b,
    double coefficient_c, double &root_one, double &root_two) noexcept
{
    const double half_b = coefficient_b * 0.5;
    const double discriminant = half_b * half_b - coefficient_a * coefficient_c;
    if (discriminant >= std::numeric_limits<double>::min()
        && discriminant <= std::numeric_limits<double>::max()
        && std::fpclassify(coefficient_a) != FP_ZERO)
    {
        double signed_square_root = std::sqrt(discriminant);
        if (coefficient_b < 0.0)
            signed_square_root = -signed_square_root;
        const double numerator = -half_b - signed_square_root;
        const double first = numerator / coefficient_a;
        const double second = coefficient_c / numerator;
        root_one = first;
        root_two = second;
        return (FT_ERR_SUCCESS);
    }

    root_one = 0.0;
    root_two = 0.0;
    if (!std::isfinite(coefficient_a) || !std::isfinite(coefficient_b)
        || !std::isfinite(coefficient_c) || (std::fpclassify(coefficient_a) == FP_ZERO
            && std::fpclassify(coefficient_b) == FP_ZERO))
        return (FT_ERR_INVALID_ARGUMENT);
    if (std::fpclassify(coefficient_a) == FP_ZERO)
    {
        const double root = -coefficient_c / coefficient_b;
        root_one = root;
        root_two = root;
        return (FT_ERR_SUCCESS);
    }
    if (std::fpclassify(coefficient_c) == FP_ZERO)
    {
        root_one = -coefficient_b / coefficient_a;
        return (FT_ERR_SUCCESS);
    }
    if (std::isnormal(discriminant) && discriminant < 0.0)
        return (FT_ERR_INVALID_ARGUMENT);
    return (math_quadratic_scaled(coefficient_a, coefficient_b, coefficient_c,
        root_one, root_two));
}

static int32_t math_lagrange_locked(const ft_vector<double> &x_values,
    const ft_vector<double> &y_values, double input_value, double &result) noexcept
{
    constexpr double MIN_KNOT_DISTANCE = 1e-12;

    const double *knots;
    const double *samples;
    ft_size_t count;
    ft_size_t sample_count;
    int32_t error = math_polynomial_coefficients(x_values, knots, count);
    if (error != FT_ERR_SUCCESS)
        return (error);
    error = math_polynomial_coefficients(y_values, samples, sample_count);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (count != sample_count)
        return (FT_ERR_INVALID_ARGUMENT);

    double sum = 0.0;
    ft_size_t index = 0U;
    while (index < count)
    {
        double term = samples[index];
        const double node = knots[index];
        ft_size_t inner_index = 0U;
        while (inner_index < count)
        {
            if (inner_index != index)
            {
                const double denominator = node - knots[inner_index];
                if (std::fabs(denominator) <= MIN_KNOT_DISTANCE)
                    return (FT_ERR_INVALID_ARGUMENT);
                term = term * (input_value - knots[inner_index]) / denominator;
            }
            inner_index += 1U;
        }
        sum += term;
        index += 1U;
    }
    result = sum;
    return (FT_ERR_SUCCESS);
}

int32_t math_polynomial_lagrange_interpolate(const ft_vector<double> &x_values,
    const ft_vector<double> &y_values, double input_value, double &result) noexcept
{
    const ft_vector<double> *first = &x_values;
    const ft_vector<double> *second = &y_values;
    if (std::less<const ft_vector<double> *>{}(second, first))
        std::swap(first, second);
    ft_bool first_acquired = FT_FALSE;
    int32_t error = first->lock(&first_acquired);
    if (error != FT_ERR_SUCCESS)
    {
        result = 0.0;
        return (error);
    }
    ft_bool second_acquired = FT_FALSE;
    if (first != second)
        error = second->lock(&second_acquired);
    double value = 0.0;
    if (error == FT_ERR_SUCCESS)
    {
        error = math_lagrange_locked(x_values, y_values, input_value, value);
        result = value;
        if (first != second)
            second->unlock(second_acquired);
    }
    else
        result = 0.0;
    first->unlock(first_acquired);
    return (error);
}

static double math_bezier_reduce(double *values, ft_size_t count, double parameter) noexcept
{
    const double inverse = 1.0 - parameter;
    ft_size_t level = count - 1U;
    while (level != 0U)
    {
        ft_size_t index = 0U;
        while (index < level)
        {
            values[index] = inverse * values[index] + parameter * values[index + 1U];
            index += 1U;
        }
        level -= 1U;
    }
    return (values[0]);
}

int32_t math_bezier_evaluate(const ft_vector<double> &control_points,
    double parameter, double &result) noexcept
{
    if (parameter < 0.0 || parameter > 1.0)
    {
        result = 0.0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    const ft_size_t count = control_points.size();
    int32_t error = control_points.get_error();
    if (error == FT_ERR_SUCCESS && count == 0U)
        error = FT_ERR_INVALID_ARGUMENT;
    ft_vector<double> working;
    if (error == FT_ERR_SUCCESS)
        error = working.initialize();
    if (error == FT_ERR_SUCCESS)
        math_polynomial_copy_vector(control_points, working, count, error);
    if (error != FT_ERR_SUCCESS)
    {
        result = 0.0;
        return (error);
    }
    result = math_bezier_reduce(working.begin(), count, parameter);
    return (FT_ERR_SUCCESS);
}

static int32_t math_bezier_copy_coordinates(const ft_vector<vector2> &control_points,
    ft_vector<double> &coordinates, ft_size_t &count) noexcept
{
    ft_bool acquired = FT_FALSE;
    int32_t error = control_points.lock(&acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    count = control_points.size();
    error = control_points.get_error();
    if (error == FT_ERR_SUCCESS && count == 0U)
        error = FT_ERR_INVALID_ARGUMENT;
    if (error == FT_ERR_SUCCESS && count > FT_SYSTEM_SIZE_MAX / sizeof(double) / 2U)
        error = FT_ERR_NO_MEMORY;
    if (error == FT_ERR_SUCCESS)
    {
        coordinates.resize(count * 2U);
        error = coordinates.get_error();
    }
    if (error == FT_ERR_SUCCESS)
    {
        const vector2 *points = control_points.begin();
        error = control_points.get_error();
        double *x_values = coordinates.begin();
        double *y_values = x_values + count;
        ft_size_t index = 0U;
        while (error == FT_ERR_SUCCESS && index < count)
        {
            x_values[index] = points[index].get_x();
            error = points[index].get_error();
            if (error != FT_ERR_SUCCESS)
                break ;
            y_values[index] = points[index].get_y();
            error = points[index].get_error();
            index += 1U;
        }
    }
    control_points.unlock(acquired);
    return (error);
}

int32_t math_bezier_evaluate_vector2(const ft_vector<vector2> &control_points,
    double parameter, vector2 &result) noexcept
{
    if (parameter < 0.0 || parameter > 1.0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_vector<double> coordinates;
    int32_t error = coordinates.initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    ft_size_t count;
    error = math_bezier_copy_coordinates(control_points, coordinates, count);
    if (error != FT_ERR_SUCCESS)
        return (error);

    double *data = coordinates.begin();
    const double x_value = math_bezier_reduce(data, count, parameter);
    const double y_value = math_bezier_reduce(data + count, count, parameter);
    vector2 replacement;
    error = replacement.initialize(x_value, y_value);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (result.move(replacement));
}

static ft_cubic_spline *math_polynomial_destroy_spline_and_return_null(
    ft_cubic_spline *spline) noexcept
{
    if (spline != ft_nullptr)
        delete spline;
    return (ft_nullptr);
}

static ft_bool math_spline_solve_interior(const double *widths, double *diagonal,
    double *upper, double *coefficients, ft_size_t count) noexcept
{
    constexpr double PIVOT_EPSILON = 1e-12;

    const ft_size_t last = count - 2U;
    diagonal[1] = (widths[0] + widths[1]) * (widths[0] + 2.0 * widths[1]);
    upper[1] = widths[1] * widths[1] - widths[0] * widths[0];
    coefficients[1] *= widths[1];
    ft_size_t index = 2U;
    while (index < last)
    {
        diagonal[index] = 2.0 * (widths[index - 1U] + widths[index]);
        upper[index] = widths[index];
        index += 1U;
    }
    const double left_width = widths[last - 1U];
    const double right_width = widths[last];
    diagonal[last] = (left_width + right_width) * (2.0 * left_width + right_width);
    coefficients[last] *= left_width;
    index = 2U;
    while (index <= last)
    {
        const double pivot = diagonal[index - 1U];
        if (!(std::fabs(pivot) > PIVOT_EPSILON))
            return (FT_FALSE);
        double lower = widths[index - 1U];
        if (index == last)
            lower = left_width * left_width - right_width * right_width;
        const double factor = lower / pivot;
        diagonal[index] -= factor * upper[index - 1U];
        coefficients[index] -= factor * coefficients[index - 1U];
        index += 1U;
    }
    index = last;
    while (index != 0U)
    {
        if (!(std::fabs(diagonal[index]) > PIVOT_EPSILON))
            return (FT_FALSE);
        if (index < last)
            coefficients[index] -= upper[index] * coefficients[index + 1U];
        coefficients[index] /= diagonal[index];
        index -= 1U;
    }
    coefficients[0] = ((widths[0] + widths[1]) * coefficients[1]
        - widths[0] * coefficients[2]) / widths[1];
    coefficients[count - 1U] = ((left_width + right_width) * coefficients[last]
        - right_width * coefficients[last - 1U]) / left_width;
    return (FT_TRUE);
}

static ft_bool math_spline_build_coefficients(ft_cubic_spline &spline, ft_size_t count) noexcept
{
    constexpr ft_size_t ARRAYS = 3U;
    if (count > (FT_SYSTEM_SIZE_MAX / sizeof(double)) / ARRAYS)
        return (FT_FALSE);

    const ft_size_t segments = count - 1U;
    spline._b_coefficients.resize(segments);
    if (spline._b_coefficients.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    spline._c_coefficients.resize(count);
    if (spline._c_coefficients.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    spline._d_coefficients.resize(segments);
    if (spline._d_coefficients.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const double *knots = spline._x_values.begin();
    const double *values = spline._a_coefficients.begin();
    double *first_order = spline._b_coefficients.begin();
    double *second_order = spline._c_coefficients.begin();
    double *third_order = spline._d_coefficients.begin();

    if (segments == 1U)
    {
        const double width = knots[1] - knots[0];
        if (!(width > 0.0))
            return (FT_FALSE);
        first_order[0] = (values[1] - values[0]) / width;
        return (FT_TRUE);
    }

    ft_vector<double> workspace;
    if (workspace.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    workspace.resize(count * ARRAYS);
    if (workspace.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    double *widths = workspace.begin();
    ft_size_t index = 0U;
    while (index < segments)
    {
        widths[index] = knots[index + 1U] - knots[index];
        if (!(widths[index] > 0.0))
            return (FT_FALSE);
        index += 1U;
    }
    index = 1U;
    while (index < segments)
    {
        second_order[index] = 3.0 * (values[index + 1U] - values[index]) / widths[index]
            - 3.0 * (values[index] - values[index - 1U]) / widths[index - 1U];
        index += 1U;
    }
    if (segments == 2U)
    {
        constexpr double PIVOT_EPSILON = 1e-12;
        const double denominator = 2.0 * (widths[0] + widths[1]);
        if (!(std::fabs(denominator) > PIVOT_EPSILON))
            return (FT_FALSE);
        second_order[1] /= denominator;
    }
    else if (!math_spline_solve_interior(widths, widths + count, widths + count * 2U,
            second_order, count))
        return (FT_FALSE);
    index = 0U;
    while (index < segments)
    {
        const double width = widths[index];
        first_order[index] = (values[index + 1U] - values[index]) / width
            - width * (second_order[index + 1U] + 2.0 * second_order[index]) / 3.0;
        third_order[index] = (second_order[index + 1U] - second_order[index]) / (3.0 * width);
        index += 1U;
    }
    return (FT_TRUE);
}

ft_cubic_spline *ft_cubic_spline_build(const ft_vector<double> &x_values_input,
    const ft_vector<double> &y_values) noexcept
{
    const ft_size_t count = x_values_input.size();
    if (x_values_input.get_error() != FT_ERR_SUCCESS || count < 2U
        || y_values.size() != count || y_values.get_error() != FT_ERR_SUCCESS)
        return (ft_nullptr);

    ft_cubic_spline *spline = new (std::nothrow) ft_cubic_spline();
    if (spline == ft_nullptr)
        return (ft_nullptr);
    if (spline->initialize() != FT_ERR_SUCCESS)
        return (math_polynomial_destroy_spline_and_return_null(spline));
    int32_t error;
    math_polynomial_copy_vector(x_values_input, spline->_x_values, count, error);
    if (error != FT_ERR_SUCCESS)
        return (math_polynomial_destroy_spline_and_return_null(spline));
    math_polynomial_copy_vector(y_values, spline->_a_coefficients, count, error);
    if (error != FT_ERR_SUCCESS || !math_spline_build_coefficients(*spline, count))
        return (math_polynomial_destroy_spline_and_return_null(spline));
    return (spline);
}

static ft_size_t math_spline_segment(const double *knots, ft_size_t count,
    double &input_value) noexcept
{
    if (input_value < knots[0])
        input_value = knots[0];
    if (input_value > knots[count - 1U])
        input_value = knots[count - 1U];
    // Exclude the endpoints, so the result is always a valid segment.
    return (static_cast<ft_size_t>(std::upper_bound(knots + 1, knots + count - 1U,
        input_value) - knots - 1));
}

static double math_spline_evaluate_locked(const ft_cubic_spline &spline,
    double input_value) noexcept
{
    ft_bool knots_acquired = FT_FALSE;
    if (spline._x_values.lock(&knots_acquired) != FT_ERR_SUCCESS)
        return (0.0);
    const ft_size_t count = spline._x_values.size();
    if (count < 2U)
    {
        spline._x_values.unlock(knots_acquired);
        return (0.0);
    }
    const double *knots = spline._x_values.begin();
    const ft_size_t index = math_spline_segment(knots, count, input_value);
    const double delta = input_value - knots[index];
    spline._x_values.unlock(knots_acquired);
    return (spline._a_coefficients[index]
        + spline._b_coefficients[index] * delta
        + spline._c_coefficients[index] * delta * delta
        + spline._d_coefficients[index] * delta * delta * delta);
}

double ft_cubic_spline_evaluate(const ft_cubic_spline &spline, double input_value) noexcept
{
    spline.abort_if_not_initialised("ft_cubic_spline_evaluate");
    if (pt_recursive_mutex_lock_if_not_null(spline._mutex) != FT_ERR_SUCCESS)
        return (0.0);
    const double value = math_spline_evaluate_locked(spline, input_value);
    (void)pt_recursive_mutex_unlock_if_not_null(spline._mutex);
    return (value);
}

static constexpr double INTEGRATION_EPSILON = 1e-12;

int32_t math_integrate_trapezoidal(math_unary_function function, void *user_data,
    double lower_bound, double upper_bound, ft_size_t subdivisions, double &result) noexcept
{
    if (function == ft_nullptr || !std::isfinite(lower_bound) || !std::isfinite(upper_bound))
    {
        result = 0.0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (std::fabs(lower_bound - upper_bound) <= INTEGRATION_EPSILON)
    {
        result = 0.0;
        return (FT_ERR_SUCCESS);
    }
    if (subdivisions == 0U)
    {
        result = 0.0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    double orientation = 1.0;
    if (upper_bound < lower_bound)
    {
        std::swap(lower_bound, upper_bound);
        orientation = -1.0;
    }
    const double step = (upper_bound - lower_bound) / static_cast<double>(subdivisions);
    double sum = 0.0;
    ft_size_t index = 1U;
    while (index < subdivisions)
    {
        const double value = function(lower_bound + step * static_cast<double>(index), user_data);
        if (std::isnan(value))
        {
            result = 0.0;
            return (FT_ERR_INVALID_OPERATION);
        }
        sum += value;
        index += 1U;
    }
    const double lower_value = function(lower_bound, user_data);
    const double upper_value = function(upper_bound, user_data);
    if (std::isnan(lower_value) || std::isnan(upper_value))
    {
        result = 0.0;
        return (FT_ERR_INVALID_OPERATION);
    }
    result = orientation * (((lower_value + upper_value) * 0.5 + sum) * step);
    return (FT_ERR_SUCCESS);
}

int32_t math_integrate_simpson(math_unary_function function, void *user_data,
    double lower_bound, double upper_bound, double tolerance, double &result) noexcept
{
    constexpr ft_size_t MAX_REFINEMENTS = 10U;

    if (function == ft_nullptr || !std::isfinite(lower_bound) || !std::isfinite(upper_bound)
        || !(tolerance > 0.0))
    {
        result = 0.0;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (std::fabs(lower_bound - upper_bound) <= INTEGRATION_EPSILON)
    {
        result = 0.0;
        return (FT_ERR_SUCCESS);
    }
    double orientation = 1.0;
    if (upper_bound < lower_bound)
    {
        std::swap(lower_bound, upper_bound);
        orientation = -1.0;
    }
    const double lower_value = function(lower_bound, user_data);
    const double upper_value = function(upper_bound, user_data);
    if (std::isnan(lower_value) || std::isnan(upper_value))
    {
        result = 0.0;
        return (FT_ERR_INVALID_OPERATION);
    }

    const double endpoints = lower_value + upper_value;
    const double span = upper_bound - lower_bound;
    ft_size_t subdivisions = 2U;
    ft_size_t refinement = 0U;
    double even_sum = 0.0;
    double previous = 0.0;
    while (refinement < MAX_REFINEMENTS)
    {
        const double step = span / static_cast<double>(subdivisions);
        double odd_sum = 0.0;
        ft_size_t index = 1U;
        while (index < subdivisions)
        {
            const double value = function(lower_bound + step * static_cast<double>(index), user_data);
            if (std::isnan(value))
            {
                result = 0.0;
                return (FT_ERR_INVALID_OPERATION);
            }
            odd_sum += value;
            index += 2U;
        }
        const double estimate = (endpoints + 4.0 * odd_sum + 2.0 * even_sum) * (step / 3.0);
        if (refinement > 0U && std::fabs(estimate - previous) <= tolerance + INTEGRATION_EPSILON)
        {
            result = orientation * estimate;
            return (FT_ERR_SUCCESS);
        }
        // Old interior nodes become the even nodes when the step halves.
        even_sum += odd_sum;
        previous = estimate;
        subdivisions *= 2U;
        refinement += 1U;
    }
    result = orientation * previous;
    return (FT_ERR_INVALID_OPERATION);
}
