#include "math.hpp"

double math_rad2deg(double radians)
{
    constexpr double DEGREES_PER_RADIAN = 180.0 / 3.14159265358979323846;
    return (radians * DEGREES_PER_RADIAN);
}
