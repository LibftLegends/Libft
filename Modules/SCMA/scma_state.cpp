#include <cstdlib>
#include <cstring>
#include <algorithm>
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
static ft_size_t g_scma_compaction_cursor =
    static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
static ft_size_t g_scma_compaction_target_offset = 0;
static ft_bool g_scma_compaction_in_live_order = FT_FALSE;
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

void scma_recompute_used_size(void)
{
    ft_size_t index;
    ft_size_t maximum_end;

    index = 0;
    maximum_end = 0;
    while (index < scma_block_count_ref())
    {
        scma_block *block;
        ft_size_t block_end;

        block = &scma_blocks_data_ref()[index];
        if (block->in_use
            && block->size <= static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                - block->offset)
        {
            block_end = block->offset + block->size;
            if (block_end > maximum_end)
                maximum_end = block_end;
        }
        index += 1;
    }
    scma_used_size_ref() = maximum_end;
    return ;
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

void    scma_reset_compaction(void)
{
    g_scma_compaction_cursor = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    g_scma_compaction_target_offset = 0;
    g_scma_compaction_in_live_order = FT_FALSE;
    return ;
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

static int32_t scma_validate_live_list(scma_block_span span,
    ft_bool &in_offset_order, ft_size_t *live_count = ft_nullptr)
{
    const ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    ft_size_t current_index = scma_live_head_ref();
    ft_size_t previous_index = invalid_index;
    ft_size_t visited_count = 0;
    ft_size_t linked_size = 0;
    ft_size_t in_use_count = 0;
    ft_size_t in_use_size = 0;
    ft_size_t index = 0;
    ft_size_t previous_end = 0;

    in_offset_order = FT_TRUE;
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
        if (block->size == 0 || block->offset < previous_end)
            in_offset_order = FT_FALSE;
        previous_end = block->offset + block->size;
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
    if (live_count != ft_nullptr)
        *live_count = visited_count;
    return (FT_ERR_SUCCESS);
}

static int32_t scma_order_live_blocks(scma_block_span span,
    ft_size_t live_count)
{
    if (live_count > SIZE_MAX / sizeof(ft_size_t))
        return (FT_ERR_NO_MEMORY);
    ft_size_t *indices = static_cast<ft_size_t *>(std::malloc(
            FT_UINT64_TO_NATIVE_SIZE_CAST(live_count * sizeof(ft_size_t))));
    if (indices == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    ft_size_t index = 0U;
    ft_size_t current = scma_live_head_ref();
    while (index < live_count)
    {
        indices[index] = current;
        current = span.data[current].next_index;
        ++index;
    }
    std::sort(indices, indices + live_count, [span](ft_size_t first, ft_size_t second)
    {
        return (span.data[first].offset < span.data[second].offset);
    });
    ft_size_t previous_end = 0U;
    index = 0U;
    while (index < live_count)
    {
        const scma_block &block = span.data[indices[index]];
        if (block.offset < previous_end || block.size == 0U)
        {
            std::free(indices);
            return (FT_ERR_INVALID_STATE);
        }
        previous_end = block.offset + block.size;
        ++index;
    }
    /* Only the links move here. Handle indices and generations stay put. */
    ft_size_t previous = FT_SYSTEM_SIZE_MAX;
    index = 0U;
    while (index < live_count)
    {
        current = indices[index];
        span.data[current].prev_index = previous;
        if (previous != FT_SYSTEM_SIZE_MAX)
            span.data[previous].next_index = current;
        previous = current;
        ++index;
    }
    span.data[previous].next_index = FT_SYSTEM_SIZE_MAX;
    scma_live_head_ref() = indices[0U];
    scma_live_tail_ref() = previous;
    std::free(indices);
    return (FT_ERR_SUCCESS);
}

static ft_size_t scma_find_first_compaction_block(void)
{
    ft_size_t index;
    ft_size_t selected_index;
    ft_size_t selected_offset;

    index = 0;
    selected_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    selected_offset = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    while (index < scma_block_count_ref())
    {
        if (scma_blocks_data_ref()[index].in_use
            && scma_blocks_data_ref()[index].offset < selected_offset)
        {
            selected_index = index;
            selected_offset = scma_blocks_data_ref()[index].offset;
        }
        index += 1;
    }
    return (selected_index);
}

static ft_size_t scma_find_next_compaction_block(ft_size_t previous_offset)
{
    ft_size_t index;
    ft_size_t selected_index;
    ft_size_t selected_offset;

    index = 0;
    selected_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    selected_offset = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    while (index < scma_block_count_ref())
    {
        if (scma_blocks_data_ref()[index].in_use
            && scma_blocks_data_ref()[index].offset > previous_offset
            && scma_blocks_data_ref()[index].offset < selected_offset)
        {
            selected_index = index;
            selected_offset = scma_blocks_data_ref()[index].offset;
        }
        index += 1;
    }
    return (selected_index);
}

int32_t scma_compact_incremental(ft_size_t byte_budget)
{
    scma_block_span span;
    unsigned char *heap_data;
    ft_size_t moved_bytes;
    ft_size_t previous_used_size;

    if (!scma_initialised_ref())
        return (FT_ERR_INVALID_STATE);
    span = scma_get_block_span();
    if (span.count == 0)
    {
        if (scma_live_head_ref() != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            || scma_live_tail_ref()
                != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            || scma_live_size_ref() != 0)
            return (FT_ERR_INVALID_STATE);
        scma_used_size_ref() = 0;
        scma_compaction_needed_ref() = FT_FALSE;
        scma_reset_compaction();
        return (FT_ERR_SUCCESS);
    }
    heap_data = scma_get_heap_data();
    if (g_scma_compaction_cursor
            == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        ft_size_t free_index;
        ft_size_t live_count;
        ft_bool in_offset_order;

        // Reused holes can put the live links out of physical offset order.
        if (scma_validate_live_list(span, in_offset_order, &live_count) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        if (in_offset_order == FT_FALSE)
        {
            const int32_t order_error = scma_order_live_blocks(span, live_count);
            if (order_error == FT_ERR_SUCCESS)
                in_offset_order = FT_TRUE;
            else if (order_error != FT_ERR_NO_MEMORY)
                return (order_error);
        }
        g_scma_compaction_in_live_order = in_offset_order;
        g_scma_free_head = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
        free_index = 0;
        while (free_index < g_scma_block_count)
        {
            if (!span.data[free_index].in_use)
            {
                span.data[free_index].size = 0;
                span.data[free_index].next_free_index =
                    static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
            }
            free_index += 1;
        }
        if (g_scma_compaction_in_live_order == FT_TRUE)
            g_scma_compaction_cursor = scma_live_head_ref();
        else
            g_scma_compaction_cursor = scma_find_first_compaction_block();
        g_scma_compaction_target_offset = 0;
    }
    moved_bytes = 0;
    previous_used_size = scma_used_size_ref();
    while (g_scma_compaction_cursor
            != static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
        && (moved_bytes < byte_budget || moved_bytes == 0))
    {
        scma_block *block;
        ft_size_t current_index;

        current_index = g_scma_compaction_cursor;
        if (current_index >= span.count)
            return (FT_ERR_INVALID_STATE);
        block = &span.data[current_index];
        {
            ft_size_t source_offset;

            source_offset = block->offset;
            if (block->offset != g_scma_compaction_target_offset)
            {
                std::memmove(heap_data + g_scma_compaction_target_offset,
                    heap_data + block->offset,
                    block->size);
                block->offset = g_scma_compaction_target_offset;
            }
            g_scma_compaction_target_offset += block->size;
            moved_bytes += block->size;
            if (g_scma_compaction_in_live_order == FT_TRUE)
                g_scma_compaction_cursor = block->next_index;
            else
                g_scma_compaction_cursor =
                    scma_find_next_compaction_block(source_offset);
        }
    }
    if (g_scma_compaction_cursor == static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX))
    {
        scma_used_size_ref() = g_scma_compaction_target_offset;
        if (previous_used_size > g_scma_compaction_target_offset)
            scma_secure_bzero(heap_data + g_scma_compaction_target_offset,
                previous_used_size - g_scma_compaction_target_offset);
        {
            ft_size_t free_index;

            g_scma_free_head = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
            free_index = 0;
            while (free_index < g_scma_block_count)
            {
                if (!span.data[free_index].in_use)
                {
                    span.data[free_index].size = 0;
                    span.data[free_index].next_free_index =
                        static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
                }
                free_index += 1;
            }
        }
        scma_compaction_needed_ref() = FT_FALSE;
        scma_reset_compaction();
    }
    return (FT_ERR_SUCCESS);
}

int32_t scma_compact(void)
{
    scma_block_span span;

    span = scma_get_block_span();
    if (g_scma_compaction_cursor != FT_SYSTEM_SIZE_MAX)
    {
        ft_bool in_offset_order;

        if (scma_validate_live_list(span, in_offset_order) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        g_scma_compaction_in_live_order = in_offset_order;
    }
    if (span.count == 0 && scma_live_size_ref() != 0)
        return (FT_ERR_INVALID_STATE);
    scma_compaction_needed_ref() = FT_TRUE;
    while (scma_compaction_needed_ref() == FT_TRUE)
    {
        if (scma_compact_incremental(static_cast<ft_size_t>(65536))
                != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
    }
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

static void scma_discard_free_link(ft_size_t *link, scma_block &block)
{
    *link = block.next_free_index;
    block.size = 0;
    block.next_free_index = FT_SYSTEM_SIZE_MAX;
    return ;
}

void scma_discard_free_spans_from(ft_size_t offset)
{
    const scma_block_span span = scma_get_block_span();
    ft_size_t *link = &scma_free_head_ref();

    while (*link != FT_SYSTEM_SIZE_MAX)
    {
        if (*link >= span.count)
            return ;
        scma_block &block = span.data[*link];
        if (block.offset >= offset
            || (block.size <= FT_SYSTEM_SIZE_MAX - block.offset
                && block.size > offset - block.offset))
            scma_discard_free_link(link, block);
        else
            link = &block.next_free_index;
    }
    return ;
}

static ft_bool scma_free_span_overlaps_live(const scma_block *free_block)
{
    ft_size_t free_end;
    ft_size_t index;

    if (free_block == ft_nullptr || free_block->size == 0)
        return (FT_TRUE);
    if (free_block->offset > static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
            - free_block->size)
        return (FT_TRUE);
    /* used_size is the maximum end of any live block. A free span beyond
     * this boundary cannot overlap a handle. */
    if (free_block->offset >= scma_used_size_ref())
        return (FT_FALSE);
    free_end = free_block->offset + free_block->size;
    index = 0;
    while (index < scma_block_count_ref())
    {
        scma_block *live_block;
        ft_size_t live_end;

        live_block = &scma_blocks_data_ref()[index];
        if (live_block->in_use
            && live_block->size <= static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX)
                - live_block->offset)
        {
            live_end = live_block->offset + live_block->size;
            if (free_block->offset < live_end
                && live_block->offset < free_end)
                return (FT_TRUE);
        }
        index += 1;
    }
    return (FT_FALSE);
}

int32_t scma_take_free_span(ft_size_t size, ft_size_t *index_out)
{
    ft_size_t current_index;
    scma_block *block;
    scma_block *remainder_block;
    ft_size_t remainder_index;
    ft_size_t remainder_size;
    ft_size_t remainder_offset;
    int32_t capacity_result;
    ft_bool expands_tail;
    const ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);
    ft_size_t previous_index = invalid_index;

    if (index_out == ft_nullptr || size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    *index_out = invalid_index;
    expands_tail = FT_FALSE;
    ft_size_t *link = &scma_free_head_ref();
    while (*link != invalid_index)
    {
        if (*link >= scma_block_count_ref())
            return (FT_ERR_INVALID_STATE);
        block = &scma_blocks_data_ref()[*link];
        if (block->in_use)
            return (FT_ERR_INVALID_STATE);
        expands_tail = static_cast<ft_bool>(block->size < size
            && block->offset == scma_used_size_ref()
            && block->offset <= scma_heap_capacity_ref()
            && size <= scma_heap_capacity_ref() - block->offset);
        if (block->size >= size || expands_tail == FT_TRUE)
        {
            /* Validate a span before consuming it. Non-candidates cannot
             * affect this allocation and need no scan of live blocks. */
            if (scma_free_span_overlaps_live(block) == FT_FALSE)
                break ;
            scma_discard_free_link(link, *block);
        }
        else
        {
            previous_index = *link;
            link = &block->next_free_index;
        }
    }
    current_index = *link;
    if (current_index == invalid_index)
        return (FT_ERR_SUCCESS);
    block = &scma_blocks_data_ref()[current_index];
    remainder_size = 0;
    remainder_offset = block->offset;
    if (expands_tail == FT_FALSE)
    {
        remainder_size = block->size - size;
        remainder_offset = block->offset + size;
    }
    if (remainder_offset < block->offset)
        return (FT_ERR_OUT_OF_RANGE);
    if (remainder_size > 0)
    {
        capacity_result = scma_ensure_block_capacity(
            scma_block_count_ref() + 1);
        if (capacity_result == 0)
            return (FT_ERR_NO_MEMORY);
    }
    block = &scma_blocks_data_ref()[current_index];
    /* Splitting may have moved the block table, so resolve the link again. */
    link = &scma_free_head_ref();
    if (previous_index != invalid_index)
        link = &scma_blocks_data_ref()[previous_index].next_free_index;
    *link = block->next_free_index;
    block->next_free_index = invalid_index;
    if (expands_tail == FT_TRUE)
    {
        scma_discard_free_spans_from(block->offset);
        block->size = size;
    }
    else if (remainder_size > 0)
    {
        remainder_index = scma_block_count_ref();
        remainder_block = &scma_blocks_data_ref()[remainder_index];
        remainder_block->offset = remainder_offset;
        remainder_block->size = remainder_size;
        remainder_block->in_use = 0;
        remainder_block->generation = 1;
        remainder_block->prev_index = invalid_index;
        remainder_block->next_index = invalid_index;
        remainder_block->next_free_index = invalid_index;
        scma_block_count_ref() += 1;
        scma_push_free_block(remainder_index);
        block->size = size;
    }
    *index_out = current_index;
    return (FT_ERR_SUCCESS);
}

void scma_coalesce_free_block(ft_size_t index)
{
    scma_block *blocks_data;
    scma_block *block;
    ft_size_t *current_link;
    ft_size_t current_index;
    ft_size_t block_end;
    ft_size_t other_end;
    const ft_size_t invalid_index = static_cast<ft_size_t>(FT_SYSTEM_SIZE_MAX);

    if (index >= scma_block_count_ref())
        return ;
    blocks_data = scma_blocks_data_ref();
    block = &blocks_data[index];
    if (block->in_use)
        return ;
    current_link = &scma_free_head_ref();
    while (*current_link != invalid_index)
    {
        current_index = *current_link;
        if (current_index >= scma_block_count_ref())
            return ;
        scma_block &other = blocks_data[current_index];
        if (current_index != index && !other.in_use && other.size > 0
            && block->offset <= invalid_index - block->size
            && other.offset <= invalid_index - other.size
            && block->size <= invalid_index - other.size)
        {
            block_end = block->offset + block->size;
            other_end = other.offset + other.size;
            if (other_end == block->offset || block_end == other.offset)
            {
                if (other_end == block->offset)
                    block->offset = other.offset;
                block->size += other.size;
                *current_link = other.next_free_index;
                other.next_free_index = invalid_index;
                other.size = 0;
                continue ;
            }
        }
        current_link = &other.next_free_index;
    }
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
    constexpr ft_size_t MAX_BLOCK_COUNT = SIZE_MAX / sizeof(scma_block);
    ft_size_t new_capacity;
    void *new_data;
    scma_block *&blocks_data = scma_blocks_data_ref();
    ft_size_t &block_capacity = scma_block_capacity_ref();

    if (block_capacity >= required_count)
        return (1);
    if (required_count > MAX_BLOCK_COUNT)
        return (0);
    new_capacity = required_count;
    if (block_capacity <= MAX_BLOCK_COUNT / 2U
        && block_capacity * 2U > new_capacity)
        new_capacity = block_capacity * 2U;
    new_data = std::realloc(blocks_data,
            FT_UINT64_TO_NATIVE_SIZE_CAST(new_capacity * sizeof(scma_block)));
    if (new_data == ft_nullptr && new_capacity > required_count)
    {
        new_capacity = required_count;
        new_data = std::realloc(blocks_data,
                FT_UINT64_TO_NATIVE_SIZE_CAST(new_capacity * sizeof(scma_block)));
    }
    if (!new_data)
        return (0);
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
