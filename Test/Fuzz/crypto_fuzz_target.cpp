#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t key[32] = {0U};
    uint8_t nonce[12] = {0U};
    uint8_t digest[32] = {0U};
    uint8_t public_key[32] = {0U};
    uint8_t shared_secret[32] = {0U};
    uint8_t tag[16] = {0U};
    const uint8_t *payload = ft_nullptr;
    ft_size_t payload_size = 0U;
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    const uint8_t *ciphertext_data;
    ft_size_t associated_data_size;
    size_t index;

    if (data == ft_nullptr)
        return (0);
    index = 0U;
    while (index < sizeof(key) && index < size)
    {
        key[index] = data[index];
        index += 1U;
    }
    while (index < sizeof(key))
    {
        key[index] = 0U;
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(nonce) && index < size)
    {
        nonce[index] = data[index];
        index += 1U;
    }
    index = sizeof(key) + sizeof(nonce);
    if (size > index)
    {
        payload = data + index;
        payload_size = size - index;
    }
    associated_data_size = size;
    if (associated_data_size > 32U)
        associated_data_size = 32U;
    (void)crypto_sha256_hash(data, size, digest);
    (void)crypto_x25519_public_key(key, public_key);
    (void)crypto_x25519_shared_secret(key, public_key, shared_secret);
    (void)crypto_chacha20_poly1305_seal(key, nonce, data,
        associated_data_size, payload, payload_size, ciphertext, tag);
    if (ciphertext.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        ciphertext_data = ft_nullptr;
        if (ciphertext.size() > 0U)
            ciphertext_data = &ciphertext[0];
        (void)crypto_chacha20_poly1305_open(key, nonce, data,
            associated_data_size,
            ciphertext_data,
            ciphertext.size(), tag, plaintext);
    }
    (void)ciphertext.destroy();
    (void)plaintext.destroy();
    return (0);
}
