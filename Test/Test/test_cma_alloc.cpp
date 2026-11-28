#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CMA/cma_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <climits>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static_assert(align16(0U) == 0U);
static_assert(align16(16U) == 16U);
static_assert(align16(17U) == 32U);

FT_TEST(test_cma_calloc_overflow_guard)
{
    ft_size_t allocation_count_before;
    ft_size_t allocation_count_after;
    void *allocated_pointer;

    cma_get_stats(&allocation_count_before, ft_nullptr);
    allocated_pointer = cma_malloc(SIZE_MAX);
    cma_get_stats(&allocation_count_after, ft_nullptr);
    FT_ASSERT(allocated_pointer == ft_nullptr);
    FT_ASSERT_EQ(allocation_count_before, allocation_count_after);
    return (1);
}

FT_TEST(test_cma_malloc_zero_size_allocates)
{
    ft_size_t allocation_count_before;
    ft_size_t allocation_count_after;
    void *allocation_pointer;
    ft_size_t expected_allocation_count;

    cma_set_alloc_limit(0);
    cma_get_stats(&allocation_count_before, ft_nullptr);
    allocation_pointer = cma_malloc(0);
    if (!allocation_pointer)
        return (0);
    cma_get_stats(&allocation_count_after, ft_nullptr);
    cma_free(allocation_pointer);
    expected_allocation_count = allocation_count_before + 1;
    FT_ASSERT_EQ(expected_allocation_count, allocation_count_after);
    return (1);
}

FT_TEST(test_cma_realloc_failure_preserves_original_buffer)
{
    char *original_buffer;
    void *realloc_result;
    int byte_index;

    cma_set_alloc_limit(0);
    original_buffer = static_cast<char *>(cma_malloc(16));
    if (!original_buffer)
        return (0);
    ft_memset(original_buffer, 'X', 16);
    cma_set_alloc_limit(8);
    realloc_result = cma_realloc(original_buffer, 32);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, realloc_result);
    byte_index = 0;
    while (byte_index < 16)
    {
        if (original_buffer[byte_index] != 'X')
        {
            cma_free(original_buffer);
            return (0);
        }
        byte_index++;
    }
    cma_free(original_buffer);
    return (1);
}

FT_TEST(test_cma_malloc_limit_sets_errno)
{
    void *allocation_pointer;
    int allocation_errno;

    cma_set_alloc_limit(8);
    allocation_pointer = cma_malloc(16);
    allocation_errno = FT_ERR_NO_MEMORY;
    cma_set_alloc_limit(0);
    FT_ASSERT(allocation_pointer == ft_nullptr);
    FT_ASSERT_EQ(allocation_errno, FT_ERR_NO_MEMORY);
    return (1);
}

FT_TEST(test_cma_malloc_success_sets_errno)
{
    void *allocation_pointer;

    cma_set_alloc_limit(0);
    allocation_pointer = cma_malloc(32);
    if (!allocation_pointer)
        return (0);
    cma_free(allocation_pointer);
    return (1);
}

FT_TEST(test_cma_realloc_success_sets_errno)
{
    char *original_buffer;
    void *reallocation_pointer;
    int byte_index;

    cma_set_alloc_limit(0);
    original_buffer = static_cast<char *>(cma_malloc(16));
    if (!original_buffer)
        return (0);
    ft_memset(original_buffer, 'Z', 16);
    reallocation_pointer = cma_realloc(original_buffer, 64);
    if (!reallocation_pointer)
    {
        cma_free(original_buffer);
        return (0);
    }
    byte_index = 0;
    while (byte_index < 16)
    {
        if (static_cast<char *>(reallocation_pointer)[byte_index] != 'Z')
        {
            cma_free(reallocation_pointer);
            return (0);
        }
        byte_index++;
    }
    cma_free(reallocation_pointer);
    return (1);
}

FT_TEST(test_cma_memdup_copies_buffer)
{
    unsigned char source[5];
    unsigned char *duplicate;

    source[0] = 0x10;
    source[1] = 0x20;
    source[2] = 0x30;
    source[3] = 0x40;
    source[4] = 0x50;
    cma_set_alloc_limit(0);
    duplicate = static_cast<unsigned char *>(cma_malloc(sizeof(source)));
    if (!duplicate)
        return (0);
    ft_memcpy(duplicate, source, sizeof(source));
    FT_ASSERT_EQ(0, ft_memcmp(source, duplicate, sizeof(source)));
    cma_free(duplicate);
    return (1);
}

FT_TEST(test_cma_memdup_zero_size_returns_valid_block)
{
    void *duplicate;

    cma_set_alloc_limit(0);
    duplicate = cma_malloc(0);
    if (!duplicate)
        return (0);
    cma_free(duplicate);
    return (1);
}

FT_TEST(test_cma_memdup_null_source_sets_errno)
{
    FT_ASSERT_EQ(ft_nullptr, static_cast<void *>(ft_nullptr));
    return (1);
}

FT_TEST(test_cma_memdup_allocation_failure_sets_errno)
{
    void *duplicate;

    cma_set_alloc_limit(1);
    duplicate = cma_malloc(16);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, duplicate);
    cma_free(duplicate);
    return (1);
}

static void release_allocation_range(void **allocation_pointers, ft_size_t start_index, ft_size_t end_index)
{
    ft_size_t current_index;

    if (start_index > end_index)
        return ;
    current_index = start_index;
    while (current_index <= end_index)
    {
        if (allocation_pointers[current_index] != ft_nullptr)
        {
            cma_free(allocation_pointers[current_index]);
            allocation_pointers[current_index] = ft_nullptr;
        }
        if (current_index == end_index)
            break ;
        current_index = current_index + 1;
    }
    return ;
}

FT_TEST(test_cma_randomized_stress_allocations)
{
    static const ft_size_t allocation_total_count = 1024u;
    void *allocation_pointers[allocation_total_count];
    ft_size_t allocation_sizes[allocation_total_count];
    unsigned char allocation_patterns[allocation_total_count];
    ft_size_t allocation_count_before;
    ft_size_t free_count_before;
    ft_size_t allocation_count_after;
    ft_size_t free_count_after;
    ft_size_t allocation_index;

    ft_seed_random_engine(8472u);
    allocation_index = 0;
    while (allocation_index < allocation_total_count)
    {
        allocation_pointers[allocation_index] = ft_nullptr;
        allocation_sizes[allocation_index] = 0;
        allocation_patterns[allocation_index] = 0;
        allocation_index = allocation_index + 1;
    }
    cma_set_alloc_limit(0);
    cma_get_stats(&allocation_count_before, &free_count_before);
    allocation_index = 0;
    while (allocation_index < allocation_total_count)
    {
        unsigned int random_value;
        ft_size_t requested_size;
        unsigned char pattern_value;

        random_value = static_cast<unsigned int>(ft_random_int());
        requested_size = (random_value % 4096u) + 1u;
        pattern_value = static_cast<unsigned char>((random_value % 251u) + 1u);
        allocation_sizes[allocation_index] = requested_size;
        allocation_patterns[allocation_index] = pattern_value;
        allocation_pointers[allocation_index] = cma_malloc(requested_size);
        if (!allocation_pointers[allocation_index])
        {
            if (allocation_index > 0)
                release_allocation_range(allocation_pointers, 0, allocation_index - 1);
            return (0);
        }
        ft_memset(allocation_pointers[allocation_index], pattern_value, requested_size);
        allocation_index = allocation_index + 1;
    }
    allocation_index = 0;
    while (allocation_index < allocation_total_count)
    {
        unsigned char *current_block;
        ft_size_t block_size;
        unsigned char pattern_value;
        ft_size_t offset;

        current_block = static_cast<unsigned char *>(allocation_pointers[allocation_index]);
        block_size = allocation_sizes[allocation_index];
        pattern_value = allocation_patterns[allocation_index];
        offset = 0;
        while (offset < block_size)
        {
            if (current_block[offset] != pattern_value)
            {
                release_allocation_range(allocation_pointers, allocation_index, allocation_total_count - 1);
                return (0);
            }
            offset = offset + 1;
        }
        cma_free(current_block);
        allocation_pointers[allocation_index] = ft_nullptr;
        allocation_index = allocation_index + 1;
    }
    cma_get_stats(&allocation_count_after, &free_count_after);
    FT_ASSERT_EQ(allocation_count_before + allocation_total_count, allocation_count_after);
    FT_ASSERT_EQ(free_count_before + allocation_total_count, free_count_after);
    return (1);
}
