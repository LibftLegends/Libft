#include "math.hpp"
#include "math_midpoint.hpp"

int32_t math_average(int32_t first_number, int32_t second_number)
{
    const int64_t sum = static_cast<int64_t>(first_number) + second_number;
    return (static_cast<int32_t>(sum / 2));
}

int64_t math_average(int64_t first_number, int64_t second_number)
{
    if ((first_number < 0) != (second_number < 0))
        return ((first_number + second_number) / 2);
    const int64_t remainder = (first_number % 2) + (second_number % 2);
    return (first_number / 2 + second_number / 2 + remainder / 2);
}

double math_average(double first_number, double second_number)
{
    return (math_float_midpoint(first_number, second_number));
}
