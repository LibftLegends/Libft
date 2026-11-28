#include "../../../Modules/Crypto/crypto_primitives.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_hash(uint32_t length, uint32_t iterations)
{
    uint8_t input[65536] = {0x81U};
    uint8_t digest[32];
    uint32_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        input[0] = static_cast<uint8_t>(iteration);
        if (crypto_sha256_hash(input, length, digest) != FT_ERR_SUCCESS)
            return (1);
        checksum += digest[0];
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("SHA-256, %u bytes: %.3f us per hash, checksum %u\n",
        length, elapsed_us / iterations, checksum);
    return (0);
}

static int32_t measure_hkdf(uint32_t length, uint32_t iterations)
{
    const uint8_t pseudorandom_key[32] = {0x18U};
    const uint8_t info[16] = {0x24U};
    uint8_t output[255U * 32U];
    uint32_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (crypto_hkdf_sha256_expand(pseudorandom_key, info, sizeof(info),
                output, length) != FT_ERR_SUCCESS)
            return (1);
        checksum += output[length - 1U];
        iteration += 1U;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("HKDF, %u bytes: %.3f us per expansion, checksum %u\n",
        length, elapsed_us / iterations, checksum);
    return (0);
}

int32_t main()
{
    if (measure_hash(32U, 100000U) != 0 || measure_hash(65536U, 1000U) != 0
        || measure_hkdf(32U, 50000U) != 0 || measure_hkdf(44U, 50000U) != 0
        || measure_hkdf(8160U, 1000U) != 0)
        return (1);
    return (0);
}
