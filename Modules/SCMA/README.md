# SCMA

The `SCMA` module is a safe compact memory allocator that returns handles instead of raw pointers. Callers allocate blocks, read/write by handle and offset, resize/free handles, and can use typed accessors for structured data.

## Handles and Runtime

- `scma_handle` - Public handle with `index` and `generation`.
- `scma_invalid_handle()` - Returns the sentinel invalid handle.
- `scma_initialize(ft_size_t initial_capacity)` - Initializes the allocator heap.
- `scma_shutdown()` - Releases allocator runtime state after zeroing the managed heap and block metadata.
- `scma_is_initialised()` - Reports allocator initialization state.
- `scma_runtime_mutex()` - Returns the allocator runtime mutex.

## Allocation and IO

- `scma_allocate(ft_size_t size)` - Allocates a block and returns a handle.
- `scma_free(scma_handle handle)` - Frees a handle.
- `scma_resize(scma_handle handle, ft_size_t new_size)` - Resizes a handle's block.
- `scma_get_size(scma_handle handle)` - Returns the block size for a valid handle.
- `scma_handle_is_valid(scma_handle handle)` - Reports whether a handle currently refers to a live block.
- `scma_write(scma_handle handle, ft_size_t offset, const void *source, ft_size_t size)` - Writes bytes into a handle.
- `scma_read(scma_handle handle, ft_size_t offset, void *destination, ft_size_t size)` - Reads bytes from a handle.
- `scma_write_batch(...)` / `scma_read_batch(...)` - Process multiple validated I/O requests under one allocator lock. Write batches are fully validated before any bytes are changed.

SCMA compacts lazily when fragmented space is needed. Freed and truncated bytes are securely wiped immediately, while stale handles remain protected by generation validation.

## Stats, Debug, and Thread Safety

- `scma_stats` - Block count, used size, and heap capacity snapshot.
- `scma_get_stats(scma_stats *out_stats)` - Writes allocator stats.
- `scma_debug_dump()` - Prints allocator debug information.
- `scma_enable_thread_safety()` / `scma_disable_thread_safety()` / `scma_is_thread_safe_enabled()` - Manage global allocator synchronization.
- `scma_mutex_lock()` / `scma_mutex_unlock()` / `scma_mutex_close()` - Operate on the allocator mutex.
- `scma_mutex_lock_count()` - Returns the lock counter used for validation.

## Test-Build Leak API

- `scma_leak_entry` - One live allocation range in test builds.
- `scma_leak_summary` - Live and ignored block/byte totals.
- `scma_get_leak_summary(...)`, `scma_get_leak_entries(...)`, `scma_report_leaks()`, `scma_untrack_leak(...)`, and `scma_track_leak(...)` - Test-only leak inspection and leak-tracking controls.

## `scma_handle_accessor<TValue>`

- Lifecycle: constructor, destructor, `initialize`, handle initialization, `destroy`, and `move`.
- `destroy()` zeroes the bound handle before marking the accessor destroyed.
- `bind(scma_handle handle)` - Binds the accessor to an existing handle.
- `is_initialised()` / `is_bound()` / `get_handle()` - Inspect accessor state.
- Operators `*`, `->`, and `[]` - Return proxy objects for structured handle access.
- `read_struct(...)` / `write_struct(...)` - Read or write one `TValue` at the start of the block.
- `read_at(...)` / `write_at(...)` - Read or write one `TValue` at an element index.
- `get_count()` - Returns how many `TValue` elements fit in the bound block.
- `get_error()` / `get_error_str()` - Return accessor-local error state.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Forward to SCMA synchronization behavior.
