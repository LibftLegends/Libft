#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"

#include <cstdint>

int main()
{
    const uint8_t expected[32] = {
        0x7cU, 0x39U, 0x11U, 0xe0U, 0xabU, 0x25U, 0x86U, 0xfdU,
        0x86U, 0x44U, 0x97U, 0x29U, 0x7eU, 0x57U, 0x5eU, 0x6fU,
        0x3bU, 0xc6U, 0x01U, 0xc0U, 0x88U, 0x3cU, 0x30U, 0xdfU,
        0x5fU, 0x4dU, 0xd2U, 0xd2U, 0x4fU, 0x66U, 0x54U, 0x24U
    };
    uint8_t scalar[32] = {9U};
    uint8_t u_coordinate[32] = {9U};
    uint8_t old_scalar[32];
    uint8_t result[32];
    uint32_t iteration;

    iteration = 0U;
    while (iteration < 1000000U)
    {
        ft_memcpy(old_scalar, scalar, sizeof(old_scalar));
        if (crypto_x25519_shared_secret(scalar, u_coordinate, result)
            != FT_ERR_SUCCESS)
            return (1);
        ft_memcpy(scalar, result, sizeof(scalar));
        ft_memcpy(u_coordinate, old_scalar, sizeof(u_coordinate));
        iteration += 1U;
    }
    if (ft_memcmp(scalar, expected, sizeof(expected)) != 0)
        return (1);
    return (0);
}
