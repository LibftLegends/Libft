#include "../../../Modules/File/file_utils.hpp"
#include <chrono>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 25000U;
    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_string *relative = file_path_relative("C:/Game/world/overworld/region/0/deep",
            "c:/game/world/overworld/region/1/chunk.bin");
        if (relative == ft_nullptr)
            return (1);
        const ft_bool matches = *relative == "../../1/chunk.bin";
        delete relative;
        if (matches == FT_FALSE)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("file_path_relative us/op=%.3f\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
