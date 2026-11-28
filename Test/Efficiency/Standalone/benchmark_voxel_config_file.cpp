#include "../../../Modules/Filesystem/filesystem.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Voxel/voxel_api.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <memory>
#include <new>

#ifdef GAME_USE_VOXEL_REGION_BACKEND
int main()
{
    constexpr uint32_t ITERATIONS = 256U;
    std::unique_ptr<voxel_generation_config> config(new (std::nothrow) voxel_generation_config());
    std::unique_ptr<voxel_generation_config> loaded(new (std::nothrow) voxel_generation_config());
    ft_string path;
    ft_string contents;
    if (!config || !loaded || path.initialize() != FT_ERR_SUCCESS
        || contents.initialize() != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_voxel_config_load", "bin", &path) != FT_ERR_SUCCESS
        || voxel_default_generation_config(*config) != FT_ERR_SUCCESS)
        return (1);
    int32_t result = FT_ERR_SUCCESS;
    uint32_t iteration = 0U;
    const auto save_started = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = voxel_generation_config_save_file(path.c_str(), *config);
        ++iteration;
    }
    const auto save_elapsed = std::chrono::steady_clock::now() - save_started;
    const uint32_t signature = voxel_generation_config_signature(*config);
    uint64_t checksum = 0U;
    iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && result == FT_ERR_SUCCESS)
    {
        result = voxel_generation_config_load_file(path.c_str(), *loaded);
        if (result != FT_ERR_SUCCESS)
            break ;
        if (loaded->sea_level != config->sea_level)
            result = FT_ERR_IO;
        checksum += static_cast<uint32_t>(loaded->sea_level);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (result == FT_ERR_SUCCESS)
        result = file_read_all(path.c_str(), contents);
    const int32_t cleanup_status = file_delete(path.c_str());
    if (result != FT_ERR_SUCCESS || cleanup_status != FT_ERR_SUCCESS
        || voxel_generation_config_signature(*loaded) != signature)
        return (1);
    const ft_size_t size = contents.size();
    const char *bytes = contents.c_str();
    uint64_t wire_checksum = 0U;
    ft_size_t offset = 0U;
    while (offset < size)
        wire_checksum = wire_checksum * 31U + static_cast<uint8_t>(bytes[offset++]);
    std::printf("voxel config file save %.2f us/op load %.2f us/op, bytes %" PRIu64
        ", wire checksum %" PRIu64 ", load checksum %" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(save_elapsed).count() / ITERATIONS,
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS,
        static_cast<uint64_t>(size), wire_checksum, checksum);
    return (0);
}
#endif
