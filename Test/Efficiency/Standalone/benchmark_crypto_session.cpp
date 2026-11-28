#include "../../../Modules/Crypto/crypto_primitives_internal.hpp"
#include "../../../Modules/Crypto/crypto_session.hpp"
#include "../../../Modules/Crypto/crypto_x25519.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_session()
{
    constexpr uint32_t ITERATIONS = 2000U;
    const uint8_t private_key[32] = {0x23U, 0x54U};
    const uint8_t peer_private_key[32] = {0x91U, 0x76U};
    const uint8_t transcript[176] = {0x81U};
    uint8_t peer_public_key[32];
    uint8_t send_key[32];
    uint8_t receive_key[32];
    uint8_t send_iv[12];
    uint8_t receive_iv[12];
    if (crypto_x25519_public_key(peer_private_key, peer_public_key) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (crypto_derive_session_keys(private_key, peer_public_key, transcript,
                sizeof(transcript), crypto_session_role::CLIENT,
                send_key, receive_key, send_iv, receive_iv) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("Session derivation: %.3f us per call, check byte %u\n",
        elapsed_us / ITERATIONS, static_cast<uint32_t>(send_key[0]));
    return (0);
}

static int32_t measure_labels(ft_bool batched)
{
    constexpr uint32_t ITERATIONS = 75000U;
    const uint8_t pseudorandom_key[32] = {0x18U};
    const uint8_t info[4][8] = {{1U}, {2U}, {3U}, {4U}};
    uint8_t output[4][32];
    const crypto_hkdf_sha256_label labels[] = {
        {info[0], sizeof(info[0]), output[0], 32U},
        {info[1], sizeof(info[1]), output[1], 32U},
        {info[2], sizeof(info[2]), output[2], 12U},
        {info[3], sizeof(info[3]), output[3], 12U}
    };
    constexpr ft_size_t LABEL_COUNT = sizeof(labels) / sizeof(labels[0]);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (batched != FT_FALSE)
        {
            if (crypto_hkdf_sha256_expand_labels(pseudorandom_key, labels,
                    LABEL_COUNT) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            ft_size_t index = 0U;
            while (index < LABEL_COUNT)
            {
                const crypto_hkdf_sha256_label &label = labels[index];
                if (crypto_hkdf_sha256_expand(pseudorandom_key, label.info,
                        label.info_length, label.output, label.output_length) != FT_ERR_SUCCESS)
                    return (1);
                index += 1U;
            }
        }
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const char *operation = "Separate HKDF expansions";
    if (batched != FT_FALSE)
        operation = "Batched HKDF expansions";
    std::printf("%s: %.3f us per set, check byte %u\n", operation,
        elapsed_us / ITERATIONS, static_cast<uint32_t>(output[3][11]));
    return (0);
}

int32_t main()
{
    if (measure_session() != 0 || measure_labels(FT_FALSE) != 0
        || measure_labels(FT_TRUE) != 0)
        return (1);
    return (0);
}
