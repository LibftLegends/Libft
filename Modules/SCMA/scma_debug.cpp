#include <cstdio>
#include <cinttypes>
#include "../Basic/basic.hpp"
#include "../Compatebility/compatebility_stack_trace.hpp"
#include "../Errno/errno.hpp"
#include "SCMA.hpp"
#include "scma_internal.hpp"
#ifdef LIBFT_TEST_BUILD
# include "../System_utils/test_system_utils_runner.hpp"
#endif
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t    scma_get_stats(scma_stats *out_stats)
{
    scma_stats stats;
    ft_size_t &block_count = scma_block_count_ref();
    ft_size_t &used_size = scma_live_size_ref();
    ft_size_t &heap_capacity = scma_heap_capacity_ref();

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    }
    if (!scma_initialised_ref())
    {
        return (scma_unlock_and_return_int(FT_ERR_NOT_INITIALISED));
    }
    if (!out_stats)
    {
        return (scma_unlock_and_return_int(FT_ERR_INVALID_ARGUMENT));
    }
    stats.block_count = block_count;
    stats.used_size = used_size;
    stats.heap_capacity = heap_capacity;
    *out_stats = stats;
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

void    scma_debug_dump(void)
{
    scma_block_span span;
    ft_size_t index;
    ft_size_t &used_size = scma_used_size_ref();
    ft_size_t &heap_capacity_ref_value = scma_heap_capacity_ref();
    uint64_t block_count_print;
    uint64_t used_size_print;
    uint64_t heap_capacity_print;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
    {
        return ;
    }
    if (!scma_initialised_ref())
    {
        std::printf("[scma] not initialised\n");
        scma_unlock_and_return_void();
        return ;
    }
    span = scma_get_block_span();
    block_count_print = span.count;
    used_size_print = used_size;
    heap_capacity_print = heap_capacity_ref_value;
    std::printf("[scma] blocks=%" PRIu64 " used=%" PRIu64 " capacity=%" PRIu64 "\n",
        block_count_print,
        used_size_print,
        heap_capacity_print);
    index = 0;
    while (index < span.count)
    {
        scma_block *block;
        uint64_t index_print;
        uint64_t offset_print;
        uint64_t size_print;
        uint64_t generation_print;

        block = &span.data[index];
        index_print = index;
        offset_print = block->offset;
        size_print = block->size;
        generation_print = block->generation;
        std::printf("  [%" PRIu64 "] offset=%" PRIu64 " size=%" PRIu64 " in_use=%d generation=%" PRIu64 "\n",
            index_print, offset_print, size_print, block->in_use, generation_print);
        index++;
    }
    scma_unlock_and_return_void();
    return ;
}

#ifdef LIBFT_TEST_BUILD
void    scma_capture_leak_stack(scma_block *block, ft_size_t skip_count)
{
    if (block == ft_nullptr)
        return ;
    block->leak_test_name = ft_test_runner_current_test_name();
    block->leak_stack_frame_count = cmp_stack_trace_capture(
            block->leak_stack_frames,
            CMP_STACK_TRACE_MAX_FRAMES,
            skip_count);
    return ;
}

int32_t scma_get_leak_summary(scma_leak_summary *out_summary)
{
    scma_block_span span;
    ft_size_t entry_index;
    scma_leak_summary summary;

    if (out_summary == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!scma_initialised_ref())
        return (scma_unlock_and_return_int(FT_ERR_NOT_INITIALISED));
    summary.live_block_count = 0;
    summary.live_bytes = 0;
    summary.ignored_block_count = 0;
    summary.ignored_bytes = 0;
    span = scma_get_block_span();
    entry_index = 0;
    while (entry_index < span.count)
    {
        scma_block *block;

        block = &span.data[entry_index];
        if (block->in_use)
        {
            if (block->leak_ignored == FT_TRUE)
            {
                summary.ignored_block_count += 1;
                summary.ignored_bytes += block->size;
            }
            else
            {
                summary.live_block_count += 1;
                summary.live_bytes += block->size;
            }
        }
        entry_index += 1;
    }
    *out_summary = summary;
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t scma_get_leak_entries(scma_leak_entry *entries, ft_size_t capacity,
    ft_size_t *entry_count)
{
    scma_block_span span;
    ft_size_t leak_count;
    ft_size_t entry_index;
    ft_size_t output_index;

    if (entry_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (entries == ft_nullptr && capacity != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!scma_initialised_ref())
        return (scma_unlock_and_return_int(FT_ERR_NOT_INITIALISED));
    span = scma_get_block_span();
    leak_count = 0;
    entry_index = 0;
    while (entry_index < span.count)
    {
        scma_block *block;

        block = &span.data[entry_index];
        if (block->in_use && block->leak_ignored == FT_FALSE)
            leak_count += 1;
        entry_index += 1;
    }
    *entry_count = leak_count;
    if (capacity < leak_count)
        return (scma_unlock_and_return_int(FT_ERR_INVALID_OPERATION));
    output_index = 0;
    entry_index = 0;
    while (entry_index < span.count)
    {
        scma_block *block;

        block = &span.data[entry_index];
        if (block->in_use && block->leak_ignored == FT_FALSE)
        {
            entries[output_index].handle.index = entry_index;
            entries[output_index].handle.generation = block->generation;
            entries[output_index].offset = block->offset;
            entries[output_index].size = block->size;
            entries[output_index].generation = block->generation;
            output_index += 1;
        }
        entry_index += 1;
    }
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t scma_report_leaks(void)
{
    scma_block_span span;
    ft_size_t entry_index;
    scma_leak_summary summary;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!scma_initialised_ref())
        return (scma_unlock_and_return_int(FT_ERR_NOT_INITIALISED));
    summary.live_block_count = 0;
    summary.live_bytes = 0;
    summary.ignored_block_count = 0;
    summary.ignored_bytes = 0;
    span = scma_get_block_span();
    entry_index = 0;
    while (entry_index < span.count)
    {
        scma_block *block;

        block = &span.data[entry_index];
        if (block->in_use)
        {
            if (block->leak_ignored == FT_TRUE)
            {
                summary.ignored_block_count += 1;
                summary.ignored_bytes += block->size;
            }
            else
            {
                summary.live_block_count += 1;
                summary.live_bytes += block->size;
            }
        }
        entry_index += 1;
    }
    std::printf("[scma] leak summary: live_blocks=%" PRIu64 " live_bytes=%" PRIu64
        " ignored_blocks=%" PRIu64 " ignored_bytes=%" PRIu64 "\n",
        summary.live_block_count, summary.live_bytes,
        summary.ignored_block_count, summary.ignored_bytes);
    entry_index = 0;
    while (entry_index < span.count)
    {
        scma_block *block;

        block = &span.data[entry_index];
        if (block->in_use && block->leak_ignored == FT_FALSE)
        {
            const char *test_name;

            test_name = block->leak_test_name;
            if (test_name == nullptr)
                test_name = "<unknown>";
            std::printf("  [leak] handle={index=%" PRIu64 ",generation=%" PRIu64
                "} test=%s offset=%" PRIu64 " size=%" PRIu64 "\n",
                entry_index, block->generation, test_name,
                block->offset, block->size);
            cmp_stack_trace_print(stdout, block->leak_stack_frames,
                block->leak_stack_frame_count);
        }
        entry_index += 1;
    }
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t scma_untrack_leak(scma_handle handle)
{
    scma_block *block;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!scma_validate_handle(handle, &block))
        return (scma_unlock_and_return_int(FT_ERR_INVALID_HANDLE));
    block->leak_ignored = FT_TRUE;
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}

int32_t scma_track_leak(scma_handle handle)
{
    scma_block *block;

    if (scma_mutex_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_SYS_MUTEX_LOCK_FAILED);
    if (!scma_validate_handle(handle, &block))
        return (scma_unlock_and_return_int(FT_ERR_INVALID_HANDLE));
    block->leak_ignored = FT_FALSE;
    return (scma_unlock_and_return_int(FT_ERR_SUCCESS));
}
#endif
