#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_parent_paths()
{
    constexpr uint32_t ITERATIONS = 40000U;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_string *parent = file_path_dirname_string("world/season/./overworld/region/r.0.0.ftvr");
        if (parent == ft_nullptr)
            return (1);
        const ft_bool matches = *parent == "world/season/overworld/region";
        delete parent;
        if (matches == FT_FALSE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("file_path_dirname_string us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_atomic_writes(const char *directory)
{
    constexpr uint32_t ITERATIONS = 300U;
    constexpr char PAYLOAD[] = "small save payload";
    ft_string *path = file_path_join(directory, "region-save.bin");
    if (path == ft_nullptr)
        return (1);
    uint32_t iteration = 0U;
    int32_t result = FT_ERR_SUCCESS;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = filesystem_atomic_write(path->c_str(), PAYLOAD, sizeof(PAYLOAD) - 1U);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;

    ft_string contents;
    int32_t check = contents.initialize();
    if (check == FT_ERR_SUCCESS)
        check = file_read_all(path->c_str(), contents);
    delete path;
    if (result != FT_ERR_SUCCESS || check != FT_ERR_SUCCESS || (contents == PAYLOAD) == FT_FALSE)
        return (1);
    std::printf("filesystem_atomic_write us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_parent_paths() != 0)
        return (1);
    ft_string directory;
    if (directory.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_benchmark_replacement", "dir", &directory) != FT_ERR_SUCCESS
        || file_create_directories(directory.c_str(), 0700) != FT_ERR_SUCCESS)
        return (1);
    const int32_t result = measure_atomic_writes(directory.c_str());
    const int32_t cleanup = file_delete_recursive(directory.c_str());
    if (result != 0 || cleanup != FT_ERR_SUCCESS)
        return (1);
    return (0);
}
