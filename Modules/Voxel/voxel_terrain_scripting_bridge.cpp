#include "terrain_scripting_bridge.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Template/function.hpp"
#include "../Template/vector.hpp"

struct terrain_script_execution_context
{
    game_voxel_chunk *chunk;
    terrain_generation_config *config;
    const char *seed_string;
    int32_t world_block_origin_x;
    int32_t world_block_origin_z;
};

static terrain_script_execution_context *terrain_script_get_context(
    game_script_context &context) noexcept
{
    return (static_cast<terrain_script_execution_context *>(
        context.get_user_data()));
}

static int32_t terrain_script_parse_int32(const ft_string &argument,
    int32_t *value) noexcept
{
    int64_t parsed_value;
    char *end_pointer;
    int32_t error_code;

    if (value == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    end_pointer = ft_nullptr;
    error_code = ft_parse_int64(argument.c_str(), &end_pointer, &parsed_value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (end_pointer == ft_nullptr || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    if (parsed_value < INT32_MIN || parsed_value > INT32_MAX)
        return (FT_ERR_OUT_OF_RANGE);
    *value = static_cast<int32_t>(parsed_value);
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_script_parse_uint32(const ft_string &argument,
    uint32_t *value) noexcept
{
    char *end_pointer;
    int32_t error_code;

    if (value == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    end_pointer = ft_nullptr;
    error_code = ft_parse_uint32(argument.c_str(), &end_pointer, value);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (end_pointer == ft_nullptr || *end_pointer != '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_script_set_sea_level(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    int32_t sea_level;
    int32_t error_code;

    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_int32(arguments[0], &sea_level);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->config->set_sea_level(sea_level));
}

static int32_t terrain_script_set_noise_scales(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    int32_t large_scale;
    int32_t detail_scale;
    int32_t detail_percent;
    int32_t error_code;

    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_int32(arguments[0], &large_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[1], &detail_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[2], &detail_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->config->set_noise_scales(large_scale,
        detail_scale, detail_percent));
}

static int32_t terrain_script_set_biome_height(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    uint32_t biome_index;
    int32_t surface_height;
    int32_t height_variation;
    int32_t topsoil_depth;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_uint32(arguments[0], &biome_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[1], &surface_height);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[2], &height_variation);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[3], &topsoil_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->config->set_biome_height_profile(biome_index,
        surface_height, height_variation, topsoil_depth));
}

static int32_t terrain_script_set_biome_blocks(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    uint32_t biome_index;
    uint32_t surface_block;
    uint32_t subsurface_block;
    uint32_t deep_block;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_uint32(arguments[0], &biome_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_uint32(arguments[1], &surface_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_uint32(arguments[2], &subsurface_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_uint32(arguments[3], &deep_block);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->config->set_biome_block_palette(biome_index,
        surface_block, subsurface_block, deep_block));
}

static int32_t terrain_script_set_biome_transitions(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    uint32_t enabled;
    int32_t noise_scale;
    uint32_t noise_strength;
    int32_t error_code;

    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_uint32(arguments[0], &enabled);
    if (error_code == FT_ERR_SUCCESS && enabled > 1U)
        error_code = FT_ERR_INVALID_ARGUMENT;
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[1], &noise_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_uint32(arguments[2], &noise_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = terrain_context->config->set_biome_transitions_enabled(
        static_cast<ft_bool>(enabled));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->config->set_biome_transition_settings(
        noise_scale, noise_strength));
}

static int32_t terrain_script_generate_chunk(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;

    if (arguments.size() != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->chunk == ft_nullptr
        || terrain_context->config == ft_nullptr
        || terrain_context->seed_string == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (terrain_generate_chunk(*terrain_context->chunk,
        terrain_context->world_block_origin_x,
        terrain_context->world_block_origin_z,
        terrain_context->seed_string, *terrain_context->config));
}

static int32_t terrain_script_write_generated_block(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_script_execution_context *terrain_context;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context = terrain_script_get_context(context);
    if (terrain_context == ft_nullptr || terrain_context->chunk == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = terrain_script_parse_int32(arguments[0], &local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[1], &local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_int32(arguments[2], &local_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_parse_uint32(arguments[3], &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (terrain_context->chunk->write_generated_block(local_x, local_y,
        local_z, block_id));
}

static int32_t terrain_script_register_block(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    terrain_block_registration registration;
    uint32_t metadata_values[7];
    uint32_t block_id;
    uint32_t argument_index;
    uint32_t asset_index;
    int32_t error_code;

    if (arguments.size() != 15U)
        return (FT_ERR_INVALID_ARGUMENT);
    registration.name = arguments[0].c_str();
    argument_index = 0U;
    while (argument_index < 7U)
    {
        error_code = terrain_script_parse_uint32(
            arguments[argument_index + 1U], &metadata_values[argument_index]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (argument_index != 6U && metadata_values[argument_index] > 1U)
            return (FT_ERR_INVALID_ARGUMENT);
        argument_index += 1U;
    }
    registration.metadata.solid = static_cast<ft_bool>(metadata_values[0]);
    registration.metadata.transparent = static_cast<ft_bool>(metadata_values[1]);
    registration.metadata.liquid = static_cast<ft_bool>(metadata_values[2]);
    registration.metadata.replaceable = static_cast<ft_bool>(metadata_values[3]);
    registration.metadata.light_emitting = static_cast<ft_bool>(metadata_values[4]);
    registration.metadata.occludes_faces = static_cast<ft_bool>(metadata_values[5]);
    registration.metadata.hardness = metadata_values[6];
    error_code = terrain_script_parse_uint32(arguments[8], &metadata_values[0]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (metadata_values[0] > 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    registration.metadata.breakable = static_cast<ft_bool>(metadata_values[0]);
    asset_index = 0U;
    while (asset_index < TERRAIN_BLOCK_ASSET_FACE_COUNT)
    {
        registration.asset_paths[asset_index] = arguments[9U + asset_index].c_str();
        asset_index += 1U;
    }
    error_code = terrain_register_block(registration, &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    context.set_result_integer(static_cast<int64_t>(block_id));
    return (FT_ERR_SUCCESS);
}

static int32_t terrain_script_register_function(game_script_bridge &bridge,
    const char *name, int32_t (*callback)(game_script_context &,
        const ft_vector<ft_string> &)) noexcept
{
    ft_string function_name;
    ft_function<int32_t(game_script_context &,
        const ft_vector<ft_string> &)> function(callback);
    int32_t error_code;

    error_code = function_name.initialize(name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (!function)
        return (FT_ERR_NO_MEMORY);
    return (bridge.register_function(function_name, function));
}

int32_t terrain_script_register_api(game_script_bridge &bridge) noexcept
{
    int32_t error_code;

    error_code = terrain_script_register_function(bridge,
        "terrain_set_sea_level", terrain_script_set_sea_level);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_set_noise_scales", terrain_script_set_noise_scales);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_set_biome_height", terrain_script_set_biome_height);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_set_biome_blocks", terrain_script_set_biome_blocks);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_set_biome_transitions",
            terrain_script_set_biome_transitions);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_generate_chunk", terrain_script_generate_chunk);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_write_generated_block",
            terrain_script_write_generated_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = terrain_script_register_function(bridge,
            "terrain_register_block", terrain_script_register_block);
    return (error_code);
}

int32_t terrain_script_execute(game_script_bridge &bridge,
    const ft_string &script, game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, terrain_generation_config &config) noexcept
{
    terrain_script_execution_context terrain_context;

    if (seed_string == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    terrain_context.chunk = &chunk;
    terrain_context.config = &config;
    terrain_context.seed_string = seed_string;
    terrain_context.world_block_origin_x = world_block_origin_x;
    terrain_context.world_block_origin_z = world_block_origin_z;
    return (bridge.execute_lua_with_user_data(script, ft_nullptr,
        &terrain_context));
}

#endif
