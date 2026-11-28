#include "../../../Modules/Voxel/voxel_api.hpp"
#include <cinttypes>
#include <cstdio>
#include <new>

#ifdef GAME_USE_VOXEL_REGION_BACKEND
int32_t main()
{
    voxel_generation_config *config = new (std::nothrow) voxel_generation_config();
    if (config == ft_nullptr)
        return (1);
    voxel_tree_template_block blocks[] = {
        {-2, 3, -4, VOXEL_GENERATOR_OAK_LOG_BLOCK},
        {5, 6, 7, VOXEL_GENERATOR_OAK_LEAVES_BLOCK}};
    const voxel_tree_template tree{blocks, 2U};
    ft_byte_buffer bytes;
    if (voxel_default_generation_config(*config) != FT_ERR_SUCCESS
        || config->set_biome_tree_template_override(0U, &tree) != FT_ERR_SUCCESS
        || bytes.initialize() != FT_ERR_SUCCESS
        || voxel_generation_config_serialize(*config, bytes) != FT_ERR_SUCCESS)
    {
        delete config;
        return (1);
    }
    delete config;
    const uint8_t *data = bytes.data();
    const ft_size_t length = bytes.size();
    uint64_t checksum = 14695981039346656037ULL;
    ft_size_t index = 0U;
    while (index < length)
    {
        checksum ^= data[index];
        checksum *= 1099511628211ULL;
        ++index;
    }
    std::printf("voxel config %" PRIu64 " bytes, checksum %" PRIu64 "\n", length, checksum);
    return (0);
}
#endif
