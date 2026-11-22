#include <cstdlib>
#include <cstring>
#include "CMA.hpp"
#include "cma_internal.hpp"

#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static void cma_secure_bzero(void *memory_pointer, ft_size_t size)
{
    volatile unsigned char *pointer;

    if (memory_pointer == nullptr || size == 0)
        return ;
    pointer = static_cast<volatile unsigned char *>(memory_pointer);
    while (size > 0)
    {
        *pointer = 0;
        pointer++;
        size--;
    }
    return ;
}

void cma_bzero_and_free(void* memory_pointer)
{
    ft_bool lock_acquired = FT_FALSE;

    if (!memory_pointer)
        return ;
    if (OFFSWITCH == 1)
    {
        std::free(memory_pointer);
        g_cma_free_count++;
        cma_record_allocation_log("cma_bzero_and_free %p", memory_pointer);
        return ;
    }
    if (cma_backend_is_enabled() && cma_backend_owns_pointer(memory_pointer))
    {
        ft_size_t allocation_size = 0;

        if (cma_backend_checked_block_size(memory_pointer, &allocation_size)
            == FT_ERR_SUCCESS)
            cma_secure_bzero(memory_pointer, allocation_size);
        cma_backend_deallocate(memory_pointer);
        return ;
    }
    if (cma_lock_allocator_arena(&lock_acquired) != FT_ERR_SUCCESS)
        return ;
    if (cma_small_arena_owns_pointer_locked(memory_pointer) == FT_TRUE)
    {
        ft_size_t allocation_size = cma_small_arena_block_size_locked(
                memory_pointer);

        cma_secure_bzero(memory_pointer, allocation_size);
        (void)cma_small_arena_deallocate_locked(memory_pointer);
        cma_unlock_allocator(lock_acquired);
        lock_acquired = FT_FALSE;
        cma_record_allocation_log("cma_bzero_and_free %p", memory_pointer);
        return ;
    }
    if (cma_require_metadata_locked() != FT_ERR_SUCCESS)
    {
        (void)cma_unlock_allocator(lock_acquired);
        return ;
    }
    Block *block = cma_find_block_for_pointer(memory_pointer);
    if (!block)
    {
        cma_unlock_allocator(lock_acquired);
        lock_acquired = FT_FALSE;
        return ;
    }
    ft_size_t freed_size = 0;

    cma_validate_block(block, "cma_bzero_and_free", memory_pointer);
    if (cma_block_is_free(block))
    {
        cma_unlock_allocator(lock_acquired);
        lock_acquired = FT_FALSE;
        su_sigabrt();
    }
    freed_size = block->size;
    cma_debug_release_allocation(block, "cma_bzero_and_free", memory_pointer);
    cma_secure_bzero(cma_block_user_pointer(block), freed_size);
    cma_mark_block_free(block);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    block->leak_stack_frame_count = 0;
    block->leak_test_name = nullptr;
#endif
    block = merge_block(block);
    cma_debug_initialize_block(block);
    Page *page = find_page_of_block(block);
    free_page_if_empty(page);
    if (g_cma_current_bytes >= freed_size)
        g_cma_current_bytes -= freed_size;
    else
        g_cma_current_bytes = 0;
    g_cma_free_count++;
    cma_unlock_allocator(lock_acquired);
    lock_acquired = FT_FALSE;
    cma_record_allocation_log("cma_bzero_and_free %p", memory_pointer);
    return ;
}
