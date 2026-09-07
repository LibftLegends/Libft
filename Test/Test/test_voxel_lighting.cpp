#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef GAME_USE_VOXEL_REGION_BACKEND

#include "../../Modules/Voxel/voxel_lighting.hpp"
#include "../../Modules/Voxel/voxel_api.hpp"
#include "../../Modules/Voxel/voxel_types.hpp"

struct voxel_lighting_lookup_context
{
    ft_bool has_opaque_roof;
    int32_t roof_height;
    ft_bool has_side_opening;
    int32_t opening_x;
    int32_t opening_z;
    ft_bool has_emitter = FT_FALSE;
    int32_t emitter_x = 0;
    int32_t emitter_y = 0;
    int32_t emitter_z = 0;
    int32_t second_emitter_x = 0;
    int32_t second_emitter_y = 0;
    int32_t second_emitter_z = 0;
};

static int32_t voxel_lighting_lookup_block(void *user_data,
    int32_t world_x, int32_t world_y, int32_t world_z,
    uint32_t *block_id) noexcept
{
    voxel_lighting_lookup_context *context;

    (void)world_x;
    (void)world_z;
    if (block_id == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    context = static_cast<voxel_lighting_lookup_context *>(user_data);
    if (context != ft_nullptr && context->has_emitter == FT_TRUE
        && ((world_x == context->emitter_x
                && world_y == context->emitter_y
                && world_z == context->emitter_z)
            || (world_x == context->second_emitter_x
                && world_y == context->second_emitter_y
                && world_z == context->second_emitter_z)))
        *block_id = VOXEL_GENERATOR_SHIMMER_STONE_BLOCK;
    else if (context != ft_nullptr && context->has_opaque_roof == FT_TRUE
        && world_y == context->roof_height
        && (context->has_side_opening == FT_FALSE
            || world_x != context->opening_x
            || world_z != context->opening_z))
        *block_id = VOXEL_GENERATOR_STONE_BLOCK;
    else
        *block_id = VOXEL_GENERATOR_AIR_BLOCK;
    return (FT_ERR_SUCCESS);
}

static ft_bool voxel_lighting_chunks_equal(const voxel_light_chunk &left,
    const voxel_light_chunk &right) noexcept
{
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    local_z = 0;
    while (local_z < 16)
    {
        local_y = 0;
        while (local_y < 256)
        {
            local_x = 0;
            while (local_x < 16)
            {
                if (left.get(local_x, local_y, local_z)
                    != right.get(local_x, local_y, local_z))
                    return (FT_FALSE);
                local_x += 1;
            }
            local_y += 1;
        }
        local_z += 1;
    }
    return (FT_TRUE);
}

FT_TEST(test_voxel_lighting_pack_unpack_clamping_and_combined_darkening)
{
    uint8_t packed_light;

    packed_light = voxel_light_pack(12U, 7U);
    FT_ASSERT_EQ(static_cast<uint8_t>(0x7CU), packed_light);
    FT_ASSERT_EQ(static_cast<uint8_t>(12U), voxel_light_sky(packed_light));
    FT_ASSERT_EQ(static_cast<uint8_t>(7U), voxel_light_block(packed_light));
    packed_light = voxel_light_pack(16U, 255U);
    FT_ASSERT_EQ(static_cast<uint8_t>(0xFFU), packed_light);
    FT_ASSERT_EQ(static_cast<uint8_t>(15U), voxel_light_sky(packed_light));
    FT_ASSERT_EQ(static_cast<uint8_t>(15U), voxel_light_block(packed_light));
    packed_light = voxel_light_pack(12U, 7U);
    FT_ASSERT_EQ(static_cast<uint8_t>(12U),
        voxel_light_combined(packed_light, 0U));
    FT_ASSERT_EQ(static_cast<uint8_t>(7U),
        voxel_light_combined(packed_light, 5U));
    FT_ASSERT_EQ(static_cast<uint8_t>(7U),
        voxel_light_combined(packed_light, 15U));
    FT_ASSERT_EQ(static_cast<uint8_t>(2U),
        voxel_light_combined(voxel_light_pack(4U, 0U), 2U));
    return (1);
}

FT_TEST(test_voxel_lighting_update_config_defaults_and_validation)
{
    voxel_light_update_config config;

    voxel_light_update_config_defaults(config);
    FT_ASSERT_EQ(32U, config.min_nodes_per_frame);
    FT_ASSERT_EQ(128U, config.target_nodes_per_frame);
    FT_ASSERT_EQ(512U, config.max_nodes_per_frame);
    FT_ASSERT_EQ(static_cast<uint64_t>(1000U),
        config.time_budget_microseconds);
    FT_ASSERT_EQ(FT_TRUE, voxel_light_update_config_is_valid(config));
    config.min_nodes_per_frame = 0U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    config.min_nodes_per_frame = 129U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    voxel_light_update_config_defaults(config);
    config.target_nodes_per_frame = 513U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    voxel_light_update_config_defaults(config);
    config.max_nodes_per_frame = 1048577U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    voxel_light_update_config_defaults(config);
    config.time_budget_microseconds = 0U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    config.time_budget_microseconds = 1000001U;
    FT_ASSERT_EQ(FT_FALSE, voxel_light_update_config_is_valid(config));
    config.min_nodes_per_frame = 1048576U;
    config.target_nodes_per_frame = 1048576U;
    config.max_nodes_per_frame = 1048576U;
    config.time_budget_microseconds = 1000000U;
    FT_ASSERT_EQ(FT_TRUE, voxel_light_update_config_is_valid(config));
    return (1);
}

FT_TEST(test_voxel_lighting_metadata_contract_and_edit_equivalence)
{
    const voxel_block_metadata &stone_metadata =
        voxel_get_block_metadata(VOXEL_GENERATOR_STONE_BLOCK);
    const voxel_block_metadata &water_metadata =
        voxel_get_block_metadata(VOXEL_GENERATOR_WATER_BLOCK);
    voxel_light_chunk edited_light;
    voxel_light_chunk rebuilt_light;
    voxel_light_build_operation operation;
    voxel_light_update_config update_config;
    voxel_lighting_lookup_context context;
    ft_bool complete;
    uint32_t step_count;

    FT_ASSERT_EQ(stone_metadata.light_attenuation,
        voxel_block_light_attenuation(VOXEL_GENERATOR_STONE_BLOCK));
    FT_ASSERT_EQ(water_metadata.light_attenuation,
        voxel_block_light_attenuation(VOXEL_GENERATOR_WATER_BLOCK));
    FT_ASSERT(stone_metadata.light_attenuation <= 15U);
    FT_ASSERT(water_metadata.light_attenuation <= 15U);

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_TRUE;
    context.opening_x = -1;
    context.opening_z = 8;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.initialize(edited_light, 0, 0,
        voxel_lighting_lookup_block, &context, FT_TRUE));
    voxel_light_update_config_defaults(update_config);
    update_config.min_nodes_per_frame = 512U;
    update_config.target_nodes_per_frame = 512U;
    update_config.max_nodes_per_frame = 512U;
    complete = FT_FALSE;
    step_count = 0U;
    while (complete == FT_FALSE && step_count < 20000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.step(update_config,
            ft_nullptr, &complete));
        step_count += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, complete);
    /* Rebuild from the resulting edited state. It must be identical to the
     * incremental-path result rather than depending on prior light storage. */
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk(rebuilt_light,
        0, 0, voxel_lighting_lookup_block, &context));
    FT_ASSERT_EQ(FT_TRUE, voxel_lighting_chunks_equal(edited_light,
        rebuilt_light));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rebuilt_light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, edited_light.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_local_all_air_has_direct_skylight)
{
    voxel_light_chunk light_chunk;
    voxel_lighting_lookup_context context;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    context.has_opaque_roof = FT_FALSE;
    context.roof_height = 0;
    context.has_side_opening = FT_FALSE;
    context.opening_x = 0;
    context.opening_z = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(light_chunk,
        64, -32, voxel_lighting_lookup_block, &context));
    local_y = 0;
    while (local_y < 256)
    {
        local_z = 0;
        while (local_z < 16)
        {
            local_x = 0;
            while (local_x < 16)
            {
                FT_ASSERT_EQ(static_cast<uint8_t>(15U),
                    voxel_light_sky(light_chunk.get(local_x, local_y,
                        local_z)));
                FT_ASSERT_EQ(static_cast<uint8_t>(0U),
                    voxel_light_block(light_chunk.get(local_x, local_y,
                        local_z)));
                local_x += 1;
            }
            local_z += 1;
        }
        local_y += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_local_opaque_roof_completely_occludes_skylight)
{
    voxel_light_chunk light_chunk;
    voxel_lighting_lookup_context context;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_FALSE;
    context.opening_x = 0;
    context.opening_z = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(light_chunk,
        0, 0, voxel_lighting_lookup_block, &context));
    local_z = 0;
    while (local_z < 16)
    {
        local_x = 0;
        while (local_x < 16)
        {
            local_y = 0;
            while (local_y <= context.roof_height)
            {
                FT_ASSERT_EQ(static_cast<uint8_t>(0U),
                    voxel_light_sky(light_chunk.get(local_x, local_y,
                        local_z)));
                local_y += 1;
            }
            FT_ASSERT_EQ(static_cast<uint8_t>(15U),
                voxel_light_sky(light_chunk.get(local_x,
                    context.roof_height + 1, local_z)));
            local_x += 1;
        }
        local_z += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_multiple_emitters_use_maximum_propagated_value)
{
    voxel_light_chunk light_chunk;
    voxel_lighting_lookup_context context;

    context.has_opaque_roof = FT_FALSE;
    context.roof_height = 0;
    context.has_side_opening = FT_FALSE;
    context.opening_x = 0;
    context.opening_z = 0;
    context.has_emitter = FT_TRUE;
    context.emitter_x = 4;
    context.emitter_y = 128;
    context.emitter_z = 8;
    context.second_emitter_x = 8;
    context.second_emitter_y = 128;
    context.second_emitter_z = 8;
    FT_ASSERT_EQ(FT_TRUE, voxel_block_emits_light(
        VOXEL_GENERATOR_SHIMMER_STONE_BLOCK));
    FT_ASSERT_EQ(static_cast<uint8_t>(15U), voxel_block_emitted_light_level(
        VOXEL_GENERATOR_SHIMMER_STONE_BLOCK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(light_chunk,
        0, 0, voxel_lighting_lookup_block, &context));
    FT_ASSERT_EQ(static_cast<uint8_t>(15U), voxel_light_block(
        light_chunk.get(4, 128, 8)));
    FT_ASSERT_EQ(static_cast<uint8_t>(14U), voxel_light_block(
        light_chunk.get(5, 128, 8)));
    FT_ASSERT_EQ(static_cast<uint8_t>(13U), voxel_light_block(
        light_chunk.get(6, 128, 8)));
    FT_ASSERT_EQ(static_cast<uint8_t>(14U), voxel_light_block(
        light_chunk.get(7, 128, 8)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_local_build_is_deterministic_with_meaningful_stats)
{
    voxel_light_chunk first_light_chunk;
    voxel_light_chunk second_light_chunk;
    voxel_lighting_lookup_context context;
    voxel_light_build_stats first_stats;
    voxel_light_build_stats second_stats;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_FALSE;
    context.opening_x = 0;
    context.opening_z = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(
        first_light_chunk, 0, 0, voxel_lighting_lookup_block, &context,
        &first_stats));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(
        second_light_chunk, 0, 0, voxel_lighting_lookup_block, &context,
        &second_stats));
    FT_ASSERT_EQ(static_cast<uint64_t>(16U * 256U * 16U),
        first_stats.scanned_cells);
    FT_ASSERT(first_stats.propagated_cells > 0U);
    FT_ASSERT(first_stats.queue_peak > 0U);
    FT_ASSERT_EQ(first_stats.scanned_cells, second_stats.scanned_cells);
    FT_ASSERT_EQ(first_stats.propagated_cells, second_stats.propagated_cells);
    FT_ASSERT_EQ(first_stats.queue_peak, second_stats.queue_peak);
    local_y = 0;
    while (local_y < 256)
    {
        local_z = 0;
        while (local_z < 16)
        {
            local_x = 0;
            while (local_x < 16)
            {
                FT_ASSERT_EQ(first_light_chunk.get(local_x, local_y, local_z),
                    second_light_chunk.get(local_x, local_y, local_z));
                local_x += 1;
            }
            local_z += 1;
        }
        local_y += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_light_chunk.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_cave_opening_propagates_from_neighbor_and_stops_at_radius)
{
    voxel_light_chunk light_chunk;
    voxel_lighting_lookup_context context;
    uint8_t near_opening;
    uint8_t beyond_opening;

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_TRUE;
    context.opening_x = -1;
    context.opening_z = 8;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk(light_chunk,
        0, 0, voxel_lighting_lookup_block, &context));
    near_opening = voxel_light_sky(light_chunk.get(0, 127, 8));
    beyond_opening = voxel_light_sky(light_chunk.get(15, 127, 8));
    FT_ASSERT_EQ(static_cast<uint8_t>(14U), near_opening);
    FT_ASSERT_EQ(static_cast<uint8_t>(0U), beyond_opening);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_build_operation_is_bounded_and_reconstructs_build)
{
    voxel_light_chunk expected_light;
    voxel_light_chunk actual_light;
    voxel_light_build_operation operation;
    voxel_light_update_config configuration;
    voxel_light_build_stats expected_stats;
    voxel_light_build_stats actual_stats;
    voxel_lighting_lookup_context context;
    ft_bool complete;
    uint32_t step_count;
    int32_t local_x;
    int32_t local_y;
    int32_t local_z;

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_TRUE;
    context.opening_x = -1;
    context.opening_z = 8;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk(expected_light,
        0, 0, voxel_lighting_lookup_block, &context, &expected_stats));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.initialize(actual_light, 0, 0,
        voxel_lighting_lookup_block, &context, FT_TRUE));
    voxel_light_update_config_defaults(configuration);
    configuration.min_nodes_per_frame = 512U;
    configuration.target_nodes_per_frame = 512U;
    configuration.max_nodes_per_frame = 512U;
    step_count = 0U;
    complete = FT_FALSE;
    while (complete == FT_FALSE && step_count < 20000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.step(configuration,
            &actual_stats, &complete));
        step_count += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, complete);
    FT_ASSERT(step_count > 1U);
    FT_ASSERT_EQ(expected_stats.scanned_cells, actual_stats.scanned_cells);
    FT_ASSERT_EQ(expected_stats.propagated_cells,
        actual_stats.propagated_cells);
    local_y = 0;
    while (local_y < 256)
    {
        local_z = 0;
        while (local_z < 16)
        {
            local_x = 0;
            while (local_x < 16)
            {
                FT_ASSERT_EQ(expected_light.get(local_x, local_y, local_z),
                    actual_light.get(local_x, local_y, local_z));
                local_x += 1;
            }
            local_z += 1;
        }
        local_y += 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual_light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_light.destroy());
    return (1);
}

FT_TEST(test_voxel_lighting_build_operation_respects_slice_bound)
{
    voxel_light_chunk light_chunk;
    voxel_light_chunk expected_light;
    voxel_light_build_operation operation;
    voxel_light_update_config configuration;
    voxel_light_build_stats stats;
    voxel_lighting_lookup_context context;
    ft_bool complete;
    uint64_t previous_work;
    uint64_t current_work;
    uint32_t step_count;

    context.has_opaque_roof = FT_TRUE;
    context.roof_height = 128;
    context.has_side_opening = FT_FALSE;
    context.opening_x = 0;
    context.opening_z = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.initialize(light_chunk, 0, 0,
        voxel_lighting_lookup_block, &context, FT_FALSE));
    voxel_light_update_config_defaults(configuration);
    configuration.min_nodes_per_frame = 8U;
    configuration.target_nodes_per_frame = 8U;
    configuration.max_nodes_per_frame = 8U;
    complete = FT_FALSE;
    previous_work = 0U;
    step_count = 0U;
    while (complete == FT_FALSE && step_count < 100000U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.step(configuration, &stats,
            &complete));
        current_work = stats.scanned_cells + stats.propagated_cells;
        FT_ASSERT(current_work >= previous_work);
        FT_ASSERT(current_work - previous_work <= 8U);
        previous_work = current_work;
        step_count += 1U;
    }
    FT_ASSERT_EQ(FT_TRUE, complete);
    FT_ASSERT(step_count > 1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_light_build_chunk_local(expected_light,
        0, 0, voxel_lighting_lookup_block, &context));
    FT_ASSERT_EQ(FT_TRUE, voxel_lighting_chunks_equal(light_chunk,
        expected_light));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, operation.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected_light.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, light_chunk.destroy());
    return (1);
}

#endif
