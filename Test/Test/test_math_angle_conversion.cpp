#include "../test_internal.hpp"
#include "../../Modules/Math/math.hpp"
#include <cmath>
#include <limits>
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Math/math_interval.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_math_rad2deg_converts_positive_angle)
{
    double result;

    result = math_rad2deg(3.14159265358979323846 / 3.0);
    FT_ASSERT(math_fabs(result - 60.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_rad2deg_converts_negative_angle)
{
    double result;

    result = math_rad2deg(-3.14159265358979323846 / 4.0);
    FT_ASSERT(math_fabs(result + 45.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_deg2rad_converts_full_rotation)
{
    double result;

    result = math_deg2rad(180.0);
    FT_ASSERT(math_fabs(result - 3.14159265358979323846) < 0.000001);
    return (1);
}

FT_TEST(test_math_deg2rad_handles_fractional_degrees)
{
    double result;

    result = math_deg2rad(22.5);
    FT_ASSERT(math_fabs(result - 0.39269908169872414) < 0.000001);
    return (1);
}

FT_TEST(test_math_rad2deg_zero_angle)
{
    double result;

    result = math_rad2deg(0.0);
    FT_ASSERT(math_fabs(result - 0.0) < 0.000001);
    return (1);
}

FT_TEST(test_math_angle_conversion_avoids_intermediate_overflow)
{
    const double maximum = std::numeric_limits<double>::max();
    const double radians = math_deg2rad(maximum);
    const double degrees = math_rad2deg(maximum / 64.0);

    FT_ASSERT(std::isfinite(radians));
    FT_ASSERT(std::isfinite(degrees));
    FT_ASSERT(math_fabs(radians / maximum - 0.017453292519943295) < 1e-16);
    FT_ASSERT(math_fabs(degrees / maximum - 0.8952465548919113) < 1e-15);
    FT_ASSERT_EQ(-0.0, math_deg2rad(-0.0));
    FT_ASSERT_EQ(-0.0, math_rad2deg(-0.0));
    return (1);
}
