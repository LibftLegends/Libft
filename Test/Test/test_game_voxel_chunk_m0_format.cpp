#include "../../Modules/Game/game_voxel_chunk.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_game_voxel_chunk_biome_id_defaults_to_zero_and_is_settable)
{
    game_voxel_chunk value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0U, value.get_biome_id());
    value.set_biome_id(4U);
    FT_ASSERT_EQ(4U, value.get_biome_id());
    return (1);
}

FT_TEST(test_game_voxel_chunk_player_override_replaces_and_restores)
{
    game_voxel_chunk value;
    game_voxel_block_override override_value;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(3, 4, 5, 7U));
    FT_ASSERT_EQ(FT_FALSE, value.is_block_player_modified(3, 4, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(3, 4, 5, 9U));
    FT_ASSERT_EQ(1U, value.get_player_override_count());
    FT_ASSERT_EQ(FT_TRUE, value.is_block_player_modified(3, 4, 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_generated_block(3, 4, 5,
        &block_id));
    FT_ASSERT_EQ(7U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_player_override(0U,
        &override_value));
    FT_ASSERT_EQ(7U, override_value.generated_block_id);
    FT_ASSERT_EQ(9U, override_value.current_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(3, 4, 5, 11U));
    FT_ASSERT_EQ(1U, value.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.read_block(3, 4, 5, &block_id));
    FT_ASSERT_EQ(11U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(3, 4, 5, 7U));
    FT_ASSERT_EQ(0U, value.get_player_override_count());
    FT_ASSERT_EQ(FT_FALSE, value.is_block_player_modified(3, 4, 5));
    return (1);
}

FT_TEST(test_game_voxel_chunk_generation_preserves_player_override)
{
    game_voxel_chunk value;
    game_voxel_block_override override_value;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(1, 2, 3, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(1, 2, 3, 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(1, 2, 3, 6U));
    FT_ASSERT_EQ(1U, value.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(8U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_generated_block(1, 2, 3,
        &block_id));
    FT_ASSERT_EQ(6U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.get_player_override(0U,
        &override_value));
    FT_ASSERT_EQ(6U, override_value.generated_block_id);
    FT_ASSERT_EQ(8U, override_value.current_block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(1, 2, 3, 8U));
    FT_ASSERT_EQ(0U, value.get_player_override_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.read_block(1, 2, 3, &block_id));
    FT_ASSERT_EQ(8U, block_id);
    return (1);
}

FT_TEST(test_game_voxel_chunk_serialization_round_trips_overrides)
{
    game_voxel_chunk value;
    game_voxel_chunk restored_value;
    game_voxel_block_override override_value;
    ft_byte_buffer buffer;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize());
    value.set_biome_id(2U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(6, 70, 4, 10U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(6, 70, 4, 12U));
    FT_ASSERT_EQ(1U, value.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.serialize(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.deserialize(buffer));
    FT_ASSERT_EQ(2U, restored_value.get_biome_id());
    FT_ASSERT_EQ(1U, restored_value.get_revision());
    FT_ASSERT_EQ(1U, restored_value.get_player_override_count());
    FT_ASSERT_EQ(FT_TRUE, restored_value.is_block_player_modified(6, 70, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.read_block(6, 70, 4,
        &block_id));
    FT_ASSERT_EQ(12U, block_id);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_value.get_player_override(0U,
        &override_value));
    FT_ASSERT_EQ(10U, override_value.generated_block_id);
    FT_ASSERT_EQ(12U, override_value.current_block_id);
    return (1);
}

FT_TEST(test_game_voxel_chunk_revision_changes_only_for_player_changes)
{
    game_voxel_chunk value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize());
    FT_ASSERT_EQ(0U, value.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_generated_block(0, 0, 0, 1U));
    FT_ASSERT_EQ(0U, value.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(0, 0, 0, 2U));
    FT_ASSERT_EQ(1U, value.get_revision());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.write_block(0, 0, 0, 1U));
    FT_ASSERT_EQ(2U, value.get_revision());
    return (1);
}

FT_TEST(test_game_voxel_chunk_migrates_version_four_history)
{
    game_voxel_chunk source_chunk;
    game_voxel_chunk restored_chunk;
    ft_byte_buffer serialized;
    ft_byte_buffer legacy;
    const uint8_t *serialized_data;
    ft_size_t legacy_prefix_size;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_generated_block(
        3, 4, 5, 77U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.serialize(serialized));
    legacy_prefix_size = serialized.size() - 12U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized.view(0U, serialized.size(),
        &serialized_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.append(serialized_data, 4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.append_u32_le(4U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.append(serialized_data + 8U,
        legacy_prefix_size - 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, legacy.append_u32_le(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.deserialize(legacy));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, restored_chunk.read_block(3, 4, 5,
        &block_id));
    FT_ASSERT_EQ(77U, block_id);
    FT_ASSERT_EQ(0U, restored_chunk.get_player_override_count());
    FT_ASSERT_EQ(0U, restored_chunk.get_revision());
    return (1);
}

FT_TEST(test_game_voxel_chunk_deserialize_failure_preserves_destination)
{
    game_voxel_chunk destination_chunk;
    game_voxel_chunk source_chunk;
    ft_byte_buffer serialized;
    ft_byte_buffer truncated;
    const uint8_t *serialized_data;
    uint32_t block_id;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.write_block(
        1, 2, 3, 91U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.write_generated_block(
        4, 5, 6, 92U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_chunk.serialize(serialized));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, serialized.view(0U,
        serialized.size() - 1U, &serialized_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, truncated.append(serialized_data,
        serialized.size() - 1U));
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, destination_chunk.deserialize(truncated));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_chunk.read_block(1, 2, 3,
        &block_id));
    FT_ASSERT_EQ(91U, block_id);
    return (1);
}
