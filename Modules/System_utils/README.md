# System_utils

The `System_utils` module wraps environment variables, low-level file descriptors, process/system information, abort/signal helpers, locale helpers, service helpers, resource tracing, simple file streams, filesystem utilities, and health checks.

## Environment

- `su_getenv`, `su_setenv`, `su_unsetenv`, and `su_putenv` - Read or modify process environment variables.
- `t_su_environment_snapshot` - Owns a vector of `KEY=VALUE` entries captured from the environment.
- `su_environment_snapshot_capture(...)`, `restore(...)`, and `dispose(...)` - Capture, restore, and release environment snapshots.
- `su_environment_sandbox_begin(...)` / `su_environment_sandbox_end(...)` - Begin and end an environment sandbox.
- `su_environment_enable_thread_safety()` / `su_environment_disable_thread_safety()` - Manage environment synchronization.
- `su_get_home_directory()` - Returns an allocated home-directory path.

## File Descriptors and File Streams

- `su_open(...)` overloads - Open a file path with default or explicit flags/mode.
- `su_open_with_error(...)` - Opens once and returns the captured, mapped
  platform error without a later library call overwriting it.
- `su_read(...)` / `su_write(...)` / `su_lseek(...)` / `su_close(...)` - Low-level descriptor IO.
- `su_write_with_error(...)` - Writes with the exact mapped error returned
  through an output parameter while retaining the normal EINTR retry behavior.
- Under `LIBFT_TEST_BUILD`, the open-error and interrupted-write injectors
  exercise the same mapping/retry paths without adding production symbols.
- Under `LIBFT_TEST_BUILD`, `t_su_write_syscall_hook`, `su_set_write_syscall_hook(...)`, and `su_reset_write_syscall_hook()` provide a test hook for write calls.
- `su_file` - Lightweight stream wrapper with mutex, closed flag, and descriptor.
- Under `LIBFT_TEST_BUILD`, `su_force_file_stream_allocation_failure(...)` injects stream allocation failures.
- `su_fopen(...)`, `su_fclose(...)`, `su_fread(...)`, `su_fwrite(...)`, `su_fseek(...)`, and `su_ftell(...)` - File-stream style API over descriptors.
- `su_file_prepare_thread_safety(...)`, `teardown`, `lock`, and `unlock` - Optional stream synchronization helpers.

## Process, Signals, and Resource Tracing

- `su_get_cpu_count()` / `su_get_total_memory()` - Return basic system capacity information.
- `su_abort()` and signal helpers `su_sigabrt`, `su_sigfpe`, `su_sigill`, `su_sigint`, `su_sigsegv`, `su_sigterm` - Abort/signal handling entry points. `su_abort()` prints the abort reason before terminating in normal builds; test builds suppress that diagnostic output so the runner stays limited to status lines.
- `su_exit(int32_t exit_code)` - Terminates the process immediately through the shared `System_utils` exit wrapper.
- `su_assert(ft_bool condition, const char *message)` - Aborts with a message when a condition is false.
- `t_su_resource_tracer` - Callback invoked before abort/resource diagnostics.
- `su_register_resource_tracer(...)`, `su_unregister_resource_tracer(...)`, `su_clear_resource_tracers()`, and `su_run_resource_tracers(...)` - Manage resource tracer callbacks.
- `su_internal_set_abort_reason(...)` / `su_internal_take_abort_reason()` - Store and consume the current abort reason.

## Locale, Filesystem, and Services

- `su_locale_compare(...)` - Locale-aware comparison with output result.
- `su_locale_casefold(...)` - Locale-aware case folding into an `ft_string`.
- `su_copy_file(...)` / `su_copy_directory_recursive(...)` - Copy files or directory trees.
- `su_inspect_permissions(...)` - Reads filesystem permissions.
- `su_chmod(...)` - Sets owner/group/other permissions.
- `t_su_service_signal_handler` - Service signal callback.
- `su_service_force_no_fork(...)` - Test-build hook for daemonization.
- `su_service_daemonize(...)` - Turns the process into a daemon/service.
- `su_service_install_signal_handlers(...)` / `su_service_clear_signal_handlers()` - Manage service signal callbacks.

## Health Checks

- `t_su_health_check` - Callback that writes health detail text.
- `t_su_health_check_result` - One health-check result with name, healthy flag, detail, and error code.
- `su_health_register_check(...)` / `su_health_unregister_check(...)` / `su_health_clear_checks()` - Manage health-check registrations.
- `su_health_run_checks(...)` - Runs all registered checks into caller storage.
- `su_health_run_check(...)` - Runs one named check.
