#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>
#include <vector>

static int32_t measure_file_reads()
{
    constexpr ft_size_t PAYLOAD_SIZE = 8U * 1024U * 1024U;
    constexpr uint32_t ITERATIONS = 64U;
    ft_string path;
    ft_string contents;
    if (path.initialize() != FT_ERR_SUCCESS || contents.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_benchmark_read_all", "bin", &path) != FT_ERR_SUCCESS)
        return (1);
    const std::vector<char> payload(PAYLOAD_SIZE, 'a');
    int32_t result = file_write_all(path.c_str(), payload.data(), payload.size());
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = file_read_all(path.c_str(), contents);
        if (result == FT_ERR_SUCCESS && (contents.size() != payload.size() || contents.back() != 'a'))
            result = FT_ERR_IO;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (file_delete(path.c_str()) != FT_ERR_SUCCESS || result != FT_ERR_SUCCESS)
        return (1);
    std::printf("file_read_all bytes=%zu us/op=%.3f\n", PAYLOAD_SIZE,
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_file_joins()
{
    constexpr uint32_t ITERATIONS = 50000U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_string *path = file_path_join("world\\dimension/./overworld",
            "region/chunks/../r.0.0.ftvr");
        if (path == ft_nullptr)
            return (1);
        const ft_bool matches = *path == "world/dimension/overworld/region/r.0.0.ftvr";
        delete path;
        if (matches == FT_FALSE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("file_path_join us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_file_joins() != 0)
        return (1);
    return (measure_file_reads());
}
