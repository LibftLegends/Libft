#include "../../../Modules/DUMB/sound_clip.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 64;
    if (argument_count != 2)
        return (2);
    ft_sound_clip clip;
    if (clip.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint64_t checksum = 0;
    uint32_t index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (clip.load_wav(arguments[1]) != FT_ERR_SUCCESS)
            return (1);
        const ft_size_t size = clip.get_size();
        if (size == 0)
            return (1);
        checksum = checksum * 31U + size + clip.get_data()[index % size];
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("wav %.3f us/load checksum=%" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
