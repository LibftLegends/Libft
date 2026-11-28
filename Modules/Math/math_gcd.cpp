#include "math.hpp"
#include "math_integer_gcd.hpp"

int32_t math_gcd(int32_t first_number, int32_t second_number)
{
    const uint32_t result = math_unsigned_gcd(
        math_integer_magnitude<uint32_t>(first_number),
        math_integer_magnitude<uint32_t>(second_number));
    if (result > static_cast<uint32_t>(INT32_MAX))
        return (INT32_MAX);
    return (static_cast<int32_t>(result));
}

int64_t math_gcd(int64_t first_number, int64_t second_number)
{
    const uint64_t result = math_unsigned_gcd(
        math_integer_magnitude<uint64_t>(first_number),
        math_integer_magnitude<uint64_t>(second_number));
    if (result > static_cast<uint64_t>(INT64_MAX))
        return (INT64_MAX);
    return (static_cast<int64_t>(result));
}
