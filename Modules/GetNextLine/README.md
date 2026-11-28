# GetNextLine

The `GetNextLine` module reads line-oriented text from file descriptors, `FILE *` handles, or caller-provided callbacks. It keeps per-stream leftover state for the C API and provides `gnl_stream` for lifecycle-managed stream reading.

## C API

- `ft_strjoin_gnl(char *string_one, char *string_two)` - Joins two line fragments and frees `string_one` on success. Release the returned string with `cma_free`.
- `get_next_line(int32_t file_descriptor, ft_size_t buffer_size)` - Returns the next line with its newline when present, or a null pointer at EOF or on failure. Release each returned line with `cma_free`. Coordinate access to the descriptor cache externally and clear a descriptor's entry before reusing it.
- `gnl_set_leftover_alloc_hook(void *(*hook)(ft_size_t size))` - Installs a test-build allocation hook for leftover storage.
- `gnl_reset_leftover_alloc_hook()` - Restores the default leftover allocator in test builds.
- `gnl_reset_all_streams()` - Clears cached leftover state for all tracked descriptors.
- `gnl_clear_stream(int32_t file_descriptor)` - Clears cached leftover state for one descriptor.
- `ft_read_file_lines(int32_t file_descriptor, ft_vector<ft_string> &lines, ft_size_t buffer_size)` - Reads all lines from a descriptor into a vector.
- `ft_open_and_read_file(const char *path, ft_vector<ft_string> &lines, ft_size_t buffer_size)` - Opens a file path and reads all lines into a vector.

## `gnl_stream`

- `gnl_stream()` / `~gnl_stream()` - Construct and destroy an uninitialized stream wrapper.
- `initialize()` / `destroy()` / `move(gnl_stream &other)` - Manage lifecycle and explicit state transfer.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional recursive locking.
- `init_from_fd(int32_t file_descriptor)` - Configures the stream to read from a file descriptor.
- `init_from_file(FILE *file_handle, ft_bool close_on_reset)` - Configures the stream to read from a C file handle, with optional ownership. An owned file is closed on reset, destruction, or replacement. Configure the same `FILE *` with `close_on_reset` set to false to keep it open when switching to its underlying descriptor.
- `init_from_callback(...)` - Configures the stream to read through a caller callback and user data pointer.
- `reset()` - Clears the current input source and closes it when configured to do so.
- `read(char *buffer, ft_size_t max_size)` - Reads bytes from the configured source into a caller buffer.
