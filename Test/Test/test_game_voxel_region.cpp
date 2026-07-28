#include "../test_internal.hpp"
#include "../../Modules/Game/game_voxel_region.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Printf/printf.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <climits>
#include <cstdio>
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static int32_t test_voxel_make_temp_path(char *buffer,
    ft_size_t buffer_size) noexcept
{
    ft_string temporary_path;
    int32_t error_code;
    ft_size_t path_length;

    if (buffer == ft_nullptr || buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = temporary_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = filesystem_temp_path("libft_voxel_region", ft_nullptr,
            &temporary_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary_path.destroy();
        return (error_code);
    }
    path_length = temporary_path.size();
    if (path_length + 1 > buffer_size)
    {
        (void)temporary_path.destroy();
        return (FT_ERR_INVALID_ARGUMENT);
    }
    std::memcpy(buffer, temporary_path.c_str(), path_length + 1);
    (void)temporary_path.destroy();
    return (FT_ERR_SUCCESS);
}

static void test_voxel_cleanup_region_dir(const char *directory_path) noexcept
{
    (void)file_delete_recursive(directory_path);
    return ;
}

static int32_t test_voxel_write_unique_blocks(game_voxel_chunk &chunk,
    uint32_t block_count) noexcept
{
    uint32_t block_index;
    int32_t write_error;

    block_index = 1;
    while (block_index <= block_count)
    {
        write_error = chunk.write_block(static_cast<int32_t>(block_index & 15U),
            static_cast<int32_t>((block_index >> 8) & 15U),
            static_cast<int32_t>((block_index >> 4) & 15U),
            block_index);
        if (write_error != FT_ERR_SUCCESS)
            return (write_error);
        block_index += 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t test_voxel_initialize_unit_cube_frustum(
    geometry_frustum &frustum) noexcept
{
    if (frustum.planes[0].normal.initialize(1.0, 0.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[0].distance = 1.0;
    if (frustum.planes[1].normal.initialize(-1.0, 0.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[1].distance = 1.0;
    if (frustum.planes[2].normal.initialize(0.0, 1.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[2].distance = 1.0;
    if (frustum.planes[3].normal.initialize(0.0, -1.0, 0.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[3].distance = 1.0;
    if (frustum.planes[4].normal.initialize(0.0, 0.0, 1.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[4].distance = 1.0;
    if (frustum.planes[5].normal.initialize(0.0, 0.0, -1.0) != FT_ERR_SUCCESS)
        return (FT_ERR_INITIALIZATION_FAILED);
    frustum.planes[5].distance = 1.0;
    return (FT_ERR_SUCCESS);
}

static void test_voxel_chunk_get_section_out_of_range_operation(void)
{
    game_voxel_chunk chunk;

    (void)chunk.initialize();
    (void)chunk.get_section(GAME_VOXEL_CHUNK_SECTION_COUNT);
    return ;
}

static void test_voxel_section_get_block_out_of_range_operation(void)
{
    game_voxel_chunk_section section;

    (void)section.initialize();
    (void)section.get_block(GAME_VOXEL_SECTION_BLOCKS);
    return ;
}

FT_TEST(test_game_voxel_chunk_empty_reads_air)
{
    game_voxel_chunk chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(15, 255, 15, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.read_block(-1, 0, 0, &block_id));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.read_block(16, 0, 0, &block_id));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.read_block(0, 256, 0, &block_id));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_palette_sections)
{
    game_voxel_chunk chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(0).is_uniform());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 2, 3, 7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(7U, block_id);
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(0).is_materialized());
    FT_ASSERT_EQ(2, chunk.get_section(0).get_palette_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(2, 2, 3, 8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(2, 2, 3, &block_id));
    FT_ASSERT_EQ(8U, block_id);
    FT_ASSERT_EQ(3, chunk.get_section(0).get_palette_size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 20, 1, 9));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 20, 1, &block_id));
    FT_ASSERT_EQ(9U, block_id);
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(1).is_materialized());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 2, 3,
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_section_collapses_to_uniform_air)
{
    game_voxel_chunk chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(4, 4, 4, 12));
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(0).is_materialized());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(4, 4, 4,
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(0).is_uniform());
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK,
        chunk.get_section(0).get_uniform_block());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(4, 4, 4, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_dirty_and_serialization_round_trip)
{
    game_voxel_chunk chunk;
    game_voxel_chunk loaded_chunk;
    ft_byte_buffer buffer;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_FALSE, chunk.is_dirty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(0, 0, 0, 44));
    FT_ASSERT_EQ(FT_TRUE, chunk.is_dirty());
    chunk.clear_dirty();
    FT_ASSERT_EQ(FT_FALSE, chunk.is_dirty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(15, 255, 15, 45));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.deserialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(44U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.read_block(15, 255, 15,
        &block_id));
    FT_ASSERT_EQ(45U, block_id);
    FT_ASSERT_EQ(FT_FALSE, loaded_chunk.is_dirty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_rejects_bad_serialized_data)
{
    game_voxel_chunk chunk;
    game_voxel_chunk loaded_chunk;
    ft_byte_buffer truncated_buffer;
    ft_byte_buffer bad_section_buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.append_u32_le(0));
    FT_ASSERT_EQ(FT_ERR_IO, loaded_chunk.deserialize(truncated_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, loaded_chunk._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bad_section_buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.serialize(bad_section_buffer));
    bad_section_buffer._data[8] = 99;
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        loaded_chunk.deserialize(bad_section_buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, loaded_chunk._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bad_section_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated_buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_section_deserialize_failure_destroys_partial_state)
{
    game_voxel_chunk_section section;
    ft_byte_buffer buffer;
    uint16_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, section.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u8(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u16_le(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u32_le(901));
    index = 0;
    while (index < GAME_VOXEL_SECTION_BLOCKS)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.append_u8(1));
        index += 1;
    }
    FT_ASSERT_EQ(FT_ERR_IO, section.deserialize(buffer));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, section._initialised_state);
    FT_ASSERT(section._palette == ft_nullptr);
    FT_ASSERT(section._indices == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, section.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_direct_access_bounds_abort)
{
    FT_ASSERT_EQ(1, test_expect_sigabrt_signal(
        test_voxel_chunk_get_section_out_of_range_operation));
    FT_ASSERT_EQ(1, test_expect_sigabrt_signal(
        test_voxel_section_get_block_out_of_range_operation));
    return (1);
}

FT_TEST(test_game_voxel_chunk_palette_limit_is_reported)
{
    game_voxel_chunk chunk;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_write_unique_blocks(chunk, 255));
    FT_ASSERT_EQ(256, chunk.get_section(0).get_palette_size());
    FT_ASSERT_EQ(FT_ERR_FULL, chunk.write_block(0, 1, 0, 300));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_rejects_invalid_write_coordinates)
{
    game_voxel_chunk chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(-1, 0, 0, 1));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(0, -1, 0, 1));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(0, 0, -1, 1));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(16, 0, 0, 1));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(0, 256, 0, 1));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, chunk.write_block(0, 0, 16, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_rejects_null_read_output)
{
    game_voxel_chunk chunk;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        chunk.read_block(0, 0, 0, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, chunk.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_move_transfers_sections)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(3, 4, 5, 101));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(7, 31, 8, 102));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.move(source_chunk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(3, 4, 5,
        &block_id));
    FT_ASSERT_EQ(101U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(7, 31, 8,
        &block_id));
    FT_ASSERT_EQ(102U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_move_from_destroyed_source_destroys_destination)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.move(source_chunk));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED,
        destination_chunk._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_deserialize_replaces_existing_blocks)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer buffer;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(1, 1, 1, 201));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.write_block(2, 2, 2, 202));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.deserialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 1, 1,
        &block_id));
    FT_ASSERT_EQ(201U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(2, 2, 2,
        &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_materialize_allocation_failure_preserves_air)
{
    game_voxel_chunk chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    cma_set_alloc_limit(512);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, chunk.write_block(1, 1, 1, 401));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_TRUE, chunk.get_section(0).is_uniform());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 1, 1, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(1, 1, 1, 401));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.read_block(1, 1, 1, &block_id));
    FT_ASSERT_EQ(401U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_chunk_deserialize_materialized_alloc_failure)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk destination_chunk;
    ft_byte_buffer buffer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_block(2, 2, 2, 402));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.serialize(buffer));
    cma_set_alloc_limit(512);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination_chunk.deserialize(buffer));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.destroy());
    return (1);
}

FT_TEST(test_game_voxel_region_file_names)
{
    ft_string file_name;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_name.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name_from_start(0, 0, file_name));
    FT_ASSERT_STR_EQ("region_P0P0.dat", file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name_from_start(16, -16, file_name));
    FT_ASSERT_STR_EQ("region_P16N16.dat", file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name_from_start(-16, -16, file_name));
    FT_ASSERT_STR_EQ("region_N16N16.dat", file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name_from_start(INT_MIN, INT_MIN, file_name));
    FT_ASSERT_STR_EQ("region_N2147483648N2147483648.dat",
        file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name(0, 0, file_name));
    FT_ASSERT_STR_EQ("region_P0P0.dat", file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name(512, 0, file_name));
    FT_ASSERT_STR_EQ("region_P512P0.dat", file_name.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        game_voxel_region_file_name(-1, -1, file_name));
    FT_ASSERT_STR_EQ("region_N512N512.dat", file_name.c_str());
    FT_ASSERT_EQ(-512, game_voxel_region_start_coordinate(-1));
    FT_ASSERT_EQ(0, game_voxel_region_start_coordinate(511));
    FT_ASSERT_EQ(512, game_voxel_region_start_coordinate(512));
    FT_ASSERT_EQ(-512, game_voxel_region_start_coordinate(-512));
    FT_ASSERT_EQ(-1024, game_voxel_region_start_coordinate(-513));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_name.destroy());
    return (1);
}

FT_TEST(test_game_voxel_region_negative_coordinate_boundaries)
{
    char directory_path[256];
    char expected_file_path[300];
    game_voxel_region region;
    game_voxel_region loaded_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(-1, -1, directory_path));
    FT_ASSERT_EQ(-512, region.get_region_start_x());
    FT_ASSERT_EQ(-512, region.get_region_start_z());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(-512, 0, -512, 31));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(-1, 255, -1, 32));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, region.write_block(-513, 0, -512, 33));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    (void)pf_snprintf(expected_file_path, sizeof(expected_file_path),
        "%s/region_N512N512.dat", directory_path);
    FT_ASSERT_EQ(0, access(expected_file_path, F_OK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(-1, -1,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.load_region(-1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(-512, 0, -512,
        &block_id));
    FT_ASSERT_EQ(31U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(-1, 255, -1,
        &block_id));
    FT_ASSERT_EQ(32U, block_id);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, loaded_region.read_block(0, 0, -1,
        &block_id));
    FT_ASSERT_EQ(FT_TRUE, loaded_region.is_chunk_loaded(-32, -32));
    FT_ASSERT_EQ(FT_TRUE, loaded_region.is_chunk_loaded(-1, -1));
    FT_ASSERT_EQ(FT_FALSE, loaded_region.is_chunk_loaded(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_save_load_round_trip)
{
    char directory_path[256];
    char expected_file_path[300];
    game_voxel_region region;
    game_voxel_region loaded_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(0, 0, 0, 22));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(15, 255, 15, 23));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(511, 10, 511, 24));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    (void)pf_snprintf(expected_file_path, sizeof(expected_file_path),
        "%s/region_P0P0.dat", directory_path);
    FT_ASSERT_EQ(0, access(expected_file_path, F_OK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(22U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(15, 255, 15,
        &block_id));
    FT_ASSERT_EQ(23U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(511, 10, 511,
        &block_id));
    FT_ASSERT_EQ(24U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(16, 1, 16,
        &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_TRUE, loaded_region.is_chunk_loaded(0, 0));
    FT_ASSERT_EQ(FT_FALSE, loaded_region.is_chunk_loaded(1, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_empty_file_uses_explicit_table_size)
{
    char directory_path[256];
    char expected_file_path[300];
    game_voxel_region region;
    game_voxel_region loaded_region;
    struct stat file_status;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    (void)pf_snprintf(expected_file_path, sizeof(expected_file_path),
        "%s/region_P0P0.dat", directory_path);
    FT_ASSERT_EQ(0, stat(expected_file_path, &file_status));
    FT_ASSERT_EQ(16 + (12 * GAME_VOXEL_REGION_CHUNK_COUNT),
        file_status.st_size);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_save_chunk_updates_slot)
{
    char directory_path[256];
    game_voxel_chunk chunk;
    game_voxel_region region;
    game_voxel_region loaded_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.write_block(3, 4, 5, 77));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_chunk(2, 3, chunk));
    FT_ASSERT_EQ(FT_TRUE, region.has_chunk(2, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(35, 4, 53, &block_id));
    FT_ASSERT_EQ(77U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(35, 4, 53,
        &block_id));
    FT_ASSERT_EQ(77U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_missing_storage_path_errors)
{
    char directory_path[256];
    game_voxel_region region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        region.set_region_storage_path(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, region.save_region());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    return (1);
}

FT_TEST(test_game_voxel_region_missing_and_corrupt_files)
{
    char directory_path[256];
    char file_path[300];
    game_voxel_region region;
    FILE *file;
    uint32_t bad_magic;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(-1, -1, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.load_region(-1, -1));
    (void)pf_snprintf(file_path, sizeof(file_path), "%s/region_N512N512.dat",
        directory_path);
    file = std::fopen(file_path, "wb");
    FT_ASSERT(file != ft_nullptr);
    bad_magic = 0;
    FT_ASSERT_EQ(1, std::fwrite(&bad_magic, sizeof(bad_magic), 1, file));
    (void)std::fclose(file);
    FT_ASSERT_EQ(FT_ERR_IO, region.load_region(-1, -1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_load_chunk_creates_empty_chunk)
{
    char directory_path[256];
    game_voxel_region region;
    game_voxel_chunk *chunk;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    chunk = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.load_chunk(4, 5, &chunk));
    FT_ASSERT(chunk != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, region.has_chunk(4, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk->read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_chunk_visibility_uses_frustum)
{
    char directory_path[256];
    game_voxel_region region;
    game_voxel_chunk *chunk;
    geometry_frustum frustum;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_initialize_unit_cube_frustum(
        frustum));
    chunk = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.load_chunk(0, 0, &chunk));
    FT_ASSERT_EQ(FT_TRUE, region.is_chunk_visible(0, 0, frustum));
    FT_ASSERT_EQ(FT_FALSE, region.is_chunk_visible(4, 4, frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_chunk_visibility_rejects_unloaded_chunk)
{
    char directory_path[256];
    game_voxel_region region;
    geometry_frustum frustum;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_initialize_unit_cube_frustum(
        frustum));
    FT_ASSERT_EQ(FT_FALSE, region.is_chunk_visible(0, 0, frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_chunk_visibility_keeps_edge_chunk_visible)
{
    char directory_path[256];
    game_voxel_region region;
    geometry_frustum frustum;
    game_voxel_chunk *chunk;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_initialize_unit_cube_frustum(
        frustum));
    chunk = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.load_chunk(0, 0, &chunk));
    FT_ASSERT_EQ(FT_TRUE, region.is_chunk_visible(0, 0, frustum));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_air_write_does_not_allocate_missing_chunk)
{
    char directory_path[256];
    game_voxel_region region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(33, 2, 49,
        GAME_VOXEL_AIR_BLOCK));
    FT_ASSERT_EQ(FT_FALSE, region.has_chunk(2, 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(33, 2, 49, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_rejects_chunk_requests_outside_region)
{
    char directory_path[256];
    game_voxel_region region;
    game_voxel_chunk chunk;
    game_voxel_chunk *chunk_out;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    chunk_out = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, region.load_chunk(-1, 0, &chunk_out));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, region.load_chunk(32, 0, &chunk_out));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, region.load_chunk(0, 32, &chunk_out));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, region.save_chunk(32, 0, chunk));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        region.load_chunk(0, 0, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, chunk.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_move_transfers_loaded_chunks)
{
    char directory_path[256];
    game_voxel_region source_region;
    game_voxel_region destination_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.write_block(48, 7, 64, 303));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.move(source_region));
    FT_ASSERT_EQ(FT_TRUE, destination_region.has_chunk(3, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.read_block(48, 7, 64,
        &block_id));
    FT_ASSERT_EQ(303U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_move_from_destroyed_source_destroys_destination)
{
    char directory_path[256];
    game_voxel_region source_region;
    game_voxel_region destination_region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.move(source_region));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED,
        destination_region._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_move_failure_keeps_source_chunks)
{
    char directory_path[256];
    char long_directory_path[256];
    game_voxel_region source_region;
    game_voxel_region destination_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    (void)pf_snprintf(long_directory_path, sizeof(long_directory_path),
        "%s/%s", directory_path,
        "very_long_voxel_storage_directory_name_used_for_move_failure");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.initialize(0, 0,
        long_directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.write_block(16, 3, 16, 804));
    cma_set_alloc_limit(8);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination_region.move(source_region));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_TRUE, source_region.has_chunk(1, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.read_block(16, 3, 16,
        &block_id));
    FT_ASSERT_EQ(804U, block_id);
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED,
        destination_region._initialised_state);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_read_block_updates_region_error)
{
    char directory_path[256];
    game_voxel_region region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, region.read_block(0, 0, 0,
        ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, region.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(0, 0, 0, 805));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(805U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_write_block_chunk_allocation_failure)
{
    char directory_path[256];
    game_voxel_region region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    cma_set_alloc_limit(512);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, region.write_block(0, 0, 0, 501));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_FALSE, region.has_chunk(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(GAME_VOXEL_AIR_BLOCK, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(0, 0, 0, 501));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.read_block(0, 0, 0, &block_id));
    FT_ASSERT_EQ(501U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_storage_path_assignment_failure)
{
    char directory_path[256];
    char long_directory_path[256];
    game_voxel_region region;
    void *allocation_guard;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    (void)pf_snprintf(long_directory_path, sizeof(long_directory_path),
        "%s/%s", directory_path,
        "very_long_voxel_storage_directory_name_used_for_failure_testing_"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
        "cccccccccccccccccccccccccccccccccccccccccccccccc");
    allocation_guard = ft_nullptr;
    cma_set_alloc_limit(1);
    allocation_guard = cma_malloc(1);
    FT_ASSERT(allocation_guard != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY,
        region.set_region_storage_path(long_directory_path));
    cma_set_alloc_limit(0);
    cma_free(allocation_guard);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.set_region_storage_path(directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_save_region_buffer_allocation_failure)
{
    char directory_path[256];
    game_voxel_region region;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(1, 1, 1, 502));
    cma_set_alloc_limit(1);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, region.save_region());
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_load_region_chunk_buffer_allocation_failure)
{
    char directory_path[256];
    game_voxel_region region;
    game_voxel_region loaded_region;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(1, 1, 1, 503));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(2, 1, 1, 504));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0,
        directory_path));
    cma_set_alloc_limit(1024);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, loaded_region.load_region(0, 0));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.read_block(1, 1, 1,
        &block_id));
    FT_ASSERT_EQ(503U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_load_region_truncated_chunk_payload)
{
    char directory_path[256];
    char file_path[300];
    game_voxel_region region;
    game_voxel_region loaded_region;
    FILE *file;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(1, 1, 1, 505));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    (void)pf_snprintf(file_path, sizeof(file_path), "%s/region_P0P0.dat",
        directory_path);
    file = std::fopen(file_path, "ab");
    FT_ASSERT(file != ft_nullptr);
    FT_ASSERT_EQ(0, ftruncate(fileno(file), 64));
    (void)std::fclose(file);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_IO, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_load_region_rejects_invalid_table_offset)
{
    char directory_path[256];
    char file_path[300];
    game_voxel_region region;
    game_voxel_region loaded_region;
    FILE *file;
    uint64_t invalid_offset;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(1, 1, 1, 601));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(17, 1, 1, 602));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.save_region());
    (void)pf_snprintf(file_path, sizeof(file_path), "%s/region_P0P0.dat",
        directory_path);
    file = std::fopen(file_path, "r+b");
    FT_ASSERT(file != ft_nullptr);
    invalid_offset = 0x7FFFFFFFFFFFFFFFULL;
    FT_ASSERT_EQ(0, std::fseek(file, 16 + 12, SEEK_SET));
    FT_ASSERT_EQ(1, std::fwrite(&invalid_offset, sizeof(invalid_offset), 1,
        file));
    (void)std::fclose(file);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.initialize(0, 0,
        directory_path));
    FT_ASSERT_EQ(FT_ERR_IO, loaded_region.load_region(0, 0));
    FT_ASSERT_EQ(FT_FALSE, loaded_region.has_chunk(0, 0));
    FT_ASSERT_EQ(FT_FALSE, loaded_region.has_chunk(1, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, loaded_region.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}

FT_TEST(test_game_voxel_region_save_region_cleans_buffers_on_open_failure)
{
    char directory_path[256];
    char missing_path[300];
    game_voxel_region region;
    ft_size_t allocation_count;
    ft_size_t free_count;
    ft_size_t current_bytes_before;
    ft_size_t current_bytes_after;
    ft_size_t peak_bytes;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_voxel_make_temp_path(directory_path,
        sizeof(directory_path)));
    test_voxel_cleanup_region_dir(directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directory(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.initialize(0, 0, directory_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(1, 1, 1, 701));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.write_block(17, 1, 1, 702));
    (void)pf_snprintf(missing_path, sizeof(missing_path),
        "%s/missing/subdir", directory_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.set_region_storage_path(missing_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_extended_stats(&allocation_count,
        &free_count, &current_bytes_before, &peak_bytes));
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, region.save_region());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_extended_stats(&allocation_count,
        &free_count, &current_bytes_after, &peak_bytes));
    FT_ASSERT_EQ(current_bytes_before, current_bytes_after);
    FT_ASSERT_EQ(FT_TRUE, region._chunks[0]->is_dirty());
    FT_ASSERT_EQ(FT_TRUE, region._chunks[1]->is_dirty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, region.destroy());
    test_voxel_cleanup_region_dir(directory_path);
    return (1);
}
