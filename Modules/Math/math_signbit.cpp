#include "math.hpp"
#include "math_float_bits.hpp"

int32_t math_signbit(double number)
{
    const uint64_t bits = math_double_bits(number);
    return (bits >= MATH_DOUBLE_SIGN_BIT
        && math_double_magnitude_bits(bits) <= MATH_DOUBLE_INFINITY_BITS);
}
