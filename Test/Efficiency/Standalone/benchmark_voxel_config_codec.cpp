#include "../../../Modules/Voxel/voxel_api.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t configure_features(voxel_generation_config &config)
{
    if (config.set_feature_count(VOXEL_MAX_FEATURE_RULES) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < VOXEL_MAX_FEATURE_RULES)
    {
        voxel_feature_rule &feature = config.features[index];
        if (feature.set_biome_range(-1, -8, 160) != FT_ERR_SUCCESS
            || feature.set_chance(index + 1U) != FT_ERR_SUCCESS)
            return (1);
        if ((index & 1U) != 0U
            && feature.set_template(&voxel_small_cactus_tree_template()) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    return (0);
}

static ft_bool features_match(const voxel_generation_config &source,
    const voxel_generation_config &destination)
{
    if (source.feature_count != destination.feature_count)
        return (FT_FALSE);
    uint32_t index = 0U;
    while (index < source.feature_count)
    {
        const voxel_feature_rule &expected = source.features[index];
        const voxel_feature_rule &actual = destination.features[index];
        if (expected.biome_index != actual.biome_index || expected.minimum_height != actual.minimum_height
            || expected.maximum_height != actual.maximum_height || expected.chance_percent != actual.chance_percent
            || expected.requires_dry_land != actual.requires_dry_land)
            return (FT_FALSE);
        if (expected.template_data == ft_nullptr)
        {
            if (actual.template_data != ft_nullptr)
                return (FT_FALSE);
        }
        else if (actual.template_data == ft_nullptr
            || actual.template_data->block_count != expected.template_data->block_count
            || actual.template_data->blocks[0].block_id != expected.template_data->blocks[0].block_id)
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

static int32_t configure_template_indices(voxel_generation_config &config)
{
    while (config.tree_template_count < VOXEL_MAX_BIOME_TREE_TEMPLATES)
    {
        uint32_t index;
        if (voxel_generation_config_add_tree_template(config, voxel_small_cactus_tree_template(), &index)
            != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t biome_index = 0U;
    while (biome_index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        voxel_biome_definition &biome = config.biomes[biome_index];
        biome.tree_template_count = VOXEL_MAX_BIOME_TREE_TEMPLATES;
        uint32_t index = 0U;
        while (index < VOXEL_MAX_BIOME_TREE_TEMPLATES)
        {
            biome.tree_template_indices[index] = VOXEL_MAX_BIOME_TREE_TEMPLATES - index - 1U;
            ++index;
        }
        ++biome_index;
    }
    return (0);
}

static int32_t benchmark_invalid_version(ft_byte_buffer &buffer,
    voxel_generation_config &destination)
{
    constexpr uint32_t ITERATIONS = 32768U;
    if (buffer.append_u32_le(UINT32_C(0x54434F4E)) != FT_ERR_SUCCESS
        || buffer.append_u32_le(UINT32_MAX) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generation_config_deserialize(destination, buffer) != FT_ERR_INVALID_ARGUMENT
            || buffer.read_position() != 2U * sizeof(uint32_t))
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (destination.is_initialised() != FT_FALSE)
        return (1);
    std::printf("invalid version %.3f us/config\n", elapsed / ITERATIONS);
    return (0);
}

static int32_t benchmark_default_config_lifecycle(voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 16384U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_default_generation_config(config) != FT_ERR_SUCCESS
            || config.destroy() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (voxel_default_generation_config(config) != FT_ERR_SUCCESS)
        return (1);
    std::printf("default lifecycle %.3f us/config signature=%" PRIu32 "\n",
        elapsed / ITERATIONS, voxel_generation_config_signature(config));
    return (0);
}

static int32_t benchmark_generation_context(const voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 2048U;
    voxel_generation_context context;
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (context.initialize(config) != FT_ERR_SUCCESS)
            return (1);
        checksum = checksum * 31U + context.configuration_signature();
        checksum = checksum * 31U + context.region_configuration_signature();
        if (context.destroy() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("generation context %.3f us/config checksum=%" PRIu64 "\n",
        elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t benchmark_template_removal(voxel_generation_config &config)
{
    constexpr uint32_t ITERATIONS = 8192U;
    const voxel_tree_template &tree_template = voxel_small_cactus_tree_template();
    uint32_t slot;
    while (config.tree_template_count < VOXEL_MAX_TREE_TEMPLATES)
    {
        if (voxel_generation_config_add_tree_template(config, tree_template, &slot)
            != FT_ERR_SUCCESS)
            return (1);
    }
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generation_config_remove_tree_template(config, 0U) != FT_ERR_SUCCESS
            || voxel_generation_config_add_tree_template(config, tree_template, &slot)
                != FT_ERR_SUCCESS
            || slot != VOXEL_MAX_TREE_TEMPLATES - 1U)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("remove first template %.3f us/pair signature=%" PRIu32 "\n",
        elapsed / ITERATIONS, voxel_generation_config_signature(config));
    return (0);
}

int main(int argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 1024U;
    voxel_generation_config source;
    voxel_generation_config destination;
    ft_byte_buffer buffer;
    if (voxel_default_generation_config(source) != FT_ERR_SUCCESS
        || source.set_biome_size_range_for_biome(0U, 32, 128) != FT_ERR_SUCCESS
        || buffer.initialize() != FT_ERR_SUCCESS)
        return (1);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--invalid-version") == 0)
        return (benchmark_invalid_version(buffer, destination));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--default-lifecycle") == 0)
        return (benchmark_default_config_lifecycle(destination));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--generation-context") == 0)
        return (benchmark_generation_context(source));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--template-removal") == 0)
        return (benchmark_template_removal(source));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--features") == 0
        && configure_features(source) != 0)
        return (1);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--tree-indices") == 0
        && configure_template_indices(source) != 0)
        return (1);
    uint32_t index = 0U;
    const auto encode_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generation_config_serialize(source, buffer) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double encode_time = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - encode_start).count();

    index = 0U;
    const auto decode_start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (voxel_generation_config_deserialize(destination, buffer) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double decode_time = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - decode_start).count();
    if (source.feature_count != 0U && features_match(source, destination) == FT_FALSE)
        return (1);
    if (source.feature_count == 0U
        && voxel_generation_config_signature(destination) != voxel_generation_config_signature(source))
        return (1);
    const ft_size_t size = buffer.size();
    const uint8_t *bytes = buffer.data();
    uint64_t checksum = 0U;
    ft_size_t offset = 0U;
    while (offset < size)
        checksum = checksum * 31U + bytes[offset++];
    std::printf("encode %.2f us/config decode %.2f us/config bytes=%" PRIu64 " checksum=%" PRIu64 "\n",
        encode_time / ITERATIONS, decode_time / ITERATIONS, static_cast<uint64_t>(size), checksum);
    return (0);
}
