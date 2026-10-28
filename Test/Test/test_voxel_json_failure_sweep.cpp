#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Voxel/voxel_api.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "test_cma_failure_injection.hpp"

typedef int32_t (*voxel_json_sweep_serializer)(void *instance,
    ft_string &output) noexcept;

#define VOXEL_JSON_SWEEP_ADAPTER(adapter_name, class_name) \
    static int32_t adapter_name(void *instance, ft_string &output) noexcept \
    { \
        return (static_cast<class_name *>(instance)->serialize_json(output)); \
    }

VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_biome,
    voxel_biome_definition)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_feature, voxel_feature_rule)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_ore, voxel_ore_rule)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_underground,
    voxel_underground_structure_config)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_fluid, voxel_fluid_config)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_layer, voxel_layer_config)
VOXEL_JSON_SWEEP_ADAPTER(voxel_json_sweep_generation,
    voxel_generation_config)

static int32_t voxel_json_sweep_allocation_failures(void *instance,
    voxel_json_sweep_serializer serializer) noexcept
{
    ft_string output;
    test_cma_failure_controller controller;
    test_cma_failure_operation operations[3] = {
        TEST_CMA_FAILURE_ALLOCATE, TEST_CMA_FAILURE_REALLOCATE,
        TEST_CMA_FAILURE_ALIGNED_ALLOCATE};
    ft_size_t operation_counts[3];
    ft_size_t operation_index;
    ft_size_t operation_count;
    ft_size_t safety_mode;
    ft_size_t failure_ordinal;
    ft_size_t leaked_allocations;
    int32_t result;
    int32_t cleanup_result;
    ft_bool output_unchanged;

    result = test_cma_failure_controller_initialize(controller);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = test_cma_failure_controller_begin(controller);
    if (result != FT_ERR_SUCCESS)
        return (result);
    result = serializer(instance, output);
    operation_index = 0U;
    operation_count = 0U;
    while (operation_index < 3U)
    {
        operation_counts[operation_index]
            = test_cma_failure_controller_attempt_count(controller,
                operations[operation_index]);
        operation_count += operation_counts[operation_index];
        operation_index += 1U;
    }
    if (output.is_initialised() == FT_TRUE)
        (void)output.destroy();
    leaked_allocations = controller.allocation_slots;
    cleanup_result = test_cma_failure_controller_end(controller);
    if (result != FT_ERR_SUCCESS || operation_count == 0U
        || leaked_allocations != 0U || cleanup_result != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    safety_mode = 0U;
    while (safety_mode < 3U)
    {
        operation_index = 0U;
        while (operation_index < 3U)
        {
            failure_ordinal = 0U;
            while (failure_ordinal < operation_counts[operation_index])
            {
                result = FT_ERR_SUCCESS;
                if (safety_mode != 2U)
                    result = output.initialize("sentinel");
                if (result == FT_ERR_SUCCESS && safety_mode == 1U)
                    result = output.enable_thread_safety();
                if (result != FT_ERR_SUCCESS)
                    return (result);
                result = test_cma_failure_controller_initialize(controller);
                if (result == FT_ERR_SUCCESS)
                    result = test_cma_failure_controller_begin(controller);
                if (result == FT_ERR_SUCCESS)
                    result = test_cma_failure_controller_fail_after(controller,
                        operations[operation_index], failure_ordinal);
                if (result != FT_ERR_SUCCESS)
                {
                    (void)test_cma_failure_controller_end(controller);
                    (void)output.destroy();
                    return (result);
                }
                result = serializer(instance, output);
                output_unchanged = FT_FALSE;
                if (safety_mode == 2U)
                    output_unchanged = output.is_initialised() == FT_FALSE;
                else if (output.is_initialised() == FT_TRUE
                    && output.size() == 8U
                    && ft_memcmp(output.c_str(), "sentinel", 8U) == 0)
                    output_unchanged = FT_TRUE;
                leaked_allocations = controller.allocation_slots;
                if (output.is_initialised() == FT_TRUE)
                    (void)output.destroy();
                cleanup_result = test_cma_failure_controller_end(controller);
                if (result == FT_ERR_SUCCESS || output_unchanged == FT_FALSE
                    || leaked_allocations != 0U
                    || cleanup_result != FT_ERR_SUCCESS)
                    return (FT_ERR_INVALID_STATE);
                failure_ordinal += 1U;
            }
            operation_index += 1U;
        }
        safety_mode += 1U;
    }
    result = output.initialize("sentinel");
    if (result == FT_ERR_SUCCESS)
        result = output.enable_thread_safety();
    if (result == FT_ERR_SUCCESS)
        result = serializer(instance, output);
    if (result == FT_ERR_SUCCESS
        && (output.size() < 2U || output.c_str()[output.size() - 1U] != '\n'
            || output.c_str()[output.size() - 2U] == '\n'))
        result = FT_ERR_INVALID_STATE;
    (void)output.destroy();
    return (result);
}

FT_TEST(test_voxel_json_allocation_failure_sweep_is_transactional)
{
    voxel_generation_config generation;
    int32_t result;

    voxel_default_generation_config(generation);
    result = voxel_json_sweep_allocation_failures(&generation.biomes[0],
        voxel_json_sweep_biome);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(&generation.features[0],
        voxel_json_sweep_feature);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(&generation.ores[0],
        voxel_json_sweep_ore);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(
        &generation.underground_structures, voxel_json_sweep_underground);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(&generation.fluids,
        voxel_json_sweep_fluid);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(&generation.layers,
        voxel_json_sweep_layer);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    result = voxel_json_sweep_allocation_failures(&generation,
        voxel_json_sweep_generation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    return (1);
}

FT_TEST(test_voxel_json_replaces_empty_large_and_thread_safe_outputs)
{
    voxel_generation_config generation;
    ft_string output;
    ft_string expected;
    char large_sentinel[8193];
    uint32_t index;

    voxel_default_generation_config(generation);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize(""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.serialize_json(output));
    FT_ASSERT_EQ('\n', output.c_str()[output.size() - 1U]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected.initialize(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.serialize_json(output));
    FT_ASSERT_EQ(expected.size(), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(expected.c_str(), output.c_str(), output.size()));
    index = 0U;
    while (index < sizeof(large_sentinel) - 1U)
    {
        large_sentinel[index] = 'x';
        index += 1U;
    }
    large_sentinel[index] = '\0';
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign(large_sentinel,
        sizeof(large_sentinel) - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.serialize_json(output));
    FT_ASSERT_EQ(expected.size(), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(expected.c_str(), output.c_str(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.serialize_json(output));
    FT_ASSERT_EQ(expected.size(), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(expected.c_str(), output.c_str(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}

FT_TEST(test_voxel_json_commit_lock_failure_preserves_output)
{
    voxel_generation_config generation;
    ft_string output;
    test_cma_failure_controller controller;
    ft_size_t live_allocations;
    int32_t result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_default_generation_config(generation));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("sentinel"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, voxel_json_test_fail_commit_lock(
        FT_ERR_SYS_MUTEX_LOCK_FAILED));
    result = generation.serialize_json(output);
    voxel_json_test_clear_file_failure();
    live_allocations = controller.allocation_slots;
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, result);
    FT_ASSERT_EQ(0U, live_allocations);
    FT_ASSERT_EQ(8U, output.size());
    FT_ASSERT_EQ(0, ft_memcmp(output.c_str(), "sentinel", 8U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, generation.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    return (1);
}
