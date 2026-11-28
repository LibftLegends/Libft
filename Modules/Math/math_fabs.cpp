#include "math.hpp"
#include "math_float_bits.hpp"

double math_fabs(double number)
{
    const uint64_t magnitude = math_double_magnitude_bits(math_double_bits(number));
    if (magnitude > MATH_DOUBLE_INFINITY_BITS)
        return (number);
    return (math_double_from_bits(magnitude));
}
