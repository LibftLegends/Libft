#include "../../../Modules/Networking/networking_websocket_handshake_internal.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 20000U;
    constexpr char EXPECTED[] = "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=";
    char key[] = "dGhlIHNhbXBsZSBub25jZQ==";
    char accept[WEBSOCKET_ACCEPT_KEY_LENGTH + 1U];
    if (!websocket_compute_accept_key(key, sizeof(key) - 1U, accept)
        || ft_memcmp(accept, EXPECTED, sizeof(EXPECTED)) != 0)
        return (1);
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t frees = 0U;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    uint32_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        key[0] = static_cast<char>('A' + iteration % 26U);
        if (!websocket_compute_accept_key(key, sizeof(key) - 1U, accept))
            return (1);
        checksum += static_cast<uint8_t>(accept[0]);
        ++iteration;
    }
    const double elapsed_us = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("%.3f us per accept key, %" FT_PRIu64 " allocations (%u keys, checksum %u)\n",
        elapsed_us / ITERATIONS, after - before, ITERATIONS, checksum);
    return (0);
}
