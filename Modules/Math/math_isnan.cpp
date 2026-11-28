#include "math.hpp"
#include "math_float_bits.hpp"

int32_t math_isnan(double number)
{
    return (math_double_magnitude_bits(math_double_bits(number))
        > MATH_DOUBLE_INFINITY_BITS);
}
