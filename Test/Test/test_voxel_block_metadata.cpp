#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/terrain_api.hpp"
#include "../../Modules/Voxel/voxel_mesh.hpp"

FT_TEST(test_terrain_block_metadata_registry_reports_expected_properties)
{
    const terrain_block_metadata &air_metadata =
        terrain_get_block_metadata(GAME_VOXEL_AIR_BLOCK);
    const terrain_block_metadata &grass_metadata =
        terrain_get_block_metadata(TERRAIN_GENERATOR_GRASS_BLOCK);
    const terrain_block_metadata &shrub_metadata =
        terrain_get_block_metadata(TERRAIN_GENERATOR_SHRUB_BLOCK);
    const terrain_block_metadata &leaf_metadata =
        terrain_get_block_metadata(TERRAIN_GENERATOR_OAK_LEAVES_BLOCK);
    const terrain_block_metadata &stone_metadata =
        terrain_get_block_metadata(TERRAIN_GENERATOR_STONE_BLOCK);

    FT_ASSERT_EQ(FT_FALSE, air_metadata.solid);
    FT_ASSERT_EQ(FT_TRUE, air_metadata.transparent);
    FT_ASSERT_EQ(FT_TRUE, air_metadata.replaceable);
    FT_ASSERT_EQ(0U, air_metadata.hardness);
    FT_ASSERT_EQ(FT_FALSE, air_metadata.occludes_faces);
    FT_ASSERT_EQ(FT_TRUE, terrain_block_is_transparent(
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_TRUE, terrain_block_is_replaceable(
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_TRUE, grass_metadata.solid);
    FT_ASSERT_EQ(FT_FALSE, grass_metadata.transparent);
    FT_ASSERT_EQ(FT_FALSE, grass_metadata.replaceable);
    FT_ASSERT_EQ(1U, grass_metadata.hardness);
    FT_ASSERT_EQ(FT_FALSE, shrub_metadata.solid);
    FT_ASSERT_EQ(FT_TRUE, shrub_metadata.transparent);
    FT_ASSERT_EQ(FT_TRUE, shrub_metadata.replaceable);
    FT_ASSERT_EQ(FT_FALSE, shrub_metadata.liquid);
    FT_ASSERT_EQ(1U, shrub_metadata.hardness);
    FT_ASSERT_EQ(FT_TRUE, shrub_metadata.occludes_faces);
    FT_ASSERT_EQ(FT_FALSE, leaf_metadata.solid);
    FT_ASSERT_EQ(FT_TRUE, leaf_metadata.transparent);
    FT_ASSERT_EQ(FT_TRUE, leaf_metadata.replaceable);
    FT_ASSERT_EQ(FT_FALSE, leaf_metadata.liquid);
    FT_ASSERT_EQ(1U, leaf_metadata.hardness);
    FT_ASSERT_EQ(FT_TRUE, leaf_metadata.occludes_faces);
    FT_ASSERT_EQ(FT_TRUE, stone_metadata.solid);
    FT_ASSERT_EQ(FT_FALSE, stone_metadata.transparent);
    FT_ASSERT_EQ(FT_FALSE, stone_metadata.replaceable);
    FT_ASSERT_EQ(4U, stone_metadata.hardness);
    FT_ASSERT_EQ(FT_TRUE, stone_metadata.occludes_faces);
    FT_ASSERT_EQ(FT_FALSE, terrain_block_is_liquid(
        TERRAIN_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_FALSE, terrain_block_emits_light(
        TERRAIN_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_FALSE, terrain_block_is_known(9999U));
    return (1);
}

FT_TEST(test_chunk_mesh_respects_transparent_neighbors)
{
    game_voxel_chunk chunk;
    chunk_mesh mesh;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1,
        TERRAIN_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(2, 1, 1,
        TERRAIN_GENERATOR_SHRUB_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_initialize(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_generate_from_chunk(mesh, chunk));
    FT_ASSERT_EQ(40, mesh.vertices.size());
    FT_ASSERT_EQ(60, mesh.indices.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk_mesh_destroy(mesh));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_terrain_can_place_tree_template_allows_replaceable_blocks)
{
    game_voxel_chunk chunk;
    const terrain_tree_template &cactus_tree_template =
        terrain_small_cactus_tree_template();
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(8, 12, 8,
        TERRAIN_GENERATOR_SHRUB_BLOCK));
    FT_ASSERT_EQ(FT_TRUE, terrain_can_place_tree_template(chunk, 8, 12, 8,
        cactus_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, terrain_place_tree_template(chunk, 8, 12, 8,
        cactus_tree_template));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(8, 12, 8, &block_id));
    FT_ASSERT_EQ(TERRAIN_GENERATOR_CACTUS_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

#endif
