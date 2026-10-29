#include "crypto_random.hpp"
#include "crypto_primitives.hpp"
#include "../Errno/errno.hpp"
#include "../RNG/rng.hpp"
#ifdef LIBFT_TEST_BUILD
# include "crypto_test_hooks.hpp"
#endif

int32_t crypto_random_bytes(uint8_t *output, ft_size_t length) noexcept
{
    int32_t result;

    if (output == ft_nullptr || length == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
#ifdef LIBFT_TEST_BUILD
    if (crypto_test_random_should_fail() != FT_FALSE)
    {
        (void)crypto_secure_wipe(output, length);
        return (FT_ERR_IO);
    }
    if (crypto_test_random_bytes(output, length) != FT_FALSE)
        return (FT_ERR_SUCCESS);
#endif
    result = rng_secure_bytes(output, length);
    if (result != FT_ERR_SUCCESS)
    {
        (void)crypto_secure_wipe(output, length);
        return (result);
    }
    return (FT_ERR_SUCCESS);
}
