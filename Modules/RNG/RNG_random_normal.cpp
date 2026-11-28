#include "rng.hpp"
#include "../Errno/errno.hpp"
#include "../Math/math.hpp"

float ft_random_normal(void)
{
    constexpr float PI_VALUE = 3.14159265358979323846f;
    constexpr float MIN_UNIFORM = 0.0000000001f;
    float uniforms[2];
    if (ft_random_float_vector(uniforms, 2U) != FT_ERR_SUCCESS)
    {
        uniforms[0] = ft_random_float();
        uniforms[1] = ft_random_float();
    }
    if (uniforms[0] < MIN_UNIFORM)
        uniforms[0] = MIN_UNIFORM;
    const double log_value = math_log(uniforms[0]);
    const float radius = static_cast<float>(math_sqrt(-2.0 * log_value));
    const float angle = 2.0f * PI_VALUE * uniforms[1];
    return (radius * static_cast<float>(math_cos(angle)));
}
