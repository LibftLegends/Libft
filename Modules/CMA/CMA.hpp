#ifndef CMA_HPP
# define CMA_HPP

#include "../Basic/basic.hpp"

typedef void    *(*cma_backend_allocate_function)(ft_size_t size, void *user_data);
typedef void    *(*cma_backend_reallocate_function)(void *memory_pointer,
            ft_size_t size, void *user_data);
typedef void    (*cma_backend_deallocate_function)(void *memory_pointer,
            void *user_data);
typedef void    *(*cma_backend_aligned_allocate_function)(ft_size_t alignment,
            ft_size_t size, void *user_data);
typedef ft_size_t    (*cma_backend_get_allocation_size_function)(
            const void *memory_pointer, void *user_data);
typedef ft_bool (*cma_backend_owns_allocation_function)(
            const void *memory_pointer, void *user_data);

struct cma_backend_hooks
{
    cma_backend_allocate_function allocate;
    cma_backend_reallocate_function reallocate;
    cma_backend_deallocate_function deallocate;
    cma_backend_aligned_allocate_function aligned_allocate;
    cma_backend_get_allocation_size_function get_allocation_size;
    cma_backend_owns_allocation_function owns_allocation;
    void    *user_data;
};

int32_t     cma_set_backend(const cma_backend_hooks *hooks)
                __attribute__ ((warn_unused_result));
int32_t     cma_clear_backend(void);
ft_bool     cma_backend_is_enabled(void) __attribute__ ((warn_unused_result));
void        *cma_malloc(ft_size_t size) __attribute__ ((warn_unused_result, hot));
void        *cma_realloc(void *memory_pointer, ft_size_t size)
                __attribute__ ((warn_unused_result, hot));
void        *cma_aligned_alloc(ft_size_t alignment, ft_size_t size)
                __attribute__ ((warn_unused_result, hot));
void        cma_free(void* memory_pointer) __attribute__ ((hot));
void        cma_bzero_and_free(void* memory_pointer) __attribute__ ((hot));
int32_t      cma_checked_free(void* memory_pointer) __attribute__ ((warn_unused_result, hot));
void        cma_free_double(char **content);
ft_size_t    cma_block_size(const void *memory_pointer)
                __attribute__ ((warn_unused_result, hot));
int32_t     cma_checked_block_size(const void *memory_pointer,
                ft_size_t *block_size) __attribute__ ((warn_unused_result, hot));
ft_size_t   cma_alloc_size(const void *memory_pointer)
                __attribute__ ((warn_unused_result, hot));
int32_t     cma_set_alloc_limit(ft_size_t limit);
void        cma_set_alloc_logging(ft_bool enable);
ft_bool     cma_get_alloc_logging(void);
int32_t     cma_set_thread_safety(ft_bool enable);
int32_t     cma_enable_thread_safety(void);
int32_t     cma_disable_thread_safety(void);
int32_t     cma_enable_thread_safety_timed(uint64_t timeout_ms);
int32_t     cma_disable_thread_safety_timed(uint64_t timeout_ms);
int32_t     cma_set_thread_safety_timed(ft_bool enable, uint64_t timeout_ms);
int32_t     cma_try_set_thread_safety(ft_bool enable);
ft_bool cma_is_thread_safe_enabled(void);
int32_t     cma_get_stats(ft_size_t *allocation_count, ft_size_t *free_count);
int32_t     cma_get_extended_stats(ft_size_t *allocation_count,
                ft_size_t *free_count, ft_size_t *current_bytes,
                ft_size_t *peak_bytes);

#ifdef LIBFT_TEST_BUILD
struct cma_leak_entry
{
    void        *memory_pointer;
    ft_size_t   size;
};

struct cma_leak_summary
{
    ft_size_t   live_block_count;
    ft_size_t   live_bytes;
    ft_size_t   ignored_block_count;
    ft_size_t   ignored_bytes;
};

int32_t     cma_get_leak_summary(cma_leak_summary *out_summary);
int32_t     cma_get_leak_entries(cma_leak_entry *entries, ft_size_t capacity,
                ft_size_t *entry_count);
int32_t     cma_report_leaks(void);
int32_t     cma_untrack_leak(void *memory_pointer);
int32_t     cma_track_leak(void *memory_pointer);
#endif
#endif
