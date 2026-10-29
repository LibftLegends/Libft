# Compression

The `Compression` module provides zlib-style buffer compression, stream compression, Base64 helpers, and vector/string adapters.

## Buffer Compression

- `compression_max_size` - Maximum supported compression buffer size.
- `compress_buffer(const unsigned char *input_buffer, std::size_t input_size, std::size_t *compressed_size)` - Compresses a memory buffer and returns an allocated compressed buffer.
- `decompress_buffer(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size)` - Decompresses a memory buffer and returns an allocated decompressed buffer.
- `ft_compress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *compressed_size)` - Public alias for buffer compression.
- `ft_decompress(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decompressed_size)` - Public alias for buffer decompression.

## Stream Options

- `t_compress_stream_progress` - Progress snapshot with total bytes read and written.
- `t_compress_stream_progress_callback` - Callback invoked with progress updates.
- `t_compress_stream_cancel_callback` - Callback used to cancel a streaming operation.
- `t_compress_stream_options` - Lifecycle class holding stream buffer sizes, callbacks, zlib compression level, window bits, memory level, strategy, and optional thread safety.

### `t_compress_stream_options` Methods

- `t_compress_stream_options()` / `~t_compress_stream_options()` - Construct and destroy the options object.
- `initialize()` / `destroy()` - Enter and leave the initialized lifecycle state.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional synchronization for option access. Transition operations serialize with active accessors; disabling waits for an active accessor and destroys the heap mutex before returning.
- `reset()` - Restores default option values.
- `set_input_buffer_size(...)` / `set_output_buffer_size(...)` - Configure stream chunk sizes.
- `set_progress_callback(...)` / `set_cancel_callback(...)` / `set_callbacks(...)` - Configure progress and cancellation callbacks plus user data.
- `set_compression_level(...)` - Sets zlib compression level.
- `set_window_bits(...)` - Sets the zlib window-bits parameter.
- `set_memory_level(...)` - Sets the zlib memory-level parameter.
- `set_strategy(...)` - Sets the zlib compression strategy.
- `get_input_buffer_size()` / `get_output_buffer_size()` - Return configured buffer sizes with status.
- `get_progress_callback()` / `get_cancel_callback()` / `get_callback_user_data()` - Return configured callbacks and user data with status.
- `get_compression_level()` / `get_window_bits()` / `get_memory_level()` / `get_strategy()` - Return zlib option values with status.
- `snapshot(...)` - Copies all option values into an internal snapshot structure for stream execution.

## Stream Compression

- `ft_compress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options)` - Compresses from one file descriptor to another using explicit options.
- `ft_decompress_stream_with_options(int input_file_descriptor, int output_file_descriptor, const t_compress_stream_options *options)` - Decompresses from one file descriptor to another using explicit options.
- `ft_compress_stream(int input_file_descriptor, int output_file_descriptor)` - Compresses using default options.
- `ft_decompress_stream(int input_file_descriptor, int output_file_descriptor)` - Decompresses using default options.
- `ft_compress_stream_apply_speed_preset(t_compress_stream_options *options)` - Applies low-latency compression defaults.
- `ft_compress_stream_apply_ratio_preset(t_compress_stream_options *options)` - Applies higher-compression-ratio defaults.

## Base64 and Container Adapters

- `ft_base64_encode(const unsigned char *input_buffer, std::size_t input_size, std::size_t *encoded_size)` - Encodes binary input into an allocated Base64 buffer.
- `ft_base64_decode(const unsigned char *input_buffer, std::size_t input_size, std::size_t *decoded_size)` - Decodes Base64 input into an allocated binary buffer.
- `ft_compress_string_to_vector(const ft_string &input, ft_vector<unsigned char> &output)` - Compresses a string into an output vector.
- `ft_compress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output)` - Compresses a byte vector into another vector.
- `ft_decompress_vector_to_string(const ft_vector<unsigned char> &input, ft_string &output)` - Decompresses a byte vector into a string.
- `ft_decompress_vector_to_vector(const ft_vector<unsigned char> &input, ft_vector<unsigned char> &output)` - Decompresses a byte vector into another byte vector.
