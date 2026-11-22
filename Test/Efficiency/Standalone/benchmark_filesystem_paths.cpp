#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cstdio>

static int32_t measure_glob(const char *label, const char *pattern, const char *path,
    ft_bool expected, uint32_t iterations)
{
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (filesystem_match_glob(pattern, path) != expected)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("filesystem_match_glob %s us/op=%.3f\n", label,
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations);
    return (0);
}

static int32_t measure_split_path()
{
    constexpr uint32_t ITERATIONS = 25000U;
    ft_string directory;
    ft_string basename;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (filesystem_split_path("world/overworld/region/r.0.0.ftvr", &directory, &basename)
                != FT_ERR_SUCCESS
            || (directory == "world/overworld/region") == FT_FALSE
            || (basename == "r.0.0.ftvr") == FT_FALSE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("filesystem_split_path us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

int32_t main()
{
    if (measure_glob("single_star", "world/region/*.ftvr", "world/region/r.10.20.ftvr",
            FT_TRUE, 100000) != 0)
        return (1);
    if (measure_glob("multiple_star_near_miss", "*a*a*a*a*a*a*a*a*Z", "aaaaaaaaaaaaaaaaaaY",
            FT_FALSE, 25) != 0)
        return (1);
    return (measure_split_path());
}
