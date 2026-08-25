#include "../../Modules/Crypto/crypto_aead.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t key[32] = {0U};
    uint8_t nonce[12] = {0U};
    uint8_t tag[16] = {0U};
    const uint8_t *payload;
    ft_size_t payload_size;
    ft_vector<uint8_t> ciphertext;
    ft_vector<uint8_t> plaintext;
    ft_size_t index;
    ft_size_t associated_data_size;

    if (data == ft_nullptr && size != 0U)
        return (0);
    index = 0U;
    while (index < sizeof(key) && index < size)
    {
        key[index] = data[index];
        index += 1U;
    }
    index = 0U;
    while (index < sizeof(nonce) && index < size)
    {
        nonce[index] = data[index];
        index += 1U;
    }
    payload = ft_nullptr;
    payload_size = 0U;
    if (size > sizeof(key) + sizeof(nonce))
    {
        payload = data + sizeof(key) + sizeof(nonce);
        payload_size = size - sizeof(key) - sizeof(nonce);
    }
    associated_data_size = size;
    if (associated_data_size > 32U)
        associated_data_size = 32U;
    (void)crypto_chacha20_poly1305_seal(key, nonce, data,
        associated_data_size, payload, payload_size, ciphertext, tag);
    if (ciphertext.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        const uint8_t *ciphertext_data = ft_nullptr;

        if (ciphertext.size() > 0U)
            ciphertext_data = &ciphertext[0];
        (void)crypto_chacha20_poly1305_open(key, nonce, data,
            associated_data_size, ciphertext_data, ciphertext.size(),
            tag, plaintext);
    }
    (void)ciphertext.destroy();
    (void)plaintext.destroy();
    return (0);
}
