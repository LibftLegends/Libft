#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Crypto/crypto_random.hpp"
#include "../../Modules/Crypto/crypto_test_hooks.hpp"
#include "../../Modules/Errno/errno.hpp"

#ifdef LIBFT_TEST_BUILD

FT_TEST(test_crypto_random_large_request_and_deterministic_provider)
{
    uint8_t first[4096];
    uint8_t second[4096];
    uint32_t index = 0U;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        crypto_random_bytes(ft_nullptr, sizeof(first)));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, crypto_random_bytes(first, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x9f34a2d1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_random_bytes(first, sizeof(first)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x9f34a2d1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_random_bytes(second, sizeof(second)));
    while (index < sizeof(first))
    {
        FT_ASSERT_EQ(static_cast<int32_t>(first[index]),
            static_cast<int32_t>(second[index]));
        index = index + 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}

FT_TEST(test_crypto_random_provider_failure_wipes_requested_output)
{
    uint8_t output[257];
    uint32_t index = 0U;

    ft_memset(output, 0xa5, sizeof(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_seed(0x12345678U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_fail_next());
    FT_ASSERT_EQ(FT_ERR_IO, crypto_random_bytes(output, sizeof(output)));
    while (index < sizeof(output))
    {
        FT_ASSERT_EQ(0, output[index]);
        index = index + 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, crypto_test_random_clear());
    return (1);
}

#endif
