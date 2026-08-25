#include "crypto_test_hooks.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <atomic>

namespace
{
    static std::atomic<uint64_t> g_random_state(0U);
    static std::atomic<ft_bool> g_random_override(FT_FALSE);
    static std::atomic<ft_bool> g_random_failure(FT_FALSE);
}

int32_t crypto_test_random_seed(uint64_t seed) noexcept
{
    if (seed == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    g_random_state.store(seed, std::memory_order_release);
    g_random_override.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t crypto_test_random_clear() noexcept
{
    g_random_override.store(FT_FALSE, std::memory_order_release);
    g_random_state.store(0U, std::memory_order_release);
    g_random_failure.store(FT_FALSE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t crypto_test_random_fail_next() noexcept
{
    g_random_failure.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

ft_bool crypto_test_random_should_fail() noexcept
{
    ft_bool expected;

    expected = FT_TRUE;
    if (g_random_failure.compare_exchange_strong(expected, FT_FALSE,
            std::memory_order_acq_rel) == FT_TRUE)
        return (FT_TRUE);
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
