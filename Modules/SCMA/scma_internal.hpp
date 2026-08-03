#ifndef SCMA_INTERNAL_HPP
# define SCMA_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include <cstddef>
#include "../Compatebility/compatebility_stack_trace.hpp"
#include "SCMA.hpp"

struct scma_block
{
    ft_size_t    offset;
    ft_size_t    size;
    int32_t          in_use;
    ft_size_t    generation;
    ft_size_t    prev_index;
    ft_size_t    next_index;
    ft_size_t    next_free_index;
#ifdef LIBFT_TEST_BUILD
    ft_bool leak_ignored;
    ft_size_t leak_stack_frame_count;
    void *leak_stack_frames[CMP_STACK_TRACE_MAX_FRAMES];
    const char *leak_test_name;
#endif
};

struct scma_block_span
{
    scma_block    *data;
    ft_size_t     count;
};

unsigned char    *&scma_heap_data_ref(void);
ft_size_t        &scma_heap_capacity_ref(void);
scma_block       *&scma_blocks_data_ref(void);
ft_size_t        &scma_block_capacity_ref(void);
ft_size_t        &scma_block_count_ref(void);
ft_size_t        &scma_used_size_ref(void);
ft_size_t        &scma_live_size_ref(void);
ft_size_t        &scma_live_head_ref(void);
ft_size_t        &scma_live_tail_ref(void);
ft_size_t        &scma_free_head_ref(void);
ft_bool          &scma_compaction_needed_ref(void);
void    scma_reset_compaction(void);
int32_t              &scma_initialised_ref(void);

scma_block_span    scma_get_block_span(void);
unsigned char    *scma_get_heap_data(void);

scma_handle    scma_invalid_handle(void);
int32_t     scma_handle_is_invalid(scma_handle handle);
int32_t scma_compact(void);
int32_t scma_compact_incremental(ft_size_t byte_budget);
int32_t     scma_validate_handle(scma_handle handle, scma_block **out_block);
int32_t     scma_ensure_block_capacity(ft_size_t required_count);
int32_t     scma_ensure_capacity(ft_size_t required_size);
ft_size_t    scma_next_generation(ft_size_t generation);
void    scma_link_block_at_tail(ft_size_t index);
void    scma_unlink_block(ft_size_t index);
void    scma_push_free_block(ft_size_t index);
ft_size_t scma_pop_free_block(void);
int32_t     scma_unlock_and_return_int(int32_t value);
ft_size_t    scma_unlock_and_return_size(ft_size_t value);
scma_handle    scma_unlock_and_return_handle(scma_handle value);
void    *scma_unlock_and_return_pointer(void *value);
void    scma_unlock_and_return_void(void);

#ifdef LIBFT_TEST_BUILD
void    scma_capture_leak_stack(scma_block *block, ft_size_t skip_count);
void    scma_test_secure_wipe_runtime(void);
#endif

#endif
