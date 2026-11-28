#include "math.hpp"
#include <cmath>

double math_acos(double dot)
{
    constexpr double TOLERANCE = 1e-13;
    constexpr double PI = 3.14159265358979323846;

    if (std::fabs(dot - 1.0) <= TOLERANCE)
        return (0.0);
    if (std::fabs(dot + 1.0) <= TOLERANCE)
        return (PI);
    if (!(dot >= -1.0 && dot <= 1.0))
        return (math_nan());
    if (std::fabs(dot) <= TOLERANCE)
        return (PI * 0.5);
    return (std::acos(dot));
}
