#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <csignal>
#include <pthread.h>
#include "../Errno/errno.hpp"
#include "cma_internal.hpp"
#include "CMA.hpp"
#include "cma_internal.hpp"

#include "../Basic/limits.hpp"
#include "../System_utils/system_utils.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(LIBFT_ENABLE_ANALYTICS)
# include "../Analytics/analytics.hpp"
#endif

static void *cma_malloc_uninstrumented(ft_size_t size)
{
    void *result = nullptr;
    ft_bool lock_acquired = FT_FALSE;
    ft_size_t request_size = size;
    ft_size_t instrumented_size = 0;
    ft_size_t aligned_size = 0;
    Block *block = nullptr;

    if (size > FT_SYSTEM_SIZE_MAX)
        return (nullptr);
    if (size == 0)
        size = 1;
    if (g_cma_alloc_limit != 0 && size > g_cma_alloc_limit)
        return (nullptr);
    if (cma_backend_is_enabled())
        return (cma_backend_allocate(size, nullptr));
    if (OFFSWITCH == 1)
    {
        result = malloc(size);

        if (result)
        {
            g_cma_allocation_count++;
        }
        else
            return (nullptr);
        cma_record_allocation_log("cma_malloc %llu -> %p",
            static_cast<unsigned long long>(size), result);
        return (result);
    }
    if (cma_lock_allocator(&lock_acquired) != FT_ERR_SUCCESS)
        return (nullptr);
    result = cma_small_arena_allocate_locked(size);
    if (result != nullptr)
    {
        ft_size_t arena_size = cma_small_arena_block_size_locked(result);

        g_cma_allocation_count++;
        g_cma_current_bytes += arena_size;
        if (g_cma_current_bytes > g_cma_peak_bytes)
            g_cma_peak_bytes = g_cma_current_bytes;
        cma_unlock_allocator(lock_acquired);
        lock_acquired = FT_FALSE;
        cma_record_allocation_log("cma_malloc %llu -> %p",
            static_cast<unsigned long long>(size), result);
        return (result);
    }
    instrumented_size = cma_debug_allocation_size(size);
    if (instrumented_size > FT_SYSTEM_SIZE_MAX - 15)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (nullptr);
    }
    aligned_size = align16(instrumented_size);
    block = find_free_block(aligned_size);
    if (!block)
    {
        Page* page = create_page(aligned_size);

        if (!page)
        {
            if (lock_acquired)
                cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        block = page->blocks;
    }
    cma_validate_block(block, "cma_malloc", nullptr);
    if (!cma_block_is_free(block))
    {
        cma_unlock_allocator(lock_acquired);
        lock_acquired = FT_FALSE;
        su_sigabrt();
    }
    block = split_block(block, aligned_size);
    cma_validate_block(block, "cma_malloc split", nullptr);
    cma_mark_block_allocated(block);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    cma_capture_leak_stack(block, 2);
#endif
    g_cma_allocation_count++;
    g_cma_current_bytes += block->size;
    if (g_cma_current_bytes > g_cma_peak_bytes)
        g_cma_peak_bytes = g_cma_current_bytes;
    cma_debug_prepare_allocation(block, size);
    result = static_cast<void *>(cma_block_user_pointer(block));
    cma_unlock_allocator(lock_acquired);
    lock_acquired = FT_FALSE;
    if (request_size == size)
        cma_record_allocation_log("cma_malloc %llu -> %p",
            static_cast<unsigned long long>(aligned_size), result);
    else
        cma_record_allocation_log("cma_malloc %llu (rounded to %llu) -> %p",
            static_cast<unsigned long long>(request_size),
            static_cast<unsigned long long>(aligned_size), result);
    if (lock_acquired)
        cma_unlock_allocator(lock_acquired);
    return (result);
}

void *cma_malloc(ft_size_t size)
{
#if defined(LIBFT_ENABLE_ANALYTICS)
    analytics_runtime_scope_token token;
    int32_t analytics_error;
    void *result;

    analytics_error = analytics_runtime_scope_begin(
        analytics_runtime_region::CMA_MALLOC, &token);
    if (analytics_error != FT_ERR_SUCCESS)
        return (cma_malloc_uninstrumented(size));
    result = cma_malloc_uninstrumented(size);
    analytics_error = analytics_runtime_scope_end(&token);
    if (analytics_error != FT_ERR_SUCCESS)
        std::fprintf(stderr, "[LIBFT][Analytics] cma_malloc scope failed: %d\n",
            analytics_error);
    return (result);
#else
    return (cma_malloc_uninstrumented(size));
#endif
}
