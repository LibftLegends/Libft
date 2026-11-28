#ifndef MATH_MIDPOINT_HPP
# define MATH_MIDPOINT_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include <cmath>
#include <limits>

inline double math_float_midpoint(double first, double second) noexcept
{
    constexpr double HALF_MAX = std::numeric_limits<double>::max() * 0.5;

    if (std::fabs(first) > HALF_MAX || std::fabs(second) > HALF_MAX)
        return (first * 0.5 + second * 0.5);
    return ((first + second) * 0.5);
}

#endif
