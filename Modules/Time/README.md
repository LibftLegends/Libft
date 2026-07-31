# Time

## Active clocks

`t_active_clock` measures elapsed active time in microseconds using the
platform monotonic clock. It is independent state, so callers can maintain
multiple clocks and restart one without affecting any other clock.

- `time_active_clock_init` initializes a paused clock at zero.
- `time_active_clock_start` starts a clock or continues a paused clock.
- `time_active_clock_stop` pauses the clock while preserving accumulated time.
- `time_active_clock_resume` continues a paused clock.
- `time_active_clock_restart` resets the clock to zero and starts it.
- `time_active_clock_report` returns accumulated active microseconds without
  changing the clock state.

The `Time` module provides wall-clock time, monotonic time, duration structs, timezone conversion, parsing/formatting, relative fixed-span and calendar-aware helpers, async sleep state, benchmarking, trace events, FPS pacing, and countdown timers.

## Core Types

- `t_time` - Signed timestamp type used by the module.
- `t_monotonic_time_point` - Monotonic millisecond point with optional mutex state.
- `t_duration_milliseconds` - Duration wrapper with optional mutex state.
- `t_high_resolution_time_point` - Nanosecond-resolution time point.
- `t_time_info` - Broken-down local time fields with optional mutex state.
- `t_time_async_sleep` - Pollable async sleep state.
- `t_time_benchmark` - Online benchmark accumulator.
- `t_time_benchmark_snapshot` - Readable benchmark summary.
- `t_time_clock_now_hook` - Test hook for replacing the system clock source.

## Wall, Monotonic, and Local Time

- `time_now()` / `ft_time_ms()` / `time_now_ms()` - Return current wall time in seconds or milliseconds.
- `ft_time_format(char *buffer, ft_size_t buffer_size)` - Formats current time into a caller buffer.
- `time_monotonic()` - Returns monotonic milliseconds.
- `time_monotonic_point_now()` / `time_monotonic_point_create(...)` - Create monotonic points.
- `time_monotonic_point_add_ms(...)` / `time_monotonic_point_diff_ms(...)` / `time_monotonic_point_compare(...)` - Perform monotonic point arithmetic and comparison.
- `time_duration_ms_create(...)` - Creates a duration wrapper.
- `time_local(t_time time_value, t_time_info *out)` - Converts a timestamp to local broken-down time.
- `time_sleep(uint32_t seconds)` / `time_sleep_ms(uint32_t milliseconds)` - Sleep the current thread.

## Synchronization Helpers

- `time_monotonic_point_enable_thread_safety(...)`, `time_monotonic_point_disable_thread_safety(...)`, `time_monotonic_point_lock(...)`, `time_monotonic_point_unlock(...)`, `time_monotonic_point_is_thread_safe(...)` - Manage optional locking for monotonic points.
- `time_duration_ms_enable_thread_safety(...)`, `time_duration_ms_disable_thread_safety(...)`, `time_duration_ms_lock(...)`, `time_duration_ms_unlock(...)`, `time_duration_ms_is_thread_safe(...)` - Manage optional locking for durations.
- `time_info_enable_thread_safety(...)`, `time_info_disable_thread_safety(...)`, `time_info_lock(...)`, `time_info_unlock(...)`, `time_info_is_thread_safe(...)` - Manage optional locking for broken-down time values.

## Formatting, Parsing, and Timezones

- `time_strftime(...)` - Formats `t_time_info` with a strftime-compatible format.
- `time_format_iso8601(...)` - Allocates an ISO-8601 UTC/local timestamp string.
- `time_format_iso8601_with_offset(...)` - Allocates an ISO-8601 string with explicit offset minutes.
- `time_format_timezone_offset(...)` - Formats a timezone offset as `+HH:MM` into a caller buffer.
- `time_format_rfc3339(...)` / `time_format_rfc3339_with_offset(...)` - Allocates RFC 3339 timestamp strings with `Z` or explicit offsets.
- `time_format_duration(...)` - Allocates an ISO-8601 duration string with day/hour/minute/second granularity and millisecond precision.
- `time_format_interval(...)` - Allocates an RFC 3339 interval string in `start/end` form.
- `time_add_seconds(...)` / `time_add_minutes(...)` / `time_add_hours(...)` / `time_add_weeks(...)` - Add fixed time spans to a `t_time` value with clamping at the timestamp bounds.
- `time_add_days(...)` - Adds whole days to a `t_time` value with clamping at the timestamp bounds.
- `time_add_months(...)` / `time_add_quarters(...)` - Add calendar months or quarters in UTC, clamping month-end days like January 31 -> February 28/29.
- `time_add_years(...)` - Adds calendar years in UTC, clamping leap-day dates like February 29 -> February 28 when needed.
- `time_floor_to_second(...)` / `time_floor_to_minute(...)` / `time_floor_to_hour(...)` / `time_floor_to_day(...)` / `time_floor_to_week(...)` / `time_floor_to_month(...)` / `time_floor_to_quarter(...)` - Round timestamps down to the start of the requested unit, including for negative timestamps. Month and quarter flooring are UTC calendar-aware.
- `time_ceiling_to_second(...)` / `time_ceiling_to_minute(...)` / `time_ceiling_to_hour(...)` / `time_ceiling_to_day(...)` / `time_ceiling_to_week(...)` / `time_ceiling_to_month(...)` / `time_ceiling_to_quarter(...)` - Round timestamps up to the end of the requested unit, including for negative timestamps. Month and quarter ceiling are UTC calendar-aware.
- `time_parse_iso8601(...)` - Parses ISO-8601 text into `std::tm` and timestamp outputs.
- `time_parse_rfc3339(...)` - Parses RFC 3339 timestamps with `Z` or `+HH:MM` offsets.
- `time_parse_custom(...)` - Parses text with a caller format, optionally interpreting it as UTC.
- `time_parse_timezone_offset(...)` - Parses `Z` or `+HH:MM` timezone offsets into signed minutes.
- `time_parse_duration(...)` - Parses ISO-8601 duration text into `t_duration_milliseconds` using days, hours, minutes, seconds, and optional fractional seconds.
- `time_parse_interval(...)` - Parses RFC 3339 interval text in `start/end`, `start/duration`, or `duration/end` form. Duration-based forms require whole-second durations.
- `time_high_resolution_now(...)` - Captures a high-resolution timestamp.
- `time_high_resolution_diff_ns(...)` / `time_high_resolution_diff_seconds(...)` - Compute high-resolution elapsed time.
- `time_get_local_offset(...)` - Gets local UTC offset and daylight-saving flag.
- `time_convert_timezone(...)` - Converts a timestamp between source and target offsets.
- `time_get_monotonic_wall_anchor(...)`, `time_monotonic_to_wall_ms(...)`, `time_wall_ms_to_monotonic(...)` - Convert between monotonic and wall-clock milliseconds using an anchor.

## Async Sleep, Hooks, Benchmarks, and Tracing

- `time_async_sleep_init(...)` - Initializes a pollable sleep deadline.
- `time_async_sleep_is_complete(...)` - Reports whether the sleep has elapsed.
- `time_async_sleep_remaining_ms(...)` - Returns remaining delay.
- `time_async_sleep_poll(...)` - Polls sleep state through an event loop.
- `time_set_clock_now_hook(...)` / `time_reset_clock_now_hook()` - Install or remove a thread-local test clock hook; changing the hook affects only the calling thread.
- `time_benchmark_init(...)` / `time_benchmark_reset(...)` - Initialize or clear benchmark accumulators.
- `time_benchmark_add_sample(...)` / `time_benchmark_add_duration(...)` - Add timing samples.
- `time_benchmark_snapshot(...)` and benchmark getters - Read sample count, average, jitter, min, max, and error state.
- `time_trace_begin_session(...)` / `time_trace_end_session()` - Start and stop trace output.
- `time_trace_begin_event(...)` / `time_trace_end_event()` / `time_trace_instant_event(...)` - Emit trace events.

## `time_fps`

- Lifecycle: `time_fps()`, `~time_fps()`, `initialize`, copy/move initialization, `destroy`, and `move`.
- `get_frames_per_second()` / `set_frames_per_second(...)` - Read or set target FPS.
- `sleep_to_next_frame()` - Sleeps until the next frame slot based on the configured FPS.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional locking.

## `time_timer`

- Lifecycle: `time_timer()`, `~time_timer()`, `initialize`, copy/move initialization, `destroy`, and `move`.
- `start(int64_t duration_ms)` - Starts a countdown.
- `update()` - Returns remaining time after updating the timer state.
- `add_time(...)` / `remove_time(...)` - Adjust remaining timer duration.
- `sleep_remaining()` - Sleeps until the timer expires.
- `enable_thread_safety()` / `disable_thread_safety()` / `is_thread_safe()` - Manage optional locking.
## Additional API

- `time_format_relative(t_duration_milliseconds duration_value)` - Formats future durations with an `in ` prefix while preserving signed duration text for past values.
