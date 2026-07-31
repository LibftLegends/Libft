#ifndef CMA_INTERNAL_HPP
# define CMA_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../Basic/basic.hpp"

#include "../Compatebility/compatebility_stack_trace.hpp"
#include <cstdint>
#include <stdint.h>

#define PAGE_SIZE 131072
#define BYPASS_ALLOC DEBUG
#define MAGIC_NUMBER_ALLOCATED 0xDEADBEEF
#define MAGIC_NUMBER_FREE 0xBEEFDEAD

#define OFFSWITCH 0

#define CMA_SIZE 100
#define SMALL_SIZE (CMA_SIZE)
#define MEDIUM_SIZE (CMA_SIZE * 10)
#define CMA_SMALL_ARENA_MAX_ALLOCATION 256
#define CMA_FREE_BIN_COUNT 64

#define BASE_SIZE 1024
#define SMALL_ALLOC (BASE_SIZE * 1)
#define MEDIUM_ALLOC (BASE_SIZE * 10)

#ifndef DEBUG
# define DEBUG 0
#endif

#if __has_include(<valgrind/memcheck.h>)
#include <valgrind/memcheck.h>
#define PROTECT_METADATA(memory_pointer, size) VALGRIND_MAKE_MEM_NOACCESS(memory_pointer, size)
#define UNPROTECT_METADATA(memory_pointer, size) VALGRIND_MAKE_MEM_DEFINED(memory_pointer, size)
#else
#define PROTECT_METADATA(memory_pointer, size) ((void)0)
#define UNPROTECT_METADATA(memory_pointer, size) ((void)0)
#endif

extern ft_size_t    g_cma_alloc_limit;
extern ft_bool    g_cma_alloc_logging;
extern ft_size_t    g_cma_allocation_count;
extern ft_size_t    g_cma_free_count;
extern ft_size_t    g_cma_current_bytes;
extern ft_size_t    g_cma_peak_bytes;
extern int64_t    g_cma_metadata_access_depth;


struct Block
{
    uint32_t            magic;
    ft_size_t           size;
    ft_bool            free;
#ifdef LIBFT_TEST_BUILD
    ft_bool            leak_ignored;
    ft_size_t           leak_stack_frame_count;
    void                *leak_stack_frames[CMP_STACK_TRACE_MAX_FRAMES];
    const char          *leak_test_name;
#endif
    Block               *next;
    Block               *prev;
    Block               *free_next;
    Block               *free_prev;
    ft_size_t           free_bin_index;
    ft_bool             free_listed;
    int8_t              alloc_size_type;
    unsigned char       *payload;
#if DEBUG
    unsigned char       *debug_base_pointer;
    ft_size_t           debug_user_size;
#endif
} __attribute__ ((aligned(16)));

struct Page
{
    void                *start;
    ft_size_t           size;
    Page                *next;
    Page                *prev;
    Block               *blocks;
    ft_bool                heap;
    int8_t              alloc_size_type;
} __attribute__ ((aligned(16)));

struct cma_arena
{
    uint8_t     *buffer;
    ft_size_t   capacity;
    ft_size_t   offset;
    ft_bool     owns_buffer;
    uint8_t     _initialised_state;
};

extern Page *page_list;
extern Block *g_cma_free_bins[CMA_FREE_BIN_COUNT];

Block    *split_block(Block *block, ft_size_t size);
Page    *create_page(ft_size_t size);
Block    *find_free_block(ft_size_t size);
Block    *merge_block(Block *block);
Page    *find_page_of_block(Block *block);
void    free_page_if_empty(Page *page);
void    cma_validate_block(Block *block, const char *context, void *user_pointer);
Block    *cma_find_block_for_pointer(const void *memory_pointer);
void    cma_free_list_insert(Block *block);
void    cma_free_list_remove(Block *block);
void    cma_free_list_reset(void);
ft_size_t cma_free_bin_for_size(ft_size_t size);
int32_t cma_lock_allocator(ft_bool *lock_acquired);
int32_t cma_unlock_allocator(ft_bool lock_acquired);
int32_t cma_enable_thread_safety(void);
int32_t cma_disable_thread_safety(void);
ft_bool cma_is_thread_safe_enabled(void);
void cma_set_alloc_logging(ft_bool enable);
ft_bool cma_get_alloc_logging(void);
void cma_record_allocation_log(const char *format_string, ...)
            __attribute__ ((format (printf, 1, 2)));
ft_bool cma_backend_is_enabled(void) __attribute__ ((warn_unused_result));
int32_t cma_clear_backend(void);
ft_bool cma_backend_owns_pointer(const void *memory_pointer)
            __attribute__ ((warn_unused_result));
void    *cma_backend_allocate(ft_size_t size, int32_t *error_code)
            __attribute__ ((warn_unused_result, hot));
void    *cma_backend_reallocate(void *memory_pointer, ft_size_t size,
            int32_t *error_code)
            __attribute__ ((warn_unused_result, hot));
int32_t cma_backend_deallocate(void *memory_pointer) __attribute__ ((hot));
void    *cma_backend_aligned_allocate(ft_size_t alignment, ft_size_t size,
            int32_t *error_code)
            __attribute__ ((warn_unused_result, hot));
ft_size_t    cma_backend_block_size(const void *memory_pointer)
            __attribute__ ((warn_unused_result, hot));
int32_t cma_backend_checked_block_size(const void *memory_pointer,
            ft_size_t *block_size) __attribute__ ((warn_unused_result, hot));
void    *cma_small_arena_allocate_locked(ft_size_t size)
            __attribute__ ((warn_unused_result, hot));
void    *cma_small_arena_aligned_allocate_locked(ft_size_t alignment,
            ft_size_t size) __attribute__ ((warn_unused_result, hot));
ft_bool cma_small_arena_owns_pointer_locked(const void *memory_pointer)
            __attribute__ ((warn_unused_result, hot));
ft_size_t cma_small_arena_block_size_locked(const void *memory_pointer)
            __attribute__ ((warn_unused_result, hot));
int32_t cma_small_arena_deallocate_locked(void *memory_pointer)
            __attribute__ ((hot));
void    *cma_small_arena_reallocate_locked(void *memory_pointer,
            ft_size_t size) __attribute__ ((warn_unused_result, hot));
void    cma_small_arena_reset_for_tests(void);
#ifndef CMA_ENABLE_METADATA_PROTECTION
# define CMA_ENABLE_METADATA_PROTECTION 1
#endif

int32_t cma_metadata_make_writable(void);
void    cma_metadata_make_inaccessible(void);
ft_bool    cma_metadata_guard_increment(void);
ft_bool    cma_metadata_guard_decrement(void);
Block    *cma_metadata_allocate_block(void) __attribute__ ((warn_unused_result));
void    cma_metadata_release_block(Block *block);
void    cma_metadata_reset(void);

#ifdef LIBFT_TEST_BUILD
void    cma_capture_leak_stack(Block *block, ft_size_t skip_count);
#endif

#if DEBUG
ft_size_t    cma_debug_allocation_size(ft_size_t requested_size);
void    cma_debug_initialize_block(Block *block);
void    cma_debug_prepare_allocation(Block *block, ft_size_t user_size);
void    cma_debug_release_allocation(Block *block, const char *context,
            const void *user_pointer);
unsigned char    *cma_debug_user_pointer(const Block *block);
ft_size_t    cma_debug_user_size(const Block *block);
ft_size_t    cma_debug_guard_size(void);
#else
inline ft_size_t cma_debug_allocation_size(ft_size_t requested_size)
{
    return (requested_size);
}

inline void cma_debug_initialize_block(Block *block)
{
#ifdef LIBFT_TEST_BUILD
    if (block != nullptr)
    {
        block->leak_ignored = FT_FALSE;
        block->leak_stack_frame_count = 0;
        block->leak_test_name = nullptr;
    }
#endif
    (void)block;
    return ;
}

inline void cma_debug_prepare_allocation(Block *block, ft_size_t user_size)
{
    (void)block;
    (void)user_size;
    return ;
}

inline void cma_debug_release_allocation(Block *block, const char *context,
        const void *user_pointer)
{
    (void)block;
    (void)context;
    (void)user_pointer;
    return ;
}

inline unsigned char *cma_debug_user_pointer(const Block *block)
{
    if (!block)
        return (nullptr);
    return (block->payload);
}

inline ft_size_t cma_debug_user_size(const Block *block)
{
    if (!block)
        return (0);
    return (block->size);
}

inline ft_size_t cma_debug_guard_size(void)
{
    return (0);
}
#endif

inline unsigned char *cma_block_user_pointer(const Block *block)
{
    return (cma_debug_user_pointer(block));
}

inline ft_size_t cma_block_user_size(const Block *block)
{
    return (cma_debug_user_size(block));
}

inline __attribute__((always_inline, hot)) ft_size_t align16(ft_size_t size)
{
    return ((size + 15) & ~static_cast<ft_size_t>(15));
}

inline __attribute__((always_inline, hot)) ft_bool cma_block_is_free(const Block *block)
{
    if (!block)
        return (FT_FALSE);
    if (block->magic == MAGIC_NUMBER_FREE)
        return (FT_TRUE);
    return (FT_FALSE);
}

inline __attribute__((always_inline, hot)) void cma_mark_block_free(Block *block)
{
    if (!block)
        return ;
    block->free = FT_TRUE;
    block->magic = MAGIC_NUMBER_FREE;
    return ;
}

inline __attribute__((always_inline, hot)) void cma_mark_block_allocated(Block *block)
{
    if (!block)
        return ;
    if (block->free_listed == FT_TRUE)
        cma_free_list_remove(block);
    block->free = FT_FALSE;
    block->magic = MAGIC_NUMBER_ALLOCATED;
    return ;
}

inline __attribute__((always_inline, hot)) void cma_update_block_magic(Block *block)
{
    if (!block)
        return ;
    if (cma_block_is_free(block))
    {
        cma_mark_block_free(block);
        return ;
    }
    cma_mark_block_allocated(block);
    return ;
}

#endif
