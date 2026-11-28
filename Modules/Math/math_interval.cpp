#include "math_interval.hpp"
#include "math_midpoint.hpp"
#include "../Basic/class_nullptr.hpp"

static ft_interval ft_interval_error(int32_t error_code)
{
    return (ft_interval{0.0, 0.0, error_code});
}

static const ft_interval *ft_interval_input_error(const ft_interval &left,
    const ft_interval &right)
{
    if (left._error_code != FT_ERR_SUCCESS)
        return (&left);
    if (right._error_code != FT_ERR_SUCCESS)
        return (&right);
    return (ft_nullptr);
}

ft_interval ft_interval_create(double lower, double upper) noexcept
{
    if (!(lower <= upper))
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));
    return (ft_interval{lower, upper, FT_ERR_SUCCESS});
}

ft_interval ft_interval_from_measurement(double value, double absolute_error) noexcept
{
    if (!(absolute_error >= 0.0) || std::isnan(value))
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));
    return (ft_interval_create(value - absolute_error, value + absolute_error));
}

ft_interval ft_interval_add(const ft_interval &left, const ft_interval &right) noexcept
{
    if (const ft_interval *error = ft_interval_input_error(left, right))
        return (*error);
    return (ft_interval_create(left.lower + right.lower, left.upper + right.upper));
}

ft_interval ft_interval_subtract(const ft_interval &left, const ft_interval &right) noexcept
{
    if (const ft_interval *error = ft_interval_input_error(left, right))
        return (*error);
    return (ft_interval_create(left.lower - right.upper, left.upper - right.lower));
}

static double ft_interval_min(double first, double second)
{
    if (first < second)
        return (first);
    return (second);
}

static double ft_interval_max(double first, double second)
{
    if (first > second)
        return (first);
    return (second);
}

static ft_interval ft_interval_endpoint_bounds(double lower_lower, double lower_upper,
    double upper_lower, double upper_upper)
{
    if (std::isnan(lower_lower) || std::isnan(lower_upper)
        || std::isnan(upper_lower) || std::isnan(upper_upper))
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));
    const double lower = ft_interval_min(ft_interval_min(lower_lower, lower_upper),
        ft_interval_min(upper_lower, upper_upper));
    const double upper = ft_interval_max(ft_interval_max(lower_lower, lower_upper),
        ft_interval_max(upper_lower, upper_upper));
    return (ft_interval{lower, upper, FT_ERR_SUCCESS});
}

ft_interval ft_interval_multiply(const ft_interval &left, const ft_interval &right) noexcept
{
    if (const ft_interval *error = ft_interval_input_error(left, right))
        return (*error);
    return (ft_interval_endpoint_bounds(left.lower * right.lower, left.lower * right.upper,
        left.upper * right.lower, left.upper * right.upper));
}

ft_interval ft_interval_divide(const ft_interval &left, const ft_interval &right) noexcept
{
    if (const ft_interval *error = ft_interval_input_error(left, right))
        return (*error);
    if (right.lower <= 0.0 && right.upper >= 0.0)
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));
    return (ft_interval_endpoint_bounds(left.lower / right.lower, left.lower / right.upper,
        left.upper / right.lower, left.upper / right.upper));
}

ft_interval ft_interval_widen(const ft_interval &interval, double absolute_error) noexcept
{
    if (interval._error_code != FT_ERR_SUCCESS)
        return (interval);
    if (!(absolute_error >= 0.0))
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));
    return (ft_interval_create(interval.lower - absolute_error,
        interval.upper + absolute_error));
}

ft_interval ft_interval_propagate_linear(const ft_interval *components,
    const double *sensitivities, ft_size_t component_count) noexcept
{
    if (components == ft_nullptr || sensitivities == ft_nullptr)
        return (ft_interval_error(FT_ERR_INVALID_ARGUMENT));

    double minimum_sum = 0.0;
    double maximum_sum = 0.0;
    ft_size_t index = 0U;
    while (index < component_count)
    {
        const ft_interval &component = components[index];
        if (component._error_code != FT_ERR_SUCCESS)
            return (ft_interval_error(component._error_code));
        const double sensitivity = sensitivities[index];
        if (sensitivity >= 0.0)
        {
            minimum_sum += sensitivity * component.lower;
            maximum_sum += sensitivity * component.upper;
        }
        else
        {
            minimum_sum += sensitivity * component.upper;
            maximum_sum += sensitivity * component.lower;
        }
        index += 1U;
    }
    return (ft_interval_create(minimum_sum, maximum_sum));
}

int32_t ft_interval_get_error(const ft_interval *interval) noexcept
{
    if (interval == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (interval->_error_code);
}

const char *ft_interval_get_error_str(const ft_interval *interval) noexcept
{
    return (ft_strerror(ft_interval_get_error(interval)));
}

double ft_interval_midpoint(const ft_interval &interval) noexcept
{
    if (interval._error_code != FT_ERR_SUCCESS)
        return (0.0);
    return (math_float_midpoint(interval.lower, interval.upper));
}

double ft_interval_radius(const ft_interval &interval) noexcept
{
    if (interval._error_code != FT_ERR_SUCCESS)
        return (0.0);
    return (math_float_midpoint(-interval.lower, interval.upper));
}

int32_t ft_interval_contains(const ft_interval &interval, double value) noexcept
{
    return (interval._error_code == FT_ERR_SUCCESS
        && value >= interval.lower && value <= interval.upper);
}
