#include "../../../Modules/Printf/printf_internal.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

int32_t main()
{
    constexpr uint32_t ITERATIONS = 8192U;
#ifdef _WIN32
    const int32_t descriptor = open("NUL", O_WRONLY);
#else
    const int32_t descriptor = open("/dev/null", O_WRONLY);
#endif
    if (descriptor < 0)
        return (1);
    ft_size_t written_count = 0U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && written_count != SIZE_MAX)
    {
        const double value = static_cast<double>(iteration) / 32.0;
        ft_putfloat_fd(value, descriptor, &written_count, 4);
        ft_putscientific_fd(value, FT_TRUE, descriptor, &written_count, 6);
        ft_putgeneral_fd(value, FT_FALSE, descriptor, &written_count, 8);
        iteration += 1U;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (close(descriptor) != 0 || written_count == SIZE_MAX)
        return (1);
    std::printf("format three floats to descriptor %.2f ns/op, bytes %" PRIu64 "\n",
        elapsed / ITERATIONS, static_cast<uint64_t>(written_count));
    return (0);
}
