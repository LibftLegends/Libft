#ifndef MATH_VECTOR_NORM_HPP
# define MATH_VECTOR_NORM_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "../Basic/limits.hpp"
#include <cmath>

template <ft_size_t Count>
inline double math_vector_norm_squared(const double (&components)[Count]) noexcept
{
    double sum = components[0] * components[0];
    ft_size_t index = 1U;
    while (index < Count)
    {
        sum += components[index] * components[index];
        index += 1U;
    }
    return (sum);
}

template <ft_size_t Count>
inline double math_vector_maximum_component(const double (&components)[Count]) noexcept
{
    double largest = 0.0;
    ft_size_t index = 0U;
    while (index < Count)
    {
        const double magnitude = std::fabs(components[index]);
        if (magnitude > largest)
            largest = magnitude;
        index += 1U;
    }
    return (largest);
}

template <ft_size_t Count>
inline double math_vector_length(const double (&components)[Count]) noexcept
{
    const double squared = math_vector_norm_squared(components);
    if (std::isnormal(squared))
        return (std::sqrt(squared));
    if (std::isnan(squared))
        return (squared);

    const double scale = math_vector_maximum_component(components);
    if (std::fpclassify(scale) == FP_ZERO || !std::isfinite(scale))
        return (scale);
    double scaled[Count];
    ft_size_t index = 0U;
    while (index < Count)
    {
        scaled[index] = components[index] / scale;
        index += 1U;
    }
    return (scale * std::sqrt(math_vector_norm_squared(scaled)));
}

template <ft_size_t Count>
inline ft_bool math_vector_normalize(double (&components)[Count]) noexcept
{
    constexpr double MINIMUM_LENGTH = 1e-7;

    double squared = math_vector_norm_squared(components);
    if (std::isinf(squared))
    {
        const double scale = math_vector_maximum_component(components);
        if (std::isfinite(scale))
        {
            ft_size_t index = 0U;
            while (index < Count)
            {
                components[index] /= scale;
                index += 1U;
            }
            squared = math_vector_norm_squared(components);
        }
    }
    const double length = std::sqrt(squared);
    if (length <= MINIMUM_LENGTH)
        return (FT_FALSE);
    const double inverse = 1.0 / length;
    ft_size_t index = 0U;
    while (index < Count)
    {
        components[index] *= inverse;
        index += 1U;
    }
    return (FT_TRUE);
}

#endif
