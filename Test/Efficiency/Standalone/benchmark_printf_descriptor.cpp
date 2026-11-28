#include "../../../Modules/Printf/printf.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 4096U;
#ifdef _WIN32
    const int32_t descriptor = open("NUL", O_WRONLY);
#else
    const int32_t descriptor = open("/dev/null", O_WRONLY);
#endif
    if (descriptor < 0)
        return (1);
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        const int32_t length = pf_printf_fd(descriptor, "item=%u status=%s\n", iteration, "queued");
        if (length < 0)
        {
            (void)close(descriptor);
            return (1);
        }
        checksum += static_cast<uint32_t>(length);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (close(descriptor) != 0)
        return (1);
    std::printf("format to descriptor %.2f ns/op, checksum %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
