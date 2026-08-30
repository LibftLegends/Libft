#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t key[64] = {0U};
    uint8_t digest[32] = {0U};
    uint8_t pseudorandom_key[32] = {0U};
    uint8_t output[256] = {0U};
    const uint8_t *message;
    const uint8_t *salt;
    const uint8_t *info;
    ft_size_t key_length;
    ft_size_t message_length;
    ft_size_t salt_length;
    ft_size_t info_length;

    if (data == ft_nullptr && size != 0U)
        return (0);
    key_length = size;
    if (key_length > sizeof(key))
        key_length = sizeof(key);
    if (key_length != 0U)
        ft_memcpy(key, data, key_length);
    message = ft_nullptr;
    message_length = 0U;
    if (size > key_length)
    {
        message = data + key_length;
        message_length = size - key_length;
    }
    salt = message;
    salt_length = message_length;
    if (salt_length > 32U)
        salt_length = 32U;
    info = message;
    info_length = message_length;
    if (info_length > 64U)
        info_length = 64U;
    (void)crypto_hmac_sha256(key, key_length, message, message_length,
        digest);
    (void)crypto_hkdf_sha256_extract(salt, salt_length, key, key_length,
        pseudorandom_key);
    (void)crypto_hkdf_sha256_expand(pseudorandom_key, info, info_length,
        output, sizeof(output));
    (void)crypto_secure_wipe(key, sizeof(key));
    (void)crypto_secure_wipe(digest, sizeof(digest));
    (void)crypto_secure_wipe(pseudorandom_key, sizeof(pseudorandom_key));
    (void)crypto_secure_wipe(output, sizeof(output));
    return (0);
}
