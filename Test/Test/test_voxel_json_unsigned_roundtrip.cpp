#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Voxel/voxel_api.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Advanced/advanced.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

typedef int32_t (*voxel_json_roundtrip_serializer)(void *instance,
    ft_string &output) noexcept;
typedef void (*voxel_json_roundtrip_setter)(void *instance,
    uint32_t value) noexcept;

static int32_t voxel_json_roundtrip_parse_u32(const char *decimal,
    uint32_t *value_out) noexcept
{
    uint32_t value = 0U;
    ft_size_t index = 0U;

    if (decimal == ft_nullptr || value_out == ft_nullptr
        || decimal[0] == '\0')
        return (FT_ERR_INVALID_ARGUMENT);
    while (decimal[index] != '\0')
    {
        uint32_t digit;

        if (decimal[index] < '0' || decimal[index] > '9')
            return (FT_ERR_INVALID_ARGUMENT);
        digit = static_cast<uint32_t>(decimal[index] - '0');
        if (value > (UINT32_MAX - digit) / 10U)
            return (FT_ERR_OUT_OF_RANGE);
        value = value * 10U + digit;
        index = index + 1U;
    }
    *value_out = value;
    return (FT_ERR_SUCCESS);
}

#define VOXEL_JSON_ROUNDTRIP_SERIALIZER(adapter_name, class_name) \
    static int32_t adapter_name(void *instance, ft_string &output) noexcept \
    { \
        return (static_cast<class_name *>(instance)->serialize_json(output)); \
    }

#define VOXEL_JSON_ROUNDTRIP_SETTER(setter_name, class_name, field_name) \
    static void setter_name(void *instance, uint32_t value) noexcept \
    { \
        static_cast<class_name *>(instance)->field_name = value; \
        return ; \
    }

VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_biome,
    voxel_biome_definition)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_feature,
    voxel_feature_rule)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_ore,
    voxel_ore_rule)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_underground,
    voxel_underground_structure_config)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_fluid,
    voxel_fluid_config)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_layer,
    voxel_layer_config)
VOXEL_JSON_ROUNDTRIP_SERIALIZER(voxel_json_roundtrip_generation,
    voxel_generation_config)

VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_biome_surface,
    voxel_biome_definition, surface_block_id)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_feature_chance,
    voxel_feature_rule, chance_percent)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_ore_block,
    voxel_ore_rule, block_id)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_ore_size,
    voxel_ore_rule, vein_size)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_ore_count,
    voxel_ore_rule, veins_per_chunk_max)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_ravine_width,
    voxel_underground_structure_config, ravine_width)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_cave_radius,
    voxel_underground_structure_config, cave_large_radius)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_lake_chance,
    voxel_fluid_config, lake_chance_percent)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_lake_depth,
    voxel_fluid_config, underground_lake_depth)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_layer_block,
    voxel_layer_config, beach_block_id)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_layer_depth,
    voxel_layer_config, beach_depth)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_generation_count,
    voxel_generation_config, biome_count)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_generation_transition,
    voxel_generation_config, biome_transition_noise_strength)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_generation_mountain,
    voxel_generation_config, mountain_ridge_strength)
VOXEL_JSON_ROUNDTRIP_SETTER(voxel_json_set_generation_erosion,
    voxel_generation_config, erosion_strength)

static int32_t voxel_json_check_unsigned_roundtrip(void *instance,
    voxel_json_roundtrip_serializer serializer,
    voxel_json_roundtrip_setter setter, const char *field_name,
    uint32_t value, const char *expected_decimal) noexcept
{
    ft_string serialized;
    ft_string reserialized;
    ft_string wrapped;
    ft_string expected_token;
    json_group *first_groups = ft_nullptr;
    json_group *second_groups = ft_nullptr;
    json_group *group;
    json_item *item;
    char *first_written = ft_nullptr;
    char *second_written = ft_nullptr;
    uint32_t parsed_value = 0U;
    int32_t result;

    setter(instance, value);
    result = serializer(instance, serialized);
    if (result == FT_ERR_SUCCESS)
        result = expected_token.initialize("\"");
    if (result == FT_ERR_SUCCESS)
        result = expected_token.append(field_name);
    if (result == FT_ERR_SUCCESS)
        result = expected_token.append("\":");
    if (result == FT_ERR_SUCCESS)
        result = expected_token.append(expected_decimal);
    if (result == FT_ERR_SUCCESS
        && ft_strstr(serialized.c_str(), expected_token.c_str()) == ft_nullptr)
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
        result = wrapped.initialize("{\"voxel_roundtrip\":");
    if (result == FT_ERR_SUCCESS)
        result = wrapped.append(serialized.c_str(), serialized.size());
    if (result == FT_ERR_SUCCESS)
        result = wrapped.append("}");
    if (result == FT_ERR_SUCCESS)
        first_groups = json_read_from_string(wrapped.c_str());
    if (result == FT_ERR_SUCCESS && first_groups == ft_nullptr)
        result = FT_ERR_INVALID_ARGUMENT;
    if (result == FT_ERR_SUCCESS)
    {
        group = json_find_group(first_groups, "voxel_roundtrip");
        item = json_find_item(group, field_name);
        if (item == ft_nullptr || item->value == ft_nullptr
            || ft_strcmp(item->value, expected_decimal) != FT_ERR_SUCCESS)
            result = FT_ERR_INVALID_STATE;
    }
    if (result == FT_ERR_SUCCESS)
        result = voxel_json_roundtrip_parse_u32(item->value, &parsed_value);
    if (result == FT_ERR_SUCCESS)
    {
        setter(instance, parsed_value);
        result = serializer(instance, reserialized);
    }
    if (result == FT_ERR_SUCCESS
        && (serialized.size() != reserialized.size()
            || ft_memcmp(serialized.c_str(), reserialized.c_str(),
                serialized.size()) != 0))
        result = FT_ERR_INVALID_STATE;
    if (result == FT_ERR_SUCCESS)
    {
        first_written = json_write_to_string(first_groups);
        if (first_written == ft_nullptr)
            result = FT_ERR_NO_MEMORY;
    }
    if (result == FT_ERR_SUCCESS)
    {
        second_groups = json_read_from_string(first_written);
        if (second_groups == ft_nullptr)
            result = FT_ERR_INVALID_ARGUMENT;
    }
    if (result == FT_ERR_SUCCESS)
    {
        group = json_find_group(second_groups, "voxel_roundtrip");
        item = json_find_item(group, field_name);
        if (item == ft_nullptr || item->value == ft_nullptr
            || ft_strcmp(item->value, expected_decimal) != FT_ERR_SUCCESS)
            result = FT_ERR_INVALID_STATE;
    }
    if (result == FT_ERR_SUCCESS)
    {
        second_written = json_write_to_string(second_groups);
        if (second_written == ft_nullptr)
            result = FT_ERR_NO_MEMORY;
    }
    if (result == FT_ERR_SUCCESS
        && ft_strcmp(first_written, second_written) != FT_ERR_SUCCESS)
        result = FT_ERR_INVALID_STATE;
    if (first_groups != ft_nullptr)
        json_free_groups(first_groups);
    if (second_groups != ft_nullptr)
        json_free_groups(second_groups);
    if (first_written != ft_nullptr)
        cma_free(first_written);
    if (second_written != ft_nullptr)
        cma_free(second_written);
    if (serialized.is_initialised() == FT_TRUE)
        (void)serialized.destroy();
    if (reserialized.is_initialised() == FT_TRUE)
        (void)reserialized.destroy();
    if (wrapped.is_initialised() == FT_TRUE)
        (void)wrapped.destroy();
    if (expected_token.is_initialised() == FT_TRUE)
        (void)expected_token.destroy();
    return (result);
}

FT_TEST(test_voxel_json_unsigned_boundaries_roundtrip_all_families)
{
    voxel_generation_config generation;
    uint32_t values[6] = {0U, 1U, 2147483647U, 2147483648U,
        4294967294U, 4294967295U};
    const char *expected[6] = {"0", "1", "2147483647", "2147483648",
        "4294967294", "4294967295"};
    uint32_t index = 0U;
    int32_t result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_default_generation_config(generation));
    while (index < 6U)
    {
        result = voxel_json_check_unsigned_roundtrip(
            &generation.biomes[0], voxel_json_roundtrip_biome,
            voxel_json_set_biome_surface, "surface_block_id", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.features[0], voxel_json_roundtrip_feature,
            voxel_json_set_feature_chance, "chance_percent", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.ores[0], voxel_json_roundtrip_ore,
            voxel_json_set_ore_block, "block_id", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.ores[0], voxel_json_roundtrip_ore,
            voxel_json_set_ore_size, "vein_size", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.ores[0], voxel_json_roundtrip_ore,
            voxel_json_set_ore_count, "veins_per_chunk_max", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.underground_structures,
            voxel_json_roundtrip_underground, voxel_json_set_ravine_width,
            "ravine_width", values[index], expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.underground_structures,
            voxel_json_roundtrip_underground, voxel_json_set_cave_radius,
            "cave_large_radius", values[index], expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.fluids, voxel_json_roundtrip_fluid,
            voxel_json_set_lake_chance, "lake_chance_percent", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.fluids, voxel_json_roundtrip_fluid,
            voxel_json_set_lake_depth, "underground_lake_depth", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.layers, voxel_json_roundtrip_layer,
            voxel_json_set_layer_block, "beach_block_id", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation.layers, voxel_json_roundtrip_layer,
            voxel_json_set_layer_depth, "beach_depth", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation, voxel_json_roundtrip_generation,
            voxel_json_set_generation_count, "biome_count", values[index],
            expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation, voxel_json_roundtrip_generation,
            voxel_json_set_generation_transition,
            "biome_transition_noise_strength", values[index], expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation, voxel_json_roundtrip_generation,
            voxel_json_set_generation_mountain, "mountain_ridge_strength",
            values[index], expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        result = voxel_json_check_unsigned_roundtrip(
            &generation, voxel_json_roundtrip_generation,
            voxel_json_set_generation_erosion, "erosion_strength",
            values[index], expected[index]);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
        index = index + 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.destroy());
    return (1);
}

static int32_t voxel_json_assert_uninitialised_preserves_output(
    void *instance, voxel_json_roundtrip_serializer serializer,
    ft_string &output) noexcept
{
    int32_t result = serializer(instance, output);

    if (result != FT_ERR_NOT_INITIALISED || output.size() != 8U
        || ft_memcmp(output.c_str(), "sentinel", 8U) != 0)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_voxel_json_uninitialised_config_preserves_output)
{
    voxel_biome_definition biome;
    voxel_feature_rule feature;
    voxel_ore_rule ore;
    voxel_underground_structure_config underground;
    voxel_fluid_config fluid;
    voxel_layer_config layer;
    voxel_generation_config generation;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("sentinel"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &biome, voxel_json_roundtrip_biome, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &feature, voxel_json_roundtrip_feature, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &ore, voxel_json_roundtrip_ore, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &underground, voxel_json_roundtrip_underground, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &fluid, voxel_json_roundtrip_fluid, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &layer, voxel_json_roundtrip_layer, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_assert_uninitialised_preserves_output(
        &generation, voxel_json_roundtrip_generation, output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}

#endif
