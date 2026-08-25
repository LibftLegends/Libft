#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t private_key[32] = {0U};
    uint8_t public_key[32] = {0U};
    uint8_t output[32] = {0U};
    ft_size_t index;

    if (data == ft_nullptr && size != 0U)
        return (0);
    index = 0U;
    while (index < sizeof(private_key) && index < size)
    {
        private_key[index] = data[index];
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(public_key) && index + sizeof(private_key) < size)
    {
        public_key[index] = data[index + sizeof(private_key)];
        index += 1U;
    }
    (void)crypto_x25519_public_key(private_key, output);
    (void)crypto_x25519_shared_secret(private_key, public_key, output);
    return (0);
}
