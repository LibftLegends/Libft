#include "math.hpp"
#include "math_integer_gcd.hpp"

int32_t math_lcm(int32_t first_number, int32_t second_number)
{
    if (first_number == 0 || second_number == 0
        || first_number == INT32_MIN || second_number == INT32_MIN)
        return (0);
    const uint32_t magnitude_first = math_integer_magnitude<uint32_t>(first_number);
    const uint32_t magnitude_second = math_integer_magnitude<uint32_t>(second_number);
    const uint32_t quotient = magnitude_first
        / math_unsigned_gcd(magnitude_first, magnitude_second);
    const uint64_t result = static_cast<uint64_t>(quotient) * magnitude_second;
    if (result > static_cast<uint64_t>(INT32_MAX))
        return (0);
    return (static_cast<int32_t>(result));
}

int64_t math_lcm(int64_t first_number, int64_t second_number)
{
    if (first_number == 0 || second_number == 0
        || first_number == INT64_MIN || second_number == INT64_MIN)
        return (0);
    const uint64_t magnitude_first = math_integer_magnitude<uint64_t>(first_number);
    const uint64_t magnitude_second = math_integer_magnitude<uint64_t>(second_number);
    const uint64_t quotient = magnitude_first
        / math_unsigned_gcd(magnitude_first, magnitude_second);
    if (magnitude_second > static_cast<uint64_t>(INT64_MAX) / quotient)
        return (0);
    return (static_cast<int64_t>(quotient * magnitude_second));
}
