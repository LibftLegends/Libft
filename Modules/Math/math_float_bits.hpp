#ifndef MATH_FLOAT_BITS_HPP
# define MATH_FLOAT_BITS_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "../Basic/basic.hpp"
#include <limits>

static_assert(sizeof(double) == sizeof(uint64_t)
    && std::numeric_limits<double>::is_iec559
    && std::numeric_limits<double>::digits == 53);

inline constexpr uint64_t MATH_DOUBLE_SIGN_BIT = 0x8000000000000000ULL;
inline constexpr uint64_t MATH_DOUBLE_INFINITY_BITS = 0x7ff0000000000000ULL;

inline uint64_t math_double_bits(double value)
{
    uint64_t bits;
    ft_memmove_fixed<sizeof(bits)>(&bits, &value);
    return (bits);
}

inline double math_double_from_bits(uint64_t bits)
{
    double value;
    ft_memmove_fixed<sizeof(value)>(&value, &bits);
    return (value);
}

inline constexpr uint64_t math_double_magnitude_bits(uint64_t bits)
{
    return (bits & ~MATH_DOUBLE_SIGN_BIT);
}

#endif
