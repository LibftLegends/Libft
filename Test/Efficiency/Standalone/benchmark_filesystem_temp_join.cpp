#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_temp_paths()
{
    constexpr uint32_t ITERATIONS = 20000U;
    uint32_t iteration = 0;
    ft_string path;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (filesystem_temp_path("libft_perm_bench", ".tmp", &path) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("filesystem_temp_path us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_safe_joins()
{
    constexpr uint32_t ITERATIONS = 20000U;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_string *path = filesystem_safe_join_path("C:/world/season/overworld", "region/sub/../r.0.0.ftvr");
        if (path == ft_nullptr)
            return (1);
        delete path;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("filesystem_safe_join_path us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int main()
{
    if (measure_safe_joins() != 0)
        return (1);
    return (measure_temp_paths());
}
