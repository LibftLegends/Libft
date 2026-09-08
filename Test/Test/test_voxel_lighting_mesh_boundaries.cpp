#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/Voxel/voxel_api.hpp"
#include "../../Modules/Voxel/voxel_lighting.hpp"
#include "../../Modules/Voxel/voxel_mesh.hpp"

static_assert(sizeof(chunk_mesh_vertex) == 16U,
    "packed light must fit the pre-light chunk vertex stride");

static int32_t test_voxel_lighting_empty_block_lookup(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint32_t *block_id) noexcept
{
    (void)user_data;
    (void)world_x;
    (void)world_y;
    (void)world_z;
    if (block_id == nullptr)
        return (FT_ERR_INVALID_POINTER);
    *block_id = GAME_VOXEL_AIR_BLOCK;
    return (FT_ERR_SUCCESS);
}

static int32_t test_voxel_lighting_neighbor_block_lookup(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint32_t *block_id)
{
    (void)user_data;
    (void)world_x;
    (void)world_y;
    (void)world_z;
    if (block_id == nullptr)
        return (FT_ERR_INVALID_POINTER);
    *block_id = GAME_VOXEL_AIR_BLOCK;
    return (FT_ERR_SUCCESS);
}

static int32_t test_voxel_lighting_boundary_lookup(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint8_t *packed_light) noexcept
{
    (void)user_data;
    (void)world_y;
    if (packed_light == nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (world_x == -1)
        *packed_light = voxel_light_pack(1U, 1U);
    else if (world_x == 16)
        *packed_light = voxel_light_pack(2U, 2U);
    else if (world_z == -1)
        *packed_light = voxel_light_pack(3U, 3U);
    else if (world_z == 16)
        *packed_light = voxel_light_pack(4U, 4U);
    else
        *packed_light = voxel_light_pack(0U, 0U);
    return (FT_ERR_SUCCESS);
}

static ft_bool test_voxel_lighting_mesh_has_face_light(
    const chunk_mesh &mesh, chunk_mesh_face face, uint8_t packed_light)
{
    ft_size_t vertex_index;

    vertex_index = 0U;
    while (vertex_index < mesh.vertices.size())
    {
        if (mesh.vertices[vertex_index].face == face
            && mesh.vertices[vertex_index].packed_light == packed_light)
            return (FT_TRUE);
        vertex_index += 1U;
    }
    return (FT_FALSE);
}

FT_TEST(test_voxel_lighting_build_operation_lookup_contract)
{
    voxel_light_build_operation operation;
    voxel_light_chunk light_chunk;
    voxel_light_update_config configuration;
    ft_bool complete;
    uint8_t packed_light;
    uint32_t step_count;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.initialize(light_chunk, 0, 0,
        test_voxel_lighting_empty_block_lookup, nullptr, FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, operation.get_packed_light(0, 0, 0,
        nullptr));
    packed_light = 0U;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, operation.get_packed_light(0, 0, 0,
        &packed_light));
    voxel_light_update_config_defaults(configuration);
    configuration.min_nodes_per_frame = 4096U;
    configuration.target_nodes_per_frame = 4096U;
    configuration.max_nodes_per_frame = 4096U;
    complete = FT_FALSE;
    step_count = 0U;
    while (complete == FT_FALSE && step_count < 10000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.step(configuration, nullptr,
            &complete));
        step_count += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, complete);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.get_packed_light(0, 0, 0,
        &packed_light));
    FT_ASSERT_EQ(voxel_light_pack(15U, 0U), packed_light);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.get_packed_light(-15, 0, -15,
        &packed_light));
    FT_ASSERT_EQ(voxel_light_pack(15U, 0U), packed_light);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, operation.get_packed_light(-16, 0, 0,
        &packed_light));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, operation.get_packed_light(0, -1, 0,
        &packed_light));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, operation.get_packed_light(0, 256, 0,
        &packed_light));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_operation_lookup(
        &operation, 0, 0, 0, &packed_light));
    FT_ASSERT_EQ(voxel_light_pack(15U, 0U), packed_light);
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, voxel_light_build_operation_lookup(
        nullptr, 0, 0, 0, &packed_light));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_neighbor_faces_use_world_light_callback)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    voxel_light_chunk light;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 4, 4,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(15, 5, 5,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(5, 6, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(6, 7, 15,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light.initialize(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        chunk_mesh_generate_from_chunk_with_neighbors_and_light_lookup(
            mesh, chunk, 0, 0, test_voxel_lighting_neighbor_block_lookup,
            nullptr, &light, test_voxel_lighting_boundary_lookup, nullptr));
    FT_ASSERT_EQ(FT_TRUE, test_voxel_lighting_mesh_has_face_light(mesh,
        CHUNK_MESH_FACE_WEST, voxel_light_pack(1U, 1U)));
    FT_ASSERT_EQ(FT_TRUE, test_voxel_lighting_mesh_has_face_light(mesh,
        CHUNK_MESH_FACE_EAST, voxel_light_pack(2U, 2U)));
    FT_ASSERT_EQ(FT_TRUE, test_voxel_lighting_mesh_has_face_light(mesh,
        CHUNK_MESH_FACE_NORTH, voxel_light_pack(3U, 3U)));
    FT_ASSERT_EQ(FT_TRUE, test_voxel_lighting_mesh_has_face_light(mesh,
        CHUNK_MESH_FACE_SOUTH, voxel_light_pack(4U, 4U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_different_light_prevents_greedy_merge)
{
    game_voxel_chunk chunk;
    chunk_mesh same_light_mesh;
    chunk_mesh different_light_mesh;
    voxel_light_chunk same_light;
    voxel_light_chunk different_light;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 1, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 0,
        VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, same_light.initialize(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, different_light.initialize(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, same_light.set(0, 2, 0,
        voxel_light_pack(7U, 0U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, same_light.set(1, 2, 0,
        voxel_light_pack(7U, 0U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, different_light.set(0, 2, 0,
        voxel_light_pack(7U, 0U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, different_light.set(1, 2, 0,
        voxel_light_pack(8U, 0U)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(same_light_mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        chunk_mesh_initialize(different_light_mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_with_light(
        same_light_mesh, chunk, same_light));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        chunk_mesh_generate_from_chunk_with_light(different_light_mesh,
            chunk, different_light));
    FT_ASSERT_EQ(24, same_light_mesh.vertices.size());
    FT_ASSERT_EQ(28, different_light_mesh.vertices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(different_light_mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(same_light_mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, different_light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, same_light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_emissive_face_keeps_emission)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;
    voxel_light_chunk light;
    ft_size_t vertex_index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        VOXEL_GENERATOR_SHIMMER_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light.initialize(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk_with_light(
        mesh, chunk, light));
    vertex_index = 0U;
    while (vertex_index < mesh.vertices.size())
    {
        FT_ASSERT_EQ(static_cast<uint8_t>(15U), voxel_light_block(
            mesh.vertices[vertex_index].packed_light));
        vertex_index += 1U;
    }
    FT_ASSERT_EQ(16U, sizeof(chunk_mesh_vertex));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

#endif
