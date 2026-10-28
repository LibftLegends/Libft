# Compatebility

The `Compatebility` module contains platform-specific adapters used by higher-level modules. Most headers intentionally require `LIBFT_INTERNAL_HEADERS`; they are documented here so maintainers can find the internal service boundaries.

## CMA Platform

- `cmp_cma_get_page_size(ft_size_t *page_size_out)` - Returns the OS page size.
- `cmp_cma_memory_map_read_write(ft_size_t mapping_size)` - Maps read/write memory.
- `cmp_cma_memory_protect_none(...)` - Makes a mapped region inaccessible.
- `cmp_cma_memory_protect_read_write(...)` - Restores read/write access to a mapped region.
- `cmp_cma_memory_unmap(...)` - Unmaps a region.

## Cross-Process Platform

- `cmp_cross_process_mapping` - Platform mapping state: mapped address, length, platform handle, and mutex address.
- `cmp_cross_process_mutex_state` - Platform mutex state used while a shared mutex is locked.
- `cmp_cross_process_send_descriptor(...)` / `cmp_cross_process_receive_descriptor(...)` - Platform descriptor transfer helpers.
- `cmp_cross_process_open_mapping(...)` / `cmp_cross_process_close_mapping(...)` - Open or close a shared-memory mapping.
- `cmp_cross_process_lock_mutex(...)` / `cmp_cross_process_unlock_mutex(...)` - Lock or unlock the shared cross-process mutex.

## File Watch Platform

- `cmp_file_watch_context` - Opaque platform watch context.
- `cmp_file_watch_create()` / `cmp_file_watch_destroy(...)` - Allocate or release a watch context.
- `cmp_file_watch_start(...)` / `cmp_file_watch_stop(...)` - Start or stop watching a path.
- `cmp_file_watch_wait_event(...)` - Waits for one platform file-watch event.

## File Descriptor Platform

- `cmp_retrieve_handle(...)` - Returns the platform handle tracked for a descriptor on Windows.
- `cmp_lseek(...)` - Platform-specific descriptor seek helper used by `System_utils`.

## Secure Randomness

- `cmp_rng_secure_bytes(...)` - Uses the platform cryptographic random source. Under `LIBFT_TEST_BUILD`, the Windows provider exposes one-shot failure hooks for provider acquisition, byte generation, and provider release; these hooks are absent from production builds.

## Stack Trace and Optional SQLite

- `CMP_STACK_TRACE_MAX_FRAMES` - Maximum platform stack frames captured by the compatibility helper.
- `cmp_stack_trace_capture(...)` - Captures return addresses into caller storage.
- `cmp_stack_trace_symbolize_address(...)` - Resolves one address into symbol and source-location strings on supported platforms. Windows builds use DbgHelp to resolve symbols and source lines.
- `cmp_stack_trace_print(...)` - Prints captured frames to a file stream using the platform symbolizer when available.
- `SQLITE3_AVAILABLE` - Compile-time availability flag for SQLite support.

## Platform Sound Backends

The ALSA, CoreAudio, and Win32 sound-device headers provide platform implementations behind `ft_create_sound_device()`. They are backend adapters rather than general-purpose public APIs.
