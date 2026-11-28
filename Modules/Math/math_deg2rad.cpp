#include "math.hpp"

double math_deg2rad(double degrees)
{
    constexpr double RADIANS_PER_DEGREE = 3.14159265358979323846 / 180.0;
    return (degrees * RADIANS_PER_DEGREE);
}
