#include "../../../Modules/Voxel/voxel_internal.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint64_t SEED = UINT64_C(0x2a92198858);
    constexpr int32_t SCALES[] = {8, 17, 32, 96};
    constexpr uint32_t ITERATIONS = 2097152U;
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        const int32_t world_x = static_cast<int32_t>(index % 2048U) - 1024;
        const int32_t world_z = static_cast<int32_t>(index / 2048U) - 512;
        const int32_t scale = SCALES[index % 4U];
        const double sample = voxel_value_noise(SEED, world_x, world_z, scale);
        uint64_t bits;
        static_assert(sizeof(bits) == sizeof(sample));
        ft_memcpy_fixed<sizeof(bits)>(&bits, &sample);
        checksum = checksum * 31U + bits;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("value noise %.3f ns/sample checksum=%" PRIu64 "\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}
