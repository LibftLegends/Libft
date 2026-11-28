#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_rng_test_utils.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Math/math.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include <limits>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_rng_batched_samplers_preserve_seeded_mixed_sequence)
{
    constexpr uint32_t SEED = 873911U;
    constexpr int32_t TRIAL_COUNT = 259;
    constexpr double PROBABILITY = 0.36;
    constexpr float PI_VALUE = 3.14159265358979323846f;
    std::mt19937 reference(SEED);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    std::uniform_int_distribution<int32_t> integers(0, INT32_MAX);
    const float radius = static_cast<float>(math_sqrt(-2.0 * math_log(uniform(reference))));
    const float angle = 2.0f * PI_VALUE * uniform(reference);
    const float normal_reference = radius * static_cast<float>(math_cos(angle));
    int32_t binomial_reference = 0;
    int32_t trial_index = 0;
    while (trial_index < TRIAL_COUNT)
    {
        if (static_cast<double>(uniform(reference)) < PROBABILITY)
            binomial_reference += 1;
        trial_index += 1;
    }
    const float exponential_reference = static_cast<float>(-math_log(uniform(reference))) / 2.0f;
    const int32_t next_reference = integers(reference);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    ft_seed_random_engine(SEED);
    const float normal = ft_random_normal();
    const int32_t empty = ft_random_binomial(0, PROBABILITY);
    const int32_t binomial = ft_random_binomial(TRIAL_COUNT, PROBABILITY);
    const float exponential = ft_random_exponential(2.0f);
    const int32_t next = ft_random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());
    FT_ASSERT(normal_reference == normal);
    FT_ASSERT_EQ(0, empty);
    FT_ASSERT_EQ(binomial_reference, binomial);
    FT_ASSERT(exponential_reference == exponential);
    FT_ASSERT_EQ(next_reference, next);
    return (1);
}

FT_TEST(test_rng_variable_length_samplers_stop_without_using_extra_draws)
{
    constexpr uint32_t SEED = 67181U;
    constexpr double PROBABILITY = 0.004;
    constexpr double LAMBDA = 10.0;
    std::mt19937 reference(SEED);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    std::uniform_int_distribution<int32_t> integers(0, INT32_MAX);
    const double limit = math_exp(-LAMBDA);
    int32_t geometric_reference = 1;
    while (static_cast<double>(uniform(reference)) >= PROBABILITY)
        geometric_reference += 1;
    double product = 1.0;
    int32_t poisson_reference = -1;
    while (product > limit)
    {
        product *= static_cast<double>(uniform(reference));
        poisson_reference += 1;
    }
    const int32_t next_reference = integers(reference);
    FT_ASSERT(geometric_reference > 64);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    ft_seed_random_engine(SEED);
    const int32_t geometric = ft_random_geometric(PROBABILITY);
    const int32_t poisson = ft_random_poisson(LAMBDA);
    const int32_t next = ft_random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());
    FT_ASSERT_EQ(geometric_reference, geometric);
    FT_ASSERT_EQ(poisson_reference, poisson);
    FT_ASSERT_EQ(next_reference, next);
    return (1);
}

FT_TEST(test_rng_variable_length_samplers_reject_nonfinite_inputs)
{
    constexpr uint32_t SEED = 7191U;
    std::mt19937 reference(SEED);
    std::uniform_int_distribution<int32_t> integers(0, INT32_MAX);

    ft_seed_random_engine(SEED);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        ft_random_geometric(std::numeric_limits<double>::quiet_NaN()));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        ft_random_poisson(std::numeric_limits<double>::infinity()));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        ft_random_poisson(std::numeric_limits<double>::quiet_NaN()));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, ft_random_poisson(1.0e100));
    FT_ASSERT_EQ(integers(reference), ft_random_int());
    return (1);
}

FT_TEST(test_rng_seed_override_from_environment)
{
    const char  *environment_name = "FT_RNG_TEST_SEED";
    char        *original_value;
    ft_string    original_copy;
    bool        restore_original = false;
    uint32_t    baseline_seed;
    uint32_t    override_seed;
    uint32_t    expected_override_seed;
    int         first_sample;
    int         second_sample;

    original_value = su_getenv(environment_name);
    if (original_value != ft_nullptr && *original_value != '\0')
    {
        if (original_copy.initialize() != FT_ERR_SUCCESS)
            return (0);
        original_copy = original_value;
        if (original_copy.get_error() != FT_ERR_SUCCESS)
            return (0);
        restore_original = true;
    }
    su_unsetenv(environment_name);
    baseline_seed = ft_rng_test_seed_value(100u, "baseline");
    FT_ASSERT_EQ(100u, baseline_seed);
    expected_override_seed = ft_random_seed("override");
    expected_override_seed = expected_override_seed ^ ft_random_seed("baseline");
    expected_override_seed = expected_override_seed ^ 100u;
    if (su_setenv(environment_name, "override", 1) != 0)
    {
        if (restore_original == true)
            su_setenv(environment_name, original_copy.c_str(), 1);
        else
            su_unsetenv(environment_name);
        return (0);
    }
    override_seed = ft_rng_test_seed_value(100u, "baseline");
    FT_ASSERT_EQ(expected_override_seed, override_seed);
    ft_rng_test_seed_engine(100u, "baseline");
    first_sample = ft_random_int();
    ft_rng_test_seed_engine(100u, "baseline");
    second_sample = ft_random_int();
    FT_ASSERT_EQ(first_sample, second_sample);
    if (restore_original == true)
    {
        if (su_setenv(environment_name, original_copy.c_str(), 1) != 0)
            return (0);
    }
    else
        su_unsetenv(environment_name);
    return (1);
}

FT_TEST(test_rng_random_int_reproducible)
{
    ft_rng_test_seed_engine(246u, "rng_random_int_first");
    int first_sequence[3];
    int index = 0;
    while (index < 3)
    {
        first_sequence[index] = ft_random_int();
        index = index + 1;
    }
    ft_rng_test_seed_engine(246u, "rng_random_int_first");
    int second_sequence[3];
    index = 0;
    while (index < 3)
    {
        second_sequence[index] = ft_random_int();
        index = index + 1;
    }
    index = 0;
    while (index < 3)
    {
        FT_ASSERT_EQ(first_sequence[index], second_sequence[index]);
        index = index + 1;
    }
    ft_rng_test_seed_engine(135u, "rng_random_int_second");
    int third_sequence[3];
    index = 0;
    while (index < 3)
    {
        third_sequence[index] = ft_random_int();
        index = index + 1;
    }
    bool difference_found = false;
    index = 0;
    while (index < 3)
    {
        if (third_sequence[index] != first_sequence[index])
            difference_found = true;
        index = index + 1;
    }
    if (difference_found == false)
        return (0);
    return (1);
}

FT_TEST(test_rng_random_float_reproducible)
{
    ft_rng_test_seed_engine(97531u, "rng_random_float_first");
    float first_sequence[3];
    int index = 0;
    while (index < 3)
    {
        first_sequence[index] = ft_random_float();
        if (first_sequence[index] < 0.0f)
            return (0);
        if (first_sequence[index] >= 1.0f)
            return (0);
        index = index + 1;
    }
    ft_rng_test_seed_engine(97531u, "rng_random_float_first");
    float second_sequence[3];
    index = 0;
    while (index < 3)
    {
        second_sequence[index] = ft_random_float();
        float difference_value = second_sequence[index] - first_sequence[index];
        if (difference_value < 0.0f)
            difference_value = -difference_value;
        if (difference_value > 0.0000001f)
            return (0);
        if (second_sequence[index] < 0.0f)
            return (0);
        if (second_sequence[index] >= 1.0f)
            return (0);
        index = index + 1;
    }
    ft_rng_test_seed_engine(123u, "rng_random_float_second");
    float third_sequence[3];
    index = 0;
    while (index < 3)
    {
        third_sequence[index] = ft_random_float();
        if (third_sequence[index] < 0.0f)
            return (0);
        if (third_sequence[index] >= 1.0f)
            return (0);
        index = index + 1;
    }
    bool difference_found = false;
    index = 0;
    while (index < 3)
    {
        float difference_value = third_sequence[index] - first_sequence[index];
        if (difference_value < 0.0f)
            difference_value = -difference_value;
        if (difference_value > 0.0000001f)
            difference_found = true;
        index = index + 1;
    }
    if (difference_found == false)
        return (0);
    return (1);
}

FT_TEST(test_rng_random_normal)
{
    ft_rng_test_seed_engine(123u, "rng_random_normal");
    int sample_count = 10000;
    int index = 0;
    float sum_values = 0.0f;
    while (index < sample_count)
    {
        sum_values = sum_values + ft_random_normal();
        index = index + 1;
    }
    float mean_value = sum_values / static_cast<float>(sample_count);
    if (mean_value < -0.1f)
        return (0);
    if (mean_value > 0.1f)
        return (0);
    return (1);
}

FT_TEST(test_rng_random_exponential)
{
    ft_rng_test_seed_engine(123u, "rng_random_exponential");
    int sample_count = 10000;
    int index = 0;
    float sum_values = 0.0f;
    while (index < sample_count)
    {
        sum_values = sum_values + ft_random_exponential(2.0f);
        index = index + 1;
    }
    float mean_value = sum_values / static_cast<float>(sample_count);
    if (mean_value < 0.4f)
        return (0);
    if (mean_value > 0.6f)
        return (0);
    return (1);
}

FT_TEST(test_rng_random_poisson)
{
    ft_rng_test_seed_engine(123u, "rng_random_poisson");
    int sample_count = 10000;
    int index = 0;
    int sum_values = 0;
    double lambda_value = 4.0;
    while (index < sample_count)
    {
        sum_values = sum_values + ft_random_poisson(lambda_value);
        index = index + 1;
    }
    double mean_value = static_cast<double>(sum_values) /
                         static_cast<double>(sample_count);
    if (mean_value < lambda_value - 0.1)
        return (0);
    if (mean_value > lambda_value + 0.1)
        return (0);
    return (1);
}

FT_TEST(test_rng_random_binomial)
{
    ft_rng_test_seed_engine(123u, "rng_random_binomial");
    int sample_count = 10000;
    int index = 0;
    int sum_values = 0;
    int trial_count = 10;
    double success_probability = 0.5;
    while (index < sample_count)
    {
        sum_values = sum_values +
            ft_random_binomial(trial_count, success_probability);
        index = index + 1;
    }
    double mean_value = static_cast<double>(sum_values) /
                         static_cast<double>(sample_count);
    if (mean_value < 4.9)
        return (0);
    if (mean_value > 5.1)
        return (0);
    FT_ASSERT_EQ(0, ft_random_binomial(0, success_probability));
    FT_ASSERT_EQ(0, ft_random_binomial(trial_count, 0.0));
    FT_ASSERT_EQ(trial_count, ft_random_binomial(trial_count, 1.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_binomial(-3, success_probability));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_binomial(trial_count, -0.1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_binomial(trial_count, 1.5));
    return (1);
}

FT_TEST(test_rng_random_geometric)
{
    ft_rng_test_seed_engine(123u, "rng_random_geometric");
    int sample_count = 10000;
    int index = 0;
    int sum_values = 0;
    double success_probability = 0.25;
    while (index < sample_count)
    {
        sum_values = sum_values + ft_random_geometric(success_probability);
        index = index + 1;
    }
    double mean_value = static_cast<double>(sum_values) /
                         static_cast<double>(sample_count);
    if (mean_value < 3.9)
        return (0);
    if (mean_value > 4.1)
        return (0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_geometric(0.0));
    FT_ASSERT_EQ(1, ft_random_geometric(1.0));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_geometric(-0.1));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_random_geometric(1.5));
    return (1);
}
