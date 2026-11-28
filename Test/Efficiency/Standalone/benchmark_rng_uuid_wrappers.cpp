#include "../../../Modules/RNG/rng.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>

static ft_size_t g_bytes_per_request = 0U;
static uint32_t g_request_count = 0U;

// Link only the UUID and wrapper sources to measure formatting without OS calls.
int32_t rng_secure_bytes(unsigned char *buffer, ft_size_t length)
{
    g_bytes_per_request = length;
    g_request_count += 1U;
    ft_size_t index = 0U;
    while (index < length)
    {
        buffer[index] = static_cast<unsigned char>(g_request_count + 37U * index);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t main()
{
    constexpr uint32_t ITERATIONS = 700000U;
    char known_uuid[37];
    ft_generate_uuid(known_uuid);
    if (std::strcmp(known_uuid, "01264b70-95ba-4f04-a94e-7398bde2072c") != 0)
        return (1);
    g_request_count = 0U;
    uint32_t known_integer = 0U;
    if (rng_secure_uint32(&known_integer) != FT_ERR_SUCCESS || known_integer != 0x704B2601U)
        return (1);
    g_request_count = 0U;
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto uuid_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        char uuid[37];
        ft_generate_uuid(uuid);
        checksum += static_cast<unsigned char>(uuid[index % 36U]);
        index += 1U;
    }
    const double uuid_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - uuid_start).count();
    std::printf("UUID: %.2f ns, checksum %" FT_PRIu64 ", bytes %" FT_PRIu64 "\n",
        uuid_ns / ITERATIONS, checksum, static_cast<uint64_t>(g_bytes_per_request));

    index = 0U;
    g_request_count = 0U;
    checksum = 0U;
    const auto integer_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        uint32_t value = 0U;
        if (rng_secure_uint32(&value) != FT_ERR_SUCCESS)
            return (1);
        checksum += value;
        index += 1U;
    }
    const double integer_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - integer_start).count();
    std::printf("uint32: %.2f ns, checksum %" FT_PRIu64 ", bytes %" FT_PRIu64 "\n",
        integer_ns / ITERATIONS, checksum, static_cast<uint64_t>(g_bytes_per_request));
    return (0);
}
