#include "../../../Modules/GetNextLine/get_next_line.hpp"
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

static int32_t benchmark_reads(const char *path, const char *label,
    ft_size_t buffer_size)
{
    constexpr uint32_t ITERATIONS = 5U;
    uint32_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        int32_t flags = O_RDONLY;
#ifdef _WIN32
        flags |= O_BINARY;
#endif
        const int32_t descriptor = open(path, flags);
        if (descriptor < 0)
            return (1);
        char *line = get_next_line(descriptor, buffer_size);
        uint32_t line_count = 0U;
        while (line != ft_nullptr)
        {
            const ft_size_t length = ft_strlen_size_t(line);
            checksum += static_cast<uint32_t>(length + line[0]);
            cma_free(line);
            line_count += 1U;
            line = get_next_line(descriptor, buffer_size);
        }
        int32_t clear_error = gnl_clear_stream(descriptor);
        int32_t close_error = close(descriptor);
        if (line_count == 0U || clear_error != FT_ERR_SUCCESS || close_error != 0)
            return (1);
        iteration += 1U;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("%s: %.2f ns/op, checksum %u\n",
        label, nanoseconds / ITERATIONS, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2)
    {
        std::fprintf(stderr, "Usage: benchmark_get_next_line INPUT_FILE\n");
        return (1);
    }
    if (benchmark_reads(arguments[1], "read file with 64-byte chunks", 64U)
        || benchmark_reads(arguments[1], "read file with 4096-byte chunks", 4096U))
        return (1);
    return (0);
}
