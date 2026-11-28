#include <stdint.h>
#include <stdio.h>
#include "../Basic/basic.hpp"
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Buffer/buffer_endian.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "voxel_block_registry.hpp"
#include "voxel_internal.hpp"

static const uint32_t VOXEL_SAVE_MAGIC = UINT32_C(0x54434F4E);
static const uint32_t VOXEL_SAVE_VERSION = 12U;

static constexpr ft_size_t VOXEL_SAVE_BLOCK_NAME_MAX = 255U;

static int32_t voxel_save_append_block_name(ft_byte_buffer &buffer,
    const char *block_name) noexcept
{
    if (block_name == ft_nullptr || block_name[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t name_length = ft_strlen(block_name);
    if (name_length > VOXEL_SAVE_BLOCK_NAME_MAX)
        return (FT_ERR_INVALID_ARGUMENT);
    uint8_t bytes[sizeof(uint32_t) + VOXEL_SAVE_BLOCK_NAME_MAX];
    buffer_store_le(bytes, static_cast<uint32_t>(name_length));
    (void)ft_memcpy(bytes + sizeof(uint32_t), block_name, name_length);
    int32_t error_code = buffer.append(bytes, sizeof(uint32_t) + name_length);
    if (error_code != FT_ERR_FULL && error_code != FT_ERR_NO_MEMORY
        && error_code != FT_ERR_OUT_OF_RANGE)
        return (error_code);
    // A bounded destination may hold the length without holding the name.
    error_code = buffer.append(bytes, sizeof(uint32_t));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append(bytes + sizeof(uint32_t), name_length);
    return (error_code);
}

static int32_t voxel_save_append_block_reference(ft_byte_buffer &buffer,
    uint32_t block_id) noexcept
{
    if (block_id < VOXEL_BUILTIN_BLOCK_COUNT)
        return (voxel_save_append_block_name(buffer, voxel_get_block_name(block_id)));
    voxel_runtime_block_handle handle;
    if (voxel_acquire_block(block_id, handle) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_save_append_block_name(buffer, handle.get_name()));
}

static int32_t voxel_save_read_block_reference(ft_byte_buffer &buffer,
    uint32_t *block_id_out) noexcept
{
    char block_name[VOXEL_SAVE_BLOCK_NAME_MAX + 1U];
    uint32_t name_length;
    int32_t error_code;

    if (block_id_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&name_length);
    if (error_code != FT_ERR_SUCCESS || name_length == 0U
        || name_length >= sizeof(block_name))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read(block_name, static_cast<ft_size_t>(name_length));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    block_name[name_length] = '\0';
    return (voxel_find_block_id_by_name(block_name, block_id_out));
}

static constexpr ft_size_t VOXEL_SAVE_BIOME_LIMIT_BYTES = 2U * sizeof(uint32_t) + 1U;

static int32_t voxel_save_append_biome_limits(ft_byte_buffer &buffer,
    const voxel_generation_config &config) noexcept
{
    uint8_t bytes[VOXEL_MAX_CUSTOM_BIOMES * VOXEL_SAVE_BIOME_LIMIT_BYTES];
    uint32_t index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        uint8_t *record = bytes + index * VOXEL_SAVE_BIOME_LIMIT_BYTES;
        buffer_store_le(record, static_cast<uint32_t>(config.biome_size_min_by_biome[index]));
        buffer_store_le(record + sizeof(uint32_t),
            static_cast<uint32_t>(config.biome_size_max_by_biome[index]));
        record[2U * sizeof(uint32_t)] = config.biome_size_override_enabled[index];
        ++index;
    }
    int32_t status = buffer.append(bytes, sizeof(bytes));
    if (status != FT_ERR_FULL && status != FT_ERR_NO_MEMORY && status != FT_ERR_OUT_OF_RANGE)
        return (status);
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        const uint8_t *record = bytes + index * VOXEL_SAVE_BIOME_LIMIT_BYTES;
        status = buffer.append(record, sizeof(uint32_t));
        if (status == FT_ERR_SUCCESS)
            status = buffer.append(record + sizeof(uint32_t), sizeof(uint32_t));
        if (status == FT_ERR_SUCCESS)
            status = buffer.append(record + 2U * sizeof(uint32_t), 1U);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_save_read_biome_limits(ft_byte_buffer &buffer,
    voxel_generation_config &config) noexcept
{
    uint8_t bytes[VOXEL_MAX_CUSTOM_BIOMES * VOXEL_SAVE_BIOME_LIMIT_BYTES];
    const int32_t status = buffer.read(bytes, sizeof(bytes));
    if (status == FT_ERR_OUT_OF_RANGE)
    {
        // A short input used to consume only complete fields.
        const ft_size_t available = buffer.remaining();
        const ft_size_t complete_records = available / VOXEL_SAVE_BIOME_LIMIT_BYTES;
        const ft_size_t partial_record = available % VOXEL_SAVE_BIOME_LIMIT_BYTES;
        const ft_size_t complete_bytes = complete_records * VOXEL_SAVE_BIOME_LIMIT_BYTES
            + (partial_record / sizeof(uint32_t)) * sizeof(uint32_t);
        const int32_t skip_status = buffer.skip(complete_bytes);
        if (skip_status != FT_ERR_SUCCESS)
            return (skip_status);
    }
    if (status != FT_ERR_SUCCESS)
        return (status);
    uint32_t index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        const uint8_t *record = bytes + index * VOXEL_SAVE_BIOME_LIMIT_BYTES;
        config.biome_size_min_by_biome[index] = static_cast<int32_t>(buffer_load_le<uint32_t>(record));
        config.biome_size_max_by_biome[index] = static_cast<int32_t>(
            buffer_load_le<uint32_t>(record + sizeof(uint32_t)));
        config.biome_size_override_enabled[index] = record[2U * sizeof(uint32_t)];
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_save_append_template(ft_byte_buffer &buffer,
    const voxel_tree_template *tree_template) noexcept
{
    uint32_t index;
    int32_t error_code;

    if (tree_template == ft_nullptr
        || tree_template->block_count > VOXEL_MAX_TREE_TEMPLATE_BLOCKS
        || (tree_template->block_count != 0U
            && tree_template->blocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.append_u32_le(tree_template->block_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < tree_template->block_count)
    {
        const voxel_tree_template_block &block = tree_template->blocks[index];
        uint8_t offsets[3U * sizeof(uint32_t)];
        buffer_store_le(offsets, static_cast<uint32_t>(block.offset_x));
        buffer_store_le(offsets + sizeof(uint32_t), static_cast<uint32_t>(block.offset_y));
        buffer_store_le(offsets + 2U * sizeof(uint32_t), static_cast<uint32_t>(block.offset_z));
        error_code = buffer.append(offsets, sizeof(offsets));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = voxel_save_append_block_reference(buffer, block.block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_save_read_template(ft_byte_buffer &buffer,
    voxel_tree_template_block *blocks,
    voxel_tree_template *tree_template) noexcept
{
    uint32_t block_count;
    uint32_t index;
    int32_t error_code;

    if (blocks == ft_nullptr || tree_template == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&block_count);
    if (error_code != FT_ERR_SUCCESS
        || block_count > VOXEL_MAX_TREE_TEMPLATE_BLOCKS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < block_count)
    {
        uint8_t offsets[3U * sizeof(uint32_t)];
        error_code = buffer.read(offsets, sizeof(offsets));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        blocks[index].offset_x = static_cast<int32_t>(buffer_load_le<uint32_t>(offsets));
        blocks[index].offset_y = static_cast<int32_t>(buffer_load_le<uint32_t>(
            offsets + sizeof(uint32_t)));
        blocks[index].offset_z = static_cast<int32_t>(buffer_load_le<uint32_t>(
            offsets + 2U * sizeof(uint32_t)));
        error_code = voxel_save_read_block_reference(buffer,
            &blocks[index].block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    tree_template->blocks = blocks;
    tree_template->block_count = block_count;
    return (FT_ERR_SUCCESS);
}

static constexpr uint8_t VOXEL_SAVE_BIOME_PROFILE_FIELDS[] = {4U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_BIOME_DECORATION_FIELDS[] = {1U, 1U, 1U, 1U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_FEATURE_FIELDS[] = {4U, 4U, 4U, 4U, 1U, 1U};

static constexpr uint8_t VOXEL_SAVE_UNDERGROUND_FIELDS[] = {1U, 1U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 4U, 1U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_SURFACE_FLUIDS_FIELDS[] = {1U, 1U, 4U, 4U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_SURFACE_DEPTHS_FIELDS[] = {4U, 4U};
static constexpr uint8_t VOXEL_SAVE_UNDERGROUND_LAKES_FIELDS[] = {1U, 4U, 4U, 4U, 4U, 4U, 4U};

static constexpr uint8_t VOXEL_SAVE_NOISE_FIELDS[] = {4U, 4U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_SIZE_DEFAULTS_FIELDS[] = {4U, 4U};
static constexpr uint8_t VOXEL_SAVE_LAYERS_FIELDS[] = {1U, 1U, 4U, 4U, 4U, 4U};
static constexpr uint8_t VOXEL_SAVE_TRANSITIONS_FIELDS[] = {1U, 4U, 4U, 1U, 1U};
static constexpr uint8_t VOXEL_SAVE_TERRAIN_TAIL_FIELDS[] = {4U, 4U, 4U, 4U, 1U};

template <ft_size_t Size, ft_size_t FieldCount>
static int32_t voxel_save_append_fields(ft_byte_buffer &buffer, const uint8_t (&bytes)[Size],
    const uint8_t (&widths)[FieldCount]) noexcept
{
    int32_t status = buffer.append(bytes, Size);
    if (status != FT_ERR_FULL && status != FT_ERR_NO_MEMORY && status != FT_ERR_OUT_OF_RANGE)
        return (status);
    ft_size_t offset = 0U;
    ft_size_t field = 0U;
    while (field < FieldCount)
    {
        status = buffer.append(bytes + offset, widths[field]);
        if (status != FT_ERR_SUCCESS)
            return (status);
        offset += widths[field++];
    }
    return (FT_ERR_SUCCESS);
}

template <ft_size_t Size, ft_size_t FieldCount>
static int32_t voxel_save_read_fields(ft_byte_buffer &buffer, uint8_t (&bytes)[Size],
    const uint8_t (&widths)[FieldCount]) noexcept
{
    const int32_t status = buffer.read(bytes, Size);
    if (status != FT_ERR_OUT_OF_RANGE)
        return (status);
    // Match the cursor left by the original individual field reads.
    const ft_size_t available = buffer.remaining();
    ft_size_t complete = 0U;
    ft_size_t field = 0U;
    while (field < FieldCount && widths[field] <= available - complete)
        complete += widths[field++];
    const int32_t skip_status = buffer.skip(complete);
    if (skip_status != FT_ERR_SUCCESS)
        return (skip_status);
    return (status);
}

static int32_t voxel_save_append_biome(ft_byte_buffer &buffer,
    const voxel_biome_definition &biome) noexcept
{
    uint8_t profile[12];
    buffer_store_le(profile, static_cast<uint32_t>(biome.profile.surface_height));
    buffer_store_le(profile + 4U, static_cast<uint32_t>(biome.profile.height_variation));
    buffer_store_le(profile + 8U, static_cast<uint32_t>(biome.profile.topsoil_depth));
    int32_t error_code = voxel_save_append_fields(buffer, profile, VOXEL_SAVE_BIOME_PROFILE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_block_reference(buffer,
        biome.surface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_block_reference(buffer,
        biome.subsurface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_block_reference(buffer,
        biome.deep_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t decoration[12] = {biome.allow_shrubs, biome.allow_trees,
        biome.allow_snow_caps, biome.allow_mountain_ridges};
    buffer_store_le(decoration + 4U, biome.shrub_chance_percent);
    buffer_store_le(decoration + 8U, biome.tree_chance_percent);
    return (voxel_save_append_fields(buffer, decoration, VOXEL_SAVE_BIOME_DECORATION_FIELDS));
}

static int32_t voxel_save_read_biome(ft_byte_buffer &buffer,
    voxel_biome_definition &biome) noexcept
{
    voxel_biome_profile profile;
    uint32_t surface_block_id;
    uint32_t subsurface_block_id;
    uint32_t deep_block_id;
    uint8_t profile_bytes[12];
    int32_t error_code = voxel_save_read_fields(buffer, profile_bytes, VOXEL_SAVE_BIOME_PROFILE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    profile.surface_height = static_cast<int32_t>(buffer_load_le<uint32_t>(profile_bytes));
    profile.height_variation = static_cast<int32_t>(buffer_load_le<uint32_t>(profile_bytes + 4U));
    profile.topsoil_depth = static_cast<int32_t>(buffer_load_le<uint32_t>(profile_bytes + 8U));
    error_code = voxel_save_read_block_reference(buffer, &surface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_read_block_reference(buffer,
        &subsurface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_read_block_reference(buffer, &deep_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t decoration[12];
    error_code = voxel_save_read_fields(buffer, decoration, VOXEL_SAVE_BIOME_DECORATION_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_profile(profile);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_block_palette(surface_block_id,
        subsurface_block_id, deep_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_decoration_policy(
        ft_platform_cast<ft_bool>(decoration[0]), ft_platform_cast<ft_bool>(decoration[1]),
        buffer_load_le<uint32_t>(decoration + 4U), buffer_load_le<uint32_t>(decoration + 8U));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_snow_cap_policy(
        ft_platform_cast<ft_bool>(decoration[2]));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_mountain_ridge_policy(
        ft_platform_cast<ft_bool>(decoration[3]));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (biome.set_tree_template_override(ft_nullptr));
}

static constexpr uint8_t VOXEL_SAVE_ORE_BASE_FIELDS[] = {4U, 4U, 4U, 4U, 1U, 1U};
static constexpr uint8_t VOXEL_SAVE_ORE_RANGE_FIELDS[] = {4U, 4U, 4U, 4U, 4U, 4U};

static int32_t voxel_save_append_ore(ft_byte_buffer &buffer,
    const voxel_ore_rule &ore) noexcept
{
    int32_t error_code = voxel_save_append_block_reference(buffer, ore.block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t base[18];
    buffer_store_le(base, static_cast<uint32_t>(ore.minimum_height));
    buffer_store_le(base + 4U, static_cast<uint32_t>(ore.maximum_height));
    buffer_store_le(base + 8U, ore.vein_size);
    buffer_store_le(base + 12U, ore.chance_percent);
    base[16] = ore.allow_ore_replacement;
    base[17] = ore.enabled;
    error_code = voxel_save_append_fields(buffer, base, VOXEL_SAVE_ORE_BASE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t ranges[24];
    buffer_store_le(ranges, static_cast<uint32_t>(ore.minimum_depth));
    buffer_store_le(ranges + 4U, static_cast<uint32_t>(ore.maximum_depth));
    buffer_store_le(ranges + 8U, ore.vein_size_min);
    buffer_store_le(ranges + 12U, ore.vein_size_max);
    buffer_store_le(ranges + 16U, ore.veins_per_chunk_min);
    buffer_store_le(ranges + 20U, ore.veins_per_chunk_max);
    return (voxel_save_append_fields(buffer, ranges, VOXEL_SAVE_ORE_RANGE_FIELDS));
}

static int32_t voxel_save_read_ore(ft_byte_buffer &buffer,
    voxel_ore_rule &ore, uint32_t version) noexcept
{
    uint32_t block_id;
    int32_t error_code = voxel_save_read_block_reference(buffer, &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t base[18];
    error_code = voxel_save_read_fields(buffer, base, VOXEL_SAVE_ORE_BASE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ore.block_id = block_id;
    error_code = ore.set_range(static_cast<int32_t>(buffer_load_le<uint32_t>(base)),
        static_cast<int32_t>(buffer_load_le<uint32_t>(base + 4U)));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = ore.set_vein(buffer_load_le<uint32_t>(base + 8U),
        buffer_load_le<uint32_t>(base + 12U));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = ore.set_ore_replacement(base[16]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = ore.set_enabled(ft_platform_cast<ft_bool>(base[17]));
    if (error_code != FT_ERR_SUCCESS || version < 10U)
        return (error_code);
    uint8_t ranges[24];
    error_code = voxel_save_read_fields(buffer, ranges, VOXEL_SAVE_ORE_RANGE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    const int32_t minimum_depth = static_cast<int32_t>(buffer_load_le<uint32_t>(ranges));
    const int32_t maximum_depth = static_cast<int32_t>(buffer_load_le<uint32_t>(ranges + 4U));
    const uint32_t vein_size_min = buffer_load_le<uint32_t>(ranges + 8U);
    const uint32_t vein_size_max = buffer_load_le<uint32_t>(ranges + 12U);
    if (minimum_depth != 0 || maximum_depth != 0)
    {
        error_code = ore.set_depth_range(minimum_depth, maximum_depth);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (vein_size_min != 0U || vein_size_max != 0U)
    {
        error_code = ore.set_vein_size_range(vein_size_min, vein_size_max);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    return (ore.set_frequency_range(buffer_load_le<uint32_t>(ranges + 16U),
        buffer_load_le<uint32_t>(ranges + 20U)));
}

static int32_t voxel_save_append_template_indices(ft_byte_buffer &buffer,
    const voxel_biome_definition &biome) noexcept
{
    if (biome.tree_template_count > VOXEL_MAX_BIOME_TREE_TEMPLATES)
        return (FT_ERR_INVALID_ARGUMENT);
    uint8_t bytes[(VOXEL_MAX_BIOME_TREE_TEMPLATES + 1U) * sizeof(uint32_t)];
    buffer_store_le(bytes, biome.tree_template_count);
    uint32_t index = 0U;
    while (index < biome.tree_template_count)
    {
        buffer_store_le(bytes + (index + 1U) * sizeof(uint32_t), biome.tree_template_indices[index]);
        ++index;
    }
    const ft_size_t size = (biome.tree_template_count + 1U) * sizeof(uint32_t);
    int32_t status = buffer.append(bytes, size);
    if (status != FT_ERR_FULL && status != FT_ERR_NO_MEMORY && status != FT_ERR_OUT_OF_RANGE)
        return (status);
    ft_size_t offset = 0U;
    while (offset < size)
    {
        status = buffer.append(bytes + offset, sizeof(uint32_t));
        if (status != FT_ERR_SUCCESS)
            return (status);
        offset += sizeof(uint32_t);
    }
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_serialize(
    const voxel_generation_config &config, ft_byte_buffer &buffer) noexcept
{
    uint32_t index;
    int32_t error_code;

    if (config.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (voxel_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.clear();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(VOXEL_SAVE_MAGIC);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(VOXEL_SAVE_VERSION);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t noise_fields[16];
    buffer_store_le(noise_fields, static_cast<uint32_t>(config.sea_level));
    buffer_store_le(noise_fields + 4U, static_cast<uint32_t>(config.large_noise_scale));
    buffer_store_le(noise_fields + 8U, static_cast<uint32_t>(config.detail_noise_scale));
    buffer_store_le(noise_fields + 12U, static_cast<uint32_t>(config.detail_noise_percent));
    error_code = voxel_save_append_fields(buffer, noise_fields, VOXEL_SAVE_NOISE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t size_defaults_fields[8];
    buffer_store_le(size_defaults_fields, static_cast<uint32_t>(config.biome_size_min));
    buffer_store_le(size_defaults_fields + 4U, static_cast<uint32_t>(config.biome_size_max));
    error_code = voxel_save_append_fields(buffer, size_defaults_fields, VOXEL_SAVE_SIZE_DEFAULTS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    error_code = buffer.append_u8(config.enable_biome_size_control);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_biome_limits(buffer, config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.water_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.biome_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        error_code = voxel_save_append_biome(buffer, config.biomes[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = buffer.append_u32_le(config.tree_template_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < config.tree_template_count)
    {
        error_code = voxel_save_append_template(buffer,
            &config.tree_templates[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        uint32_t override_index;

        const voxel_biome_definition &biome = config.biomes[index];
        error_code = voxel_save_append_template_indices(buffer, biome);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        override_index = VOXEL_MAX_TREE_TEMPLATES + 1U;
        uint32_t template_index = 0U;
        while (biome.tree_template != ft_nullptr && template_index < config.tree_template_count)
        {
            if (biome.tree_template == &config.tree_templates[template_index])
            {
                override_index = template_index;
                break ;
            }
            template_index += 1U;
        }
        if (config.biomes[index].tree_template != ft_nullptr
            && override_index == VOXEL_MAX_TREE_TEMPLATES + 1U)
            override_index = VOXEL_MAX_TREE_TEMPLATES;
        error_code = buffer.append_u32_le(override_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (config.biomes[index].tree_template != ft_nullptr
            && override_index == VOXEL_MAX_TREE_TEMPLATES)
        {
            error_code = voxel_save_append_template(buffer,
                config.biomes[index].tree_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    error_code = buffer.append_u32_le(config.feature_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < config.feature_count)
    {
        const voxel_feature_rule &feature = config.features[index];
        uint8_t fields[18];
        buffer_store_le(fields, static_cast<uint32_t>(feature.biome_index));
        buffer_store_le(fields + 4U, static_cast<uint32_t>(feature.minimum_height));
        buffer_store_le(fields + 8U, static_cast<uint32_t>(feature.maximum_height));
        buffer_store_le(fields + 12U, feature.chance_percent);
        fields[16] = feature.requires_dry_land;
        fields[17] = feature.template_data != ft_nullptr;
        error_code = voxel_save_append_fields(buffer, fields, VOXEL_SAVE_FEATURE_FIELDS);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (fields[17] != 0U)
        {
            error_code = voxel_save_append_template(buffer, feature.template_data);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    error_code = buffer.append_u32_le(config.ore_rule_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < VOXEL_MAX_ORE_RULES)
    {
        error_code = voxel_save_append_ore(buffer, config.ores[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    uint8_t underground_fields[55];
    underground_fields[0] = config.underground_structures.enable_ravines;
    underground_fields[1] = config.underground_structures.enable_cave_rooms;
    buffer_store_le(underground_fields + 2U, config.underground_structures.ravine_chance_percent);
    buffer_store_le(underground_fields + 6U, config.underground_structures.cave_room_chance_percent);
    buffer_store_le(underground_fields + 10U, static_cast<uint32_t>(config.underground_structures.minimum_height));
    buffer_store_le(underground_fields + 14U, static_cast<uint32_t>(config.underground_structures.maximum_height));
    buffer_store_le(underground_fields + 18U, config.underground_structures.ravine_width);
    buffer_store_le(underground_fields + 22U, config.underground_structures.ravine_depth);
    buffer_store_le(underground_fields + 26U, config.underground_structures.cave_small_radius);
    buffer_store_le(underground_fields + 30U, config.underground_structures.cave_large_radius);
    buffer_store_le(underground_fields + 34U, config.underground_structures.cave_large_chance_percent);
    buffer_store_le(underground_fields + 38U, config.underground_structures.cave_entrance_chance_percent);
    buffer_store_le(underground_fields + 42U, config.underground_structures.cave_entrance_radius);
    underground_fields[46] = config.underground_structures.enable_cavern_rooms;
    buffer_store_le(underground_fields + 47U, config.underground_structures.cavern_room_chance_percent);
    buffer_store_le(underground_fields + 51U, config.underground_structures.cavern_room_radius);
    error_code = voxel_save_append_fields(buffer, underground_fields, VOXEL_SAVE_UNDERGROUND_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t surface_fluids_fields[18];
    surface_fluids_fields[0] = config.fluids.enable_rivers;
    surface_fluids_fields[1] = config.fluids.enable_lakes;
    buffer_store_le(surface_fluids_fields + 2U, static_cast<uint32_t>(config.fluids.river_noise_scale));
    buffer_store_le(surface_fluids_fields + 6U, static_cast<uint32_t>(config.fluids.river_width));
    buffer_store_le(surface_fluids_fields + 10U, static_cast<uint32_t>(config.fluids.lake_noise_scale));
    buffer_store_le(surface_fluids_fields + 14U, config.fluids.lake_chance_percent);
    error_code = voxel_save_append_fields(buffer, surface_fluids_fields, VOXEL_SAVE_SURFACE_FLUIDS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t surface_depths_fields[8];
    buffer_store_le(surface_depths_fields + 0U, config.fluids.surface_river_depth);
    buffer_store_le(surface_depths_fields + 4U, config.fluids.surface_lake_depth);
    error_code = voxel_save_append_fields(buffer, surface_depths_fields, VOXEL_SAVE_SURFACE_DEPTHS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t underground_lakes_fields[25];
    underground_lakes_fields[0] = config.fluids.enable_underground_lakes;
    buffer_store_le(underground_lakes_fields + 1U, config.fluids.underground_lake_chance_percent);
    buffer_store_le(underground_lakes_fields + 5U, static_cast<uint32_t>(config.fluids.underground_lake_minimum_y));
    buffer_store_le(underground_lakes_fields + 9U, static_cast<uint32_t>(config.fluids.underground_lake_maximum_y));
    buffer_store_le(underground_lakes_fields + 13U, config.fluids.underground_lake_depth);
    buffer_store_le(underground_lakes_fields + 17U, config.fluids.underground_lake_floor_thickness);
    buffer_store_le(underground_lakes_fields + 21U, config.fluids.underground_lake_roof_thickness);
    error_code = voxel_save_append_fields(buffer, underground_lakes_fields, VOXEL_SAVE_UNDERGROUND_LAKES_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t layers_fields[18];
    layers_fields[0] = config.layers.enable_beaches;
    layers_fields[1] = config.layers.enable_snow_caps;
    buffer_store_le(layers_fields + 2U, config.layers.beach_depth);
    buffer_store_le(layers_fields + 6U, config.layers.underwater_depth);
    buffer_store_le(layers_fields + 10U, config.layers.snow_cap_depth);
    buffer_store_le(layers_fields + 14U, static_cast<uint32_t>(config.layers.snow_cap_minimum_height));
    error_code = voxel_save_append_fields(buffer, layers_fields, VOXEL_SAVE_LAYERS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    error_code = voxel_save_append_block_reference(buffer,
        config.layers.beach_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_block_reference(buffer,
        config.layers.underwater_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_append_block_reference(buffer,
        config.layers.snow_cap_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t transitions_fields[11];
    transitions_fields[0] = config.enable_biome_transitions;
    buffer_store_le(transitions_fields + 1U, static_cast<uint32_t>(config.biome_transition_noise_scale));
    buffer_store_le(transitions_fields + 5U, config.biome_transition_noise_strength);
    transitions_fields[9] = config.enable_mountain_ridges;
    transitions_fields[10] = config.enable_erosion;
    error_code = voxel_save_append_fields(buffer, transitions_fields, VOXEL_SAVE_TRANSITIONS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t terrain_tail_fields[17];
    buffer_store_le(terrain_tail_fields, static_cast<uint32_t>(config.mountain_ridge_scale));
    buffer_store_le(terrain_tail_fields + 4U, config.mountain_ridge_strength);
    buffer_store_le(terrain_tail_fields + 8U, static_cast<uint32_t>(config.erosion_noise_scale));
    buffer_store_le(terrain_tail_fields + 12U, config.erosion_strength);
    terrain_tail_fields[16] = config.allow_cross_chunk_features;
    return (voxel_save_append_fields(buffer, terrain_tail_fields, VOXEL_SAVE_TERRAIN_TAIL_FIELDS));
}

static int32_t voxel_save_read_config(
    voxel_generation_config &config, ft_byte_buffer &buffer, uint32_t version) noexcept
{
    uint32_t index;
    uint8_t enable_biome_transitions;
    uint8_t enable_biome_size_control;
    int32_t biome_transition_noise_scale;
    uint32_t biome_transition_noise_strength;
    uint8_t enable_mountain_ridges;
    uint8_t enable_erosion;
    uint8_t enable_ravines;
    uint8_t enable_cave_rooms;
    uint32_t ravine_chance_percent;
    uint32_t cave_room_chance_percent;
    int32_t underground_minimum_height;
    int32_t underground_maximum_height;
    uint32_t ravine_width;
    uint32_t ravine_depth;
    uint32_t cave_small_radius;
    uint32_t cave_large_radius;
    uint32_t cave_large_chance_percent;
    uint32_t cave_entrance_chance_percent;
    uint32_t cave_entrance_radius;
    uint8_t enable_cavern_rooms;
    uint32_t cavern_room_chance_percent;
    uint32_t cavern_room_radius;
    uint8_t enable_rivers;
    uint8_t enable_lakes;
    int32_t river_noise_scale;
    int32_t river_width;
    int32_t lake_noise_scale;
    uint32_t lake_chance_percent;
    uint32_t surface_river_depth = 0U;
    uint32_t surface_lake_depth = 0U;
    uint8_t enable_underground_lakes = FT_FALSE;
    uint32_t underground_lake_chance_percent = 0U;
    int32_t underground_lake_minimum_y = 0;
    int32_t underground_lake_maximum_y = 0;
    uint32_t underground_lake_depth = 0U;
    uint32_t underground_lake_floor_thickness = 0U;
    uint32_t underground_lake_roof_thickness = 0U;
    uint8_t enable_beaches;
    uint8_t enable_snow_caps;
    uint32_t beach_depth;
    uint32_t underwater_depth;
    uint32_t snow_cap_depth;
    int32_t snow_cap_minimum_height;
    uint32_t beach_block_id;
    uint32_t underwater_block_id;
    uint32_t snow_cap_block_id;
    int32_t error_code;

    voxel_generation_config loaded_config;
    error_code = voxel_default_generation_config(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t noise_fields[16];
    error_code = voxel_save_read_fields(buffer, noise_fields, VOXEL_SAVE_NOISE_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    loaded_config.sea_level = static_cast<int32_t>(buffer_load_le<uint32_t>(noise_fields));
    loaded_config.large_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(noise_fields + 4U));
    loaded_config.detail_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(noise_fields + 8U));
    loaded_config.detail_noise_percent = static_cast<int32_t>(buffer_load_le<uint32_t>(noise_fields + 12U));

    if (version >= 8U)
    {
        uint8_t size_defaults_fields[8];
        error_code = voxel_save_read_fields(buffer, size_defaults_fields, VOXEL_SAVE_SIZE_DEFAULTS_FIELDS);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        loaded_config.biome_size_min = static_cast<int32_t>(buffer_load_le<uint32_t>(size_defaults_fields));
        loaded_config.biome_size_max = static_cast<int32_t>(buffer_load_le<uint32_t>(size_defaults_fields + 4U));

        if (version >= 9U)
        {
            error_code = buffer.read_u8(&enable_biome_size_control);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            loaded_config.enable_biome_size_control = enable_biome_size_control;
            error_code = voxel_save_read_biome_limits(buffer, loaded_config);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        else
        {
            loaded_config.enable_biome_size_control = FT_TRUE;
            index = 0U;
            while (index < VOXEL_MAX_CUSTOM_BIOMES)
            {
                loaded_config.biome_size_min_by_biome[index] =
                    loaded_config.biome_size_min;
                loaded_config.biome_size_max_by_biome[index] =
                    loaded_config.biome_size_max;
                loaded_config.biome_size_override_enabled[index] = FT_FALSE;
                index += 1U;
            }
        }
    }
    else
    {
        loaded_config.enable_biome_size_control = FT_FALSE;
    }
    error_code = buffer.read_u32_le(&loaded_config.water_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.biome_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.biome_count > VOXEL_MAX_CUSTOM_BIOMES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        error_code = voxel_save_read_biome(buffer, loaded_config.biomes[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = voxel_generation_config_clear_tree_templates(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.tree_template_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.tree_template_count > VOXEL_MAX_TREE_TEMPLATES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < loaded_config.tree_template_count)
    {
        error_code = voxel_save_read_template(buffer,
            loaded_config.tree_template_blocks[index],
            &loaded_config.tree_templates[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    index = 0U;
    while (index < VOXEL_MAX_CUSTOM_BIOMES)
    {
        uint32_t template_count;
        uint32_t template_index;
        uint32_t override_index;
        voxel_tree_template_block override_blocks[
            VOXEL_MAX_TREE_TEMPLATE_BLOCKS];
        voxel_tree_template override_template;

        error_code = buffer.read_u32_le(&template_count);
        if (error_code != FT_ERR_SUCCESS
            || template_count > VOXEL_MAX_BIOME_TREE_TEMPLATES)
            return (FT_ERR_INVALID_ARGUMENT);
        loaded_config.biomes[index].tree_template_count = template_count;
        template_index = 0U;
        while (template_index < template_count)
        {
            error_code = buffer.read_u32_le(&loaded_config.biomes[index]
                .tree_template_indices[template_index]);
            if (error_code != FT_ERR_SUCCESS
                || loaded_config.biomes[index].tree_template_indices[
                    template_index] >= loaded_config.tree_template_count)
                return (FT_ERR_INVALID_ARGUMENT);
            template_index += 1U;
        }
        error_code = buffer.read_u32_le(&override_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        loaded_config.biomes[index].tree_template = ft_nullptr;
        if (override_index < loaded_config.tree_template_count)
            loaded_config.biomes[index].tree_template =
                &loaded_config.tree_templates[override_index];
        else if (override_index == VOXEL_MAX_TREE_TEMPLATES)
        {
            error_code = voxel_save_read_template(buffer, override_blocks,
                &override_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = loaded_config.set_biome_tree_template_override(
                index, &override_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    error_code = buffer.read_u32_le(&loaded_config.feature_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.feature_count > VOXEL_MAX_FEATURE_RULES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < loaded_config.feature_count)
    {
        voxel_tree_template_block feature_blocks[
            VOXEL_MAX_TREE_TEMPLATE_BLOCKS];
        voxel_tree_template feature_template;
        voxel_feature_rule loaded_feature;

        error_code = loaded_feature.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);

        uint8_t fields[18];
        error_code = voxel_save_read_fields(buffer, fields, VOXEL_SAVE_FEATURE_FIELDS);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_biome_range(
            static_cast<int32_t>(buffer_load_le<uint32_t>(fields)),
            static_cast<int32_t>(buffer_load_le<uint32_t>(fields + 4U)),
            static_cast<int32_t>(buffer_load_le<uint32_t>(fields + 8U)));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_chance(buffer_load_le<uint32_t>(fields + 12U));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_requires_dry_land(
            ft_platform_cast<ft_bool>(fields[16]));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (fields[17] != 0U)
        {
            error_code = voxel_save_read_template(buffer, feature_blocks,
                &feature_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            uint32_t template_index = 0U;
            while (template_index < loaded_config.tree_template_count
                && voxel_tree_templates_equal(loaded_config.tree_templates[template_index], feature_template) == FT_FALSE)
                ++template_index;
            if (template_index == loaded_config.tree_template_count)
                error_code = voxel_generation_config_add_tree_template(loaded_config, feature_template, &template_index);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = loaded_feature.set_template(&loaded_config.tree_templates[template_index]);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        error_code = loaded_config.features[index].initialize(loaded_feature);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = buffer.read_u32_le(&loaded_config.ore_rule_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.ore_rule_count > VOXEL_MAX_ORE_RULES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < VOXEL_MAX_ORE_RULES)
    {
        error_code = voxel_save_read_ore(buffer, loaded_config.ores[index],
            version);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    uint8_t underground_fields[55];
    error_code = voxel_save_read_fields(buffer, underground_fields, VOXEL_SAVE_UNDERGROUND_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    enable_ravines = underground_fields[0];
    enable_cave_rooms = underground_fields[1];
    ravine_chance_percent = buffer_load_le<uint32_t>(underground_fields + 2U);
    cave_room_chance_percent = buffer_load_le<uint32_t>(underground_fields + 6U);
    underground_minimum_height = static_cast<int32_t>(buffer_load_le<uint32_t>(underground_fields + 10U));
    underground_maximum_height = static_cast<int32_t>(buffer_load_le<uint32_t>(underground_fields + 14U));
    ravine_width = buffer_load_le<uint32_t>(underground_fields + 18U);
    ravine_depth = buffer_load_le<uint32_t>(underground_fields + 22U);
    cave_small_radius = buffer_load_le<uint32_t>(underground_fields + 26U);
    cave_large_radius = buffer_load_le<uint32_t>(underground_fields + 30U);
    cave_large_chance_percent = buffer_load_le<uint32_t>(underground_fields + 34U);
    cave_entrance_chance_percent = buffer_load_le<uint32_t>(underground_fields + 38U);
    cave_entrance_radius = buffer_load_le<uint32_t>(underground_fields + 42U);
    enable_cavern_rooms = underground_fields[46];
    cavern_room_chance_percent = buffer_load_le<uint32_t>(underground_fields + 47U);
    cavern_room_radius = buffer_load_le<uint32_t>(underground_fields + 51U);
    uint8_t surface_fluids_fields[18];
    error_code = voxel_save_read_fields(buffer, surface_fluids_fields, VOXEL_SAVE_SURFACE_FLUIDS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    enable_rivers = surface_fluids_fields[0];
    enable_lakes = surface_fluids_fields[1];
    river_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(surface_fluids_fields + 2U));
    river_width = static_cast<int32_t>(buffer_load_le<uint32_t>(surface_fluids_fields + 6U));
    lake_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(surface_fluids_fields + 10U));
    lake_chance_percent = buffer_load_le<uint32_t>(surface_fluids_fields + 14U);
    if (version >= 12U)
    {
        uint8_t surface_depths_fields[8];
        error_code = voxel_save_read_fields(buffer, surface_depths_fields, VOXEL_SAVE_SURFACE_DEPTHS_FIELDS);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        surface_river_depth = buffer_load_le<uint32_t>(surface_depths_fields + 0U);
        surface_lake_depth = buffer_load_le<uint32_t>(surface_depths_fields + 4U);
    }
    if (version >= 11U)
    {
        uint8_t underground_lakes_fields[25];
        error_code = voxel_save_read_fields(buffer, underground_lakes_fields, VOXEL_SAVE_UNDERGROUND_LAKES_FIELDS);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        enable_underground_lakes = underground_lakes_fields[0];
        underground_lake_chance_percent = buffer_load_le<uint32_t>(underground_lakes_fields + 1U);
        underground_lake_minimum_y = static_cast<int32_t>(buffer_load_le<uint32_t>(underground_lakes_fields + 5U));
        underground_lake_maximum_y = static_cast<int32_t>(buffer_load_le<uint32_t>(underground_lakes_fields + 9U));
        underground_lake_depth = buffer_load_le<uint32_t>(underground_lakes_fields + 13U);
        underground_lake_floor_thickness = buffer_load_le<uint32_t>(underground_lakes_fields + 17U);
        underground_lake_roof_thickness = buffer_load_le<uint32_t>(underground_lakes_fields + 21U);
    }
    uint8_t layers_fields[18];
    error_code = voxel_save_read_fields(buffer, layers_fields, VOXEL_SAVE_LAYERS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    enable_beaches = layers_fields[0];
    enable_snow_caps = layers_fields[1];
    beach_depth = buffer_load_le<uint32_t>(layers_fields + 2U);
    underwater_depth = buffer_load_le<uint32_t>(layers_fields + 6U);
    snow_cap_depth = buffer_load_le<uint32_t>(layers_fields + 10U);
    snow_cap_minimum_height = static_cast<int32_t>(buffer_load_le<uint32_t>(layers_fields + 14U));

    error_code = voxel_save_read_block_reference(buffer, &beach_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_read_block_reference(buffer,
        &underwater_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_save_read_block_reference(buffer,
        &snow_cap_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_enabled(
        ft_platform_cast<ft_bool>(enable_ravines),
        ft_platform_cast<ft_bool>(enable_cave_rooms));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_chances(
        ravine_chance_percent, cave_room_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_height_range(
        underground_minimum_height, underground_maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_shape(
        ravine_width, ravine_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_cave_shape(
        cave_small_radius, cave_large_radius, cave_large_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_cave_entrances(
        cave_entrance_chance_percent, cave_entrance_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.underground_structures.set_cavern_rooms(
        ft_platform_cast<ft_bool>(enable_cavern_rooms),
        cavern_room_chance_percent, cavern_room_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.fluids.set_enabled(
        ft_platform_cast<ft_bool>(enable_rivers), ft_platform_cast<ft_bool>(enable_lakes));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.fluids.set_river_settings(
        river_noise_scale, river_width);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.fluids.set_lake_settings(
        lake_noise_scale, lake_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (version >= 12U)
    {
        error_code = loaded_config.fluids.set_surface_water_depths(
            surface_river_depth, surface_lake_depth);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    if (version >= 11U)
    {
        error_code = loaded_config.fluids.set_underground_lakes_enabled(
            ft_platform_cast<ft_bool>(enable_underground_lakes));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_config.fluids.set_underground_lake_settings(
            underground_lake_chance_percent, underground_lake_minimum_y,
            underground_lake_maximum_y, underground_lake_depth,
            underground_lake_floor_thickness, underground_lake_roof_thickness);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = loaded_config.layers.set_enabled(
        ft_platform_cast<ft_bool>(enable_beaches),
        ft_platform_cast<ft_bool>(enable_snow_caps));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.layers.set_depths(
        beach_depth, underwater_depth, snow_cap_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.layers.set_snowline(snow_cap_minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.layers.set_block_palette(
        beach_block_id, underwater_block_id, snow_cap_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t transitions_fields[11];
    error_code = voxel_save_read_fields(buffer, transitions_fields, VOXEL_SAVE_TRANSITIONS_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    enable_biome_transitions = transitions_fields[0];
    biome_transition_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(transitions_fields + 1U));
    biome_transition_noise_strength = buffer_load_le<uint32_t>(transitions_fields + 5U);
    enable_mountain_ridges = transitions_fields[9];
    enable_erosion = transitions_fields[10];

    error_code = loaded_config.set_biome_transitions_enabled(
        ft_platform_cast<ft_bool>(enable_biome_transitions));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_biome_transition_settings(
        biome_transition_noise_scale, biome_transition_noise_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_mountain_ridges_enabled(
        ft_platform_cast<ft_bool>(enable_mountain_ridges));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_erosion_enabled(
        ft_platform_cast<ft_bool>(enable_erosion));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    uint8_t terrain_tail_fields[17];
    error_code = voxel_save_read_fields(buffer, terrain_tail_fields, VOXEL_SAVE_TERRAIN_TAIL_FIELDS);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    loaded_config.mountain_ridge_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(terrain_tail_fields));
    loaded_config.mountain_ridge_strength = buffer_load_le<uint32_t>(terrain_tail_fields + 4U);
    loaded_config.erosion_noise_scale = static_cast<int32_t>(buffer_load_le<uint32_t>(terrain_tail_fields + 8U));
    loaded_config.erosion_strength = buffer_load_le<uint32_t>(terrain_tail_fields + 12U);
    loaded_config.allow_cross_chunk_features = terrain_tail_fields[16];

    error_code = loaded_config.set_biome_selector(ft_nullptr, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_cross_chunk_writer(ft_nullptr, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (voxel_generation_config_is_valid(loaded_config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = config.initialize(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t voxel_generation_config_deserialize(
    voxel_generation_config &config, ft_byte_buffer &buffer) noexcept
{
    uint32_t magic;
    uint32_t version;
    int32_t error_code;

    if (buffer.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = buffer.reset_read_position();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&magic);
    if (error_code != FT_ERR_SUCCESS || magic != VOXEL_SAVE_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&version);
    if (error_code != FT_ERR_SUCCESS
        || (version != VOXEL_SAVE_VERSION && version != 11U && version != 10U
            && version != 9U
            && version != 8U && version != 7U))
        return (FT_ERR_INVALID_ARGUMENT);
    return (voxel_save_read_config(config, buffer, version));
}

int32_t voxel_generation_config_save_file(const char *file_path,
    const voxel_generation_config &config) noexcept
{
    ft_byte_buffer buffer;
    FILE *file;
    int32_t error_code;

    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_generation_config_serialize(config, buffer);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    file = fopen(file_path, "wb");
    if (file == ft_nullptr)
        return (FT_ERR_IO);
    (void)setvbuf(file, ft_nullptr, _IONBF, 0U);
    const ft_size_t length = buffer.size();
    const ft_size_t written = fwrite(buffer.data(), 1U,
        FT_UINT64_TO_NATIVE_SIZE_CAST(length), file);
    if (fclose(file) != 0 || written != length)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_save_read_exact(uint8_t *destination,
    ft_size_t length, void *context) noexcept
{
    FILE *file = static_cast<FILE *>(context);
    if (fread(destination, 1U, FT_UINT64_TO_NATIVE_SIZE_CAST(length), file) != length)
        return (FT_ERR_IO);
    if (fgetc(file) != EOF || ferror(file) != 0)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_save_read_stream(FILE *file, ft_byte_buffer &buffer) noexcept
{
    uint8_t bytes[4096];
    int32_t error_code = buffer.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    while (1)
    {
        const ft_size_t read_count = fread(bytes, 1U, sizeof(bytes), file);
        if (read_count > 0U)
        {
            error_code = buffer.append(bytes, read_count);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        if (read_count < sizeof(bytes))
        {
            if (ferror(file) != 0)
                return (FT_ERR_IO);
            return (FT_ERR_SUCCESS);
        }
    }
}

int32_t voxel_generation_config_load_file(const char *file_path,
    voxel_generation_config &config) noexcept
{
    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    FILE *file = fopen(file_path, "rb");
    if (file == ft_nullptr)
        return (FT_ERR_IO);
    ft_byte_buffer buffer;
    ft_size_t length;
    int32_t error_code;
    if (cmp_file_stream_size(file, &length) == FT_ERR_SUCCESS && length != 0U)
    {
        (void)setvbuf(file, ft_nullptr, _IONBF, 0U);
        error_code = buffer.initialize_from_reader(length, voxel_save_read_exact, file);
    }
    else
        error_code = voxel_save_read_stream(file, buffer);
    if (fclose(file) != 0 && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_generation_config_deserialize(config, buffer);
    return (error_code);
}

#endif
