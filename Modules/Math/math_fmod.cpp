#include "math.hpp"
#include "math_float_bits.hpp"
#include <cmath>

double math_fmod(double value, double modulus)
{
    constexpr uint64_t MIN_SUBNORMAL_BITS = 1U;

    const uint64_t value_bits = math_double_magnitude_bits(math_double_bits(value));
    const uint64_t modulus_bits = math_double_magnitude_bits(math_double_bits(modulus));
    if (value_bits >= MATH_DOUBLE_INFINITY_BITS
        || modulus_bits <= MIN_SUBNORMAL_BITS
        || modulus_bits > MATH_DOUBLE_INFINITY_BITS)
        return (math_nan());
    if (modulus_bits == MATH_DOUBLE_INFINITY_BITS)
        return (value);

    const double remainder = std::fmod(value, modulus);
    if (math_double_magnitude_bits(math_double_bits(remainder)) <= MIN_SUBNORMAL_BITS)
        return (value * 0.0);
    return (remainder);
}
