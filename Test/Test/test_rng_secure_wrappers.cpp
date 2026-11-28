#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "compatebility_system_test_hooks.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <cerrno>
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#endif

FT_TEST(test_rng_secure_uint64_null_pointer_sets_error)
{
    if (rng_secure_uint64(ft_nullptr) != FT_ERR_INVALID_ARGUMENT)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_uint32_success_returns_value)
{
    uint32_t value = 0;
    if (rng_secure_uint32(&value) != FT_ERR_SUCCESS)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_uint32_preserves_fallback_status_and_neighboring_memory)
{
    uint32_t values[] = {0xF8DC1279U, 0U, 0x3B2F4591U};
    int32_t fallback_used = -1;
#if defined(_WIN32) || defined(_WIN64)
    cmp_force_rng_windows_acquire_failure(ERROR_ACCESS_DENIED);
#else
    cmp_force_rng_open_failure(EACCES);
#endif
    const int32_t result = rng_secure_uint32(&values[1], &fallback_used);
#if defined(_WIN32) || defined(_WIN64)
    cmp_clear_force_rng_windows_failures();
#else
    cmp_clear_force_rng_failures();
#endif
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(1, fallback_used);
    FT_ASSERT_EQ(0xF8DC1279U, values[0]);
    FT_ASSERT_EQ(0x3B2F4591U, values[2]);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, rng_secure_uint32(ft_nullptr, &fallback_used));
    FT_ASSERT_EQ(0, fallback_used);
    return (1);
}

FT_TEST(test_rng_secure_bytes_with_fallback_zero_length_is_success)
{
    int32_t fallback_used = 42;
    if (rng_secure_bytes_with_fallback(ft_nullptr, 0, &fallback_used) != FT_ERR_SUCCESS)
        return (0);
    if (fallback_used != 0)
        return (0);
    return (1);
}

#if !defined(_WIN32) && !defined(_WIN64)
FT_TEST(test_rng_secure_uint64_falls_back_when_system_rng_fails)
{
    uint64_t value = 0;
    int32_t fallback_used = 0;
    cmp_clear_force_rng_failures();
    cmp_force_rng_open_failure(EACCES);
    if (rng_secure_uint64(&value, &fallback_used) != FT_ERR_SUCCESS)
    {
        cmp_clear_force_rng_failures();
        return (0);
    }
    cmp_clear_force_rng_failures();
    if (fallback_used != 1)
        return (0);
    return (1);
}
#endif

FT_TEST(test_rng_secure_bytes_with_fallback_reports_success)
{
    unsigned char buffer[16];
    int32_t fallback_used = -1;
    if (rng_secure_bytes_with_fallback(buffer, sizeof(buffer), &fallback_used) != FT_ERR_SUCCESS)
        return (0);
    if (fallback_used != 0 && fallback_used != 1)
        return (0);
    return (1);
}
