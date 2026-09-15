# Analytics

The Analytics module records frame, scope, flow, trace, and region timing
data. It can retain rolling statistics and can optionally own JSONL or CSV
file export on a persistent sleeping exporter thread.

## Public configuration

- `analytics_session_config` selects the menu/non-world output path, optional
  active-world output path, output format, per-buffer capacities, buffer count,
  and whether the exporter starts during initialization. Three buffers are the
  default; up to eight can be reserved for bursts.
- `analytics_overflow_policy` selects drop-new, recycle-oldest-completed, or
  fail-session behavior when no producer buffer is available. The exporter
  currently never recycles a buffer it owns.
- `clock_callback` and `clock_user_data` optionally provide a deterministic
  clock for tests; leaving the callback null uses Libft's monotonic clock.
- `analytics_default_session_config()` initializes every configuration field
  to a documented safe default.
- `analytics_output_format` supports `NONE`, `JSONL`, and `CSV`.
- `trace_frame_interval` controls detailed trace capture independently from
  frame and region accounting. `1` captures every frame; larger values sample
  traces while preserving frame summaries and aggregate region statistics.
Applications measuring a live renderer should use sampling to avoid the
exporter and trace formatting changing the workload being measured.
- During an active frame, scope bookkeeping uses a validated thread-local
  state pointer. This avoids repeated fallback-table scans on the hot path;
  world/menu classification is still captured at each scope's start, and
  worker and standalone APIs retain the validated fallback lookup.
- `frame_export_interval` controls frame-summary file export independently
  from frame accounting. Frames still update rolling statistics and region
  aggregates, but only every Nth frame is serialized and written. Use `1` for
  complete output; a live game should normally use a larger value such as
  `120`.
- JSONL output is newline-delimited: every frame summary and trace record ends
  with a newline so files can be streamed and parsed one record at a time.
- `analytics_session::initialize()` creates an in-memory session.
- `analytics_session::initialize(const analytics_session_config&)` creates a
  configured session and opens its output file when requested.
- `analytics_session::start_exporter()` starts Libft's persistent exporter.
- `analytics_session::destroy()` drains the exporter, flushes the file, and
  releases the session resources.
- `analytics_session::get_export_error()` reports asynchronous file errors.
- `analytics_session::set_world_active()` changes the classification for
  subsequently captured records. Frame, scope, and flow state is captured at
  start time, so transitions cannot reclassify already-recorded work. The
  primary output receives non-world records and the world output receives
  active-world records.

Minecraft or another application supplies region names and timing events;
Libft owns the buffers, serialization, file handle, exporter thread, and
shutdown ordering.
