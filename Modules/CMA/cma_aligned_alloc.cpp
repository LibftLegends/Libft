#include <cstddef>
#include <cstdint>
#include <cstdlib>
#ifdef _WIN32
# include <malloc.h>
#endif
#include "../Errno/errno.hpp"
#include "CMA.hpp"
#include "cma_internal.hpp"

#include "../System_utils/system_utils.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool normalize_alignment_padding(ft_size_t *padding)
{
    ft_size_t    aligned_padding;

    if (padding == nullptr)
        return (FT_FALSE);
    if (*padding == 0)
        return (FT_TRUE);
    aligned_padding = align16(*padding);
    if (aligned_padding < *padding)
        return (FT_FALSE);
    *padding = aligned_padding;
    return (FT_TRUE);
}

static ft_size_t    calculate_alignment_padding(Block *block, ft_size_t alignment)
{
    uintptr_t    payload_address;
    uintptr_t    alignment_value;
    uintptr_t    remainder;
    ft_size_t    padding;

    if (block == nullptr)
        return (0);
    payload_address = reinterpret_cast<uintptr_t>(block->payload);
    payload_address += cma_debug_guard_size();
    alignment_value = alignment;
    if (alignment_value == 0)
        return (0);
    remainder = payload_address & (alignment_value - 1);
    if (remainder == 0)
        return (0);
    padding = alignment_value - remainder;
    return (padding);
}

static ft_bool block_supports_aligned_request(Block *block, ft_size_t aligned_size,
            ft_size_t alignment, ft_size_t *padding)
{
    ft_size_t    remaining_size;
    ft_size_t    local_padding;
    ft_size_t    minimum_payload;

    local_padding = calculate_alignment_padding(block, alignment);
    if (normalize_alignment_padding(&local_padding) == FT_FALSE)
        return (FT_FALSE);
    minimum_payload = align16(1);
    if (minimum_payload < static_cast<ft_size_t>(16))
        minimum_payload = static_cast<ft_size_t>(16);
    if (local_padding > 0)
    {
        if (block->size <= local_padding)
            return (FT_FALSE);
        remaining_size = block->size - local_padding;
        if (remaining_size < minimum_payload)
            return (FT_FALSE);
    }
    else
        remaining_size = block->size;
    if (remaining_size < aligned_size)
        return (FT_FALSE);
    *padding = local_padding;
    return (FT_TRUE);
}

static Block   *find_aligned_free_block(ft_size_t aligned_size, ft_size_t alignment,
            ft_size_t *padding)
{
    ft_size_t bin_index;
    Block   *current_block;

    bin_index = cma_free_bin_for_size(aligned_size);
    while (bin_index < CMA_FREE_BIN_COUNT)
    {
        current_block = g_cma_free_bins[bin_index];
        while (current_block)
        {
            cma_validate_block(current_block, "cma_aligned_alloc search", nullptr);
            if (cma_block_is_free(current_block))
            {
                ft_size_t   local_padding;

                if (block_supports_aligned_request(current_block, aligned_size,
                        alignment, &local_padding) == FT_TRUE)
                {
                    *padding = local_padding;
                    return (current_block);
                }
            }
            current_block = current_block->free_next;
        }
        bin_index++;
    }
    return (nullptr);
}

static void    *aligned_alloc_offswitch(ft_size_t alignment, ft_size_t request_size,
            int32_t *error_code)
{
    ft_size_t  alignment_value;
    ft_size_t  allocation_size;
    ft_size_t  remainder;
    int32_t    local_error_code;
    int32_t    *error_target;
    void    *pointer;

    local_error_code = FT_ERR_SUCCESS;
    error_target = error_code;
    if (error_target == nullptr)
        error_target = &local_error_code;
    alignment_value = alignment;
    allocation_size = request_size;
    remainder = 0;
    if (alignment_value != 0)
        remainder = allocation_size % alignment_value;
    if (remainder != 0)
        allocation_size += alignment_value - remainder;
    if (allocation_size == 0)
        allocation_size = alignment_value;
    pointer = nullptr;
#ifdef _WIN32
    pointer = _aligned_malloc(allocation_size, alignment_value);
    if (pointer)
    {
        g_cma_allocation_count++;
        *error_target = FT_ERR_SUCCESS;
    }
    else
        *error_target = FT_ERR_NO_MEMORY;
#else
    *error_target = posix_memalign(&pointer, alignment_value, allocation_size);
    if (*error_target == 0 && pointer)
    {
        g_cma_allocation_count++;
        *error_target = FT_ERR_SUCCESS;
    }
    else
        *error_target = FT_ERR_NO_MEMORY;
#endif
    cma_record_allocation_log("cma_aligned_alloc %llu (alignment %llu) -> %p",
            static_cast<unsigned long long>(allocation_size),
            static_cast<unsigned long long>(alignment), pointer);
    return (pointer);
}

static ft_size_t    compute_extended_page_request(ft_size_t aligned_size,
            ft_size_t alignment)
{
    ft_size_t    extended_size;

    if (aligned_size > FT_SYSTEM_SIZE_MAX - alignment)
        return (0);
    extended_size = aligned_size + alignment;
    return (extended_size);
}


void    *cma_aligned_alloc(ft_size_t alignment, ft_size_t size)
{
    if ((alignment & (alignment - 1)) != 0
        || alignment < sizeof(void *))
        return (nullptr);
    if (alignment > FT_SYSTEM_SIZE_MAX || size > FT_SYSTEM_SIZE_MAX)
        return (nullptr);
    ft_size_t request_size;

    if (size == 0)
        request_size = 1;
    else
        request_size = size;
    ft_size_t backend_aligned_size = align16(request_size);
    if (backend_aligned_size < request_size)
        return (nullptr);
    ft_size_t backend_limit_check_size = backend_aligned_size;
    if (alignment > backend_limit_check_size)
        backend_limit_check_size = alignment;
    if (g_cma_alloc_limit != 0 && backend_limit_check_size > g_cma_alloc_limit)
        return (nullptr);
    if (cma_backend_is_enabled())
        return (cma_backend_aligned_allocate(alignment,
                backend_aligned_size, nullptr));
    ft_size_t instrumented_size = cma_debug_allocation_size(request_size);
    if (instrumented_size < request_size)
        return (nullptr);
    ft_size_t aligned_size = align16(instrumented_size);
    if (aligned_size < instrumented_size)
        return (nullptr);
    if (OFFSWITCH == 1)
        return (aligned_alloc_offswitch(alignment, request_size, nullptr));
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = cma_lock_allocator(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (nullptr);
    void *arena_pointer = cma_small_arena_aligned_allocate_locked(alignment,
            request_size);
    if (arena_pointer != nullptr)
    {
        ft_size_t arena_size = cma_small_arena_block_size_locked(arena_pointer);

        g_cma_allocation_count++;
        g_cma_current_bytes += arena_size;
        if (g_cma_current_bytes > g_cma_peak_bytes)
            g_cma_peak_bytes = g_cma_current_bytes;
        cma_unlock_allocator(lock_acquired);
        cma_record_allocation_log(
                "cma_aligned_alloc %llu (alignment %llu) -> %p",
                static_cast<unsigned long long>(request_size),
                static_cast<unsigned long long>(alignment), arena_pointer);
        return (arena_pointer);
    }
    ft_size_t padding = 0;
    Block *block = find_aligned_free_block(aligned_size, alignment, &padding);
    if (!block)
    {
        ft_size_t page_request = compute_extended_page_request(aligned_size,
                alignment);
        if (page_request == 0)
        {
            cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        Page *page = create_page(page_request);
        if (!page)
        {
            cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        block = page->blocks;
        cma_validate_block(block, "cma_aligned_alloc new page", nullptr);
        padding = calculate_alignment_padding(block, alignment);
    }
    if (normalize_alignment_padding(&padding) == FT_FALSE)
    {
        cma_unlock_allocator(lock_acquired);
        return (nullptr);
    }
    if (padding > 0)
    {
        Block *prefix_block = split_block(block, padding);
        cma_validate_block(prefix_block, "cma_aligned_alloc prefix", nullptr);
        block = prefix_block->next;
        if (!block)
        {
            cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        cma_validate_block(block, "cma_aligned_alloc aligned", nullptr);
    }
    block = split_block(block, aligned_size);
    cma_validate_block(block, "cma_aligned_alloc split", nullptr);
    if (!cma_block_is_free(block))
    {
        cma_unlock_allocator(lock_acquired);
        su_sigabrt();
    }
    cma_mark_block_allocated(block);
    g_cma_allocation_count++;
    g_cma_current_bytes += block->size;
    if (g_cma_current_bytes > g_cma_peak_bytes)
        g_cma_peak_bytes = g_cma_current_bytes;
    cma_debug_prepare_allocation(block, request_size);
    void *result = static_cast<void *>(cma_block_user_pointer(block));
    cma_unlock_allocator(lock_acquired);
    cma_record_allocation_log("cma_aligned_alloc %llu (alignment %llu) -> %p",
            static_cast<unsigned long long>(request_size),
            static_cast<unsigned long long>(alignment), result);
    return (result);
}
