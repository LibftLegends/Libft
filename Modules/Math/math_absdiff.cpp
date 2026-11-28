#include "math.hpp"

template <typename Unsigned, typename Signed>
static Unsigned math_absdiff_magnitude(Signed first_number, Signed second_number)
{
    // Ordering the signed operands makes their nonnegative difference fit in Unsigned.
    const Unsigned first_value = static_cast<Unsigned>(first_number);
    const Unsigned second_value = static_cast<Unsigned>(second_number);
    if (first_number < second_number)
        return (second_value - first_value);
    return (first_value - second_value);
}

int32_t math_absdiff(int32_t first_number, int32_t second_number)
{
    const uint32_t difference = math_absdiff_magnitude<uint32_t>(
        first_number, second_number);
    if (difference > static_cast<uint32_t>(INT32_MAX))
        return (INT32_MAX);
    return (static_cast<int32_t>(difference));
}

int64_t math_absdiff(int64_t first_number, int64_t second_number)
{
    const uint64_t difference = math_absdiff_magnitude<uint64_t>(
        first_number, second_number);
    if (difference > static_cast<uint64_t>(INT64_MAX))
        return (INT64_MAX);
    return (static_cast<int64_t>(difference));
}

double math_absdiff(double first_number, double second_number)
{
    return (math_fabs(first_number - second_number));
}
