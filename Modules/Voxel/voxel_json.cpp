#include "voxel.hpp"
#include "../Advanced/advanced.hpp"
#include "../CMA/CMA.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include <fcntl.h>

static int32_t terrain_json_prepare(ft_string &output,
    const char *type_name) noexcept
{
    int32_t error_code;

    if (output.is_initialised() == FT_TRUE)
        (void)output.destroy();
    error_code = output.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("{\"type\":\"");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(type_name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\"");
    return (error_code);
}

static int32_t terrain_json_add_separator(ft_string &output,
    ft_bool &first) noexcept
{
    int32_t error_code = FT_ERR_SUCCESS;

    if (first == FT_FALSE)
        error_code = output.append(",");
    first = FT_FALSE;
    return (error_code);
}

static int32_t terrain_json_add_i32(ft_string &output, const char *key,
    int32_t value, ft_bool &first) noexcept
{
    char *value_text;
    int32_t error_code = terrain_json_add_separator(output, first);

    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\"");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(key);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\":");
    value_text = adv_itoa(value);
    if (value_text == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(value_text);
    cma_free(value_text);
    return (error_code);
}

static int32_t terrain_json_add_u32(ft_string &output, const char *key,
    uint32_t value, ft_bool &first) noexcept
{
    return (terrain_json_add_i32(output, key, static_cast<int32_t>(value),
        first));
}

static int32_t terrain_json_add_bool(ft_string &output, const char *key,
    ft_bool value, ft_bool &first) noexcept
{
    int32_t error_code = terrain_json_add_separator(output, first);
    const char *value_text = "false";

    if (value == FT_TRUE)
        value_text = "true";
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\"");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(key);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\":");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(value_text);
    return (error_code);
}

static int32_t terrain_json_add_indexed_i32(ft_string &output,
    const char *prefix, uint32_t index, int32_t value,
    ft_bool &first) noexcept
{
    ft_string key;
    char *index_text;
    int32_t error_code = key.initialize(prefix);

    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index_text = adv_itoa(static_cast<int32_t>(index));
    if (index_text == ft_nullptr)
    {
        (void)key.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    error_code = key.append(index_text);
    cma_free(index_text);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_add_i32(output, key.c_str(), value, first);
    (void)key.destroy();
    return (error_code);
}

static int32_t terrain_json_add_indexed_bool(ft_string &output,
    const char *prefix, uint32_t index, ft_bool value,
    ft_bool &first) noexcept
{
    ft_string key;
    char *index_text;
    int32_t error_code = key.initialize(prefix);

    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index_text = adv_itoa(static_cast<int32_t>(index));
    if (index_text == ft_nullptr)
    {
        (void)key.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    error_code = key.append(index_text);
    cma_free(index_text);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_add_bool(output, key.c_str(), value, first);
    (void)key.destroy();
    return (error_code);
}

static int32_t terrain_json_finish(ft_string &output) noexcept
{
    return (output.append("}\n"));
}

static int32_t terrain_json_write_file(const char *file_path,
    const ft_string &output, terrain_json_file_mode mode) noexcept
{
    int32_t flags;
    int32_t file_descriptor;
    ft_size_t offset = 0;
    int64_t write_result;

    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (mode == TERRAIN_JSON_FILE_CREATE_ONLY)
        flags = O_WRONLY | O_CREAT | O_EXCL;
    else if (mode == TERRAIN_JSON_FILE_REPLACE)
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    else if (mode == TERRAIN_JSON_FILE_APPEND)
        flags = O_WRONLY | O_CREAT | O_APPEND;
    else
        return (FT_ERR_INVALID_ARGUMENT);
    file_descriptor = su_open(file_path, flags, 0644);
    if (file_descriptor < 0)
    {
        if (mode == TERRAIN_JSON_FILE_CREATE_ONLY)
            return (FT_ERR_ALREADY_EXISTS);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    while (offset < output.size())
    {
        write_result = su_write(file_descriptor, output.c_str() + offset,
            output.size() - offset);
        if (write_result <= 0)
        {
            (void)su_close(file_descriptor);
            return (FT_ERR_IO);
        }
        offset = offset + static_cast<ft_size_t>(write_result);
    }
    if (su_close(file_descriptor) != FT_ERR_SUCCESS)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

#define TERRAIN_JSON_I32(output, key, value) \
    do { if (error_code == FT_ERR_SUCCESS) \
        error_code = terrain_json_add_i32(output, key, value, first); } while (0)
#define TERRAIN_JSON_U32(output, key, value) \
    do { if (error_code == FT_ERR_SUCCESS) \
        error_code = terrain_json_add_u32(output, key, value, first); } while (0)
#define TERRAIN_JSON_BOOL(output, key, value) \
    do { if (error_code == FT_ERR_SUCCESS) \
        error_code = terrain_json_add_bool(output, key, value, first); } while (0)

static int32_t terrain_json_add_biome(ft_string &output,
    const terrain_biome_definition &value, ft_bool &first) noexcept
{
    int32_t error_code = FT_ERR_SUCCESS;
    TERRAIN_JSON_I32(output, "surface_height", value.profile.surface_height);
    TERRAIN_JSON_I32(output, "height_variation", value.profile.height_variation);
    TERRAIN_JSON_I32(output, "topsoil_depth", value.profile.topsoil_depth);
    TERRAIN_JSON_U32(output, "surface_block_id", value.surface_block_id);
    TERRAIN_JSON_U32(output, "subsurface_block_id", value.subsurface_block_id);
    TERRAIN_JSON_U32(output, "deep_block_id", value.deep_block_id);
    TERRAIN_JSON_BOOL(output, "allow_shrubs", value.allow_shrubs);
    TERRAIN_JSON_BOOL(output, "allow_trees", value.allow_trees);
    TERRAIN_JSON_BOOL(output, "allow_snow_caps", value.allow_snow_caps);
    TERRAIN_JSON_BOOL(output, "allow_mountain_ridges", value.allow_mountain_ridges);
    TERRAIN_JSON_U32(output, "shrub_chance_percent", value.shrub_chance_percent);
    TERRAIN_JSON_U32(output, "tree_chance_percent", value.tree_chance_percent);
    TERRAIN_JSON_U32(output, "tree_template_count", value.tree_template_count);
    return (error_code);
}

int32_t terrain_biome_definition::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_biome_definition");
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_add_biome(output, *this, first);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}

int32_t terrain_biome_definition::save_json_file(const char *file_path,
    terrain_json_file_mode mode) const noexcept
{
    ft_string output;
    int32_t error_code = this->serialize_json(output);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_write_file(file_path, output, mode);
    return (error_code);
}

int32_t terrain_feature_rule::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    ft_bool has_template = FT_FALSE;
    int32_t error_code = terrain_json_prepare(output, "terrain_feature_rule");
    if (this->template_data != ft_nullptr)
        has_template = FT_TRUE;
    TERRAIN_JSON_I32(output, "biome_index", this->biome_index);
    TERRAIN_JSON_U32(output, "chance_percent", this->chance_percent);
    TERRAIN_JSON_I32(output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_BOOL(output, "requires_dry_land", this->requires_dry_land);
    TERRAIN_JSON_BOOL(output, "has_template", has_template);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}

int32_t terrain_feature_rule::save_json_file(const char *file_path,
    terrain_json_file_mode mode) const noexcept
{
    ft_string output;
    int32_t error_code = this->serialize_json(output);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_write_file(file_path, output, mode);
    return (error_code);
}

int32_t terrain_ore_rule::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_ore_rule");
    TERRAIN_JSON_U32(output, "block_id", this->block_id);
    TERRAIN_JSON_I32(output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_U32(output, "vein_size", this->vein_size);
    TERRAIN_JSON_U32(output, "chance_percent", this->chance_percent);
    TERRAIN_JSON_BOOL(output, "allow_ore_replacement", this->allow_ore_replacement);
    TERRAIN_JSON_BOOL(output, "enabled", this->enabled);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}

int32_t terrain_ore_rule::save_json_file(const char *file_path,
    terrain_json_file_mode mode) const noexcept
{
    ft_string output;
    int32_t error_code = this->serialize_json(output);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_write_file(file_path, output, mode);
    return (error_code);
}

#define TERRAIN_JSON_SAVE_ONLY(class_name) \
int32_t class_name::save_json_file(const char *file_path, \
    terrain_json_file_mode mode) const noexcept \
{ ft_string output; int32_t error_code = this->serialize_json(output); \
    if (error_code == FT_ERR_SUCCESS) error_code = terrain_json_write_file( \
        file_path, output, mode); return (error_code); }

int32_t terrain_underground_structure_config::serialize_json(
    ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_underground_structures");
    TERRAIN_JSON_BOOL(output, "enable_ravines", this->enable_ravines);
    TERRAIN_JSON_BOOL(output, "enable_cave_rooms", this->enable_cave_rooms);
    TERRAIN_JSON_U32(output, "ravine_chance_percent", this->ravine_chance_percent);
    TERRAIN_JSON_U32(output, "cave_room_chance_percent", this->cave_room_chance_percent);
    TERRAIN_JSON_I32(output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_U32(output, "ravine_width", this->ravine_width);
    TERRAIN_JSON_U32(output, "ravine_depth", this->ravine_depth);
    TERRAIN_JSON_U32(output, "cave_small_radius", this->cave_small_radius);
    TERRAIN_JSON_U32(output, "cave_large_radius", this->cave_large_radius);
    TERRAIN_JSON_U32(output, "cave_large_chance_percent", this->cave_large_chance_percent);
    TERRAIN_JSON_U32(output, "cave_entrance_chance_percent", this->cave_entrance_chance_percent);
    TERRAIN_JSON_U32(output, "cave_entrance_radius", this->cave_entrance_radius);
    TERRAIN_JSON_BOOL(output, "enable_cavern_rooms", this->enable_cavern_rooms);
    TERRAIN_JSON_U32(output, "cavern_room_chance_percent", this->cavern_room_chance_percent);
    TERRAIN_JSON_U32(output, "cavern_room_radius", this->cavern_room_radius);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}
TERRAIN_JSON_SAVE_ONLY(terrain_underground_structure_config)

int32_t terrain_fluid_config::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_fluids");
    TERRAIN_JSON_BOOL(output, "enable_rivers", this->enable_rivers);
    TERRAIN_JSON_BOOL(output, "enable_lakes", this->enable_lakes);
    TERRAIN_JSON_I32(output, "river_noise_scale", this->river_noise_scale);
    TERRAIN_JSON_I32(output, "river_width", this->river_width);
    TERRAIN_JSON_I32(output, "lake_noise_scale", this->lake_noise_scale);
    TERRAIN_JSON_U32(output, "lake_chance_percent", this->lake_chance_percent);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}
TERRAIN_JSON_SAVE_ONLY(terrain_fluid_config)

int32_t terrain_layer_config::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_layers");
    TERRAIN_JSON_BOOL(output, "enable_beaches", this->enable_beaches);
    TERRAIN_JSON_BOOL(output, "enable_snow_caps", this->enable_snow_caps);
    TERRAIN_JSON_U32(output, "beach_depth", this->beach_depth);
    TERRAIN_JSON_U32(output, "underwater_depth", this->underwater_depth);
    TERRAIN_JSON_U32(output, "snow_cap_depth", this->snow_cap_depth);
    TERRAIN_JSON_I32(output, "snow_cap_minimum_height", this->snow_cap_minimum_height);
    TERRAIN_JSON_U32(output, "beach_block_id", this->beach_block_id);
    TERRAIN_JSON_U32(output, "underwater_block_id", this->underwater_block_id);
    TERRAIN_JSON_U32(output, "snow_cap_block_id", this->snow_cap_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}
TERRAIN_JSON_SAVE_ONLY(terrain_layer_config)

int32_t terrain_generation_config::serialize_json(ft_string &output) const noexcept
{
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(output, "terrain_generation_config");
    TERRAIN_JSON_I32(output, "sea_level", this->sea_level);
    TERRAIN_JSON_I32(output, "large_noise_scale", this->large_noise_scale);
    TERRAIN_JSON_I32(output, "detail_noise_scale", this->detail_noise_scale);
    TERRAIN_JSON_I32(output, "detail_noise_percent", this->detail_noise_percent);
    TERRAIN_JSON_BOOL(output, "enable_biome_size_control", this->enable_biome_size_control);
    TERRAIN_JSON_I32(output, "biome_size_min", this->biome_size_min);
    TERRAIN_JSON_I32(output, "biome_size_max", this->biome_size_max);
    TERRAIN_JSON_U32(output, "water_chance_percent", this->water_chance_percent);
    TERRAIN_JSON_U32(output, "biome_count", this->biome_count);
    TERRAIN_JSON_U32(output, "tree_template_count", this->tree_template_count);
    TERRAIN_JSON_U32(output, "feature_count", this->feature_count);
    TERRAIN_JSON_U32(output, "ore_rule_count", this->ore_rule_count);
    TERRAIN_JSON_BOOL(output, "enable_biome_transitions", this->enable_biome_transitions);
    TERRAIN_JSON_I32(output, "biome_transition_noise_scale", this->biome_transition_noise_scale);
    TERRAIN_JSON_U32(output, "biome_transition_noise_strength", this->biome_transition_noise_strength);
    TERRAIN_JSON_BOOL(output, "enable_mountain_ridges", this->enable_mountain_ridges);
    TERRAIN_JSON_BOOL(output, "enable_erosion", this->enable_erosion);
    TERRAIN_JSON_I32(output, "mountain_ridge_scale", this->mountain_ridge_scale);
    TERRAIN_JSON_U32(output, "mountain_ridge_strength", this->mountain_ridge_strength);
    TERRAIN_JSON_I32(output, "erosion_noise_scale", this->erosion_noise_scale);
    TERRAIN_JSON_U32(output, "erosion_strength", this->erosion_strength);
    TERRAIN_JSON_BOOL(output, "allow_cross_chunk_features", this->allow_cross_chunk_features);
    uint32_t index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        error_code = terrain_json_add_indexed_i32(output,
            "biome_size_min_by_biome_", index,
            this->biome_size_min_by_biome[index], first);
        if (error_code == FT_ERR_SUCCESS)
            error_code = terrain_json_add_indexed_i32(output,
                "biome_size_max_by_biome_", index,
                this->biome_size_max_by_biome[index], first);
        if (error_code == FT_ERR_SUCCESS)
            error_code = terrain_json_add_indexed_bool(output,
                "biome_size_override_enabled_", index,
                this->biome_size_override_enabled[index], first);
        index = index + 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(output);
    return (error_code);
}
TERRAIN_JSON_SAVE_ONLY(terrain_generation_config)

#undef TERRAIN_JSON_SAVE_ONLY
#undef TERRAIN_JSON_I32
#undef TERRAIN_JSON_U32
#undef TERRAIN_JSON_BOOL
