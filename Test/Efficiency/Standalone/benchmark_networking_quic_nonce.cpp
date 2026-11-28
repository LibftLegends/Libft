#include "../../../Modules/Networking/networking_quic_experimental.hpp"
#include <chrono>
#include <cstdio>

#if !NETWORKING_HAS_OPENSSL || !defined(LIBFT_TEST_BUILD)
# error This benchmark requires OpenSSL and LIBFT_TEST_BUILD.
#endif

int main()
{
    constexpr uint32_t ITERATIONS = 250000U;
    constexpr ft_size_t AES_GCM_NONCE_LENGTH = 12U;
    quic_experimental_session session;
    ft_vector<unsigned char> base_iv;
    unsigned char nonce[EVP_MAX_IV_LENGTH];

    if (session.initialize() != FT_ERR_SUCCESS
        || base_iv.initialize() != FT_ERR_SUCCESS)
        return (1);
    base_iv.resize(AES_GCM_NONCE_LENGTH, 0xA5U);
    if (base_iv.get_error() != FT_ERR_SUCCESS)
        return (1);
    uint32_t checksum = 0U;
    uint64_t sequence_number = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (sequence_number < ITERATIONS)
    {
        if (session.prepare_nonce(sequence_number, base_iv, nonce)
            != AES_GCM_NONCE_LENGTH)
            return (1);
        checksum += nonce[AES_GCM_NONCE_LENGTH - 1U];
        sequence_number += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%.2f ns per QUIC nonce, checksum %u\n",
        elapsed_ns / ITERATIONS, checksum);
    return (0);
}
