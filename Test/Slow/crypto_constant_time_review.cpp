#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Errno/errno.hpp"

#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdint>
#include <cstdio>

static const ft_size_t CRYPTO_TIMING_SAMPLE_COUNT = 8192U;
static const ft_size_t CRYPTO_TIMING_BATCH_SIZE = 32U;
static const ft_size_t CRYPTO_X25519_TIMING_SAMPLE_COUNT = 1024U;

static uint64_t crypto_timing_measure(const uint8_t *left,
    const uint8_t *right, volatile uint64_t &result_sink) noexcept
{
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;
    ft_size_t index;
    uint64_t local_sink = 0U;

    start_time = std::chrono::steady_clock::now();
    index = 0U;
    while (index < CRYPTO_TIMING_BATCH_SIZE)
    {
        local_sink += static_cast<uint64_t>(ft_constant_time_equal(left,
            right, 32U));
        index += 1U;
    }
    end_time = std::chrono::steady_clock::now();
    result_sink = result_sink + local_sink;
    return (static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::nanoseconds>(end_time - start_time).count()));
}

static double crypto_timing_welch_statistic(const uint64_t *first_samples,
    const uint64_t *second_samples, ft_size_t sample_count) noexcept
{
    double first_mean = 0.0;
    double second_mean = 0.0;
    double first_variance = 0.0;
    double second_variance = 0.0;
    double denominator;
    ft_size_t index;

    index = 0U;
    while (index < sample_count)
    {
        first_mean += static_cast<double>(first_samples[index]);
        second_mean += static_cast<double>(second_samples[index]);
        index += 1U;
    }
    first_mean /= static_cast<double>(sample_count);
    second_mean /= static_cast<double>(sample_count);
    index = 0U;
    while (index < sample_count)
    {
        double first_delta = static_cast<double>(first_samples[index])
            - first_mean;
        double second_delta = static_cast<double>(second_samples[index])
            - second_mean;

        first_variance += first_delta * first_delta;
        second_variance += second_delta * second_delta;
        index += 1U;
    }
    first_variance /= static_cast<double>(sample_count - 1U);
    second_variance /= static_cast<double>(sample_count - 1U);
    denominator = std::sqrt(first_variance / static_cast<double>(sample_count)
        + second_variance / static_cast<double>(sample_count));
    if (denominator == 0.0)
    {
        if (first_mean == second_mean)
            return (0.0);
        return (1.0e30);
    }
    return ((first_mean - second_mean) / denominator);
}

static uint64_t crypto_x25519_timing_measure(const uint8_t private_key[32],
    const uint8_t public_key[32], uint8_t shared_secret[32],
    volatile uint64_t &result_sink, int32_t &result) noexcept
{
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point end_time;

    start_time = std::chrono::steady_clock::now();
    result = crypto_x25519_shared_secret(private_key, public_key,
        shared_secret);
    end_time = std::chrono::steady_clock::now();
    if (result == FT_ERR_SUCCESS)
        result_sink = result_sink ^ shared_secret[0];
    return (static_cast<uint64_t>(std::chrono::duration_cast<
        std::chrono::nanoseconds>(end_time - start_time).count()));
}

int main(void)
{
    uint8_t first_value[32];
    uint8_t equal_value[32];
    uint8_t different_value[32];
    uint64_t equal_samples[CRYPTO_TIMING_SAMPLE_COUNT];
    uint64_t different_samples[CRYPTO_TIMING_SAMPLE_COUNT];
    uint8_t first_private_key[32];
    uint8_t second_private_key[32];
    uint8_t public_key[32] = {9U};
    uint8_t shared_secret[32];
    uint64_t first_x25519_samples[CRYPTO_X25519_TIMING_SAMPLE_COUNT];
    uint64_t second_x25519_samples[CRYPTO_X25519_TIMING_SAMPLE_COUNT];
    volatile uint64_t result_sink = 0U;
    ft_size_t index;
    uint32_t order_seed = 0x7135a9c1U;
    ft_bool x25519_operation_failed = FT_FALSE;
    double t_statistic;
    double x25519_t_statistic;

    index = 0U;
    while (index < sizeof(first_value))
    {
        first_value[index] = static_cast<uint8_t>(index * 17U + 3U);
        equal_value[index] = first_value[index];
        different_value[index] = first_value[index];
        index += 1U;
    }
    different_value[sizeof(different_value) - 1U] ^= 1U;
    if (ft_constant_time_equal(first_value, equal_value,
            sizeof(first_value)) != FT_TRUE
        || ft_constant_time_equal(first_value, different_value,
            sizeof(first_value)) != FT_FALSE)
    {
        std::fprintf(stderr, "%s\n",
            "constant-time comparator returned an incorrect result");
        return (1);
    }
    index = 0U;
    while (index < CRYPTO_TIMING_SAMPLE_COUNT)
    {
        order_seed = order_seed * 1664525U + 1013904223U;
        if ((order_seed & 1U) == 0U)
        {
            equal_samples[index] = crypto_timing_measure(first_value,
                equal_value, result_sink);
            different_samples[index] = crypto_timing_measure(first_value,
                different_value, result_sink);
        }
        else
        {
            different_samples[index] = crypto_timing_measure(first_value,
                different_value, result_sink);
            equal_samples[index] = crypto_timing_measure(first_value,
                equal_value, result_sink);
        }
        index += 1U;
    }
    t_statistic = crypto_timing_welch_statistic(equal_samples,
        different_samples, CRYPTO_TIMING_SAMPLE_COUNT);
    std::printf("constant-time comparison review: n=%" PRIu64
        " batch=%" PRIu64 " t=%.4f sink=%" PRIu64 "\n",
        static_cast<uint64_t>(CRYPTO_TIMING_SAMPLE_COUNT),
        static_cast<uint64_t>(CRYPTO_TIMING_BATCH_SIZE), t_statistic,
        static_cast<uint64_t>(result_sink));
    if (std::fabs(t_statistic) > 10.0)
    {
        std::fprintf(stderr, "%s\n",
            "timing review exceeded the configured Welch t threshold");
        return (1);
    }
    ft_memset(first_private_key, 0U, sizeof(first_private_key));
    ft_memset(second_private_key, 0xffU, sizeof(second_private_key));
    index = 0U;
    while (index < CRYPTO_X25519_TIMING_SAMPLE_COUNT
        && x25519_operation_failed == FT_FALSE)
    {
        int32_t operation_result;

        order_seed = order_seed * 1664525U + 1013904223U;
        if ((order_seed & 1U) == 0U)
        {
            first_x25519_samples[index] = crypto_x25519_timing_measure(
                first_private_key, public_key, shared_secret, result_sink,
                operation_result);
            if (operation_result != FT_ERR_SUCCESS)
                x25519_operation_failed = FT_TRUE;
            if (x25519_operation_failed == FT_FALSE)
                second_x25519_samples[index] = crypto_x25519_timing_measure(
                    second_private_key, public_key, shared_secret,
                    result_sink, operation_result);
        }
        else
        {
            second_x25519_samples[index] = crypto_x25519_timing_measure(
                second_private_key, public_key, shared_secret, result_sink,
                operation_result);
            if (operation_result != FT_ERR_SUCCESS)
                x25519_operation_failed = FT_TRUE;
            if (x25519_operation_failed == FT_FALSE)
                first_x25519_samples[index] = crypto_x25519_timing_measure(
                    first_private_key, public_key, shared_secret,
                    result_sink, operation_result);
        }
        if (operation_result != FT_ERR_SUCCESS)
            x25519_operation_failed = FT_TRUE;
        index += 1U;
    }
    if (x25519_operation_failed != FT_FALSE)
    {
        (void)crypto_secure_wipe(first_private_key,
            sizeof(first_private_key));
        (void)crypto_secure_wipe(second_private_key,
            sizeof(second_private_key));
        (void)crypto_secure_wipe(shared_secret, sizeof(shared_secret));
        std::fprintf(stderr, "%s\n", "X25519 timing review operation failed");
        return (1);
    }
    x25519_t_statistic = crypto_timing_welch_statistic(
        first_x25519_samples, second_x25519_samples,
        CRYPTO_X25519_TIMING_SAMPLE_COUNT);
    std::printf("X25519 scalar timing review: n=%" PRIu64 " t=%.4f\n",
        static_cast<uint64_t>(CRYPTO_X25519_TIMING_SAMPLE_COUNT),
        x25519_t_statistic);
    (void)crypto_secure_wipe(first_private_key, sizeof(first_private_key));
    (void)crypto_secure_wipe(second_private_key, sizeof(second_private_key));
    (void)crypto_secure_wipe(shared_secret, sizeof(shared_secret));
    if (std::fabs(x25519_t_statistic) > 10.0)
    {
        std::fprintf(stderr, "%s\n",
            "X25519 timing review exceeded the configured Welch t threshold");
        return (1);
    }
    return (0);
}
