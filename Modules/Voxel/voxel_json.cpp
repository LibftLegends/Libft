#include "terrain_api.hpp"
#include "../Advanced/advanced.hpp"
#include "../CMA/CMA.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <fcntl.h>

#ifdef LIBFT_TEST_BUILD
struct terrain_json_test_file_failure_state
{
    terrain_json_test_file_operation operation;
    int32_t error_code;
    ft_size_t partial_write_after;
};

static thread_local terrain_json_test_file_failure_state
    g_terrain_json_test_file_failure = {
        static_cast<terrain_json_test_file_operation>(0), FT_ERR_SUCCESS, 0U};

int32_t terrain_json_test_fail_file_operation(
    terrain_json_test_file_operation operation, int32_t error_code,
    ft_size_t partial_write_after) noexcept
{
    if (operation < TERRAIN_JSON_TEST_FILE_OPEN_CREATE_ONLY
        || operation > TERRAIN_JSON_TEST_FILE_CLOSE
        || error_code == FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (operation != TERRAIN_JSON_TEST_FILE_PARTIAL_WRITE
        && partial_write_after != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    g_terrain_json_test_file_failure.operation = operation;
    g_terrain_json_test_file_failure.error_code = error_code;
    g_terrain_json_test_file_failure.partial_write_after = partial_write_after;
    return (FT_ERR_SUCCESS);
}

void terrain_json_test_clear_file_failure(void) noexcept
{
    g_terrain_json_test_file_failure.operation =
        static_cast<terrain_json_test_file_operation>(0);
    g_terrain_json_test_file_failure.error_code = FT_ERR_SUCCESS;
    g_terrain_json_test_file_failure.partial_write_after = 0U;
    return ;
}

static ft_bool terrain_json_test_take_file_failure(
    terrain_json_test_file_operation operation, int32_t &error_code,
    ft_size_t &partial_write_after) noexcept
{
    if (g_terrain_json_test_file_failure.operation != operation)
        return (FT_FALSE);
    error_code = g_terrain_json_test_file_failure.error_code;
    partial_write_after = g_terrain_json_test_file_failure.partial_write_after;
    terrain_json_test_clear_file_failure();
    return (FT_TRUE);
}
#endif

static int32_t terrain_json_prepare(ft_string &output,
    const char *type_name) noexcept
{
    int32_t error_code;

    error_code = output.initialize();
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("{\"type\":\"");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(type_name);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\"");
    return (error_code);
}

static int32_t terrain_json_commit(ft_string &output,
    ft_string &staging_output, int32_t error_code) noexcept
{
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)staging_output.destroy();
        return (error_code);
    }
    error_code = output.move(staging_output);
    if (error_code != FT_ERR_SUCCESS)
        (void)staging_output.destroy();
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
    ft_string *value_text;
    int32_t error_code = terrain_json_add_separator(output, first);

    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\"");
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(key);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append("\":");
    value_text = adv_to_string(value);
    if (value_text == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (error_code == FT_ERR_SUCCESS)
        error_code = output.append(value_text->c_str());
    delete value_text;
    return (error_code);
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
    int32_t error_code = key.initialize(prefix);

    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    {
        ft_string *index_string = adv_to_string(index);

        if (index_string == ft_nullptr)
        {
            (void)key.destroy();
            return (FT_ERR_NO_MEMORY);
        }
        error_code = key.append(index_string->c_str());
        delete index_string;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key.destroy();
        return (error_code);
    }
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
    int32_t error_code = key.initialize(prefix);

    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    {
        ft_string *index_string = adv_to_string(index);

        if (index_string == ft_nullptr)
        {
            (void)key.destroy();
            return (FT_ERR_NO_MEMORY);
        }
        error_code = key.append(index_string->c_str());
        delete index_string;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)key.destroy();
        return (error_code);
    }
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
    int32_t system_error;
    int32_t mapped_error;
#ifdef LIBFT_TEST_BUILD
    int32_t injected_error;
    ft_size_t injected_partial_after;
    terrain_json_test_file_operation test_operation;
#endif

    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (mode == TERRAIN_JSON_FILE_CREATE_ONLY)
        flags = O_WRONLY | O_CREAT | O_EXCL;
    else if (mode == TERRAIN_JSON_FILE_REPLACE)
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    else
        return (FT_ERR_INVALID_ARGUMENT);
#ifdef LIBFT_TEST_BUILD
    test_operation = TERRAIN_JSON_TEST_FILE_OPEN_REPLACE;
    if (mode == TERRAIN_JSON_FILE_CREATE_ONLY)
        test_operation = TERRAIN_JSON_TEST_FILE_OPEN_CREATE_ONLY;
    if (terrain_json_test_take_file_failure(test_operation, injected_error,
            injected_partial_after) == FT_TRUE)
        return (injected_error);
#endif
    file_descriptor = su_open(file_path, flags, 0644);
    if (file_descriptor < 0)
    {
        system_error = cmp_get_last_open_error();
        mapped_error = cmp_map_system_error_to_ft(system_error);
        if (mapped_error != FT_ERR_SUCCESS)
            return (mapped_error);
        return (FT_ERR_FILE_OPEN_FAILED);
    }
    while (offset < output.size())
    {
#ifdef LIBFT_TEST_BUILD
        if (terrain_json_test_take_file_failure(
                TERRAIN_JSON_TEST_FILE_FIRST_WRITE, injected_error,
                injected_partial_after) == FT_TRUE)
        {
            (void)su_close(file_descriptor);
            return (injected_error);
        }
        if (terrain_json_test_take_file_failure(
                TERRAIN_JSON_TEST_FILE_INTERRUPTED_WRITE, injected_error,
                injected_partial_after) == FT_TRUE)
        {
            (void)su_close(file_descriptor);
            return (injected_error);
        }
        if (g_terrain_json_test_file_failure.operation
            == TERRAIN_JSON_TEST_FILE_PARTIAL_WRITE)
        {
            injected_partial_after =
                g_terrain_json_test_file_failure.partial_write_after;
            injected_error = g_terrain_json_test_file_failure.error_code;
            terrain_json_test_clear_file_failure();
            if (injected_partial_after <= offset)
            {
                (void)su_close(file_descriptor);
                return (injected_error);
            }
            ft_size_t partial_count = injected_partial_after - offset;
            if (partial_count > output.size() - offset)
                partial_count = output.size() - offset;
            if (su_write(file_descriptor, output.c_str() + offset,
                    partial_count) != static_cast<int64_t>(partial_count))
            {
                (void)su_close(file_descriptor);
                return (injected_error);
            }
            (void)su_close(file_descriptor);
            return (injected_error);
        }
#endif
        write_result = su_write(file_descriptor, output.c_str() + offset,
            output.size() - offset);
        if (write_result <= 0)
        {
            (void)su_close(file_descriptor);
            return (FT_ERR_IO);
        }
        offset = offset + static_cast<ft_size_t>(write_result);
    }
#ifdef LIBFT_TEST_BUILD
    if (terrain_json_test_take_file_failure(
            TERRAIN_JSON_TEST_FILE_CLOSE, injected_error,
            injected_partial_after) == FT_TRUE)
    {
        (void)su_close(file_descriptor);
        return (injected_error);
    }
#endif
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
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_biome_definition");
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_add_biome(staging_output, *this, first);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
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
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    ft_bool has_template = FT_FALSE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_feature_rule");
    if (this->template_data != ft_nullptr)
        has_template = FT_TRUE;
    TERRAIN_JSON_I32(staging_output, "biome_index", this->biome_index);
    TERRAIN_JSON_U32(staging_output, "chance_percent", this->chance_percent);
    TERRAIN_JSON_I32(staging_output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(staging_output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_BOOL(staging_output, "requires_dry_land", this->requires_dry_land);
    TERRAIN_JSON_BOOL(staging_output, "has_template", has_template);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
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
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_ore_rule");
    TERRAIN_JSON_U32(staging_output, "block_id", this->block_id);
    TERRAIN_JSON_I32(staging_output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(staging_output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_I32(staging_output, "minimum_depth", this->minimum_depth);
    TERRAIN_JSON_I32(staging_output, "maximum_depth", this->maximum_depth);
    TERRAIN_JSON_U32(staging_output, "vein_size", this->vein_size);
    TERRAIN_JSON_U32(staging_output, "vein_size_min", this->vein_size_min);
    TERRAIN_JSON_U32(staging_output, "vein_size_max", this->vein_size_max);
    TERRAIN_JSON_U32(staging_output, "veins_per_chunk_min",
        this->veins_per_chunk_min);
    TERRAIN_JSON_U32(staging_output, "veins_per_chunk_max",
        this->veins_per_chunk_max);
    TERRAIN_JSON_U32(staging_output, "chance_percent", this->chance_percent);
    TERRAIN_JSON_BOOL(staging_output, "allow_ore_replacement", this->allow_ore_replacement);
    TERRAIN_JSON_BOOL(staging_output, "enabled", this->enabled);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
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
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_underground_structures");
    TERRAIN_JSON_BOOL(staging_output, "enable_ravines", this->enable_ravines);
    TERRAIN_JSON_BOOL(staging_output, "enable_cave_rooms", this->enable_cave_rooms);
    TERRAIN_JSON_U32(staging_output, "ravine_chance_percent", this->ravine_chance_percent);
    TERRAIN_JSON_U32(staging_output, "cave_room_chance_percent", this->cave_room_chance_percent);
    TERRAIN_JSON_I32(staging_output, "minimum_height", this->minimum_height);
    TERRAIN_JSON_I32(staging_output, "maximum_height", this->maximum_height);
    TERRAIN_JSON_U32(staging_output, "ravine_width", this->ravine_width);
    TERRAIN_JSON_U32(staging_output, "ravine_depth", this->ravine_depth);
    TERRAIN_JSON_U32(staging_output, "cave_small_radius", this->cave_small_radius);
    TERRAIN_JSON_U32(staging_output, "cave_large_radius", this->cave_large_radius);
    TERRAIN_JSON_U32(staging_output, "cave_large_chance_percent", this->cave_large_chance_percent);
    TERRAIN_JSON_U32(staging_output, "cave_entrance_chance_percent", this->cave_entrance_chance_percent);
    TERRAIN_JSON_U32(staging_output, "cave_entrance_radius", this->cave_entrance_radius);
    TERRAIN_JSON_BOOL(staging_output, "enable_cavern_rooms", this->enable_cavern_rooms);
    TERRAIN_JSON_U32(staging_output, "cavern_room_chance_percent", this->cavern_room_chance_percent);
    TERRAIN_JSON_U32(staging_output, "cavern_room_radius", this->cavern_room_radius);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
}
TERRAIN_JSON_SAVE_ONLY(terrain_underground_structure_config)

int32_t terrain_fluid_config::serialize_json(ft_string &output) const noexcept
{
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_fluids");
    TERRAIN_JSON_BOOL(staging_output, "enable_rivers", this->enable_rivers);
    TERRAIN_JSON_BOOL(staging_output, "enable_lakes", this->enable_lakes);
    TERRAIN_JSON_BOOL(staging_output, "enable_underground_lakes", this->enable_underground_lakes);
    TERRAIN_JSON_I32(staging_output, "river_noise_scale", this->river_noise_scale);
    TERRAIN_JSON_I32(staging_output, "river_width", this->river_width);
    TERRAIN_JSON_I32(staging_output, "lake_noise_scale", this->lake_noise_scale);
    TERRAIN_JSON_U32(staging_output, "lake_chance_percent", this->lake_chance_percent);
    TERRAIN_JSON_U32(staging_output, "underground_lake_chance_percent", this->underground_lake_chance_percent);
    TERRAIN_JSON_I32(staging_output, "underground_lake_minimum_y", this->underground_lake_minimum_y);
    TERRAIN_JSON_I32(staging_output, "underground_lake_maximum_y", this->underground_lake_maximum_y);
    TERRAIN_JSON_U32(staging_output, "underground_lake_depth", this->underground_lake_depth);
    TERRAIN_JSON_U32(staging_output, "underground_lake_floor_thickness", this->underground_lake_floor_thickness);
    TERRAIN_JSON_U32(staging_output, "underground_lake_roof_thickness", this->underground_lake_roof_thickness);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
}
TERRAIN_JSON_SAVE_ONLY(terrain_fluid_config)

int32_t terrain_layer_config::serialize_json(ft_string &output) const noexcept
{
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_layers");
    TERRAIN_JSON_BOOL(staging_output, "enable_beaches", this->enable_beaches);
    TERRAIN_JSON_BOOL(staging_output, "enable_snow_caps", this->enable_snow_caps);
    TERRAIN_JSON_U32(staging_output, "beach_depth", this->beach_depth);
    TERRAIN_JSON_U32(staging_output, "underwater_depth", this->underwater_depth);
    TERRAIN_JSON_U32(staging_output, "snow_cap_depth", this->snow_cap_depth);
    TERRAIN_JSON_I32(staging_output, "snow_cap_minimum_height", this->snow_cap_minimum_height);
    TERRAIN_JSON_U32(staging_output, "beach_block_id", this->beach_block_id);
    TERRAIN_JSON_U32(staging_output, "underwater_block_id", this->underwater_block_id);
    TERRAIN_JSON_U32(staging_output, "snow_cap_block_id", this->snow_cap_block_id);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
}
TERRAIN_JSON_SAVE_ONLY(terrain_layer_config)

int32_t terrain_generation_config::serialize_json(ft_string &output) const noexcept
{
    ft_string staging_output;
    ft_bool first = FT_TRUE;
    int32_t error_code = terrain_json_prepare(staging_output,
        "terrain_generation_config");
    TERRAIN_JSON_I32(staging_output, "sea_level", this->sea_level);
    TERRAIN_JSON_I32(staging_output, "large_noise_scale", this->large_noise_scale);
    TERRAIN_JSON_I32(staging_output, "detail_noise_scale", this->detail_noise_scale);
    TERRAIN_JSON_I32(staging_output, "detail_noise_percent", this->detail_noise_percent);
    TERRAIN_JSON_BOOL(staging_output, "enable_biome_size_control", this->enable_biome_size_control);
    TERRAIN_JSON_I32(staging_output, "biome_size_min", this->biome_size_min);
    TERRAIN_JSON_I32(staging_output, "biome_size_max", this->biome_size_max);
    TERRAIN_JSON_U32(staging_output, "water_chance_percent", this->water_chance_percent);
    TERRAIN_JSON_U32(staging_output, "biome_count", this->biome_count);
    TERRAIN_JSON_U32(staging_output, "tree_template_count", this->tree_template_count);
    TERRAIN_JSON_U32(staging_output, "feature_count", this->feature_count);
    TERRAIN_JSON_U32(staging_output, "ore_rule_count", this->ore_rule_count);
    TERRAIN_JSON_BOOL(staging_output, "enable_biome_transitions", this->enable_biome_transitions);
    TERRAIN_JSON_I32(staging_output, "biome_transition_noise_scale", this->biome_transition_noise_scale);
    TERRAIN_JSON_U32(staging_output, "biome_transition_noise_strength", this->biome_transition_noise_strength);
    TERRAIN_JSON_BOOL(staging_output, "enable_mountain_ridges", this->enable_mountain_ridges);
    TERRAIN_JSON_BOOL(staging_output, "enable_erosion", this->enable_erosion);
    TERRAIN_JSON_I32(staging_output, "mountain_ridge_scale", this->mountain_ridge_scale);
    TERRAIN_JSON_U32(staging_output, "mountain_ridge_strength", this->mountain_ridge_strength);
    TERRAIN_JSON_I32(staging_output, "erosion_noise_scale", this->erosion_noise_scale);
    TERRAIN_JSON_U32(staging_output, "erosion_strength", this->erosion_strength);
    TERRAIN_JSON_BOOL(staging_output, "allow_cross_chunk_features", this->allow_cross_chunk_features);
    uint32_t index = 0U;
    while (error_code == FT_ERR_SUCCESS
        && index < TERRAIN_MAX_CUSTOM_BIOMES)
    {
        error_code = terrain_json_add_indexed_i32(staging_output,
            "biome_size_min_by_biome_", index,
            this->biome_size_min_by_biome[index], first);
        if (error_code == FT_ERR_SUCCESS)
            error_code = terrain_json_add_indexed_i32(staging_output,
                "biome_size_max_by_biome_", index,
                this->biome_size_max_by_biome[index], first);
        if (error_code == FT_ERR_SUCCESS)
            error_code = terrain_json_add_indexed_bool(staging_output,
                "biome_size_override_enabled_", index,
                this->biome_size_override_enabled[index], first);
        index = index + 1U;
    }
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_json_finish(staging_output);
    return (terrain_json_commit(output, staging_output, error_code));
}
TERRAIN_JSON_SAVE_ONLY(terrain_generation_config)

#undef TERRAIN_JSON_SAVE_ONLY
#undef TERRAIN_JSON_I32
#undef TERRAIN_JSON_U32
#undef TERRAIN_JSON_BOOL
