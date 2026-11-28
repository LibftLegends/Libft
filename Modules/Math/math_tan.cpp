#include "math.hpp"
#include <cmath>

double ft_tan(double value)
{
    constexpr double POLE_TOLERANCE = 1e-10;

    const double cosine = math_cos(value);
    if (std::fabs(cosine) < POLE_TOLERANCE)
        return (math_nan());
    return (ft_sin(value) / cosine);
}
