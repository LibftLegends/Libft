#include "../test_internal.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int vector_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_vector_get_error_returned = FT_FALSE;
static int32_t g_vector_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_vector_get_error_str_returned = FT_FALSE;
static const char *g_vector_get_error_str_result = ft_nullptr;

static void vector_get_error_uninitialised_operation(void)
{
    ft_vector<int> vector_value;

    g_vector_get_error_result = vector_value.get_error();
    g_vector_get_error_returned = FT_TRUE;
    return ;
}

static void vector_get_error_str_uninitialised_operation(void)
{
    ft_vector<int> vector_value;

    g_vector_get_error_str_result = vector_value.get_error_str();
    g_vector_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_ft_vector_ft_string_growth)
{
    ft_size_t allocation_count_before = 0;
    ft_size_t free_count_before = 0;
    cma_get_stats(&allocation_count_before, &free_count_before);
    {
        ft_vector<ft_string> string_vector;
        const char *sample_values[] = {
            "alpha",
            "bravo",
            "charlie",
            "delta",
            "echo",
            "foxtrot",
            "golf",
            "hotel",
            "india"
        };
        const ft_size_t sample_count = sizeof(sample_values) / sizeof(sample_values[0]);
        size_t push_index = 0;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, string_vector.initialize());
        while (push_index < sample_count)
        {
            ft_string stored_value;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, stored_value.initialize(sample_values[push_index]));
            FT_ASSERT_EQ(FT_ERR_SUCCESS, string_vector.push_back(stored_value));
            ++push_index;
        }
        FT_ASSERT_EQ(sample_count, string_vector.size());
        FT_ASSERT(string_vector.capacity() >= sample_count);
        size_t verify_index = 0;
        while (verify_index < sample_count)
        {
            FT_ASSERT(string_vector[verify_index] == sample_values[verify_index]);
            ++verify_index;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, string_vector.get_error());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, string_vector.destroy());
    }
    ft_size_t allocation_count_after = 0;
    ft_size_t free_count_after = 0;
    cma_get_stats(&allocation_count_after, &free_count_after);
    FT_ASSERT(allocation_count_after >= allocation_count_before);
    FT_ASSERT(free_count_after >= free_count_before);
    ft_size_t allocation_delta = allocation_count_after - allocation_count_before;
    ft_size_t free_delta = free_count_after - free_count_before;
    FT_ASSERT_EQ(allocation_delta, free_delta);
    return (1);
}

FT_TEST(test_ft_vector_small_buffer_inline_capacity)
{
    ft_size_t allocation_count_before = 0;
    ft_size_t free_count_before = 0;
    cma_get_stats(&allocation_count_before, &free_count_before);
    {
        ft_vector<int> inline_vector;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, inline_vector.initialize());
        size_t inline_capacity = inline_vector.capacity();
        FT_ASSERT(inline_capacity > 0);
        size_t push_index = 0;
        while (push_index < inline_capacity)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, inline_vector.push_back(static_cast<int>(push_index)));
            ++push_index;
        }
        ft_size_t allocation_count_mid = 0;
        ft_size_t free_count_mid = 0;
        cma_get_stats(&allocation_count_mid, &free_count_mid);
        FT_ASSERT_EQ(allocation_count_before, allocation_count_mid);
        FT_ASSERT_EQ(free_count_before, free_count_mid);
        size_t verify_index = 0;
        while (verify_index < inline_vector.size())
        {
            FT_ASSERT_EQ(static_cast<int>(verify_index), inline_vector[verify_index]);
            ++verify_index;
        }
        FT_ASSERT_EQ(inline_vector.capacity(), inline_capacity);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, inline_vector.destroy());
    }
    ft_size_t allocation_count_after = 0;
    ft_size_t free_count_after = 0;
    cma_get_stats(&allocation_count_after, &free_count_after);
    FT_ASSERT_EQ(allocation_count_before, allocation_count_after);
    FT_ASSERT_EQ(free_count_before, free_count_after);
    return (1);
}

FT_TEST(test_ft_vector_reuses_requested_capacity_and_releases_unused_storage)
{
    ft_size_t allocations_before = 0U;
    ft_size_t frees_before = 0U;
    cma_get_stats(&allocations_before, &frees_before);
    {
        ft_vector<uint32_t> unused_vector(64U);
        ft_vector<uint32_t> values(64U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
        FT_ASSERT(values.capacity() >= 64U);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.push_back(42U));
        FT_ASSERT_EQ(42U, values[0]);
    }
    ft_size_t allocations_after = 0U;
    ft_size_t frees_after = 0U;
    cma_get_stats(&allocations_after, &frees_after);
    FT_ASSERT_EQ(2U, allocations_after - allocations_before);
    FT_ASSERT_EQ(2U, frees_after - frees_before);
    return (1);
}

FT_TEST(test_ft_vector_reserve_preserves_trivial_elements_across_heap_growth)
{
    ft_vector<uint64_t> values;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    ft_size_t index = 0U;
    while (index < 65U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.push_back(index * 17U));
        ++index;
    }
    const ft_size_t capacity = values.capacity();
    values.reserve(FT_SYSTEM_SIZE_MAX / sizeof(uint64_t) + 1U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, values.get_error());
    FT_ASSERT_EQ(capacity, values.capacity());
    FT_ASSERT_EQ(65U, values.size());
    index = 0U;
    while (index < values.size())
    {
        FT_ASSERT_EQ(index * 17U, values[index]);
        ++index;
    }
    return (1);
}

FT_TEST(test_ft_vector_append_keeps_aliased_ranges_valid_across_growth)
{
    constexpr uint32_t VALUES[] = {3U, 1U, 4U, 1U, 5U, 9U};
    ft_vector<uint32_t> vector;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.append(VALUES, 6U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.append(vector.begin() + 1U, 6U - 1U));
    FT_ASSERT_EQ(11U, vector.size());
    FT_ASSERT_EQ(1U, vector[6]);
    FT_ASSERT_EQ(9U, vector[10]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.append(vector.begin() + 5U, 6U));
    FT_ASSERT_EQ(17U, vector.size());
    FT_ASSERT_EQ(9U, vector[11]);
    FT_ASSERT_EQ(9U, vector[16]);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, vector.append(vector.begin() + 16U, 2U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, vector.append(ft_nullptr, 1U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, vector.append(VALUES, FT_SYSTEM_SIZE_MAX));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector.append(ft_nullptr, 0U));
    FT_ASSERT_EQ(17U, vector.size());
    return (1);
}

FT_TEST(test_ft_vector_move_method_transfers_elements_and_thread_safety)
{
    ft_vector<int> source_vector;
    ft_vector<int> destination_vector;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_vector.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_vector.push_back(10));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_vector.push_back(20));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_vector.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_vector.move(source_vector));
    FT_ASSERT_EQ(2U, destination_vector.size());
    FT_ASSERT_EQ(10, destination_vector[0]);
    FT_ASSERT_EQ(20, destination_vector[1]);
    FT_ASSERT_EQ(FT_TRUE, destination_vector.is_thread_safe());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, source_vector.is_initialised());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_vector.destroy());
    return (1);
}

FT_TEST(test_ft_vector_error_queries_follow_lifecycle_contract)
{
    ft_vector<int> vector_value;

    g_vector_get_error_returned = FT_FALSE;
    g_vector_get_error_result = FT_ERR_SUCCESS;
    g_vector_get_error_str_returned = FT_FALSE;
    g_vector_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, vector_expect_sigabrt(
        vector_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_vector_get_error_returned);
    FT_ASSERT_EQ(1, vector_expect_sigabrt(
        vector_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_vector_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, vector_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(vector_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}
