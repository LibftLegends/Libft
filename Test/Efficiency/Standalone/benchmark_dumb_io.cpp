#include "../../../Modules/DUMB/dumb_console.hpp"
#include "../../../Modules/DUMB/dumb_io.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t read_lines(uint64_t &checksum)
{
    char line[256];
    if (ft_read_line(line, sizeof(line)) != FT_ERR_SUCCESS)
        return (1);
    checksum = checksum * 31U + ft_strlen_size_t(line);
    return (0);
}

static int32_t read_file(const char *path, uint64_t &checksum)
{
    char *buffer = ft_nullptr;
    ft_size_t size = 0;
    if (ft_read_file(path, &buffer, &size) != FT_ERR_SUCCESS)
        return (1);
    checksum = checksum * 31U + size;
    cma_free(buffer);
    return (0);
}

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 3)
        return (2);
    const ft_bool lines = ft_strcmp(arguments[1], "--lines") == 0;
    if (lines && std::freopen(arguments[2], "rb", stdin) == ft_nullptr)
        return (2);
    uint32_t iterations = 1000;
    if (lines)
        iterations = 5000;

    ft_size_t before;
    ft_size_t after;
    ft_size_t frees;
    if (cma_get_stats(&before, &frees) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0;
    uint64_t checksum = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (lines)
        {
            if (read_lines(checksum) != 0)
                return (1);
        }
        else if (read_file(arguments[2], checksum) != 0)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (cma_get_stats(&after, &frees) != FT_ERR_SUCCESS)
        return (1);
    std::printf("dumb io %.3f us/op allocations=%" PRIu64 " checksum=%" PRIu64 "\n",
        elapsed / iterations, ft_platform_cast<uint64_t>(after - before), checksum);
    return (0);
}
