#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_stream.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#include <cmath>
#include <limits>
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_rng_stream_retries_after_global_snapshot_lock_failure)
{
    constexpr uint32_t SEED = 78419U;
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    ft_seed_random_engine(SEED);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_lock_random_engine_mutex());
    const int32_t failed_initialize = stream.initialize();
    (void)rng_unlock_random_engine_mutex();
    int32_t retry_initialize = FT_ERR_INVALID_STATE;
    if (failed_initialize == FT_ERR_MUTEX_ALREADY_LOCKED)
        retry_initialize = stream.initialize();
    const int32_t disable_error = rng_disable_random_engine_thread_safety();
    FT_ASSERT_EQ(FT_ERR_MUTEX_ALREADY_LOCKED, failed_initialize);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retry_initialize);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, disable_error);
    const Pair<int32_t, int32_t> first = stream.random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first.key);
    FT_ASSERT_EQ(ft_random_int(), first.value);
    return (1);
}

FT_TEST(test_rng_stream_copy_and_move_depend_only_on_the_source)
{
    constexpr uint32_t SEED = 613119U;
    rng_stream source;
    rng_stream copy;
    rng_stream moved;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.reseed(SEED));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_lock_random_engine_mutex());
    const int32_t copy_error = copy.initialize(source);
    const uint32_t move_error = moved.move(source);
    (void)rng_unlock_random_engine_mutex();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_error);
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), move_error);
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());

    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);
    std::mt19937 reference(SEED);
    std::mt19937 reset_reference;
    const int32_t expected = distribution(reference);
    FT_ASSERT_EQ(expected, copy.random_int().value);
    FT_ASSERT_EQ(expected, moved.random_int().value);
    FT_ASSERT_EQ(distribution(reset_reference), source.random_int().value);
    return (1);
}

FT_TEST(test_rng_stream_failed_samples_keep_the_callers_mutex_acquisition)
{
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.enable_thread_safety());
    pt_recursive_mutex *mutex = stream.get_mutex_for_validation();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, mutex->lock());
    constexpr int32_t LOCK_ERROR = FT_ERR_SYS_MUTEX_LOCK_FAILED;
    pt_recursive_mutex_lock_override_error_code.store(LOCK_ERROR, std::memory_order_release);
    const int32_t errors[] = {
        stream.reseed(1U), stream.random_int().key, stream.random_float().key,
        stream.dice_roll(3, 6).key, stream.random_normal().key,
        stream.random_exponential(2.0f).key, stream.random_binomial(5, 0.4).key,
        stream.random_geometric(0.1).key
    };
    pt_recursive_mutex_lock_override_error_code.store(FT_ERR_SUCCESS, std::memory_order_release);
    const ft_bool owned = mutex->is_owned_by_thread(pt_thread_self());
    if (owned != FT_FALSE)
        (void)mutex->unlock();
    FT_ASSERT_EQ(FT_TRUE, owned);
    ft_size_t index = 0U;
    while (index < sizeof(errors) / sizeof(errors[0]))
    {
        FT_ASSERT_EQ(LOCK_ERROR, errors[index]);
        index += 1U;
    }
    return (1);
}

FT_TEST(test_rng_stream_preserves_seeded_scalar_and_counting_samplers)
{
    constexpr uint32_t SEED = 37977U;
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.reseed(SEED));
    ft_seed_random_engine(SEED);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.random_geometric(
        std::numeric_limits<double>::quiet_NaN()).key);
    const Pair<int32_t, float> normal = stream.random_normal();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, normal.key);
    FT_ASSERT(normal.value == ft_random_normal());
    const Pair<int32_t, float> exponential = stream.random_exponential(2.0f);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, exponential.key);
    FT_ASSERT(exponential.value == ft_random_exponential(2.0f));
    const Pair<int32_t, int32_t> dice = stream.dice_roll(55, 6);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dice.key);
    FT_ASSERT_EQ(ft_dice_roll(55, 6), dice.value);
    const Pair<int32_t, int32_t> binomial = stream.random_binomial(259, 0.27);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, binomial.key);
    FT_ASSERT_EQ(ft_random_binomial(259, 0.27), binomial.value);
    const Pair<int32_t, int32_t> geometric = stream.random_geometric(0.007);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, geometric.key);
    FT_ASSERT_EQ(ft_random_geometric(0.007), geometric.value);
    FT_ASSERT_EQ(ft_random_int(), stream.random_int().value);
    return (1);
}

FT_TEST(test_rng_gamma_family_preserves_seeded_stream_and_global_draws)
{
    constexpr uint32_t SEED = 4719U;
    rng_stream stream;
    std::mt19937 reference(SEED);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.reseed(SEED));
    ft_seed_random_engine(SEED);

    std::gamma_distribution<float> gamma(0.4f, 1.3f);
    std::gamma_distribution<float> alpha(2.1f, 1.0f);
    std::gamma_distribution<float> beta(4.6f, 1.0f);
    std::gamma_distribution<float> chi_squared(2.75f, 2.0f);
    const float gamma_reference = gamma(reference);
    const float alpha_draw = alpha(reference);
    const float beta_draw = beta(reference);
    const float beta_reference = alpha_draw / (alpha_draw + beta_draw);
    const float chi_squared_reference = chi_squared(reference);

    const Pair<int32_t, float> stream_gamma = stream.random_gamma(0.4f, 1.3f);
    const Pair<int32_t, float> stream_beta = stream.random_beta(2.1f, 4.6f);
    const Pair<int32_t, float> stream_chi_squared = stream.random_chi_squared(5.5f);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_gamma.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_beta.key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream_chi_squared.key);
    FT_ASSERT(stream_gamma.value == gamma_reference);
    FT_ASSERT(stream_beta.value == beta_reference);
    FT_ASSERT(stream_chi_squared.value == chi_squared_reference);
    FT_ASSERT(ft_random_gamma(0.4f, 1.3f) == gamma_reference);
    FT_ASSERT(ft_random_beta(2.1f, 4.6f) == beta_reference);
    FT_ASSERT(ft_random_chi_squared(5.5f) == chi_squared_reference);
    const int32_t next_reference = std::uniform_int_distribution<int32_t>(0, INT32_MAX)(reference);
    FT_ASSERT_EQ(next_reference, stream.random_int().value);
    FT_ASSERT_EQ(next_reference, ft_random_int());
    return (1);
}

FT_TEST(test_rng_gamma_family_rejects_invalid_parameters_before_sampling)
{
    constexpr uint32_t SEED = 91814U;
    const float infinity = std::numeric_limits<float>::infinity();
    const float not_a_number = std::numeric_limits<float>::quiet_NaN();
    const float tiny_degrees = std::numeric_limits<float>::denorm_min();
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.reseed(SEED));
    ft_seed_random_engine(SEED);

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.random_gamma(not_a_number, 1.0f).key);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.random_beta(1.0f, infinity).key);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.random_chi_squared(tiny_degrees).key);
    FT_ASSERT(ft_random_gamma(not_a_number, 1.0f) == 0.0f);
    FT_ASSERT(ft_random_beta(1.0f, infinity) == 0.0f);
    FT_ASSERT(ft_random_chi_squared(tiny_degrees) == 0.0f);
    std::mt19937 reference(SEED);
    const int32_t next_reference = std::uniform_int_distribution<int32_t>(0, INT32_MAX)(reference);
    FT_ASSERT_EQ(next_reference, stream.random_int().value);
    FT_ASSERT_EQ(next_reference, ft_random_int());
    return (1);
}

FT_TEST(test_rng_poisson_large_rates_match_across_seeded_engines)
{
    constexpr uint32_t SEED = 82237U;
    constexpr int32_t SAMPLE_COUNT = 8192;
    constexpr double RATES[] = {128.0, 1200.0};
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.enable_thread_safety());

    ft_size_t rate_index = 0;
    while (rate_index < sizeof(RATES) / sizeof(RATES[0]))
    {
        const double rate = RATES[rate_index];
        FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.reseed(SEED));
        ft_seed_random_engine(SEED);
        double sum = 0.0;
        double square_sum = 0.0;
        int32_t sample_index = 0;
        while (sample_index < SAMPLE_COUNT)
        {
            const Pair<int32_t, int32_t> sample = stream.random_poisson(rate);
            FT_ASSERT_EQ(FT_ERR_SUCCESS, sample.key);
            FT_ASSERT_EQ(ft_random_poisson(rate), sample.value);
            FT_ASSERT(sample.value >= 0);
            const double value = static_cast<double>(sample.value);
            sum += value;
            square_sum += value * value;
            sample_index += 1;
        }
        const double mean = sum / SAMPLE_COUNT;
        const double variance = square_sum / SAMPLE_COUNT - mean * mean;
        FT_ASSERT(std::fabs(mean - rate) < 5.0 * std::sqrt(rate / SAMPLE_COUNT));
        FT_ASSERT(std::fabs(variance - rate) < rate * 0.08);
        const Pair<int32_t, int32_t> next = stream.random_int();
        FT_ASSERT_EQ(FT_ERR_SUCCESS, next.key);
        FT_ASSERT_EQ(ft_random_int(), next.value);
        rate_index += 1;
    }
    return (1);
}

FT_TEST(test_rng_stream_poisson_validates_rate_without_consuming_engine)
{
    constexpr uint32_t SEED = 66413U;
    rng_stream stream;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stream.reseed(SEED));
    ft_seed_random_engine(SEED);

    const double tiny_rate = std::numeric_limits<double>::min();
    const Pair<int32_t, int32_t> tiny = stream.random_poisson(tiny_rate);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, tiny.key);
    FT_ASSERT_EQ(0, tiny.value);
    FT_ASSERT_EQ(0, ft_random_poisson(tiny_rate));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, stream.random_poisson(
        std::numeric_limits<double>::quiet_NaN()).key);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, stream.random_poisson(
        static_cast<double>(INT32_MAX) + 1.0).key);
    const Pair<int32_t, int32_t> next = stream.random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, next.key);
    FT_ASSERT_EQ(ft_random_int(), next.value);
    return (1);
}

FT_TEST(test_rng_stream_independent_seeds)
{
    rng_stream first_stream;
    rng_stream second_stream;
    int32_t first_values[3];
    int32_t second_values[3];
    int32_t index;
    Pair<int32_t, int32_t> first_result;
    Pair<int32_t, int32_t> second_result;

    if (first_stream.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (second_stream.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (first_stream.reseed(123U) != FT_ERR_SUCCESS)
        return (0);
    if (second_stream.reseed(456U) != FT_ERR_SUCCESS)
        return (0);
    index = 0;
    while (index < 3)
    {
        first_result = first_stream.random_int();
        second_result = second_stream.random_int();
        if (first_result.key != FT_ERR_SUCCESS || second_result.key != FT_ERR_SUCCESS)
            return (0);
        first_values[index] = first_result.value;
        second_values[index] = second_result.value;
        index = index + 1;
    }
    index = 0;
    while (index < 3)
    {
        if (first_values[index] != second_values[index])
            return (1);
        index = index + 1;
    }
    return (0);
}

FT_TEST(test_rng_stream_reseed_from_string)
{
    rng_stream stream_instance;
    int32_t initial_value;
    int32_t reseeded_value;
    Pair<int32_t, int32_t> value_result;
    int32_t reseed_error;

    if (stream_instance.initialize() != FT_ERR_SUCCESS)
        return (0);
    value_result = stream_instance.random_int();
    if (value_result.key != FT_ERR_SUCCESS)
        return (0);
    initial_value = value_result.value;
    reseed_error = stream_instance.reseed_from_string("example-seed");
    if (reseed_error != FT_ERR_SUCCESS)
        return (0);
    value_result = stream_instance.random_int();
    if (value_result.key != FT_ERR_SUCCESS)
        return (0);
    reseeded_value = value_result.value;
    if (initial_value == reseeded_value)
        return (0);
    reseed_error = stream_instance.reseed_from_string(ft_nullptr);
    if (reseed_error != FT_ERR_INVALID_ARGUMENT)
        return (0);
    return (1);
}

FT_TEST(test_rng_stream_copy_preserves_state)
{
    rng_stream original_stream;
    rng_stream copied_stream;
    int32_t first_sample;
    int32_t second_sample_original;
    int32_t second_sample_copy;
    Pair<int32_t, int32_t> sample_result;

    if (original_stream.initialize() != FT_ERR_SUCCESS)
        return (0);
    if (original_stream.reseed(987U) != FT_ERR_SUCCESS)
        return (0);
    sample_result = original_stream.random_int();
    if (sample_result.key != FT_ERR_SUCCESS)
        return (0);
    first_sample = sample_result.value;
    if (copied_stream.initialize(original_stream) != FT_ERR_SUCCESS)
        return (0);
    sample_result = original_stream.random_int();
    if (sample_result.key != FT_ERR_SUCCESS)
        return (0);
    second_sample_original = sample_result.value;
    sample_result = copied_stream.random_int();
    if (sample_result.key != FT_ERR_SUCCESS)
        return (0);
    second_sample_copy = sample_result.value;
    if (second_sample_original != second_sample_copy)
        return (0);
    if (copied_stream.reseed(1234U) != FT_ERR_SUCCESS)
        return (0);
    int32_t independent_sample_original;
    int32_t independent_sample_copy;

    sample_result = original_stream.random_int();
    if (sample_result.key != FT_ERR_SUCCESS)
        return (0);
    independent_sample_original = sample_result.value;
    sample_result = copied_stream.random_int();
    if (sample_result.key != FT_ERR_SUCCESS)
        return (0);
    independent_sample_copy = sample_result.value;
    if (independent_sample_original == independent_sample_copy)
        return (0);
    if (first_sample == second_sample_original)
        return (0);
    return (1);
}
