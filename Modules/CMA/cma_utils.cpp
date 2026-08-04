#include <cstdint>
#include <cstdlib>
#include <cstddef>
#include <cstring>
#include "CMA.hpp"
#include "cma_internal.hpp"

#include "../System_utils/system_utils.hpp"
#include "../Sink/sink.hpp"
#include <cstdarg>
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local Page *g_cma_cached_lookup_page = nullptr;
static thread_local uint64_t g_cma_cached_lookup_generation = 0;

static void verify_traversal_link(Block *current_block, Block *next_block,
        const char *context);

static ft_bool cma_page_contains_pointer(const Page *page,
        const unsigned char *target_pointer)
{
    const unsigned char *page_start;
    const unsigned char *page_end;

    if (page == nullptr || target_pointer == nullptr)
        return (FT_FALSE);
    page_start = static_cast<const unsigned char *>(page->start);
    page_end = page_start + page->size;
    if (target_pointer < page_start || target_pointer >= page_end)
        return (FT_FALSE);
    return (FT_TRUE);
}

static Block *cma_find_block_in_page(Page *page,
        const unsigned char *target_pointer)
{
    Block *current_block;

    if (cma_page_contains_pointer(page, target_pointer) == FT_FALSE)
        return (nullptr);
    current_block = page->blocks;
    while (current_block)
    {
        if (cma_block_user_pointer(current_block) == target_pointer)
            return (current_block);
        if (cma_block_user_pointer(current_block) > target_pointer)
            return (nullptr);
        verify_traversal_link(current_block, current_block->next,
            "cma_find_block_for_pointer corrupted traversal link");
        current_block = current_block->next;
    }
    return (nullptr);
}

void cma_set_alloc_logging(ft_bool enable)
{
    g_cma_alloc_logging = enable;
    return ;
}

ft_bool cma_get_alloc_logging(void)
{
    return (g_cma_alloc_logging);
}

void cma_record_allocation_log(const char *format_string, ...)
{
    char message_buffer[256];
    va_list argument_list;
    int32_t formatted_length;

    if (format_string == nullptr || g_cma_alloc_logging == FT_FALSE)
        return ;
    va_start(argument_list, format_string);
    formatted_length = std::vsnprintf(message_buffer, sizeof(message_buffer),
            format_string, argument_list);
    va_end(argument_list);
    if (formatted_length < 0)
        return ;
    (void)sink_record_message(0, message_buffer);
    return ;
}
#include "../Errno/errno.hpp"

static ft_size_t determine_page_size(ft_size_t size)
{
    if (size < SMALL_SIZE)
        return (SMALL_ALLOC);
    else if (size < MEDIUM_SIZE)
        return (MEDIUM_ALLOC);
    return (size);
}

static int8_t determine_which_block_to_use(ft_size_t size)
{
    if (size < SMALL_SIZE)
        return (0);
    else if (size < MEDIUM_SIZE)
        return (1);
    return (2);
}

static void report_corrupted_block(Block *block, const char *context,
        void *user_pointer);

ft_size_t cma_free_bin_for_size(ft_size_t size)
{
    ft_size_t bin_index;
    ft_size_t scaled_size;

    bin_index = 0;
    scaled_size = size / 16;
    while (scaled_size > 1 && bin_index + 1 < CMA_FREE_BIN_COUNT)
    {
        scaled_size >>= 1;
        bin_index++;
    }
    return (bin_index);
}

void cma_free_list_remove(Block *block)
{
    ft_size_t bin_index;

    if (block == nullptr || block->free_listed == FT_FALSE)
        return ;
    bin_index = block->free_bin_index;
    if (bin_index >= CMA_FREE_BIN_COUNT)
        report_corrupted_block(block, "cma_free_list_remove invalid bin",
            nullptr);
    if (block->free_prev != nullptr)
    {
        if (block->free_prev->free_next != block)
            report_corrupted_block(block,
                "cma_free_list_remove invalid previous link", nullptr);
        block->free_prev->free_next = block->free_next;
    }
    else
    {
        if (g_cma_free_bins[bin_index] != block)
            report_corrupted_block(block,
                "cma_free_list_remove invalid bin head", nullptr);
        g_cma_free_bins[bin_index] = block->free_next;
    }
    if (block->free_next != nullptr)
    {
        if (block->free_next->free_prev != block)
            report_corrupted_block(block,
                "cma_free_list_remove invalid next link", nullptr);
        block->free_next->free_prev = block->free_prev;
    }
    block->free_next = nullptr;
    block->free_prev = nullptr;
    block->free_bin_index = 0;
    block->free_listed = FT_FALSE;
    return ;
}

void cma_free_list_insert(Block *block)
{
    ft_size_t bin_index;

    if (block == nullptr)
        return ;
    cma_validate_block(block, "cma_free_list_insert", nullptr);
    if (cma_block_is_free(block) == FT_FALSE)
        report_corrupted_block(block,
            "cma_free_list_insert allocated block", nullptr);
    if (block->free_listed == FT_TRUE)
        return ;
    bin_index = cma_free_bin_for_size(block->size);
    block->free_prev = nullptr;
    block->free_next = g_cma_free_bins[bin_index];
    if (block->free_next != nullptr)
        block->free_next->free_prev = block;
    block->free_bin_index = bin_index;
    block->free_listed = FT_TRUE;
    g_cma_free_bins[bin_index] = block;
    return ;
}

void cma_free_list_reset(void)
{
    ft_size_t bin_index;

    bin_index = 0;
    while (bin_index < CMA_FREE_BIN_COUNT)
    {
        g_cma_free_bins[bin_index] = nullptr;
        bin_index++;
    }
    return ;
}

static void *create_stack_block(void)
{
    static char memory_block[PAGE_SIZE];

    return (memory_block);
}

static void report_corrupted_block(Block *block, const char *context,
        void *user_pointer)
{
    (void)block;
    (void)user_pointer;
    (void)context;
    su_sigabrt();
    return ;
}

static ft_bool are_blocks_adjacent(Block *left_block, Block *right_block)
{
    unsigned char   *expected_address;
    unsigned char   *actual_address;

    if (left_block == nullptr || right_block == nullptr)
        return (FT_FALSE);
    if (left_block->payload == nullptr || right_block->payload == nullptr)
        return (FT_FALSE);
    expected_address = left_block->payload + left_block->size;
    actual_address = right_block->payload;
    if (expected_address == actual_address)
        return (FT_TRUE);
    return (FT_FALSE);
}

static void verify_forward_link(Block *block, Block *next_block)
{
    if (next_block->prev != block)
        report_corrupted_block(next_block, "merge_block inconsistent next link",
            nullptr);
    if (are_blocks_adjacent(block, next_block) == FT_FALSE)
        report_corrupted_block(next_block, "merge_block detached next neighbor",
            nullptr);
    return ;
}

static void verify_backward_link(Block *block, Block *previous_block)
{
    if (previous_block->next != block)
        report_corrupted_block(previous_block,
            "merge_block inconsistent prev link", nullptr);
    if (are_blocks_adjacent(previous_block, block) == FT_FALSE)
        report_corrupted_block(previous_block,
            "merge_block detached prev neighbor", nullptr);
    return ;
}

static void verify_traversal_link(Block *current_block, Block *next_block,
        const char *context)
{
    if (current_block == nullptr || next_block == nullptr)
        return ;
    if (current_block == next_block)
        report_corrupted_block(current_block, context, nullptr);
    if (next_block->prev != current_block)
        report_corrupted_block(next_block, context, nullptr);
    if (are_blocks_adjacent(current_block, next_block) == FT_FALSE)
        report_corrupted_block(next_block, context, nullptr);
    return ;
}

void cma_validate_block(Block *block, const char *context, void *user_pointer)
{
    const char    *location;
    ft_bool            sentinel_free;
    ft_bool            sentinel_allocated;

    location = context;
    if (block == nullptr)
    {
        if (location == nullptr)
            location = "unknown";
        su_sigabrt();
    }
    sentinel_free = (block->magic == MAGIC_NUMBER_FREE);
    sentinel_allocated = (block->magic == MAGIC_NUMBER_ALLOCATED);
    if (!sentinel_free && !sentinel_allocated)
        report_corrupted_block(block, location, user_pointer);
    if (sentinel_free && block->free == FT_FALSE)
        cma_mark_block_free(block);
    if (sentinel_allocated && block->free == FT_TRUE)
        cma_mark_block_allocated(block);
    if (block->payload == nullptr)
        report_corrupted_block(block, location, user_pointer);
    return ;
}

static ft_size_t    minimum_split_payload(void)
{
    ft_size_t    minimum_payload;

    minimum_payload = align16(1);
    if (minimum_payload < static_cast<ft_size_t>(16))
        minimum_payload = static_cast<ft_size_t>(16);
    return (minimum_payload);
}

Block* split_block(Block* block, ft_size_t size)
{
    Block       *new_block;
    ft_size_t    available_size;
    ft_size_t    remaining_size;
    ft_size_t    minimum_payload;
    Block       *result_block;
    ft_bool         metadata_guarded;
    ft_bool         block_was_free;

    result_block = block;
    metadata_guarded = FT_FALSE;
    cma_validate_block(block, "split_block", nullptr);
    block_was_free = cma_block_is_free(block);
    cma_free_list_remove(block);
    metadata_guarded = cma_metadata_guard_increment();
    if (!metadata_guarded)
        goto split_block_cleanup;
    if (cma_metadata_make_writable() != FT_ERR_SUCCESS)
        goto split_block_cleanup;
    available_size = block->size;
    if (size >= available_size)
    {
        if (cma_block_is_free(block))
            cma_mark_block_free(block);
        else
            cma_mark_block_allocated(block);
        goto split_block_cleanup;
    }
    remaining_size = available_size - size;
    minimum_payload = minimum_split_payload();
    if (remaining_size <= minimum_payload)
    {
        if (cma_block_is_free(block))
            cma_mark_block_free(block);
        else
            cma_mark_block_allocated(block);
        goto split_block_cleanup;
    }
    new_block = cma_metadata_allocate_block();
    if (new_block == nullptr)
    {
        if (cma_block_is_free(block))
            cma_mark_block_free(block);
        else
            cma_mark_block_allocated(block);
        goto split_block_cleanup;
    }
    new_block->size = remaining_size;
    new_block->payload = block->payload + size;
    new_block->alloc_size_type = block->alloc_size_type;
    cma_debug_initialize_block(new_block);
    cma_mark_block_free(new_block);
    new_block->next = block->next;
    new_block->prev = block;
    if (new_block->next)
    {
        cma_validate_block(new_block->next, "split_block relink next", nullptr);
        new_block->next->prev = new_block;
    }
    block->next = new_block;
    block->size = size;
    if (cma_block_is_free(block))
        cma_mark_block_free(block);
    else
        cma_mark_block_allocated(block);
    cma_debug_initialize_block(block);
    cma_free_list_insert(new_block);
    if (block_was_free == FT_TRUE)
        cma_free_list_insert(block);
    result_block = block;
split_block_cleanup:
    if (block_was_free == FT_TRUE && cma_block_is_free(block) == FT_TRUE)
        cma_free_list_insert(block);
    if (metadata_guarded)
        cma_metadata_guard_decrement();
    return (result_block);
}

Page *create_page(ft_size_t size)
{
    ft_size_t page_size = determine_page_size(size);
    ft_bool use_heap = FT_TRUE;

    if (page_list == nullptr && page_size <= PAGE_SIZE)
    {
        page_size = PAGE_SIZE;
        use_heap = FT_FALSE;
    }
    else
    {
        if (size > determine_page_size(size))
            page_size = size;
    }
    void* memory_pointer;
    if (use_heap)
    {
        memory_pointer = std::malloc(page_size);
        if (!memory_pointer)
            return (nullptr);
    }
    else
    {
        memory_pointer = create_stack_block();
        if (!memory_pointer)
            return (nullptr);
    }
    Page* page = static_cast<Page*>(std::malloc(sizeof(Page)));
    if (!page)
    {
        if (use_heap)
            std::free(memory_pointer);
        return (nullptr);
    }
    std::memset(page, 0, sizeof(Page));
    page->heap = use_heap;
    page->start = memory_pointer;
    page->size = page_size;
    page->alloc_size_type = determine_which_block_to_use(size);
    page->next = nullptr;
    page->prev = nullptr;
    page->blocks = cma_metadata_allocate_block();
    if (page->blocks == nullptr)
    {
        if (use_heap)
            std::free(memory_pointer);
        std::free(page);
        return (nullptr);
    }
    page->blocks->size = page_size;
    page->blocks->payload = static_cast<unsigned char *>(memory_pointer);
    page->blocks->alloc_size_type = page->alloc_size_type;
    cma_debug_initialize_block(page->blocks);
    cma_mark_block_free(page->blocks);
    page->blocks->next = nullptr;
    page->blocks->prev = nullptr;
    cma_validate_block(page->blocks, "create_page", nullptr);
    if (!page_list)
    {
        page_list = page;
    }
    else
    {
        page->next = page_list;
        page_list->prev = page;
        page_list = page;
    }
    g_cma_page_generation++;
    cma_free_list_insert(page->blocks);
    return (page);
}

Block *find_free_block(ft_size_t size)
{
    ft_size_t bin_index;
    int8_t alloc_size_type;

    alloc_size_type = determine_which_block_to_use(size);
    bin_index = cma_free_bin_for_size(size);
    while (bin_index < CMA_FREE_BIN_COUNT)
    {
        Block *current_block;

        current_block = g_cma_free_bins[bin_index];
        while (current_block)
        {
            Block *next_free_block;

            cma_validate_block(current_block, "find_free_block", nullptr);
            if (current_block->free_listed == FT_FALSE
                || current_block->free_bin_index != bin_index)
                report_corrupted_block(current_block,
                    "find_free_block invalid free-list metadata", nullptr);
            next_free_block = current_block->free_next;
            if (next_free_block != nullptr
                && next_free_block->free_prev != current_block)
                report_corrupted_block(next_free_block,
                    "find_free_block corrupted free-list link", nullptr);
            if (current_block->alloc_size_type == alloc_size_type
                && current_block->size >= size)
                return (current_block);
            current_block = next_free_block;
        }
        bin_index++;
    }
    return (nullptr);
}

Block    *cma_find_block_for_pointer(const void *memory_pointer)
{
    Page    *current_page;
    const unsigned char *target_pointer;

    if (memory_pointer == nullptr)
        return (nullptr);
    target_pointer = static_cast<const unsigned char *>(memory_pointer);
    if (g_cma_cached_lookup_page != nullptr
        && g_cma_cached_lookup_generation == g_cma_page_generation
        && cma_page_contains_pointer(g_cma_cached_lookup_page, target_pointer)
            == FT_TRUE)
        return (cma_find_block_in_page(g_cma_cached_lookup_page,
                    target_pointer));
    current_page = page_list;
    while (current_page)
    {
        if (cma_page_contains_pointer(current_page, target_pointer)
                == FT_FALSE)
        {
            current_page = current_page->next;
            continue ;
        }
        g_cma_cached_lookup_page = current_page;
        g_cma_cached_lookup_generation = g_cma_page_generation;
        return (cma_find_block_in_page(current_page, target_pointer));
    }
    return (nullptr);
}

Block *merge_block(Block *block)
{
    Block       *current;
    Block       *next_block;
    Block       *previous_block;

    cma_validate_block(block, "merge_block", nullptr);
    current = block;
    cma_free_list_remove(current);
    previous_block = current->prev;
    while (previous_block && cma_block_is_free(previous_block))
    {
        cma_validate_block(previous_block, "merge_block prev", nullptr);
        cma_free_list_remove(previous_block);
        verify_backward_link(current, previous_block);
#ifdef DEBUG
#endif
        previous_block->size += current->size;
#ifdef DEBUG
#endif
        previous_block->next = current->next;
        if (current->next)
        {
            cma_validate_block(current->next, "merge_block relink prev", nullptr);
            current->next->prev = previous_block;
        }
        current->next = nullptr;
        current->prev = nullptr;
        cma_mark_block_free(current);
        cma_metadata_release_block(current);
        current = previous_block;
        previous_block = current->prev;
    }
    next_block = current->next;
    while (next_block && cma_block_is_free(next_block))
    {
        cma_validate_block(next_block, "merge_block next", nullptr);
        cma_free_list_remove(next_block);
        verify_forward_link(current, next_block);
#ifdef DEBUG
#endif
        current->size += next_block->size;
#ifdef DEBUG
#endif
        current->next = next_block->next;
        if (current->next)
        {
            cma_validate_block(current->next, "merge_block relink next", nullptr);
            current->next->prev = current;
        }
        next_block->next = nullptr;
        next_block->prev = nullptr;
        cma_mark_block_free(next_block);
        cma_metadata_release_block(next_block);
        next_block = current->next;
    }
    cma_mark_block_free(current);
    cma_free_list_insert(current);
#ifdef DEBUG
#endif
    return (current);
}

Page *find_page_of_block(Block *block)
{
    Page *page = page_list;
    const unsigned char *payload;

    if (block == nullptr)
        return (nullptr);
    payload = block->payload;
    if (g_cma_cached_lookup_page != nullptr
        && g_cma_cached_lookup_generation == g_cma_page_generation
        && cma_page_contains_pointer(g_cma_cached_lookup_page, payload)
            == FT_TRUE)
        return (g_cma_cached_lookup_page);
    while (page)
    {
        if (cma_page_contains_pointer(page, payload) == FT_TRUE)
        {
            g_cma_cached_lookup_page = page;
            g_cma_cached_lookup_generation = g_cma_page_generation;
            return (page);
        }
        page = page->next;
    }
    return (nullptr);
}

void free_page_if_empty(Page *page)
{
    if (!page || page->heap == FT_FALSE)
        return ;
    if (page->blocks && cma_block_is_free(page->blocks) &&
        page->blocks->next == nullptr &&
        page->blocks->prev == nullptr)
    {
        cma_free_list_remove(page->blocks);
        if (page->prev)
            page->prev->next = page->next;
        if (page->next)
            page->next->prev = page->prev;
        if (page_list == page)
            page_list = page->next;
        g_cma_page_generation++;
        if (g_cma_cached_lookup_page == page)
            g_cma_cached_lookup_page = nullptr;
        std::free(page->start);
        cma_metadata_release_block(page->blocks);
        std::free(page);
        return ;
    }
    return ;
}

void cma_invalidate_page_lookup_cache(void)
{
    g_cma_cached_lookup_page = nullptr;
    g_cma_cached_lookup_generation = g_cma_page_generation;
    return ;
}

int32_t cma_get_extended_stats(ft_size_t *allocation_count,
        ft_size_t *free_count,
        ft_size_t *current_bytes,
        ft_size_t *peak_bytes)
{
    ft_bool lock_acquired = FT_FALSE;
    int32_t lock_error = cma_lock_allocator(&lock_acquired);

    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (allocation_count != nullptr)
        *allocation_count = g_cma_allocation_count;
    if (free_count != nullptr)
        *free_count = g_cma_free_count;
    if (current_bytes != nullptr)
        *current_bytes = g_cma_current_bytes;
    if (peak_bytes != nullptr)
        *peak_bytes = g_cma_peak_bytes;
    cma_unlock_allocator(lock_acquired);
    return (FT_ERR_SUCCESS);
}

int32_t cma_get_stats(ft_size_t *allocation_count, ft_size_t *free_count)
{
    return (cma_get_extended_stats(allocation_count, free_count,
            nullptr, nullptr));
}
