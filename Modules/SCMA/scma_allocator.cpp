#include <cstdlib>
#include <cstring>
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include "SCMA.hpp"
#include "scma_internal.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t    scma_initialize(ft_size_t initial_capacity)
{
    int32_t thread_safety_result;
    unsigned char *&heap_data = scma_heap_data_ref();
    scma_block *&blocks_data = scma_blocks_data_ref();
    ft_size_t &heap_capacity = scma_heap_capacity_ref();
    ft_size_t &block_capacity = scma_block_capacity_ref();
    ft_size_t &block_count = scma_block_count_ref();
    ft_size_t &used_size = scma_used_size_ref();
    ft_size_t &live_size = scma_live_size_ref();
    int32_t &initialised = scma_initialised_ref();

    thread_safety_result = scma_enable_thread_safety();
    if (thread_safety_result != FT_ERR_SUCCESS)
        return (static_cast<uint32_t>(thread_safety_result));
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (initialised)
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_ALREADY_INITIALISED)));
    }
    if (initial_capacity == 0)
        initial_capacity = 1024;
    if (initial_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_OUT_OF_RANGE)));
    }
    if (heap_data)
    {
        std::free(heap_data);
        heap_data = ft_nullptr;
    }
    heap_capacity = 0;
    heap_data = static_cast<unsigned char *>(std::malloc(initial_capacity));
    if (!heap_data)
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_NO_MEMORY)));
    }
    heap_capacity = initial_capacity;
    if (blocks_data)
    {
        std::free(blocks_data);
        blocks_data = ft_nullptr;
    }
    block_capacity = 0;
    block_count = 0;
    used_size = 0;
    live_size = 0;
    scma_live_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_live_tail_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_free_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_compaction_needed_ref() = FT_FALSE;
    initialised = 1;
    return (static_cast<uint32_t>(scma_unlock_and_return_int(FT_ERR_SUCCESS)));
}

void    scma_shutdown(void)
{
    unsigned char *&heap_data = scma_heap_data_ref();
    scma_block *&blocks_data = scma_blocks_data_ref();
    ft_size_t &heap_capacity = scma_heap_capacity_ref();
    ft_size_t &block_capacity = scma_block_capacity_ref();
    ft_size_t &block_count = scma_block_count_ref();
    ft_size_t &used_size = scma_used_size_ref();
    ft_size_t &live_size = scma_live_size_ref();
    int32_t &initialised = scma_initialised_ref();

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return ;
    }
    if (!initialised)
    {
        scma_unlock_and_return_void();
        return ;
    }
    if (heap_data)
    {
        scma_secure_bzero(heap_data, heap_capacity);
        std::free(heap_data);
        heap_data = ft_nullptr;
    }
    if (blocks_data)
    {
        scma_secure_bzero(blocks_data, block_capacity * sizeof(scma_block));
        std::free(blocks_data);
        blocks_data = ft_nullptr;
    }
    heap_capacity = 0;
    block_capacity = 0;
    block_count = 0;
    used_size = 0;
    live_size = 0;
    scma_live_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_live_tail_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_free_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    scma_compaction_needed_ref() = FT_FALSE;
    initialised = 0;
    scma_unlock_and_return_void();
    return ;
}

#ifdef LIBFT_TEST_BUILD
void    scma_test_secure_wipe_runtime(void)
{
    unsigned char *&heap_data = scma_heap_data_ref();
    scma_block *&blocks_data = scma_blocks_data_ref();

    scma_secure_bzero(heap_data, scma_heap_capacity_ref());
    scma_secure_bzero(blocks_data, scma_block_capacity_ref() * sizeof(scma_block));
    return ;
}
#endif

int32_t    scma_is_initialised(void)
{
    int32_t initialised;

    initialised = 0;
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (0);
    }
    if (scma_initialised_ref())
    {
        initialised = 1;
    }
    return (scma_unlock_and_return_int(initialised));
}

scma_handle    scma_allocate(ft_size_t size)
{
    scma_handle result_handle;
    ft_size_t required_size;
    ft_size_t index;
    scma_block *block;
    ft_size_t &used_size = scma_used_size_ref();
    ft_size_t &live_size = scma_live_size_ref();
    ft_size_t &block_count = scma_block_count_ref();

    result_handle = scma_invalid_handle();
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (result_handle);
    }
    if (!scma_initialised_ref())
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    if (size == 0)
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    if (size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    required_size = used_size + size;
    if (required_size < used_size)
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    if (required_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    if (required_size > scma_heap_capacity_ref()
        && scma_compaction_needed_ref() == FT_TRUE)
    {
        scma_compact();
        if (size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - used_size)
            return (scma_unlock_and_return_handle(result_handle));
        required_size = used_size + size;
    }
    if (!scma_ensure_capacity(required_size))
    {
        return (scma_unlock_and_return_handle(result_handle));
    }
    index = scma_pop_free_block();
    if (index == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        scma_block *&blocks_data = scma_blocks_data_ref();
        ft_size_t new_index;

        if (!scma_ensure_block_capacity(block_count + 1))
        {
            return (scma_unlock_and_return_handle(result_handle));
        }
        new_index = block_count;
        block = &blocks_data[new_index];
        block->offset = used_size;
        block->size = size;
        block->in_use = 1;
        block->generation = 1;
        block->prev_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
        block->next_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
        block->next_free_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
#ifdef LIBFT_TEST_BUILD
        block->leak_ignored = FT_FALSE;
        scma_capture_leak_stack(block, 2);
#endif
        block_count = block_count + 1;
        scma_link_block_at_tail(new_index);
        used_size += size;
        live_size += size;
        result_handle.index = new_index;
        result_handle.generation = block->generation;
        return (scma_unlock_and_return_handle(result_handle));
    }
    block = &scma_blocks_data_ref()[index];
    block->offset = used_size;
    block->size = size;
    block->in_use = 1;
    block->generation = scma_next_generation(block->generation);
    block->prev_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    block->next_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    block->next_free_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    scma_capture_leak_stack(block, 2);
#endif
    result_handle.index = index;
    result_handle.generation = block->generation;
    scma_link_block_at_tail(index);
    used_size += size;
    live_size += size;
    return (scma_unlock_and_return_handle(result_handle));
}

int32_t    scma_free(scma_handle handle)
{
    scma_block *block;
    ft_size_t block_offset;
    ft_size_t block_size;
    ft_bool was_tail;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (!scma_validate_handle(handle, &block))
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_INVALID_HANDLE)));
    }
    block_offset = block->offset;
    block_size = block->size;
    was_tail = FT_FALSE;
    if (block_offset <= scma_used_size_ref()
        && block_size == scma_used_size_ref() - block_offset)
        was_tail = FT_TRUE;
    scma_secure_bzero(scma_get_heap_data() + block_offset, block_size);
    scma_unlink_block(handle.index);
    block->in_use = 0;
    block->size = 0;
    block->generation = scma_next_generation(block->generation);
#ifdef LIBFT_TEST_BUILD
    block->leak_ignored = FT_FALSE;
    block->leak_stack_frame_count = 0;
    block->leak_test_name = nullptr;
#endif
    scma_push_free_block(handle.index);
    if (scma_live_size_ref() >= block_size)
        scma_live_size_ref() -= block_size;
    else
        scma_live_size_ref() = 0;
    if (was_tail == FT_TRUE)
        scma_used_size_ref() = block_offset;
    else
        scma_compaction_needed_ref() = FT_TRUE;
    return (static_cast<uint32_t>(scma_unlock_and_return_int(FT_ERR_SUCCESS)));
}

int32_t    scma_resize(scma_handle handle, ft_size_t new_size)
{
    scma_block *block;
    ft_size_t old_size;
    ft_size_t required_size;
    ft_size_t old_offset;
    unsigned char *heap_data;
    ft_size_t &used_size = scma_used_size_ref();
    ft_size_t &live_size = scma_live_size_ref();

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (new_size == 0)
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_INVALID_ARGUMENT)));
    }
    if (!scma_validate_handle(handle, &block))
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_INVALID_HANDLE)));
    }
    old_size = block->size;
    old_offset = block->offset;
    if (new_size == old_size)
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_SUCCESS)));
    heap_data = scma_get_heap_data();
    if (new_size < old_size)
    {
        scma_secure_bzero(heap_data + old_offset + new_size,
            old_size - new_size);
        block->size = new_size;
        if (live_size >= old_size - new_size)
            live_size -= old_size - new_size;
        else
            live_size = 0;
        if (old_offset <= used_size && old_size == used_size - old_offset)
            used_size = old_offset + new_size;
        else
            scma_compaction_needed_ref() = FT_TRUE;
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_SUCCESS)));
    }
    if (old_offset <= used_size && old_size == used_size - old_offset)
    {
        if (new_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - old_offset)
            return (static_cast<uint32_t>(scma_unlock_and_return_int(
                        FT_ERR_OUT_OF_RANGE)));
        required_size = old_offset + new_size;
        if (!scma_ensure_capacity(required_size))
            return (static_cast<uint32_t>(scma_unlock_and_return_int(
                        FT_ERR_NO_MEMORY)));
        block->size = new_size;
        used_size = required_size;
        live_size += new_size - old_size;
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_SUCCESS)));
    }
    if (new_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - used_size)
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_OUT_OF_RANGE)));
    }
    required_size = used_size + new_size;
    if (required_size > scma_heap_capacity_ref()
        && scma_compaction_needed_ref() == FT_TRUE)
    {
        scma_compact();
        old_offset = block->offset;
        heap_data = scma_get_heap_data();
        if (old_offset <= used_size && old_size == used_size - old_offset)
        {
            if (new_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                - old_offset)
                return (static_cast<uint32_t>(scma_unlock_and_return_int(
                            FT_ERR_OUT_OF_RANGE)));
            required_size = old_offset + new_size;
            if (!scma_ensure_capacity(required_size))
                return (static_cast<uint32_t>(scma_unlock_and_return_int(
                            FT_ERR_NO_MEMORY)));
            block->size = new_size;
            used_size = required_size;
            live_size += new_size - old_size;
            return (static_cast<uint32_t>(scma_unlock_and_return_int(
                        FT_ERR_SUCCESS)));
        }
        if (new_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) - used_size)
            return (static_cast<uint32_t>(scma_unlock_and_return_int(
                        FT_ERR_OUT_OF_RANGE)));
        required_size = used_size + new_size;
    }
    if (!scma_ensure_capacity(required_size))
    {
        return (static_cast<uint32_t>(scma_unlock_and_return_int(
                    FT_ERR_NO_MEMORY)));
    }
    heap_data = scma_get_heap_data();
    std::memmove(heap_data + used_size, heap_data + old_offset, old_size);
    scma_secure_bzero(heap_data + old_offset, old_size);
    scma_unlink_block(handle.index);
    block->offset = used_size;
    block->size = new_size;
    scma_link_block_at_tail(handle.index);
    used_size = required_size;
    live_size += new_size - old_size;
    scma_compaction_needed_ref() = FT_TRUE;
    return (static_cast<uint32_t>(scma_unlock_and_return_int(FT_ERR_SUCCESS)));
}

ft_size_t    scma_get_size(scma_handle handle)
{
    ft_size_t size_result;
    scma_block *block;

    size_result = 0;
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (0);
    }
    if (!scma_validate_handle(handle, &block))
    {
        return (scma_unlock_and_return_size(0));
    }
    size_result = block->size;
    return (scma_unlock_and_return_size(size_result));
}

int32_t    scma_handle_is_valid(scma_handle handle)
{
    int32_t valid;

    valid = 0;
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (0);
    }
    if (scma_validate_handle(handle, ft_nullptr))
    {
        valid = 1;
        return (scma_unlock_and_return_int(valid));
    }
    return (scma_unlock_and_return_int(valid));
}
