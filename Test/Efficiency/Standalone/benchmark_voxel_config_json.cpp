#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include "../../../Modules/Filesystem/filesystem.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

template <typename Config>
static int32_t measure_json(const char *label, const Config &config)
{
    constexpr uint32_t ITERATIONS = 4096U;
    ft_string output;
    if (output.initialize() != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (config.serialize_json(output) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    const ft_size_t size = output.size();
    const char *bytes = output.c_str();
    uint64_t checksum = 0U;
    ft_size_t offset = 0U;
    while (offset < size)
        checksum = checksum * 31U + static_cast<uint8_t>(bytes[offset++]);
    std::printf("%s %.2f us/document bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        label, elapsed / ITERATIONS, static_cast<uint64_t>(size), checksum);
    return (0);
}

static int32_t measure_file_replace(const voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 256U;
    ft_string path;
    ft_string expected;
    ft_string loaded;
    if (path.initialize() != FT_ERR_SUCCESS || loaded.initialize() != FT_ERR_SUCCESS
        || config.serialize_json(expected) != FT_ERR_SUCCESS
        || filesystem_temp_path("libft_voxel_json_replace", "json", &path) != FT_ERR_SUCCESS)
        return (1);
    int32_t status = config.save_json_file(path.c_str(), VOXEL_JSON_FILE_CREATE_ONLY);
    if (status != FT_ERR_SUCCESS)
        return (1);

    const auto start = std::chrono::steady_clock::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS && status == FT_ERR_SUCCESS)
    {
        status = config.save_json_file(path.c_str(), VOXEL_JSON_FILE_REPLACE);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (status == FT_ERR_SUCCESS)
        status = file_read_all(path.c_str(), loaded);
    const int32_t cleanup_status = file_delete(path.c_str());
    if (status != FT_ERR_SUCCESS || cleanup_status != FT_ERR_SUCCESS
        || loaded.size() != expected.size()
        || ft_memcmp(loaded.c_str(), expected.c_str(), expected.size()) != 0)
        return (1);
    std::printf("file replace %.2f us/document bytes=%" PRIu64 "\n",
        elapsed / ITERATIONS, static_cast<uint64_t>(loaded.size()));
    return (0);
}

int main()
{
    voxel_generation_config config;
    if (voxel_default_generation_config(config) != FT_ERR_SUCCESS)
        return (1);
    config.biomes[0].profile.surface_height = INT32_MIN;
    config.biomes[0].surface_block_id = UINT32_MAX;
    config.biome_size_min_by_biome[0] = INT32_MIN;
    config.biome_size_override_enabled[0] = FT_TRUE;
    if (measure_json("biome", config.biomes[0]) != 0 || measure_json("generation", config) != 0
        || measure_file_replace(config) != 0)
        return (1);
    return (0);
}
