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

static unsigned char *g_scma_heap_data = ft_nullptr;
static ft_size_t g_scma_heap_capacity = 0;
static scma_block *g_scma_blocks_data = ft_nullptr;
static ft_size_t g_scma_block_capacity = 0;
static ft_size_t g_scma_block_count = 0;
static ft_size_t g_scma_used_size = 0;
static ft_size_t g_scma_live_size = 0;
static ft_size_t g_scma_live_head = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
static ft_size_t g_scma_live_tail = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
static ft_size_t g_scma_free_head = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
static ft_bool g_scma_compaction_needed = FT_FALSE;
static int32_t g_scma_initialised = 0;

int32_t    &scma_initialised_ref(void)
{
    return (g_scma_initialised);
}

static scma_handle    scma_create_invalid_handle(void)
{
    scma_handle handle;

    handle.index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    handle.generation = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    return (handle);
}

unsigned char    *&scma_heap_data_ref(void)
{
    return (g_scma_heap_data);
}

ft_size_t    &scma_heap_capacity_ref(void)
{
    return (g_scma_heap_capacity);
}

scma_block    *&scma_blocks_data_ref(void)
{
    return (g_scma_blocks_data);
}

ft_size_t    &scma_block_capacity_ref(void)
{
    return (g_scma_block_capacity);
}

ft_size_t    &scma_block_count_ref(void)
{
    return (g_scma_block_count);
}

ft_size_t    &scma_used_size_ref(void)
{
    return (g_scma_used_size);
}

ft_size_t    &scma_live_size_ref(void)
{
    return (g_scma_live_size);
}

ft_size_t    &scma_live_head_ref(void)
{
    return (g_scma_live_head);
}

ft_size_t    &scma_live_tail_ref(void)
{
    return (g_scma_live_tail);
}

ft_size_t    &scma_free_head_ref(void)
{
    return (g_scma_free_head);
}

ft_bool    &scma_compaction_needed_ref(void)
{
    return (g_scma_compaction_needed);
}

scma_block_span    scma_get_block_span(void)
{
    scma_block_span span;
    ft_size_t block_count;

    block_count = scma_block_count_ref();
    span.count = block_count;
    if (span.count == 0)
    {
        span.data = ft_nullptr;
        return (span);
    }
    span.data = scma_blocks_data_ref();
    return (span);
}

unsigned char    *scma_get_heap_data(void)
{
    unsigned char *heap_data;

    heap_data = scma_heap_data_ref();
    return (heap_data);
}

scma_handle    scma_invalid_handle(void)
{
    return (scma_create_invalid_handle());
}

int32_t    scma_handle_is_invalid(scma_handle handle)
{
    if (handle.index == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (1);
    if (handle.generation == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (1);
    return (0);
}

static int32_t scma_validate_live_list(scma_block_span span)
{
    const ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    ft_size_t current_index = scma_live_head_ref();
    ft_size_t previous_index = invalid_index;
    ft_size_t visited_count = 0;
    ft_size_t linked_size = 0;
    ft_size_t in_use_count = 0;
    ft_size_t in_use_size = 0;
    ft_size_t index = 0;

    if ((current_index == invalid_index)
        != (scma_live_tail_ref() == invalid_index))
        return (FT_ERR_INVALID_STATE);
    while (current_index != invalid_index)
    {
        scma_block *block;

        if (current_index >= span.count || visited_count >= span.count)
            return (FT_ERR_INVALID_STATE);
        block = &span.data[current_index];
        if (!block->in_use || block->prev_index != previous_index)
            return (FT_ERR_INVALID_STATE);
        if (block->offset > scma_used_size_ref()
            || block->size > scma_used_size_ref() - block->offset)
            return (FT_ERR_INVALID_STATE);
        if (linked_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            - block->size)
            return (FT_ERR_INVALID_STATE);
        linked_size += block->size;
        previous_index = current_index;
        current_index = block->next_index;
        visited_count++;
    }
    if (previous_index != scma_live_tail_ref())
        return (FT_ERR_INVALID_STATE);
    while (index < span.count)
    {
        if (span.data[index].in_use)
        {
            if (in_use_size > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                - span.data[index].size)
                return (FT_ERR_INVALID_STATE);
            in_use_size += span.data[index].size;
            in_use_count++;
        }
        index++;
    }
    if (visited_count != in_use_count || linked_size != in_use_size
        || linked_size != scma_live_size_ref())
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

int32_t scma_compact(void)
{
    scma_block_span span;
    unsigned char *heap_data;
    ft_size_t new_offset;
    ft_size_t previous_used_size;
    ft_size_t current_index;

    if (!scma_initialised_ref())
    {
        return (FT_ERR_INVALID_STATE);
    }
    span = scma_get_block_span();
    if (span.count == 0)
    {
        if (scma_live_head_ref() != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            || scma_live_tail_ref()
                != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            || scma_live_size_ref() != 0)
            return (FT_ERR_INVALID_STATE);
        scma_used_size_ref() = 0;
        return (FT_ERR_SUCCESS);
    }
    if (scma_validate_live_list(span) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    heap_data = scma_get_heap_data();
    previous_used_size = scma_used_size_ref();
    new_offset = 0;
    current_index = scma_live_head_ref();
    while (current_index != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        scma_block *block;
        block = &span.data[current_index];
        if (block->offset != new_offset)
        {
            std::memmove(heap_data + new_offset,
                heap_data + block->offset,
                block->size);
            block->offset = new_offset;
        }
        new_offset += block->size;
        current_index = block->next_index;
    }
    scma_used_size_ref() = new_offset;
    if (previous_used_size > new_offset)
        scma_secure_bzero(heap_data + new_offset,
            previous_used_size - new_offset);
    scma_compaction_needed_ref() = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

void    scma_link_block_at_tail(ft_size_t index)
{
    scma_block *blocks_data;
    ft_size_t &head = scma_live_head_ref();
    ft_size_t &tail = scma_live_tail_ref();
    ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);

    if (index >= scma_block_count_ref())
        return ;
    blocks_data = scma_blocks_data_ref();
    if (!blocks_data[index].in_use)
        return ;
    if (tail != invalid_index
        && (tail >= scma_block_count_ref() || !blocks_data[tail].in_use
            || blocks_data[tail].next_index != invalid_index))
        return ;
    blocks_data[index].prev_index = tail;
    blocks_data[index].next_index = invalid_index;
    if (tail == invalid_index)
        head = index;
    else
        blocks_data[tail].next_index = index;
    tail = index;
}

void    scma_unlink_block(ft_size_t index)
{
    scma_block *blocks_data;
    scma_block *block;
    ft_size_t &head = scma_live_head_ref();
    ft_size_t &tail = scma_live_tail_ref();
    ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);

    if (index >= scma_block_count_ref())
        return ;
    blocks_data = scma_blocks_data_ref();
    block = &blocks_data[index];
    if (!block->in_use)
        return ;
    if (block->prev_index != invalid_index
        && (block->prev_index >= scma_block_count_ref()
            || blocks_data[block->prev_index].next_index != index))
        return ;
    if (block->next_index != invalid_index
        && (block->next_index >= scma_block_count_ref()
            || blocks_data[block->next_index].prev_index != index))
        return ;
    if (block->prev_index == invalid_index)
        head = block->next_index;
    else
        blocks_data[block->prev_index].next_index = block->next_index;
    if (block->next_index == invalid_index)
        tail = block->prev_index;
    else
        blocks_data[block->next_index].prev_index = block->prev_index;
    block->prev_index = invalid_index;
    block->next_index = invalid_index;
}

void    scma_push_free_block(ft_size_t index)
{
    scma_block *blocks_data;

    if (index >= scma_block_count_ref())
        return ;
    blocks_data = scma_blocks_data_ref();
    blocks_data[index].next_free_index = scma_free_head_ref();
    scma_free_head_ref() = index;
    return ;
}

ft_size_t    scma_pop_free_block(void)
{
    ft_size_t index;
    scma_block *blocks_data;

    index = scma_free_head_ref();
    if (index == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (index);
    if (index >= scma_block_count_ref())
    {
        scma_free_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
        return (static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX));
    }
    blocks_data = scma_blocks_data_ref();
    if (blocks_data[index].in_use)
    {
        scma_free_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
        return (static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX));
    }
    scma_free_head_ref() = blocks_data[index].next_free_index;
    if (scma_free_head_ref() != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
        && scma_free_head_ref() >= scma_block_count_ref())
        scma_free_head_ref() = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    blocks_data[index].next_free_index = static_cast<ft_size_t>(
            FT_SYSTEM_SIZE_MAX);
    return (index);
}

int32_t    scma_validate_handle(scma_handle handle, scma_block **out_block)
{
    int32_t validation_result;
    scma_block_span span;
    scma_block *block;

    validation_result = 0;
    if (!scma_initialised_ref())
        return (0);
    if (scma_handle_is_invalid(handle))
        return (0);
    span = scma_get_block_span();
    if (handle.index >= span.count)
        return (0);
    block = &span.data[handle.index];
    if (!block->in_use)
        return (0);
    if (block->generation != handle.generation)
        return (0);
    if (out_block)
        *out_block = block;
    validation_result = 1;
    return (validation_result);
}

int32_t    scma_ensure_block_capacity(ft_size_t required_count)
{
    ft_size_t new_capacity;
    ft_size_t allocation_size;
    void *new_data;
    scma_block *&blocks_data = scma_blocks_data_ref();
    ft_size_t &block_capacity = scma_block_capacity_ref();

    new_capacity = block_capacity;
    if (new_capacity >= required_count)
        return (1);
    if (new_capacity == 0)
        new_capacity = required_count;
    while (new_capacity < required_count)
    {
        if (new_capacity > required_count / 2)
            new_capacity = required_count;
        else
        {
            if (new_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) / 2)
                new_capacity = required_count;
            else
                new_capacity *= 2;
        }
    }
    if (new_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        return (0);
    }
    if (new_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) / sizeof(scma_block))
    {
        return (0);
    }
    allocation_size = new_capacity * sizeof(scma_block);
    new_data = std::realloc(blocks_data, allocation_size);
    if (!new_data)
    {
        return (0);
    }
    blocks_data = static_cast<scma_block *>(new_data);
    block_capacity = new_capacity;
    return (1);
}

int32_t    scma_ensure_capacity(ft_size_t required_size)
{
    ft_size_t new_capacity;
    void *new_data;
    unsigned char *&heap_data = scma_heap_data_ref();
    ft_size_t &heap_capacity = scma_heap_capacity_ref();

    if (required_size >= static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        return (0);

    new_capacity = heap_capacity;
    if (new_capacity >= required_size)
        return (1);
    if (new_capacity == 0)
        new_capacity = required_size;
    while (new_capacity < required_size)
    {
        if (new_capacity > required_size / 2)
            new_capacity = required_size;
        else
        {
            if (new_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX) / 2)
                new_capacity = required_size;
            else
                new_capacity *= 2;
        }
    }
    if (new_capacity > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        return (0);
    }
    new_data = std::realloc(heap_data, new_capacity);
    if (!new_data)
    {
        return (0);
    }
    heap_data = static_cast<unsigned char *>(new_data);
    heap_capacity = new_capacity;
    return (1);
}

ft_size_t    scma_next_generation(ft_size_t generation)
{
    ft_size_t next_generation;

    next_generation = generation + 1;
    if (next_generation == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
        next_generation = 1;
    if (next_generation == 0)
        next_generation = 1;
    return (next_generation);
}

int32_t    scma_unlock_and_return_int(int32_t value)
{
    (void)scma_mutex_unlock();
    return (value);
}

ft_size_t    scma_unlock_and_return_size(ft_size_t value)
{
    (void)scma_mutex_unlock();
    return (value);
}

scma_handle    scma_unlock_and_return_handle(scma_handle value)
{
    (void)scma_mutex_unlock();
    return (value);
}

void    *scma_unlock_and_return_pointer(void *value)
{
    (void)scma_mutex_unlock();
    return (value);
}

void    scma_unlock_and_return_void(void)
{
    (void)scma_mutex_unlock();
    return ;
}
