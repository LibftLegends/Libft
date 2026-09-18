#include "../../Modules/Crypto/crypto_test_hooks.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <atomic>

namespace
{
    static std::atomic<uint64_t> g_random_state(0U);
    static std::atomic<ft_bool> g_random_override(FT_FALSE);
    static std::atomic<uint64_t> g_random_attempts(0U);
    static std::atomic<uint64_t> g_random_failure_target(0U);
    static std::atomic<uint64_t> g_random_failures(0U);
}

int32_t crypto_test_random_seed(uint64_t seed) noexcept
{
    if (seed == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    g_random_state.store(seed, std::memory_order_release);
    g_random_override.store(FT_TRUE, std::memory_order_release);
    g_random_attempts.store(0U, std::memory_order_release);
    g_random_failure_target.store(0U, std::memory_order_release);
    g_random_failures.store(0U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t crypto_test_random_clear() noexcept
{
    g_random_override.store(FT_FALSE, std::memory_order_release);
    g_random_state.store(0U, std::memory_order_release);
    g_random_attempts.store(0U, std::memory_order_release);
    g_random_failure_target.store(0U, std::memory_order_release);
    g_random_failures.store(0U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t crypto_test_random_fail_next() noexcept
{
    return (crypto_test_random_fail_after(0U));
}

int32_t crypto_test_random_fail_after(uint64_t successful_calls) noexcept
{
    uint64_t attempt_count;

    if (g_random_override.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (successful_calls == UINT64_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    attempt_count = g_random_attempts.load(std::memory_order_acquire);
    if (attempt_count > UINT64_MAX - successful_calls - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    g_random_failure_target.store(
        attempt_count + successful_calls + 1U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

uint64_t crypto_test_random_attempt_count() noexcept
{
    return (g_random_attempts.load(std::memory_order_acquire));
}

uint64_t crypto_test_random_failure_count() noexcept
{
    return (g_random_failures.load(std::memory_order_acquire));
}

ft_bool crypto_test_random_should_fail() noexcept
{
    uint64_t attempt_number;
    uint64_t failure_target;

    attempt_number = g_random_attempts.fetch_add(
        1U, std::memory_order_acq_rel) + 1U;
    failure_target = g_random_failure_target.load(
        std::memory_order_acquire);
    if (failure_target != 0U && failure_target == attempt_number)
    {
        if (g_random_failure_target.compare_exchange_strong(
                failure_target, 0U, std::memory_order_acq_rel,
                std::memory_order_acquire) == FT_TRUE)
        {
            g_random_failures.fetch_add(1U, std::memory_order_acq_rel);
            return (FT_TRUE);
        }
    }
    return (FT_FALSE);
}

ft_bool crypto_test_random_bytes(uint8_t *output, ft_size_t length) noexcept
{
    ft_size_t index;
    uint64_t state;

    if (g_random_override.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_FALSE);
    if (output == ft_nullptr && length != 0U)
        return (FT_FALSE);
    state = g_random_state.load(std::memory_order_acquire);
    index = 0U;
    while (index < length)
    {
        state ^= state << 13U;
        state ^= state >> 7U;
        state ^= state << 17U;
        output[index] = static_cast<uint8_t>(state >> 56U);
        index += 1U;
    }
    g_random_state.store(state, std::memory_order_release);
    return (FT_TRUE);
}
