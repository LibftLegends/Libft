#include "../../../Modules/Voxel/voxel_lighting.hpp"
#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Basic/limits.hpp"
#include "../../../Modules/Errno/errno.hpp"
#include <chrono>
#include <cstdio>

static int32_t lookup_block(void *, int32_t x, int32_t y, int32_t z,
    uint32_t *block_id) noexcept
{
    *block_id = VOXEL_GENERATOR_AIR_BLOCK;
    if (y == 128 && (x != 0 || z != 8))
        *block_id = VOXEL_GENERATOR_STONE_BLOCK;
    else if (y == 124 && z == 8 && (x == -1 || x == 10))
        *block_id = VOXEL_GENERATOR_SHIMMER_STONE_BLOCK;
    else if (y == 123 && z == 8 && x == 4)
        *block_id = VOXEL_GENERATOR_OAK_LEAVES_BLOCK;
    return (FT_ERR_SUCCESS);
}

using light_builder = int32_t (*)(voxel_light_chunk &, int32_t, int32_t,
    voxel_light_block_lookup_fn, void *, voxel_light_build_stats *) noexcept;

template<ft_bool IncludeHalo>
static int32_t build_in_slices(voxel_light_chunk &light, int32_t origin_x,
    int32_t origin_z, voxel_light_block_lookup_fn lookup, void *user_data,
    voxel_light_build_stats *stats) noexcept
{
    constexpr voxel_light_update_config CONFIG = {128U, 128U, 128U, 1000U};
    voxel_light_build_operation operation;
    int32_t error = operation.initialize(light, origin_x, origin_z, lookup, user_data, IncludeHalo);
    ft_bool complete = FT_FALSE;
    while (error == FT_ERR_SUCCESS && complete == FT_FALSE)
        error = operation.step(CONFIG, stats, &complete);
    return (error);
}

static int32_t measure_build(const char *label, light_builder build, uint32_t iterations)
{
    voxel_light_chunk light;
    voxel_light_build_stats stats = {};
    if (build(light, 0, 0, lookup_block, nullptr, &stats) != FT_ERR_SUCCESS)
        return (1);
    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < iterations)
    {
        if (build(light, 0, 0, lookup_block, nullptr, &stats) != FT_ERR_SUCCESS)
            return (1);
        iteration += 1U;
    }
    const double elapsed_ns = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    while (index < 65536U)
    {
        const int32_t x = static_cast<int32_t>(index % 16U);
        const int32_t z = static_cast<int32_t>((index / 16U) % 16U);
        const int32_t y = static_cast<int32_t>(index / 256U);
        checksum = checksum * 31U + light.get(x, y, z);
        index += 1U;
    }
    std::printf("%s: %.2f ns, checksum %" FT_PRIu64 ", scanned %" FT_PRIu64
        ", propagated %" FT_PRIu64 ", queued %" FT_PRIu64 "\n",
        label, elapsed_ns / iterations, checksum, stats.scanned_cells,
        stats.propagated_cells, stats.queue_peak);
    return (0);
}

int main()
{
    if (measure_build("local", voxel_light_build_chunk_local, 28U)
        || measure_build("halo", voxel_light_build_chunk, 12U)
        || measure_build("sliced-local", build_in_slices<FT_FALSE>, 28U)
        || measure_build("sliced-halo", build_in_slices<FT_TRUE>, 12U))
        return (1);
    return (0);
}
