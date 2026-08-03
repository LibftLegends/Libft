# CMA

The `CMA` module is the project allocator layer. It wraps allocation, reallocation, aligned allocation, block-size tracking, statistics, optional thread safety, and test-only leak inspection behind a common API.

## Backend Hooks

- `cma_backend_allocate_function` - Backend callback that allocates a block.
- `cma_backend_reallocate_function` - Backend callback that resizes an existing block.
- `cma_backend_deallocate_function` - Backend callback that releases a block.
- `cma_backend_aligned_allocate_function` - Backend callback that returns aligned storage.
- `cma_backend_get_allocation_size_function` - Backend callback that reports the size of a tracked allocation.
- `cma_backend_owns_allocation_function` - Backend callback that reports whether a pointer belongs to the backend.
- `cma_backend_hooks` - Callback table plus `user_data` pointer used to install a custom allocator backend.

## Backend Control

- `cma_set_backend(const cma_backend_hooks *hooks)` - Installs custom allocator hooks.
- `cma_clear_backend()` - Removes the custom backend and returns to the default allocator.
- `cma_backend_is_enabled()` - Reports whether a custom backend is active.

## Allocation API

- `cma_malloc(ft_size_t size)` - Allocates a block.
- `cma_realloc(void *memory_pointer, ft_size_t size)` - Resizes a block.
- `cma_aligned_alloc(ft_size_t alignment, ft_size_t size)` - Allocates a block with the requested alignment.
- `cma_free(void *memory_pointer)` - Releases a block and accepts null.
- `cma_bzero_and_free(void *memory_pointer)` - Zeroes the tracked allocation using its recorded size before releasing it.
- `cma_checked_free(void *memory_pointer)` - Releases a block and returns an error code for invalid or failed frees.
- `cma_free_double(char **content)` - Releases a null-terminated array of strings and the array itself.
- `cma_block_size(const void *memory_pointer)` - Returns the tracked block size or zero when unavailable.
- `cma_checked_block_size(const void *memory_pointer, ft_size_t *block_size)` - Writes the tracked block size and returns an error code.
- `cma_alloc_size(const void *memory_pointer)` - Returns the rounded usable allocation size known to the allocator.

## Limits, Thread Safety, and Stats

- `cma_set_alloc_limit(ft_size_t limit)` - Sets a process-wide allocation limit for allocation-failure testing and accounting.
- `cma_set_alloc_logging(ft_bool enable)` / `cma_get_alloc_logging()` - Toggles and queries allocator event mirroring into `Sink`.
- `cma_set_thread_safety(ft_bool enable)` - Enables allocator synchronization, or enters an explicitly caller-synchronized single-thread mode when disabled. Mode transitions block until active allocator operations drain; transitions requested while the calling thread owns an allocator operation return `FT_ERR_THREAD_BUSY`. Disabling destroys the synchronization mutex, and enabling creates it again as needed. Metadata validation and protection remain enabled in both modes.
- `cma_enable_thread_safety()` - Enables allocator synchronization.
- `cma_disable_thread_safety()` - Disables allocator synchronization.
- `cma_is_thread_safe_enabled()` - Reports whether allocator synchronization is enabled.
- `cma_get_stats(ft_size_t *allocation_count, ft_size_t *free_count)` - Returns allocation and free counters.
- `cma_get_extended_stats(ft_size_t *allocation_count, ft_size_t *free_count, ft_size_t *current_bytes, ft_size_t *peak_bytes)` - Returns counters plus current and peak tracked bytes.

## Test-Build Leak API

The following public declarations exist only under `LIBFT_TEST_BUILD`.

- `cma_leak_entry` - One tracked live allocation pointer and size.
- `cma_leak_summary` - Aggregate live and ignored allocation counts and byte totals.
- `cma_get_leak_summary(cma_leak_summary *out_summary)` - Writes current leak summary data.
- `cma_get_leak_entries(cma_leak_entry *entries, ft_size_t capacity, ft_size_t *entry_count)` - Copies live allocation entries into caller storage.
- `cma_report_leaks()` - Emits the current leak report.
- `cma_untrack_leak(void *memory_pointer)` - Marks an allocation as ignored by leak reporting.
- `cma_track_leak(void *memory_pointer)` - Re-enables leak reporting for a previously ignored allocation.
