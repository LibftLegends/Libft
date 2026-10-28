#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/Crypto/crypto_random.hpp"
#include "../../Modules/Crypto/crypto_test_hooks.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "compatebility_system_test_hooks.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <cerrno>

FT_TEST(test_rng_secure_bytes_null_buffer_sets_ft_einval)
{
    if (rng_secure_bytes(ft_nullptr, 16) != FT_ERR_INVALID_ARGUMENT)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_bytes_success)
{
    unsigned char buffer[8];
    if (rng_secure_bytes(buffer, 8) != FT_ERR_SUCCESS)
        return (0);
    return (1);
}

#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>

FT_TEST(test_rng_secure_bytes_windows_failures_wipe_crypto_output)
{
    uint8_t buffer[32];
    uint32_t failure_index;
    uint32_t byte_index;
    int32_t error_code;

    cmp_clear_force_rng_windows_failures();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    failure_index = 0U;
    while (failure_index < 3U)
    {
        ft_memset(buffer, 0xa5U, sizeof(buffer));
        if (failure_index == 0U)
            cmp_force_rng_windows_acquire_failure(ERROR_ACCESS_DENIED);
        else if (failure_index == 1U)
            cmp_force_rng_windows_generate_failure(ERROR_ACCESS_DENIED);
        else
            cmp_force_rng_windows_release_failure(ERROR_ACCESS_DENIED);
        error_code = crypto_random_bytes(buffer, sizeof(buffer));
        FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, error_code);
        byte_index = 0U;
        while (byte_index < sizeof(buffer))
        {
            FT_ASSERT_EQ(0U, buffer[byte_index]);
            byte_index += 1U;
        }
        failure_index += 1U;
    }
    cmp_clear_force_rng_windows_failures();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}
#endif

#if !defined(_WIN32) && !defined(_WIN64)
FT_TEST(test_rng_secure_bytes_open_failure_propagates_errno)
{
    unsigned char buffer[4];
    cmp_clear_force_rng_failures();
    cmp_force_rng_open_failure(EACCES);
    int result = rng_secure_bytes(buffer, 4);
    cmp_clear_force_rng_failures();
    if (result != FT_ERR_INVALID_OPERATION)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_bytes_read_failure_propagates_errno)
{
    unsigned char buffer[4];
    cmp_clear_force_rng_failures();
    cmp_force_rng_read_failure(EIO);
    int result = rng_secure_bytes(buffer, 4);
    cmp_clear_force_rng_failures();
    if (result != FT_ERR_IO)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_bytes_detects_unexpected_eof)
{
    unsigned char buffer[4];

    cmp_clear_force_rng_failures();
    cmp_force_rng_read_eof();
    int result = rng_secure_bytes(buffer, 4);
    cmp_clear_force_rng_failures();
    if (result != FT_ERR_IO)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_bytes_close_failure_propagates_errno)
{
    unsigned char buffer[4];
    cmp_clear_force_rng_failures();
    cmp_force_rng_close_failure(EBADF);
    int result = rng_secure_bytes(buffer, 4);
    cmp_clear_force_rng_failures();
    if (result != FT_ERR_INVALID_HANDLE)
        return (0);
    return (1);
}

FT_TEST(test_rng_secure_bytes_retries_interrupted_short_reads)
{
    unsigned char buffer[37];
    uint32_t index;
    ft_bool output_changed;

    index = 0U;
    while (index < sizeof(buffer))
    {
        buffer[index] = 0xa5U;
        index += 1U;
    }
    cmp_clear_force_rng_failures();
    cmp_force_rng_short_reads(3U);
    cmp_force_rng_read_failure(EINTR);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_secure_bytes(buffer, sizeof(buffer)));
    cmp_clear_force_rng_failures();
    output_changed = FT_FALSE;
    index = 0U;
    while (index < sizeof(buffer))
    {
        if (buffer[index] != 0xa5U)
            output_changed = FT_TRUE;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, output_changed);
    return (1);
}
#endif
