#include "voxel_scripting_bridge.hpp"
#include "voxel_api.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Template/function.hpp"
#include "../Template/vector.hpp"
#include "../File/file_utils.hpp"

struct voxel_script_execution_context
{
    game_voxel_chunk *chunk;
    voxel_generation_config *config;
    const char *seed_string;
    const char *asset_root;
    int32_t world_block_origin_x;
    int32_t world_block_origin_z;
};

static int32_t voxel_script_normalize_custom_source(
    const ft_string &script, ft_string &normalized) noexcept
{
    const char *data;
    ft_size_t index;
    uint32_t parenthesis_depth;
    int32_t error_code;

    error_code = normalized.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    data = script.c_str();
    index = 0U;
    parenthesis_depth = 0U;
    while (index < script.size())
    {
        if (data[index] == '\n' || data[index] == '\r')
        {
            if (data[index] == '\n' && index > 0U
                && data[index - 1U] == '\r')
            {
                index += 1U;
                continue ;
            }
            if (parenthesis_depth == 0U
                && (index == 0U || data[index - 1U] != ';'))
                error_code = normalized.append(';');
        }
        else
        {
            error_code = normalized.append(data[index]);
            if (data[index] == '(')
                parenthesis_depth += 1U;
            else if (data[index] == ')' && parenthesis_depth > 0U)
                parenthesis_depth -= 1U;
        }
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

static voxel_script_execution_context *voxel_script_get_context(
    game_script_context &context) noexcept
{
    return (static_cast<voxel_script_execution_context *>(
        context.get_user_data()));
}

static int32_t voxel_script_parse_int32(const ft_string &argument,
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

static int32_t voxel_script_parse_uint32(const ft_string &argument,
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

static int32_t voxel_script_set_sea_level(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    int32_t sea_level;
    int32_t error_code;

    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_int32(arguments[0], &sea_level);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_sea_level(sea_level));
}

static int32_t voxel_script_set_noise_scales(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    int32_t large_scale;
    int32_t detail_scale;
    int32_t detail_percent;
    int32_t error_code;

    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_int32(arguments[0], &large_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &detail_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[2], &detail_percent);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_noise_scales(large_scale,
        detail_scale, detail_percent));
}

static int32_t voxel_script_set_biome_size_control(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    uint32_t enabled;
    int32_t error_code;

    if (arguments.size() != 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_uint32(arguments[0], &enabled);
    if (error_code != FT_ERR_SUCCESS || enabled > 1U)
        return (error_code == FT_ERR_SUCCESS ? FT_ERR_INVALID_ARGUMENT
            : error_code);
    return (voxel_context->config->set_biome_size_control_enabled(
        static_cast<ft_bool>(enabled)));
}

static int32_t voxel_script_set_biome_size_range(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    int32_t minimum_size;
    int32_t maximum_size;
    int32_t error_code;

    if (arguments.size() != 2U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_int32(arguments[0], &minimum_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &maximum_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_biome_size_range(minimum_size,
        maximum_size));
}

static int32_t voxel_script_set_biome_size_range_for_biome(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    uint32_t biome_index;
    int32_t minimum_size;
    int32_t maximum_size;
    int32_t error_code;

    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_uint32(arguments[0], &biome_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &minimum_size);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[2], &maximum_size);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_biome_size_range_for_biome(
        biome_index, minimum_size, maximum_size));
}

static int32_t voxel_script_set_biome_height(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    uint32_t biome_index;
    int32_t surface_height;
    int32_t height_variation;
    int32_t topsoil_depth;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_uint32(arguments[0], &biome_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &surface_height);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[2], &height_variation);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[3], &topsoil_depth);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_biome_height_profile(biome_index,
        surface_height, height_variation, topsoil_depth));
}

static int32_t voxel_script_set_biome_blocks(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    uint32_t biome_index;
    uint32_t surface_block;
    uint32_t subsurface_block;
    uint32_t deep_block;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_uint32(arguments[0], &biome_index);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_uint32(arguments[1], &surface_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_uint32(arguments[2], &subsurface_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_uint32(arguments[3], &deep_block);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_biome_block_palette(biome_index,
        surface_block, subsurface_block, deep_block));
}

static int32_t voxel_script_set_biome_transitions(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    uint32_t enabled;
    int32_t noise_scale;
    uint32_t noise_strength;
    int32_t error_code;

    if (arguments.size() != 3U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->config == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_uint32(arguments[0], &enabled);
    if (error_code == FT_ERR_SUCCESS && enabled > 1U)
        error_code = FT_ERR_INVALID_ARGUMENT;
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &noise_scale);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_uint32(arguments[2], &noise_strength);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = voxel_context->config->set_biome_transitions_enabled(
        static_cast<ft_bool>(enabled));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->config->set_biome_transition_settings(
        noise_scale, noise_strength));
}

static int32_t voxel_script_generate_chunk(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;

    if (arguments.size() != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->chunk == ft_nullptr
        || voxel_context->config == ft_nullptr
        || voxel_context->seed_string == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    return (voxel_generate_chunk(*voxel_context->chunk,
        voxel_context->world_block_origin_x,
        voxel_context->world_block_origin_z,
        voxel_context->seed_string, *voxel_context->config));
}

static int32_t voxel_script_write_generated_block(
    game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_script_execution_context *voxel_context;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;
    uint32_t block_id;
    int32_t error_code;

    if (arguments.size() != 4U)
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->chunk == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    error_code = voxel_script_parse_int32(arguments[0], &local_x);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[1], &local_y);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_int32(arguments[2], &local_z);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_parse_uint32(arguments[3], &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (voxel_context->chunk->write_generated_block(local_x, local_y,
        local_z, block_id));
}

static int32_t voxel_script_register_block(game_script_context &context,
    const ft_vector<ft_string> &arguments) noexcept
{
    voxel_block_registration registration;
    uint32_t metadata_values[9];
    uint32_t block_id;
    uint32_t argument_index;
    uint32_t asset_index;
    int32_t error_code;
    voxel_script_execution_context *voxel_context;

    if (arguments.size() != 15U && arguments.size() != 17U)
        return (FT_ERR_INVALID_ARGUMENT);
    registration.name = arguments[0].c_str();
    argument_index = 0U;
    while (argument_index < 7U)
    {
        error_code = voxel_script_parse_uint32(
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
    if (arguments.size() == 17U)
    {
        error_code = voxel_script_parse_uint32(arguments[8],
            &metadata_values[7]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = voxel_script_parse_uint32(arguments[9],
            &metadata_values[8]);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (metadata_values[7] > 1U || metadata_values[8] > 1U)
            return (FT_ERR_INVALID_ARGUMENT);
        registration.metadata.can_host_ore = static_cast<ft_bool>(
            metadata_values[7]);
        registration.metadata.is_ore = static_cast<ft_bool>(
            metadata_values[8]);
        argument_index = 10U;
    }
    else
    {
        registration.metadata.can_host_ore = static_cast<ft_bool>(
            registration.metadata.solid == FT_TRUE
            && registration.metadata.replaceable == FT_FALSE);
        registration.metadata.is_ore = FT_FALSE;
        argument_index = 8U;
    }
    error_code = voxel_script_parse_uint32(arguments[argument_index],
        &metadata_values[0]);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (metadata_values[0] > 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    registration.metadata.breakable = static_cast<ft_bool>(metadata_values[0]);
    argument_index += 1U;
    asset_index = 0U;
    while (asset_index < VOXEL_BLOCK_ASSET_FACE_COUNT)
    {
        registration.asset_paths[asset_index] = arguments[
            argument_index + asset_index].c_str();
        asset_index += 1U;
    }
    voxel_context = voxel_script_get_context(context);
    if (voxel_context == ft_nullptr || voxel_context->asset_root == ft_nullptr)
        return (FT_ERR_INVALID_PATH);
    error_code = voxel_register_block_from_root(registration,
        voxel_context->asset_root, &block_id);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    context.set_result_integer(static_cast<int64_t>(block_id));
    return (FT_ERR_SUCCESS);
}

static int32_t voxel_script_register_function(game_script_bridge &bridge,
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

int32_t voxel_script_register_api(game_script_bridge &bridge) noexcept
{
    int32_t error_code;

    error_code = voxel_script_register_function(bridge,
        "voxel_set_sea_level", voxel_script_set_sea_level);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_noise_scales", voxel_script_set_noise_scales);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_size_control",
            voxel_script_set_biome_size_control);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_size_range",
            voxel_script_set_biome_size_range);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_size_range_for_biome",
            voxel_script_set_biome_size_range_for_biome);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_height", voxel_script_set_biome_height);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_blocks", voxel_script_set_biome_blocks);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_set_biome_transitions",
            voxel_script_set_biome_transitions);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_generate_chunk", voxel_script_generate_chunk);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_write_generated_block",
            voxel_script_write_generated_block);
    if (error_code == FT_ERR_SUCCESS)
        error_code = voxel_script_register_function(bridge,
            "voxel_register_block", voxel_script_register_block);
    return (error_code);
}

int32_t voxel_script_execute(game_script_bridge &bridge,
    const ft_string &script, game_voxel_chunk &chunk,
    int32_t world_block_origin_x, int32_t world_block_origin_z,
    const char *seed_string, voxel_generation_config &config,
    const char *asset_root) noexcept
{
    voxel_script_execution_context voxel_context;
    ft_string normalized_script;
    int32_t normalize_error;

    if (seed_string == ft_nullptr || asset_root == ft_nullptr
        || asset_root[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    voxel_context.chunk = &chunk;
    voxel_context.config = &config;
    voxel_context.seed_string = seed_string;
    voxel_context.asset_root = asset_root;
    voxel_context.world_block_origin_x = world_block_origin_x;
    voxel_context.world_block_origin_z = world_block_origin_z;
    normalize_error = voxel_script_normalize_custom_source(script,
        normalized_script);
    if (normalize_error != FT_ERR_SUCCESS)
        return (normalize_error);
    return (bridge.execute_custom_with_user_data(normalized_script,
        ft_nullptr, &voxel_context));
}

#endif
