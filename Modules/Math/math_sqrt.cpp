#include "math.hpp"
#include <cmath>
#include <limits>

double math_sqrt(double number)
{
    if (!(number >= 0.0))
        return (math_nan());
    if (number <= std::numeric_limits<double>::min())
        return (0.0);
    return (std::sqrt(number));
}
