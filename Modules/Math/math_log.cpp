#include "math.hpp"
#include <cmath>

double math_log(double value)
{
    if (!(value > 0.0))
        return (math_nan());
    return (std::log(value));
}
