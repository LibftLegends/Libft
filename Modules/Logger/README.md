# Logger

The `Logger` module provides global and object-oriented logging with levels, file rotation, structured fields, sinks, syslog/remote sinks, redaction, async queues, remote health checks, context stacks, and lock-contention sampling.
Formatted messages are also recorded into the separate `Sink` module so log history can be inspected without depending on the full logger stack.

## Types

- `t_log_level` - Log severity enum: debug, info, warn, error, none.
- `t_log_sink` - Sink callback receiving formatted messages and user data.
- `s_log_remote_health` - Remote sink health status.
- `s_log_field` - Structured logging key/value field with optional synchronization helpers.
- `s_log_async_metrics` - Async queue pending/peak/dropped counters with optional synchronization helpers.
- `s_log_lock_contention_sample` - One sampled mutex wait.
- `s_log_lock_contention_statistics` - Aggregate contention statistics.
- `ft_log_context_guard` - Lifecycle guard that pushes structured context fields and pops them on destroy unless released.
- `ft_logger` - Lifecycle logger object that can become the global logger.

## Global Configuration

- `ft_log_set_level(...)` - Sets global minimum log level.
- `ft_log_set_file(...)` - Logs to a file with a maximum size.
- `ft_log_set_rotation(...)` / `ft_log_get_rotation(...)` - Configure or read log rotation limits.
- `ft_log_close()` - Stops the workers and retires the sinks registered when shutdown began. Sinks registered during the drain remain available. Worker start requests made during shutdown return without starting a worker.
- `ft_log_set_alloc_logging(...)` / `ft_log_get_alloc_logging()` - Toggle allocator logging; the underlying allocator flag is mirrored through `CMA` and its messages are recorded into `Sink`.
- `ft_log_set_api_logging(...)` / `ft_log_get_api_logging()` - Toggle API logging.
- `ft_log_set_color(...)` / `ft_log_get_color()` - Toggle colored terminal output.
- `ft_log_add_sink(...)` / `ft_log_remove_sink(...)` - Manage custom sinks. Removal stops future calls to that registration and waits for its active callback. A callback can remove its own registration and remains responsible for its data until it returns.
- `ft_json_sink(...)` / `ft_syslog_sink(...)` - Built-in sink callbacks. The Logger’s JSON sink writes plain messages and flat context values as strings, and emits the payload from `ft_log_structured(...)` as a nested object. Direct calls to `ft_json_sink(...)` use the plain-message format.
- `ft_log_set_syslog(...)` / `ft_log_set_remote_sink(...)` - Configure syslog or remote logging. Remote logging accepts an IPv4 address and registers the sink after the TCP or UDP socket connects.
- On POSIX, repeated `ft_log_set_syslog(...)` calls update the process-wide syslog identifier on the existing sink. The Logger keeps its own copy of the identifier. A null identifier restores the platform default.
- `ft_log_add_redaction(...)`, `ft_log_add_redaction_with_replacement(...)`, and `ft_log_clear_redactions()` - Manage redaction rules. A message and its context use one set of rules, applied in insertion order. Structured logging applies redaction to the message, field keys, and field values before JSON escaping. Changes publish a new set for subsequent messages.

## Logging, Context, Async, and Contention

- `ft_log_debug`, `ft_log_info`, `ft_log_warn`, `ft_log_error` - Format and emit messages at the matching level.
- `ft_log_structured(...)` and level-specific structured variants - Emit structured messages with field arrays. Both delivery modes bypass the fixed variadic formatting buffer. The JSON sink retains the full nested message and field values.
- `ft_log_context_push(...)`, `ft_log_context_pop(...)`, and `ft_log_context_clear()` - Manage context fields on the calling thread. Pushing a field copies its key and value into the context.
- `ft_log_enable_async(...)`, `ft_log_enqueue(...)`, queue-limit getters/setters, and async metrics helpers - Manage asynchronous logging. An external stop drains the worker and joins it. A worker callback can request a stop, with the next external start or stop joining the old worker. Explicit calls to `ft_log_enqueue(...)` can stage messages before startup.
- `ft_log_enable_remote_health(...)`, `ft_log_set_remote_health_interval(...)`, `ft_log_probe_remote_health()`, and `ft_log_get_remote_health(...)` - Manage remote sink health checks. The worker probes once at startup, then waits for the configured interval. An interval update reschedules an idle worker, and stopping wakes it immediately. An active probe finishes before an external stop returns. Native probes retain a socket reference while sending. Each sink registration starts with a fresh health status.
- `ft_log_enable_lock_contention_sampling(...)`, interval/threshold setters, sample retrieval, statistics retrieval, and statistics reset - Manage lock-contention telemetry. A monotonic clock controls sampling cadence. Statistics count all observed waiters, including those beyond the output capacity. Priority-inversion samples require both the waiting thread and the mutex owner to be visible to the lock tracker.
- `log_field_*` and `log_async_metrics_*` helpers - Prepare, tear down, lock, and unlock public metric/field structs.

## `ft_log_context_guard`

- Lifecycle: `initialize`, field-array initialization, copy/move initialization, `destroy`, and `move`. Copies share one group of fields. The last owner’s destruction retires that group wherever it appears in the originating thread’s context.
- `release()` - Detaches the whole group from guard ownership, including any copies. The fields remain in the originating thread’s context until explicitly popped or cleared.
- `is_active()` - Reports whether the guard still owns pushed context.
- `get_error()` / `get_error_str()` - Return guard error state.

## `ft_logger`

- Lifecycle: constructor, destructor, `initialize`, configured initialization, copy/move initialization, `destroy`, and `move`.
- `set_global()` registers the instance for the global logging-option helpers and sink error reporting. Moving a registered instance transfers the registration. Destroying the registered instance detaches it and closes the shared runtime. Destroying an unregistered instance releases its local resources. Use `ft_log_close()` to close shared sinks configured without a registered instance. Synchronize direct instance calls with lifecycle operations on that instance.
- Configuration methods mirror the global API: level, file, rotation, sinks, allocation/API/color flags, syslog, remote sink, async queue, remote health, and thread safety.
- Context methods: `push_context`, `pop_context`, and `make_context_guard`.
- Emission methods: `debug`, `info`, `warn`, `error`, `structured`, and level-specific structured variants. `get_error()` reports formatting errors, synchronous output errors, or the result of async submission. A filtered message is a successful no-op. After async submission, sink callback errors are reported to the registered global Logger.
- Error methods: `get_error()` and `get_error_str()`. Status-returning configuration and query methods record their `FT_ERR_*` result. The free-function APIs keep their existing return conventions.
## Additional API

- `ft_log_context_push_field(const char *key, const char *value)` - Adds one valued field to the current logging context.
