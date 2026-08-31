#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/voxel_api.hpp"
#include "../../Modules/Voxel/voxel_block_registry.hpp"
#include "../../Modules/Buffer/byte_buffer.hpp"
#include <cstdio>
#include <cstring>

static void voxel_test_set_registration(
    voxel_block_registration &registration, const char *name,
    ft_bool can_host_ore, ft_bool is_ore) noexcept
{
    registration.name = name;
    registration.metadata.solid = FT_TRUE;
    registration.metadata.transparent = FT_FALSE;
    registration.metadata.liquid = FT_FALSE;
    registration.metadata.replaceable = FT_FALSE;
    registration.metadata.can_host_ore = can_host_ore;
    registration.metadata.is_ore = is_ore;
    registration.metadata.light_emitting = FT_FALSE;
    registration.metadata.occludes_faces = FT_TRUE;
    registration.metadata.hardness = 1U;
    registration.metadata.breakable = FT_TRUE;
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_TOP] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_BOTTOM] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_NORTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_SOUTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_EAST] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_WEST] =
        "Lua/export_values.lua";
    return ;
}

FT_TEST(test_voxel_builtin_and_runtime_block_ids_are_disjoint)
{
    voxel_block_registration registration;
    uint32_t builtin_index;
    uint32_t runtime_index;
    uint32_t block_id;
    int32_t error_code;
    char block_name[64];
    const char *builtin_name;
    const char *previous_builtin_name;

    voxel_runtime_reset_for_tests();
    builtin_index = 0U;
    while (builtin_index < VOXEL_BUILTIN_BLOCK_COUNT)
    {
        FT_ASSERT_EQ(FT_TRUE, voxel_block_is_known(builtin_index));
        builtin_name = voxel_get_block_name(builtin_index);
        FT_ASSERT(builtin_name != ft_nullptr);
        block_id = 0U;
        while (block_id < builtin_index)
        {
            previous_builtin_name = voxel_get_block_name(block_id);
            FT_ASSERT(previous_builtin_name != ft_nullptr);
            FT_ASSERT(std::strcmp(previous_builtin_name, builtin_name) != 0);
            block_id += 1U;
        }
        builtin_index += 1U;
    }
    runtime_index = 0U;
    while (runtime_index < VOXEL_RUNTIME_BLOCK_CAPACITY)
    {
        (void)std::snprintf(block_name, sizeof(block_name),
            "test:runtime_block_%u", runtime_index);
        registration.name = block_name;
        registration.metadata.solid = FT_TRUE;
        registration.metadata.transparent = FT_FALSE;
        registration.metadata.liquid = FT_FALSE;
        registration.metadata.replaceable = FT_FALSE;
        registration.metadata.can_host_ore = FT_TRUE;
        registration.metadata.is_ore = FT_FALSE;
        registration.metadata.light_emitting = FT_FALSE;
        registration.metadata.occludes_faces = FT_TRUE;
        registration.metadata.hardness = 1U;
        registration.metadata.breakable = FT_TRUE;
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_TOP] =
            "Lua/export_values.lua";
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_BOTTOM] =
            "Lua/export_values.lua";
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_NORTH] =
            "Lua/export_values.lua";
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_SOUTH] =
            "Lua/export_values.lua";
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_EAST] =
            "Lua/export_values.lua";
        registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_WEST] =
            "Lua/export_values.lua";
        block_id = 0U;
        error_code = voxel_register_block(registration, &block_id);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
        FT_ASSERT_EQ(static_cast<uint32_t>(VOXEL_BUILTIN_BLOCK_COUNT)
            + runtime_index, block_id);
        FT_ASSERT_EQ(FT_TRUE, voxel_block_is_known(block_id));
        FT_ASSERT_EQ(FT_TRUE, voxel_block_can_host_ore(block_id));
        FT_ASSERT_EQ(FT_FALSE, voxel_block_is_ore(block_id));
        runtime_index += 1U;
    }
    (void)std::snprintf(block_name, sizeof(block_name),
        "test:runtime_block_overflow");
    registration.name = block_name;
    error_code = voxel_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, error_code);
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_block_names_round_trip_to_current_ids)
{
    voxel_block_registration registration;
    uint32_t block_id;
    uint32_t resolved_block_id;
    int32_t error_code;

    voxel_runtime_reset_for_tests();
    registration.name = "test:stable_block";
    registration.metadata.solid = FT_TRUE;
    registration.metadata.transparent = FT_FALSE;
    registration.metadata.liquid = FT_FALSE;
    registration.metadata.replaceable = FT_FALSE;
    registration.metadata.can_host_ore = FT_TRUE;
    registration.metadata.is_ore = FT_FALSE;
    registration.metadata.light_emitting = FT_FALSE;
    registration.metadata.occludes_faces = FT_TRUE;
    registration.metadata.hardness = 1U;
    registration.metadata.breakable = FT_TRUE;
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_TOP] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_BOTTOM] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_NORTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_SOUTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_EAST] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_WEST] =
        "Lua/export_values.lua";
    error_code = voxel_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    error_code = voxel_find_block_id_by_name("voxel:stone",
        &resolved_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(VOXEL_GENERATOR_STONE_BLOCK,
        resolved_block_id);
    error_code = voxel_find_block_id_by_name("test:stable_block",
        &resolved_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(block_id, resolved_block_id);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, voxel_find_block_id_by_name(
        "test:missing_block", &resolved_block_id));
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_runtime_names_cannot_shadow_builtins)
{
    voxel_block_registration registration;
    uint32_t block_id;

    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "voxel:stone",
        FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, voxel_register_block(registration,
        &block_id));
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_duplicate_runtime_names_do_not_consume_ids)
{
    voxel_block_registration registration;
    uint32_t first_block_id;
    uint32_t duplicate_block_id;
    uint32_t second_block_id;

    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "test:ruby_block",
        FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_register_block(registration,
        &first_block_id));
    duplicate_block_id = UINT32_MAX;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, voxel_register_block(registration,
        &duplicate_block_id));
    FT_ASSERT_EQ(UINT32_MAX, duplicate_block_id);
    voxel_test_set_registration(registration, "test:sapphire_block",
        FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_register_block(registration,
        &second_block_id));
    FT_ASSERT_EQ(first_block_id + 1U, second_block_id);
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_block_names_require_stable_namespace_format)
{
    voxel_block_registration registration;
    uint32_t block_id;

    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "RubyBlock",
        FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_register_block(registration,
        &block_id));
    voxel_test_set_registration(registration, "test:ruby-block",
        FT_TRUE, FT_FALSE);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_register_block(registration,
        &block_id));
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_ore_metadata_rejects_ore_hosts_by_default)
{
    voxel_block_registration registration;
    uint32_t block_id;
    int32_t error_code;

    voxel_runtime_reset_for_tests();
    registration.name = "test:runtime_ore";
    registration.metadata.solid = FT_TRUE;
    registration.metadata.transparent = FT_FALSE;
    registration.metadata.liquid = FT_FALSE;
    registration.metadata.replaceable = FT_FALSE;
    registration.metadata.can_host_ore = FT_FALSE;
    registration.metadata.is_ore = FT_TRUE;
    registration.metadata.light_emitting = FT_FALSE;
    registration.metadata.occludes_faces = FT_TRUE;
    registration.metadata.hardness = 4U;
    registration.metadata.breakable = FT_TRUE;
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_TOP] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_BOTTOM] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_NORTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_SOUTH] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_EAST] =
        "Lua/export_values.lua";
    registration.asset_paths[VOXEL_BLOCK_ASSET_FACE_WEST] =
        "Lua/export_values.lua";
    error_code = voxel_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_FALSE, voxel_block_can_host_ore(block_id));
    FT_ASSERT_EQ(FT_TRUE, voxel_block_is_ore(block_id));
    registration.metadata.can_host_ore = FT_TRUE;
    registration.name = "test:invalid_ore";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, voxel_register_block(
        registration, &block_id));
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_config_persistence_remaps_runtime_block_names)
{
    voxel_block_registration registration;
    voxel_generation_config source_config;
    voxel_generation_config loaded_config;
    ft_byte_buffer buffer;
    uint32_t first_block_id;
    uint32_t second_block_id;
    int32_t error_code;

    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "test:stable_saved_block",
        FT_TRUE, FT_FALSE);
    error_code = voxel_register_block(registration, &first_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_default_generation_config(
        source_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_config.biomes[0].set_block_palette(
        first_block_id, first_block_id, first_block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_generation_config_serialize(
        source_config, buffer));
    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "test:registered_later",
        FT_TRUE, FT_FALSE);
    error_code = voxel_register_block(registration, &second_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    voxel_test_set_registration(registration, "test:stable_saved_block",
        FT_TRUE, FT_FALSE);
    error_code = voxel_register_block(registration, &second_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_NEQ(first_block_id, second_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_generation_config_deserialize(
        loaded_config, buffer));
    FT_ASSERT_EQ(second_block_id, loaded_config.biomes[0].surface_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_config.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_config.destroy());
    voxel_runtime_reset_for_tests();
    return (1);
}

FT_TEST(test_voxel_config_load_rejects_missing_runtime_block_name)
{
    voxel_block_registration registration;
    voxel_generation_config source_config;
    voxel_generation_config missing_config;
    ft_byte_buffer buffer;
    uint32_t block_id;
    int32_t error_code;

    voxel_runtime_reset_for_tests();
    voxel_test_set_registration(registration, "test:temporary_block",
        FT_TRUE, FT_FALSE);
    error_code = voxel_register_block(registration, &block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_default_generation_config(
        source_config));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_config.biomes[0].set_block_palette(
        block_id, block_id, block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_generation_config_serialize(
        source_config, buffer));
    voxel_runtime_reset_for_tests();
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, voxel_generation_config_deserialize(
        missing_config, buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_config.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_config.destroy());
    voxel_runtime_reset_for_tests();
    return (1);
}

#endif
