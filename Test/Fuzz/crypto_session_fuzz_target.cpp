#include "../../Modules/Crypto/crypto_session.hpp"
#include "../../Modules/Crypto/crypto_primitives.hpp"
#include "../../Modules/Crypto/crypto_x25519.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include <cstddef>
#include <cstdint>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    uint8_t private_key[32] = {0U};
    uint8_t peer_private_key[32] = {0U};
    uint8_t peer_public_key[32] = {0U};
    uint8_t send_key[32] = {0U};
    uint8_t receive_key[32] = {0U};
    uint8_t send_iv[12] = {0U};
    uint8_t receive_iv[12] = {0U};
    uint8_t updated_key[32] = {0U};
    uint8_t updated_iv[12] = {0U};
    const uint8_t *transcript;
    ft_size_t transcript_length;
    uint64_t epoch;
    uint32_t index;

    if (data == ft_nullptr && size != 0U)
        return (0);
    index = 0U;
    while (index < 32U)
    {
        if (size != 0U)
        {
            private_key[index] = data[index % size];
            peer_private_key[index] = data[(index * 7U + 3U) % size];
        }
        else
        {
            private_key[index] = static_cast<uint8_t>(index + 1U);
            peer_private_key[index] = static_cast<uint8_t>(index + 33U);
        }
        index += 1U;
    }
    transcript = data;
    transcript_length = size;
    if (transcript_length > 4096U)
        transcript_length = 4096U;
    if (size > 0U)
    {
        epoch = data[0];
        if (size > 1U)
            epoch = (epoch << 8U) | data[1];
    }
    else
        epoch = 0U;
    (void)crypto_x25519_public_key(peer_private_key, peer_public_key);
    (void)crypto_derive_session_keys(private_key, peer_public_key,
        transcript, transcript_length, crypto_session_role::CLIENT,
        send_key, receive_key, send_iv, receive_iv);
    (void)crypto_derive_key_update(send_key, epoch, updated_key, updated_iv);
    (void)crypto_secure_wipe(private_key, sizeof(private_key));
    (void)crypto_secure_wipe(peer_private_key, sizeof(peer_private_key));
    (void)crypto_secure_wipe(peer_public_key, sizeof(peer_public_key));
    (void)crypto_secure_wipe(send_key, sizeof(send_key));
    (void)crypto_secure_wipe(receive_key, sizeof(receive_key));
    (void)crypto_secure_wipe(send_iv, sizeof(send_iv));
    (void)crypto_secure_wipe(receive_iv, sizeof(receive_iv));
    (void)crypto_secure_wipe(updated_key, sizeof(updated_key));
    (void)crypto_secure_wipe(updated_iv, sizeof(updated_iv));
    return (0);
}
