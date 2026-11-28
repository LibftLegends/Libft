#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Voxel/voxel_generation.hpp"
#include <chrono>
#include <cstdio>

using coordinate_clock = std::chrono::steady_clock;

static int32_t measure_coordinate(const char *label, const char *chunk_x, const char *chunk_z)
{
    constexpr uint32_t HASH_COUNT = 1000000U;
    constexpr uint32_t SET_COUNT = 100000U;
    voxel_world_chunk_coordinate coordinate;
    if (coordinate.initialize() != FT_ERR_SUCCESS
        || coordinate.set_chunk_coordinates(chunk_x, chunk_z) != FT_ERR_SUCCESS)
        return (1);

    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto hash_started = coordinate_clock::now();
    while (index < HASH_COUNT)
    {
        checksum += coordinate.hash();
        index += 1U;
    }
    const double hash_time = std::chrono::duration<double, std::nano>(
        coordinate_clock::now() - hash_started).count() / HASH_COUNT;
    const auto set_started = coordinate_clock::now();
    index = 0U;
    while (index < SET_COUNT)
    {
        if (coordinate.set_chunk_coordinates(chunk_x, chunk_z) != FT_ERR_SUCCESS)
            return (1);
        checksum += coordinate.hash();
        index += 1U;
    }
    const double set_time = std::chrono::duration<double, std::nano>(
        coordinate_clock::now() - set_started).count() / SET_COUNT;
    std::printf("%s: %.2f ns/hash, %.2f ns/set and hash, checksum %" FT_PRIu64 "\n",
        label, hash_time, set_time, checksum);
    return (0);
}

int32_t main()
{
    if (measure_coordinate("short coordinates", "123", "-456") != 0)
        return (1);
    return (measure_coordinate("wide coordinates",
        "9223372036854775808123456789092233720368547758081234567890"
        "9223372036854775808123456789092233720368547758081234567890",
        "-184467440737095516161234567890184467440737095516161234567890"
        "184467440737095516161234567890184467440737095516161234567890"));
}
