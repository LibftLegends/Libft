#include "../test_internal.hpp"
#include "../../Modules/Crypto/crypto_network_backend.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_crypto_network_backend_wipe_rejects_null)
{
    crypto_network_backend backend;

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        backend.wipe(ft_nullptr, 1U));
    return (1);
}

FT_TEST(test_crypto_network_backend_wipe_clears_buffer)
{
    crypto_network_backend backend;
    uint8_t buffer[8];
    ft_size_t index;

    index = 0U;
    while (index < sizeof(buffer))
    {
        buffer[index] = 0xA5U;
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, backend.wipe(buffer, sizeof(buffer)));
    index = 0U;
    while (index < sizeof(buffer))
    {
        FT_ASSERT_EQ(0U, buffer[index]);
        index += 1U;
    }
    return (1);
}
