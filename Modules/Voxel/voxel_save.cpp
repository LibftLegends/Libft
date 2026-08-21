#include <stdint.h>
#include <stdio.h>
#include <cstring>
#include "../Basic/basic.hpp"
#include "voxel.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Errno/errno.hpp"

static const uint32_t TERRAIN_SAVE_MAGIC = UINT32_C(0x54434F4E);
static const uint32_t TERRAIN_SAVE_VERSION = 7U;

static int32_t terrain_save_append_block_reference(ft_byte_buffer &buffer,
    uint32_t block_id) noexcept
{
    const char *block_name;
    ft_size_t name_length;
    int32_t error_code;

    block_name = terrain_get_block_name(block_id);
    if (block_name == ft_nullptr || block_name[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    name_length = std::strlen(block_name);
    if (name_length > 255U)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.append_u32_le(static_cast<uint32_t>(name_length));
    if (error_code == FT_ERR_SUCCESS)
        error_code = buffer.append(block_name, name_length);
    return (error_code);
}

static int32_t terrain_save_read_block_reference(ft_byte_buffer &buffer,
    uint32_t *block_id_out) noexcept
{
    char block_name[256];
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
    return (terrain_find_block_id_by_name(block_name, block_id_out));
}

static int32_t terrain_save_append_i32(ft_byte_buffer &buffer,
    int32_t value) noexcept
{
    return (buffer.append_u32_le(static_cast<uint32_t>(value)));
}

static int32_t terrain_save_read_i32(ft_byte_buffer &buffer,
    int32_t *value_out) noexcept
{
    uint32_t value;
    int32_t error_code;

    if (value_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    *value_out = static_cast<int32_t>(value);
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_save_append_template(ft_byte_buffer &buffer,
    const terrain_tree_template *tree_template) noexcept
{
    uint32_t index;
    int32_t error_code;

    if (tree_template == ft_nullptr
        || tree_template->block_count > TERRAIN_MAX_TREE_TEMPLATE_BLOCKS
        || (tree_template->block_count != 0U
            && tree_template->blocks == ft_nullptr))
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.append_u32_le(tree_template->block_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < tree_template->block_count)
    {
        error_code = terrain_save_append_i32(buffer,
            tree_template->blocks[index].offset_x);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_append_i32(buffer,
            tree_template->blocks[index].offset_y);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_append_i32(buffer,
            tree_template->blocks[index].offset_z);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_append_block_reference(buffer,
            tree_template->blocks[index].block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_save_read_template(ft_byte_buffer &buffer,
    terrain_tree_template_block *blocks,
    terrain_tree_template *tree_template) noexcept
{
    uint32_t block_count;
    uint32_t index;
    int32_t error_code;

    if (blocks == ft_nullptr || tree_template == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&block_count);
    if (error_code != FT_ERR_SUCCESS
        || block_count > TERRAIN_MAX_TREE_TEMPLATE_BLOCKS)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < block_count)
    {
        error_code = terrain_save_read_i32(buffer, &blocks[index].offset_x);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_read_i32(buffer, &blocks[index].offset_y);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_read_i32(buffer, &blocks[index].offset_z);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_read_block_reference(buffer,
            &blocks[index].block_id);
        if (error_code != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_ARGUMENT);
        index += 1U;
    }
    tree_template->blocks = blocks;
    tree_template->block_count = block_count;
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_save_append_biome(ft_byte_buffer &buffer,
    const terrain_biome_definition &biome) noexcept
{
    int32_t error_code;

    error_code = terrain_save_append_i32(buffer,
        biome.profile.surface_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer,
        biome.profile.height_variation);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer,
        biome.profile.topsoil_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        biome.surface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        biome.subsurface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        biome.deep_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(biome.allow_shrubs);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(biome.allow_trees);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(biome.allow_snow_caps);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(biome.allow_mountain_ridges);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(biome.shrub_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (buffer.append_u32_le(biome.tree_chance_percent));
}

static int32_t terrain_save_read_biome(ft_byte_buffer &buffer,
    terrain_biome_definition &biome) noexcept
{
    terrain_biome_profile profile;
    uint32_t surface_block_id;
    uint32_t subsurface_block_id;
    uint32_t deep_block_id;
    uint32_t shrub_chance_percent;
    uint32_t tree_chance_percent;
    uint8_t allow_shrubs;
    uint8_t allow_trees;
    uint8_t allow_snow_caps;
    uint8_t allow_mountain_ridges;
    int32_t error_code;

    error_code = terrain_save_read_i32(buffer, &profile.surface_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &profile.height_variation);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &profile.topsoil_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer, &surface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer,
        &subsurface_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer, &deep_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&allow_shrubs);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&allow_trees);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&allow_snow_caps);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&allow_mountain_ridges);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&shrub_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&tree_chance_percent);
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
        ft_platform_cast<ft_bool>(allow_shrubs), ft_platform_cast<ft_bool>(allow_trees),
        shrub_chance_percent, tree_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_snow_cap_policy(
        ft_platform_cast<ft_bool>(allow_snow_caps));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = biome.set_mountain_ridge_policy(
        ft_platform_cast<ft_bool>(allow_mountain_ridges));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (biome.set_tree_template_override(ft_nullptr));
}

static int32_t terrain_save_append_ore(ft_byte_buffer &buffer,
    const terrain_ore_rule &ore) noexcept
{
    int32_t error_code;

    error_code = terrain_save_append_block_reference(buffer, ore.block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, ore.minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, ore.maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(ore.vein_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(ore.chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(ore.allow_ore_replacement);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (buffer.append_u8(ore.enabled));
}

static int32_t terrain_save_read_ore(ft_byte_buffer &buffer,
    terrain_ore_rule &ore) noexcept
{
    uint32_t block_id;
    int32_t minimum_height;
    int32_t maximum_height;
    uint32_t vein_size;
    uint32_t chance_percent;
    uint8_t allow_ore_replacement;
    uint8_t enabled;
    int32_t error_code;

    error_code = terrain_save_read_block_reference(buffer, &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&vein_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&allow_ore_replacement);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enabled);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ore.block_id = block_id;
    error_code = ore.set_range(minimum_height, maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = ore.set_vein(vein_size, chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = ore.set_ore_replacement(
        static_cast<ft_bool>(allow_ore_replacement));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (ore.set_enabled(ft_platform_cast<ft_bool>(enabled)));
}

int32_t terrain_generation_config_serialize(
    const terrain_generation_config &config, ft_byte_buffer &buffer) noexcept
{
    uint32_t index;
    int32_t error_code;

    if (config.is_initialised() == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    if (terrain_generation_config_is_valid(config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.clear();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(TERRAIN_SAVE_MAGIC);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(TERRAIN_SAVE_VERSION);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.sea_level);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.large_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.detail_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.detail_noise_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.water_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.biome_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        error_code = terrain_save_append_biome(buffer, config.biomes[index]);
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
        error_code = terrain_save_append_template(buffer,
            &config.tree_templates[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        uint32_t override_index;

        error_code = buffer.append_u32_le(
            config.biomes[index].tree_template_count);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        uint32_t template_index;
        template_index = 0U;
        while (template_index < config.biomes[index].tree_template_count)
        {
            error_code = buffer.append_u32_le(config.biomes[index]
                .tree_template_indices[template_index]);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            template_index += 1U;
        }
        override_index = TERRAIN_MAX_TREE_TEMPLATES + 1U;
        template_index = 0U;
        while (template_index < config.tree_template_count)
        {
            if (config.biomes[index].tree_template
                == &config.tree_templates[template_index])
            {
                override_index = template_index;
                break ;
            }
            template_index += 1U;
        }
        if (config.biomes[index].tree_template != ft_nullptr
            && override_index == TERRAIN_MAX_TREE_TEMPLATES + 1U)
            override_index = TERRAIN_MAX_TREE_TEMPLATES;
        error_code = buffer.append_u32_le(override_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (config.biomes[index].tree_template != ft_nullptr
            && override_index == TERRAIN_MAX_TREE_TEMPLATES)
        {
            error_code = terrain_save_append_template(buffer,
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
        error_code = terrain_save_append_i32(buffer,
            config.features[index].biome_index);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_append_i32(buffer,
            config.features[index].minimum_height);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_append_i32(buffer,
            config.features[index].maximum_height);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.append_u32_le(config.features[index].chance_percent);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.append_u8(config.features[index].requires_dry_land);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.append_u8(config.features[index].template_data
            != ft_nullptr);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (config.features[index].template_data != ft_nullptr)
        {
            error_code = terrain_save_append_template(buffer,
                config.features[index].template_data);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        index += 1U;
    }
    error_code = buffer.append_u32_le(config.ore_rule_count);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0U;
    while (index < TERRAIN_MAX_ORE_RULES)
    {
        error_code = terrain_save_append_ore(buffer, config.ores[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = buffer.append_u8(config.underground_structures.enable_ravines);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.underground_structures.enable_cave_rooms);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.ravine_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_room_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.underground_structures.minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.underground_structures.maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.ravine_width);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.ravine_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_small_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_large_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_large_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_entrance_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cave_entrance_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.underground_structures.enable_cavern_rooms);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cavern_room_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.underground_structures.cavern_room_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.fluids.enable_rivers);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.fluids.enable_lakes);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.fluids.river_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.fluids.river_width);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.fluids.lake_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.fluids.lake_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.layers.enable_beaches);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.layers.enable_snow_caps);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.layers.beach_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.layers.underwater_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.layers.snow_cap_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer,
        config.layers.snow_cap_minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        config.layers.beach_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        config.layers.underwater_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_block_reference(buffer,
        config.layers.snow_cap_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.enable_biome_transitions);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer,
        config.biome_transition_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.biome_transition_noise_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.enable_mountain_ridges);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u8(config.enable_erosion);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.mountain_ridge_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.mountain_ridge_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_append_i32(buffer, config.erosion_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.append_u32_le(config.erosion_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (buffer.append_u8(config.allow_cross_chunk_features));
}

int32_t terrain_generation_config_deserialize(
    terrain_generation_config &config, ft_byte_buffer &buffer) noexcept
{
    terrain_generation_config loaded_config;
    uint32_t magic;
    uint32_t version;
    uint32_t index;
    uint8_t enable_biome_transitions;
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

    if (buffer.is_initialised() == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_default_generation_config(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.reset_read_position();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&magic);
    if (error_code != FT_ERR_SUCCESS || magic != TERRAIN_SAVE_MAGIC)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.read_u32_le(&version);
    if (error_code != FT_ERR_SUCCESS || version != TERRAIN_SAVE_VERSION)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = terrain_save_read_i32(buffer, &loaded_config.sea_level);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &loaded_config.large_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &loaded_config.detail_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &loaded_config.detail_noise_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.water_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.biome_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.biome_count > TERRAIN_MAX_CUSTOM_BIOMES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        error_code = terrain_save_read_biome(buffer, loaded_config.biomes[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = terrain_generation_config_clear_tree_templates(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.tree_template_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.tree_template_count > TERRAIN_MAX_TREE_TEMPLATES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < loaded_config.tree_template_count)
    {
        error_code = terrain_save_read_template(buffer,
            loaded_config.tree_template_blocks[index],
            &loaded_config.tree_templates[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    index = 0U;
    while (index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        uint32_t template_count;
        uint32_t template_index;
        uint32_t override_index;
        terrain_tree_template_block override_blocks[
            TERRAIN_MAX_TREE_TEMPLATE_BLOCKS];
        terrain_tree_template override_template;

        error_code = buffer.read_u32_le(&template_count);
        if (error_code != FT_ERR_SUCCESS
            || template_count > TERRAIN_MAX_BIOME_TREE_TEMPLATES)
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
        else if (override_index == TERRAIN_MAX_TREE_TEMPLATES)
        {
            error_code = terrain_save_read_template(buffer, override_blocks,
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
        || loaded_config.feature_count > TERRAIN_MAX_FEATURE_RULES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < loaded_config.feature_count)
    {
        int32_t feature_biome;
        int32_t feature_minimum;
        int32_t feature_maximum;
        uint32_t feature_chance;
        uint8_t feature_dry_land;
        uint8_t has_template;
        terrain_tree_template_block feature_blocks[
            TERRAIN_MAX_TREE_TEMPLATE_BLOCKS];
        terrain_tree_template feature_template;
        terrain_feature_rule loaded_feature;

        error_code = loaded_feature.initialize();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);

        error_code = terrain_save_read_i32(buffer, &feature_biome);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_read_i32(buffer, &feature_minimum);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = terrain_save_read_i32(buffer, &feature_maximum);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.read_u32_le(&feature_chance);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.read_u8(&feature_dry_land);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = buffer.read_u8(&has_template);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_biome_range(
            feature_biome, feature_minimum, feature_maximum);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_chance(feature_chance);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = loaded_feature.set_requires_dry_land(
            ft_platform_cast<ft_bool>(feature_dry_land));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (has_template != 0U)
        {
            error_code = terrain_save_read_template(buffer, feature_blocks,
                &feature_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
            error_code = loaded_feature.set_template(
                &feature_template);
            if (error_code != FT_ERR_SUCCESS)
                return (error_code);
        }
        error_code = loaded_config.set_feature(index, loaded_feature);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = buffer.read_u32_le(&loaded_config.ore_rule_count);
    if (error_code != FT_ERR_SUCCESS
        || loaded_config.ore_rule_count > TERRAIN_MAX_ORE_RULES)
        return (FT_ERR_INVALID_ARGUMENT);
    index = 0U;
    while (index < TERRAIN_MAX_ORE_RULES)
    {
        error_code = terrain_save_read_ore(buffer, loaded_config.ores[index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    error_code = buffer.read_u8(&enable_ravines);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_cave_rooms);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&ravine_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_room_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &underground_minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &underground_maximum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&ravine_width);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&ravine_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_small_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_large_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_large_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_entrance_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cave_entrance_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_cavern_rooms);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cavern_room_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&cavern_room_radius);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_rivers);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_lakes);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &river_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &river_width);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &lake_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&lake_chance_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_beaches);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_snow_caps);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&beach_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&underwater_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&snow_cap_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer,
        &snow_cap_minimum_height);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer, &beach_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer,
        &underwater_block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_block_reference(buffer,
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
    error_code = buffer.read_u8(&enable_biome_transitions);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &biome_transition_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&biome_transition_noise_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_mountain_ridges);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&enable_erosion);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
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
    error_code = terrain_save_read_i32(buffer, &loaded_config.mountain_ridge_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.mountain_ridge_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_save_read_i32(buffer, &loaded_config.erosion_noise_scale);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u32_le(&loaded_config.erosion_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = buffer.read_u8(&loaded_config.allow_cross_chunk_features);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_biome_selector(ft_nullptr, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = loaded_config.set_cross_chunk_writer(ft_nullptr, ft_nullptr);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (terrain_generation_config_is_valid(loaded_config) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = config.initialize(loaded_config);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (FT_ERR_SUCCESS);
}

int32_t terrain_generation_config_save_file(const char *file_path,
    const terrain_generation_config &config) noexcept
{
    ft_byte_buffer buffer;
    FILE *file;
    ft_size_t written;
    int32_t error_code;

    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = buffer.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_generation_config_serialize(config, buffer);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)buffer.destroy();
        return (error_code);
    }
    file = fopen(file_path, "wb");
    if (file == ft_nullptr)
    {
        (void)buffer.destroy();
        return (FT_ERR_IO);
    }
    written = fwrite(buffer.data(), 1, buffer.size(), file);
    if (fclose(file) != 0 || written != buffer.size())
        error_code = FT_ERR_IO;
    else
        error_code = FT_ERR_SUCCESS;
    (void)buffer.destroy();
    return (error_code);
}

int32_t terrain_generation_config_load_file(const char *file_path,
    terrain_generation_config &config) noexcept
{
    ft_byte_buffer buffer;
    uint8_t read_data[4096];
    FILE *file;
    ft_size_t read_count;
    int32_t error_code;

    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    file = fopen(file_path, "rb");
    if (file == ft_nullptr)
        return (FT_ERR_IO);
    error_code = buffer.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)fclose(file);
        return (error_code);
    }
    while (true)
    {
        read_count = fread(read_data, 1, sizeof(read_data), file);
        if (read_count > 0U)
        {
            error_code = buffer.append(read_data, read_count);
            if (error_code != FT_ERR_SUCCESS)
                break ;
        }
        if (read_count < sizeof(read_data))
        {
            if (ferror(file) != 0)
                error_code = FT_ERR_IO;
            break ;
        }
    }
    if (fclose(file) != 0 && error_code == FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_generation_config_deserialize(config, buffer);
    (void)buffer.destroy();
    return (error_code);
}

#endif
