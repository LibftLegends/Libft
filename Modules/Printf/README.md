# Printf

The `Printf` module provides printf-compatible formatting to stdout, file descriptors, fixed buffers, and `FILE *` streams. It also exposes a low-level formatting engine and custom specifier registration.

## Hooks and Formatter Types

- `t_pf_tmpfile_function` - Test hook type used to replace temporary-file creation.
- `t_pf_fflush_function` - Test hook type used to replace `fflush`.
- `t_pf_ftell_function` - Test hook type used to replace `ftell`.
- `t_pf_custom_formatter` - Custom specifier callback that receives a `va_list`, output string, and caller context.
- `t_pf_engine_write_callback` - Low-level writer callback used by the formatting engine.

## Public API

- `pf_printf(const char *format, ...)` - Formats to standard output.
- `pf_printf_fd(int32_t file_descriptor, const char *format, ...)` - Formats to a file descriptor.
- Descriptor formatting batches short output spans. It writes pending bytes before invoking a custom formatter or storing a count through `%n`.
- `pf_snprintf(char *string, ft_size_t size, const char *format, ...)` - Formats into a bounded caller buffer.
- `pf_vsnprintf(char *string, ft_size_t size, const char *format, va_list argument_list)` - `va_list` version of `pf_snprintf`.
- Fixed-buffer formatting writes directly to the destination and returns the full formatted byte count, including bytes beyond the buffer capacity. An installed stream hook selects the temporary-stream path so hook behavior remains observable.
- `ft_vfprintf(FILE *stream, const char *format, va_list argument_list)` - Formats to a C stream from a `va_list`.
- `ft_fprintf(FILE *stream, const char *format, ...)` - Formats to a C stream.
- `pf_set_tmpfile_function(...)` / `pf_reset_tmpfile_function()` - Install or reset the temp-file hook.
- `pf_set_fflush_function(...)` / `pf_reset_fflush_function()` - Install or reset the flush hook.
- `pf_set_ftell_function(...)` / `pf_reset_ftell_function()` - Install or reset the tell hook.
- `pf_register_custom_specifier(...)` - Registers a custom format specifier.
- `pf_unregister_custom_specifier(...)` - Removes a custom format specifier.
- The custom registry synchronizes registration, removal, and lookup. Callbacks run outside the registry lock. The caller owns the handler context through any overlapping formatting calls.
- `pf_enable_thread_safety()` / `pf_disable_thread_safety()` - Compatibility entry points. The registry stays synchronized.
- `pf_engine_format(...)` - Runs the formatting engine and writes output through a callback.
- Positional formats require a conversion, width, or precision reference for every argument index from 1 through the highest index. `%%` does not consume an argument.
