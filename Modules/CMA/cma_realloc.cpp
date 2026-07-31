#include <cstdlib>
#include <cstddef>
#include <pthread.h>
#include "../Errno/errno.hpp"
#include "CMA.hpp"
#include "cma_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"

#include "../System_utils/system_utils.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool reallocate_block(void *memory_pointer, ft_size_t aligned_size, ft_size_t user_size)
{
    if (!memory_pointer)
        return (FT_FALSE);
    Block *block = cma_find_block_for_pointer(memory_pointer);
    if (!block)
        return (FT_FALSE);
    cma_validate_block(block, "cma_realloc resize", memory_pointer);
    if (block->size >= aligned_size)
    {
        split_block(block, aligned_size);
        cma_validate_block(block, "cma_realloc split in place", memory_pointer);
        cma_debug_prepare_allocation(block, user_size);
        return (FT_TRUE);
    }
    if (block->next && cma_block_is_free(block->next) &&
        (block->size + block->next->size) >= aligned_size)
    {
        Block *released_block;

        released_block = block->next;
        cma_validate_block(released_block, "cma_realloc neighbor", nullptr);
        cma_free_list_remove(released_block);
        block->size += released_block->size;
        block->next = released_block->next;
        if (block->next)
        {
            cma_validate_block(block->next, "cma_realloc relink", nullptr);
            block->next->prev = block;
        }
        released_block->next = nullptr;
        released_block->prev = nullptr;
        cma_metadata_release_block(released_block);
        split_block(block, aligned_size);
        cma_validate_block(block, "cma_realloc split after merge", memory_pointer);
        cma_debug_prepare_allocation(block, user_size);
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static void *allocate_block_locked(ft_size_t aligned_size, ft_size_t user_size)
{
    Block *block;
    Page *page;

    block = find_free_block(aligned_size);
    if (!block)
    {
        page = create_page(aligned_size);
        if (!page)
            return (nullptr);
        block = page->blocks;
    }
    cma_validate_block(block, "cma_realloc allocate", nullptr);
    if (!cma_block_is_free(block))
        su_sigabrt();
    block = split_block(block, aligned_size);
    cma_validate_block(block, "cma_realloc allocate split", nullptr);
    cma_mark_block_allocated(block);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    cma_capture_leak_stack(block, 3);
#endif
    g_cma_allocation_count++;
    g_cma_current_bytes += block->size;
    if (g_cma_current_bytes > g_cma_peak_bytes)
        g_cma_peak_bytes = g_cma_current_bytes;
    void *result;

    cma_debug_prepare_allocation(block, user_size);
    result = cma_block_user_pointer(block);
    return (result);
}

static void release_block_locked(Block *block)
{
    ft_size_t freed_size;
    Page *page;

    cma_validate_block(block, "cma_realloc release", nullptr);
    if (cma_block_is_free(block))
        su_sigabrt();
    freed_size = block->size;
    cma_debug_release_allocation(block, "cma_realloc release",
        cma_block_user_pointer(block));
    cma_mark_block_free(block);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    block->leak_stack_frame_count = 0;
    block->leak_test_name = nullptr;
#endif
    block = merge_block(block);
    cma_debug_initialize_block(block);
    page = find_page_of_block(block);
    free_page_if_empty(page);
    if (g_cma_current_bytes >= freed_size)
        g_cma_current_bytes -= freed_size;
    else
        g_cma_current_bytes = 0;
    g_cma_free_count++;
    return ;
}

void *cma_realloc(void* memory_pointer, ft_size_t new_size)
{
    if (new_size > FT_SYSTEM_SIZE_MAX)
        return (nullptr);
    if (g_cma_alloc_limit != 0 && new_size > g_cma_alloc_limit)
        return (nullptr);
    if (cma_backend_is_enabled())
    {
        if (!memory_pointer || cma_backend_owns_pointer(memory_pointer))
            return (cma_backend_reallocate(memory_pointer, new_size, nullptr));
    }
    if (OFFSWITCH == 1)
    {
        void *result = std::realloc(memory_pointer, new_size);

        if (!memory_pointer && result)
            g_cma_allocation_count++;
        else if (memory_pointer && new_size == 0)
            g_cma_free_count++;
        return (result);
    }
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = cma_lock_allocator(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
        return (nullptr);
    if (!memory_pointer)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (cma_malloc(new_size));
    }
    if (new_size == 0)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        cma_free(memory_pointer);
        return (nullptr);
    }
    if (cma_small_arena_owns_pointer_locked(memory_pointer) == FT_TRUE)
    {
        ft_size_t previous_size = cma_small_arena_block_size_locked(
                memory_pointer);
        void *arena_pointer = cma_small_arena_reallocate_locked(memory_pointer,
                new_size);

        if (arena_pointer != nullptr)
        {
            ft_size_t resized_size = cma_small_arena_block_size_locked(
                    arena_pointer);

            if (g_cma_current_bytes >= previous_size)
                g_cma_current_bytes -= previous_size;
            else
                g_cma_current_bytes = 0;
            g_cma_current_bytes += resized_size;
            if (g_cma_current_bytes > g_cma_peak_bytes)
                g_cma_peak_bytes = g_cma_current_bytes;
            if (lock_acquired)
                cma_unlock_allocator(lock_acquired);
            return (arena_pointer);
        }
        ft_size_t copy_size = previous_size;
        if (copy_size > new_size)
            copy_size = new_size;
        ft_size_t instrumented_arena_size = cma_debug_allocation_size(new_size);
        if (instrumented_arena_size < new_size)
        {
            if (lock_acquired)
                cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        ft_size_t aligned_arena_size = align16(instrumented_arena_size);
        if (aligned_arena_size < instrumented_arena_size)
        {
            if (lock_acquired)
                cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        void *new_ptr = allocate_block_locked(aligned_arena_size, new_size);
        if (new_ptr == nullptr)
        {
            if (lock_acquired)
                cma_unlock_allocator(lock_acquired);
            return (nullptr);
        }
        ft_memcpy(new_ptr, memory_pointer, copy_size);
        (void)cma_small_arena_deallocate_locked(memory_pointer);
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (new_ptr);
    }
    ft_size_t instrumented_size = cma_debug_allocation_size(new_size);
    if (instrumented_size < new_size)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (nullptr);
    }
    ft_size_t aligned_size = align16(instrumented_size);
    Block *block = cma_find_block_for_pointer(memory_pointer);
    if (!block)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (nullptr);
    }
    cma_validate_block(block, "cma_realloc header", memory_pointer);
    ft_size_t previous_size = block->size;
    ft_bool resize_succeeded = reallocate_block(memory_pointer, aligned_size, new_size);
    if (resize_succeeded == FT_TRUE)
    {
#ifdef LIBFT_TEST_BUILD
        cma_capture_leak_stack(block, 2);
#endif
        if (g_cma_current_bytes >= previous_size)
            g_cma_current_bytes -= previous_size;
        else
            g_cma_current_bytes = 0;
        g_cma_current_bytes += block->size;
        if (g_cma_current_bytes > g_cma_peak_bytes)
            g_cma_peak_bytes = g_cma_current_bytes;
        void *user_pointer = cma_block_user_pointer(block);

        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (user_pointer);
    }
    Block *old_block = block;
    cma_validate_block(old_block, "cma_realloc copy source", memory_pointer);
    ft_size_t old_user_size = cma_block_user_size(old_block);
    ft_size_t copy_size;

    if (old_user_size < new_size)
        copy_size = old_user_size;
    else
        copy_size = new_size;
    void *new_ptr = allocate_block_locked(aligned_size, new_size);
    if (!new_ptr)
    {
        if (lock_acquired)
            cma_unlock_allocator(lock_acquired);
        return (nullptr);
    }
    ft_memcpy(new_ptr, memory_pointer, copy_size);
    release_block_locked(old_block);
    if (lock_acquired)
        cma_unlock_allocator(lock_acquired);
    return (new_ptr);
}
