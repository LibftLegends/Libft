#include "../../../Modules/Game/game_voxel_region.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_region(uint32_t chunks)
{
    constexpr uint32_t ITERATIONS = 64U;
    ft_string directory;
    if (directory.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_voxel_region_io", ft_nullptr, &directory) != FT_ERR_SUCCESS
        || file_create_directory(directory.c_str(), 0700) != FT_ERR_SUCCESS)
        return (1);
    game_voxel_region source;
    game_voxel_region loaded;
    int32_t status = source.initialize(0, 0, directory.c_str());
    if (status == FT_ERR_SUCCESS)
        status = loaded.initialize(0, 0, directory.c_str());
    uint32_t index = 0U;
    while (index < chunks && status == FT_ERR_SUCCESS)
    {
        status = source.write_block(static_cast<int32_t>(index) * GAME_VOXEL_CHUNK_WIDTH,
            32, 0, index + 1U);
        ++index;
    }
    double save_time = 0.0;
    double load_time = 0.0;
    uint64_t checksum = 0U;
    ft_size_t save_allocations = 0U;
    index = 0U;
    while (index < ITERATIONS && status == FT_ERR_SUCCESS)
    {
        ft_size_t allocation_before;
        ft_size_t allocation_after;
        ft_size_t frees;
        ft_size_t current;
        ft_size_t peak;
        status = cma_get_extended_stats(&allocation_before, &frees, &current, &peak);
        if (status != FT_ERR_SUCCESS)
            break ;
        const auto start = std::chrono::steady_clock::now();
        status = source.save_region();
        const auto saved = std::chrono::steady_clock::now();
        if (status != FT_ERR_SUCCESS)
            break ;
        status = cma_get_extended_stats(&allocation_after, &frees, &current, &peak);
        if (status != FT_ERR_SUCCESS)
            break ;
        save_allocations += allocation_after - allocation_before;
        const auto load_start = std::chrono::steady_clock::now();
        status = loaded.load_region(0, 0);
        const auto end = std::chrono::steady_clock::now();
        if (status != FT_ERR_SUCCESS)
            break ;
        save_time += std::chrono::duration<double, std::micro>(saved - start).count();
        load_time += std::chrono::duration<double, std::micro>(end - load_start).count();
        uint32_t slot = 0U;
        while (slot < chunks && status == FT_ERR_SUCCESS)
        {
            uint32_t value = 0U;
            status = loaded.read_block(static_cast<int32_t>(slot) * GAME_VOXEL_CHUNK_WIDTH,
                32, 0, &value);
            if (status == FT_ERR_SUCCESS && value != slot + 1U)
                status = FT_ERR_IO;
            checksum += value;
            ++slot;
        }
        ++index;
    }
    if (file_delete_recursive(directory.c_str()) != FT_ERR_SUCCESS || status != FT_ERR_SUCCESS)
        return (1);
    std::printf("region %u chunks save %.2f us/op load %.2f us/op, "
        "save allocations %.2f/op, checksum %" PRIu64 "\n",
        chunks, save_time / ITERATIONS, load_time / ITERATIONS,
        static_cast<double>(save_allocations) / ITERATIONS, checksum);
    return (0);
}

int32_t main()
{
    if (measure_region(0U) != 0)
        return (1);
    return (measure_region(16U));
}
