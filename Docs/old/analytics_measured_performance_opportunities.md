# Analytics-Measured Performance Opportunities

Status: design and measurement report; Libft-owned analytics export migration
implemented in the current branch

Implemented from this document: reusable mesh index storage, worker-side
solid/water mesh-index partitioning, bounded visible mesh uploads,
radius-indexed stream candidate lookup, and resumable dirty remesh discovery.
The persistent upload queue/time-and-byte budget, visibility cache, and
draw-batching phases remain planned work. Deferred edits are also now applied
in bounded batches during streaming commits.

Date: 2026-09-01

Analyzed files:

- `minecraft_analytics.jsonl`
- `minecraft_world_analytics.jsonl`

Scope: real gameplay performance only. Time spent recording, buffering, or
exporting analytics is deliberately excluded from the optimization proposals.

## Analytics responsibility migration to Libft

The analytics architecture must be changed so Libft owns the complete generic
analytics runtime. Minecraft must describe what should be measured, but it
must not implement storage, aggregation, export scheduling, file formatting,
buffer rotation, or writer-thread lifecycle.

### Libft responsibilities

The Libft `Analytics` module must own:

- the `analytics_session` lifecycle and enabled/disabled state;
- stable registration of region names, categories, and numeric region IDs;
- clock acquisition and conversion to the canonical timestamp unit;
- per-thread scope stacks and validation of balanced start/stop calls;
- frame, scope, trace, flow, counter, and metadata event storage;
- inclusive/exclusive-time calculation and invocation accounting;
- rolling statistics, histograms, p50/p95/p99, maxima, and dropped-event
  counters;
- all memory allocation, capacity planning, buffer ownership, and buffer
  rotation;
- creation, sleeping, wakeup, shutdown, and joining of the persistent export
  thread;
- file opening, writing, flushing, rotation, and error reporting;
- JSONL, CSV, trace, and any later binary file schemas;
- ordering records and attaching session, frame, thread, flow, and region
  identities;
- transactional shutdown flushing so accepted events are written before the
  session is destroyed;
- deterministic manual-clock and synchronous-export modes for testing.

Minecraft-specific names such as `world_stream_drain` remain data registered
by Minecraft. Libft must not contain knowledge of Minecraft rendering,
world-generation, chunks, menus, or gameplay phases.

### Minecraft responsibilities

Minecraft must only:

- initialize a Libft analytics session using a configuration;
- register names/categories and retain the returned stable IDs;
- tell Libft when a frame or named session begins and ends;
- tell Libft when a timed region begins and ends;
- submit counters or metadata such as visible chunks, uploaded bytes, draw
  calls, or active world state;
- tell Libft which output profile/path is requested through configuration;
- report Libft errors through Minecraft's normal error/reporting path;
- shut down the Libft session during controlled application exit.

Minecraft must not maintain a second event buffer, copy completed reports into
Minecraft-owned queues, serialize JSON, manage the export condition variable,
or run its own analytics writer thread. `RuntimeAnalytics` should become a
thin adapter that maps Minecraft enums to Libft region IDs and forwards calls.
It may contain compile-time instrumentation helpers, but no storage/export
engine.

### Current migration implementation

The current branch establishes this ownership boundary:

- `analytics_session_config` selects the output path, JSONL or CSV format,
  optional active-world output path, reserved per-buffer frame/trace
  capacities, buffer count, overflow policy, optional clock callback, and
  whether the exporter starts during initialization;
- Libft opens and closes the output file, owns the persistent sleeping exporter
  thread, wakes it when frame or trace records arrive, flushes pending data,
  and reports exporter failures through `get_export_error()`;
- Libft serializes frame and trace records using the Analytics module export
  functions, so Minecraft no longer owns files, condition variables, worker
  threads, JSON formatting, or export-error state;
- region names and categories are copied into fixed preallocated Libft buffers
  during registration, removing a lifetime dependency on Minecraft strings;
- a dedicated write mutex protects the file if a synchronous flush overlaps
  the exporter thread.

The configured primary output is the menu/non-world view and the optional
world output is the active-world view. `set_world_active()` changes the
classification for subsequently captured records. Each record is captured
once into Libft's buffer and the exporter routes it to the appropriate file;
Minecraft no longer records the same scope into two sessions.

The build isolation is also enforced: normal Minecraft/Libft fingerprints do
not reuse analytics-enabled object directories, the normal Libft archive
excludes the Analytics archive, and the explicit analytics target includes it
and links `ft_vox_analytics`. The normal target was rebuilt after changing the
fingerprint and archive-graph dependencies to verify this boundary.

The producer/exporter/free-buffer rotation is now implemented. Three buffers
remain the default, while Libft can reserve up to eight buffers from the
configuration. The pool is allocated on the heap during initialization so a
stack-allocated `analytics_session` does not inherit the pool's memory cost.
Queues remain bounded and never overwrite an exporter-owned buffer. The
configured `analytics_overflow_policy` selects whether a new record is
rejected with a counter, the oldest completed buffer is recycled with its
discarded frame/trace counts recorded, or the session is disabled with
`FT_ERR_FULL` preserved as its first export error.

The configured pool is allocated during initialization, and the recording
path has a CMA failure-injection regression test that enables allocation
failures only after setup. A complete frame/scope/trace sequence succeeds with
zero post-initialization allocation attempts.

### Menu and active-world separation

The session has two logical views of one event stream:

- the primary output is the application/menu view and receives records captured
  while no world is active;
- the optional world output is the active-world view and receives records
  captured while the player is inside a loaded world.

Minecraft calls `set_world_active(FT_TRUE)` only after world startup has
completed and calls `set_world_active(FT_FALSE)` before leaving the world. Libft
captures the classification at frame start, scope start, and flow start, and
stores it with the pending event before any later transition can occur. The
exporter then routes the immutable record, so a transition cannot move an
already-captured event between files and the same event is never recorded
twice. A frame is classified by its frame-start state; individual scopes and
flows retain the state from their own start, which makes transitions inside a
frame deterministic rather than dependent on when export happens.

This deliberately does not use a second session or a global mirrored scope
depth. Scope nesting remains owned by Libft's per-thread state, while the
world/menu flag is captured into each per-thread frame/scope/flow record. The
session state is synchronized so transitions are safe to request while the
exporter is running. If a transition fails, the Minecraft adapter must retain
its previous local state and report the Libft error; it must not claim that the
world state changed. Worker-thread instrumentation must use the same capture
rules and must not infer its classification later from mutable global state.

The separation must be tested for menu-only frames, world-only frames,
entering and leaving the world between frames, transitions in the middle of a
frame, scopes that cross a transition, flows that cross a transition, traces in
both views, worker-thread records, shutdown while the world view is active, and
exporter shutdown without an artificial sleep. Tests must assert the contents
of both output files, not merely that files were created. A configured
per-buffer capacity must also return `FT_ERR_FULL` and increment the
dropped-record counter without modifying or silently losing already accepted
records. Configured/manual clocks must be used by the application adapter so
clock-dependent tests remain deterministic.

The intended call flow is:

```text
Minecraft code
    -> analytics_begin_frame(session, frame_number)
    -> analytics_begin_scope(session, region_id)
    -> measured work
    -> analytics_end_scope(session)
    -> analytics_record_counter(session, counter_id, value)
    -> analytics_end_frame(session)

Libft hot path
    -> validate IDs and scope nesting
    -> append compact records to the active preallocated producer buffer
    -> publish a completed buffer by swapping ownership

Libft export thread
    -> sleep while no completed buffer exists
    -> acquire a completed immutable buffer
    -> aggregate and serialize it
    -> write/flush according to policy
    -> return the cleared buffer to the free-buffer pool
```

### Preallocation and memory guarantees

Libft must reserve storage during `analytics_session::initialize(...)`, before
Minecraft enters the measured loop. Configuration must provide or derive:

- maximum registered regions and counters;
- maximum simultaneously active producer threads;
- maximum nesting depth per thread;
- expected and hard maximum events per frame/buffer;
- number of producer/export buffers;
- rolling-frame sample window;
- maximum queued frames/traces awaiting export;
- string-table bytes and maximum metadata records;
- output staging-buffer bytes.

The default analytics profile should reserve enough memory for sustained
Minecraft operation without allocation on ordinary begin/end calls. At least
three event buffers are recommended: one active producer buffer, one immutable
buffer owned by the exporter, and one free buffer ready for immediate rotation.
More buffers may be configured for burst tolerance.

No buffer may be shared mutably by producers and the exporter. Publishing must
transfer ownership or generation state atomically. The exporter operates only
on immutable completed buffers while producers immediately continue in a
different buffer. Buffers are cleared and recycled only after export completes.

Capacity exhaustion must never overwrite unread records or block the gameplay
thread on file I/O. The configured policy must be explicit:

- `DROP_NEW_WITH_COUNTER`: reject new records and increment a dropped counter;
- `DROP_OLDEST_COMPLETED_WITH_COUNTER`: recycle the oldest completed buffer;
- `FAIL_SESSION`: disable recording and preserve the first error;
- optional blocking mode only for offline/testing tools, never the default
  Minecraft runtime profile.

Registration and initialization may allocate. Ordinary frame/scope/counter
recording must not allocate, open files, format strings, or wait for export.
Large names and categories must be copied into Libft's preallocated string
table during registration so Minecraft-owned pointers do not become lifetime
dependencies.

### Persistent Libft export thread

The export thread is created once by Libft initialization. It remains alive for
the complete analytics session and sleeps on a condition variable when there is
no completed work. It must not be created or joined per frame or per export.

Shutdown must follow this order:

1. stop accepting new events;
2. publish the partially filled active buffer;
3. signal the exporter;
4. wait for accepted completed buffers to be written;
5. record and return the first write/flush/close error while continuing
   best-effort cleanup;
6. request exporter termination and join it;
7. close files and release all Libft-owned buffers.

An emergency shutdown option may skip draining, but it must be explicit and
must report how many accepted records were abandoned. Destructors must not
silently discard a failed shutdown result; applications must call the explicit
shutdown API and report its return code before destructor cleanup.

### Proposed Libft-facing API

The exact names may follow existing module style, but the capability boundary
should resemble:

```cpp
analytics_session session;
analytics_session_config configuration;

analytics_default_session_config(configuration);
configuration.output_path = "minecraft_analytics.jsonl";
configuration.output_format = FT_ANALYTICS_OUTPUT_JSONL;
configuration.event_capacity_per_buffer = 65536U;
configuration.buffer_count = 4U;

session.initialize(configuration);
session.register_region("gpu_batch_collect", "render", &region_id);
session.register_counter("visible_chunks", "render", &counter_id);
session.start_exporter();

analytics_begin_frame(&session, frame_number);
analytics_begin_scope(&session, region_id);
analytics_end_scope(&session);
analytics_record_counter(&session, counter_id, visible_chunk_count);
analytics_end_frame(&session);

session.shutdown();
```

File paths, format, flush interval, rotation limits, capacity policy, enabled
outputs, and manual-clock behavior belong in `analytics_session_config`.
Minecraft supplies values; Libft validates and executes them.

### Compile-out behavior

The normal Minecraft build must retain no analytics event buffers, exporter
thread, file handles, or hot-path calls. Instrumentation macros must compile to
no-ops when analytics is disabled, while direct Libft APIs remain available to
other applications that explicitly link/use the module. The analytics build
must use separate Minecraft and Libft object trees so compile flags never leak
between normal and instrumented binaries.

### Migration sequence

1. Extend `analytics_session_config` and move writer/file ownership into Libft.
2. Replace the fixed shifting export arrays with preallocated rotating buffers.
3. Add the persistent Libft export worker and explicit start/shutdown APIs.
4. Move JSONL/CSV/trace organization and header/session records into Libft.
5. Replace Minecraft's exporter thread, queue, and serialization code with the
   thin `RuntimeAnalytics` forwarding adapter.
6. Preserve Minecraft region IDs and output compatibility during migration.
7. Remove all remaining Minecraft-owned analytics buffers and thread state.

### Required tests

Libft tests must cover:

- configured preallocation succeeds and normal recording performs no further
  allocations under failure injection;
- exact capacity boundaries and every overflow policy;
- producer/export buffer ownership under TSan with multiple producer threads;
- exporter sleep/wakeup, repeated wakeups, spurious wakeups, and clean join;
- shutdown with empty, partially filled, and multiple completed buffers;
- write, flush, close, allocation, thread-creation, and thread-join failures;
- accepted-event preservation and dropped-event accounting;
- region/string lifetime after caller-owned strings are changed or destroyed;
- one-session output views: menu/non-world records must appear only in the
  primary file, active-world records must appear only in the world file, and
  changing the active-world state must not duplicate or lose records;
- world-state transitions at frame boundaries, including entering and leaving
  the world between frames and shutdown while the world view is active;
- balanced and unbalanced nesting, wrong-thread scope completion, invalid IDs,
  and maximum nesting depth;
- deterministic manual-clock inclusive/exclusive statistics and percentiles;
- JSONL/CSV/trace schema validity, escaping, partial writes, and interrupted
  writes;
- file rotation and session/header continuity;
- disabled and compile-out builds create no thread and no output file;
- repeated initialize/shutdown cycles and shutdown racing with producers;
- Minecraft integration proving its adapter owns no exporter thread or event
  storage and that the ordinary build contains no analytics worker symbols.

Benchmark tests must measure begin/end scope latency, frame-publication latency,
producer throughput, exporter throughput, and gameplay-thread p95/p99 impact.
The migration is complete only when the non-analytics Minecraft build remains
unaffected and the analytics build produces equivalent or richer output with
all storage and writing performed inside Libft.

## 1. Executive summary

The analyzed world session contains 1,145 frames, of which 930 contain the
normal GPU gameplay render path. The normal frame is comfortably below the
8.33 ms budget for 120 FPS, but several paths create avoidable steady work and
visible tail-latency spikes.

The implementation order should be:

1. separate GPU mesh upload from visibility collection and enforce a per-frame
   upload budget;
2. make chunk-result commits and remesh dispatch incremental, indexed, and
   explicitly budgeted;
3. avoid scanning every chunk and every streaming candidate every frame;
4. reduce per-chunk draw submission overhead after recording draw counts;
5. isolate the unexplained `game_update` and uninstrumented frame spikes before
   optimizing them.

The 49.565 ms `gpu_batch_collect` frame is the clearest hitch in this run.
Inspection shows that `GpuGeometryBatch::collect()` also calls
`GpuChunkMesh::sync()`. A sync partitions the complete index buffer into new
temporary solid/water vectors and performs three `glBufferData()` uploads.
Therefore the region named "collect" combines cheap culling with potentially
expensive CPU allocation, index classification, driver allocation, and GPU
upload. Uploading a newly committed chunk can consequently block one frame for
far longer than the target budget.

## 2. Measured baseline

### 2.1 World-session frame times

| Metric | Time |
|---|---:|
| Average | 2.813 ms |
| Median | 2.621 ms |
| p95 | 5.037 ms |
| p99 | 6.773 ms |
| Maximum | 54.959 ms |

The separate all-application file reports a 2,675.918 ms maximum in
`world_update`. That sample occurs outside the continuous world-session view
and is most likely world entry or initialization work. It must be analyzed as a
loading-time event, not mixed into steady gameplay statistics.

### 2.2 Important exclusive regions

| Region | Average | p95 | Maximum | Interpretation |
|---|---:|---:|---:|---|
| `world_stream_dispatch` | 0.542 ms | 1.259 ms | 4.121 ms | recurring candidate and dirty-remesh scanning/submission |
| `world_stream_drain` | 0.360 ms | 1.077 ms | 4.029 ms | main-thread result commits and deferred edits |
| `gpu_batch_collect` | 0.346 ms | 1.178 ms | 49.565 ms | culling plus synchronous mesh preparation/upload |
| `gpu_solid_flush` | 0.261 ms | 0.392 ms | 2.018 ms | per-visible-chunk uniforms, binds, and draws |
| `application_render` self time | 0.144 ms | 0.774 ms | 1.244 ms | render wrapper and currently unscoped render work |
| `gpu_clear_sky` | 0.081 ms | 0.192 ms | 0.679 ms | clear and sky draw |
| `game_update` self time | 0.064 ms | 0.001 ms | 20.498 ms | normally negligible, but contains rare unclassified stalls |
| `gpu_water_flush` | 0.008 ms | 0.015 ms | 0.025 ms | not a current target |
| `player_motion` | 0.011 ms | 0.022 ms | 0.044 ms | not a current target |
| `block_interaction` | <0.001 ms | <0.001 ms | 0.001 ms | not a current target |

These averages are per frame in which the region appeared. Nested inclusive
times must not be added together. The table uses exclusive time to avoid
double counting.

### 2.3 Worst observed frames

- Frame 215: 54.959 ms. `gpu_batch_collect` consumed 49.565 ms and solid
  submission consumed 2.018 ms. This is a render/upload hitch.
- Frame 638: 23.051 ms. `game_update` had 18.646 ms of exclusive time. The
  existing stream scopes do not explain it.
- Frame 922: 22.203 ms. `game_update` had 20.498 ms of exclusive time.
- Frame 1063: 15.071 ms. `game_update` had 12.732 ms of exclusive time.
- Frames 394 and 637: streaming reached 4.544 ms and 5.005 ms respectively,
  primarily dispatch and drain work.

## 3. Priority 1: remove synchronous mesh-upload hitches

### 3.1 Current problem

`GpuGeometryBatch::collect()` currently performs four responsibilities:

1. configure the frustum cache;
2. scan all chunk slots;
3. determine visibility;
4. call `GpuChunkMesh::sync()` for every visible stale mesh.

`GpuChunkMesh::sync()` then:

- uploads pre-partitioned solid and water index arrays prepared by the voxel
  mesh-generation path;
- creates GL objects when this is the first upload;
- replaces vertex, solid-index, and water-index storage with
  `glBufferData()`;
- configures both VAOs.

This explains why ordinary collection is small but one frame reached 49.565
ms. The work scales with the number and size of meshes becoming visible or
changing in one frame, not merely with frustum-culling complexity.

### 3.2 Required change

Solid/water partitioning has now been moved out of the render-frame hot path.
The voxel mesh-generation path produces finalized solid and water index arrays
alongside the immutable vertex/index payload, and `GpuChunkMesh::sync()` only
consumes those arrays during its render-thread upload. This removes the
temporary render-thread vectors and classification scan.

The remaining required change is to make the generation or meshing worker
produce a complete upload-ready render payload:

- compact vertex array;
- solid index array;
- water index array;
- bounds;
- mesh revision and byte counts.

The main thread should only move the completed CPU payload into committed world
state. OpenGL calls must remain on the context-owning render thread, but GPU
uploads should use a persistent queue with both a time and byte budget per
frame.

Suggested initial limits are one newly created chunk upload per frame and a
configurable byte ceiling. These are starting values, not constants to bake
into gameplay behavior. Continue an already-started oversized upload rather
than starving it forever.

Keep the old uploaded mesh drawable until its replacement upload succeeds.
Commit the new GPU handles/revision transactionally; an allocation or GL error
must leave the old mesh usable.

Where supported, benchmark orphan-and-subdata, persistent mapped buffers, and
immutable storage. Select the approach through capabilities instead of making
a platform assumption. Do not enable the inactive mega-buffer path as-is; its
layout and lifecycle do not match the active chunk meshes.

### 3.3 Expected result

This should directly target the largest measured hitch. It may not reduce the
total bytes uploaded, but it prevents a burst of mesh preparation and uploads
from consuming one complete frame.

### 3.4 Acceptance criteria

- no mesh-upload frame exceeds 4 ms CPU time under the same traversal test;
- p99 world-session frame time does not regress;
- no stale, partially uploaded, or missing mesh appears;
- the same chunk geometry and water classification are produced;
- upload queue age is bounded and exposed so budgeting cannot hide starvation.

The worker-side partitioning itself must additionally be covered by tests that
compare the concatenated solid and water index arrays with the source index
array, verify every index is in range, and exercise empty, solid-only,
water-only, and mixed meshes. GPU integration tests must confirm that the
precomputed arrays produce the same draw counts as the old classification path.

## 4. Priority 2: make streaming commit work truly incremental

### 4.1 Current problem

Streaming consumes about 0.902 ms per active frame on average and reaches
5.005 ms. The drain loop limits itself to two results and checks a 2 ms
deadline before each result, but one commit may exceed the deadline. Deferred
edits run after that loop and are outside the deadline.

A stream-result commit can move a chunk and mesh, register it, append deferred
edits, and queue four neighbor remeshes. Candidate lookup is a linear scan.
Dispatch separately scans candidates and then scans all chunk slots for dirty
remeshes. These costs compound as the loaded radius grows.

### 4.2 Required change

- Replace `find_stream_candidate()`'s linear coordinate scan with an indexed
  lookup keyed by chunk coordinate or candidate-grid index.
- Maintain a queue/set of dirty chunks when `mesh_dirty` changes from false to
  true. Dispatch from that queue instead of scanning every chunk each frame.
- Split result handling into small commit stages with measured costs: result
  validation, free-slot acquisition, chunk-state commit, deferred-edit merge,
  neighbor invalidation, and remesh submission.
- Apply one shared main-thread streaming budget across result commits,
  deferred edits, and remesh scheduling. Check the budget between bounded
  units of work.
- Preserve fairness by carrying cursors and unfinished work to the next frame.
- Coalesce repeated neighbor remesh requests by chunk coordinate and voxel
  revision.

Generation itself should remain on persistent worker threads. The main thread
should commit completed state only; it must not wait for generation or remesh
completion.

### 4.3 Acceptance criteria

- `world_stream_update` p95 below 1.25 ms in the same run;
- no individual streaming frame above 2.5 ms under normal traversal;
- identical generated chunks for the same seed and configuration;
- no missing neighbor remesh at chunk boundaries;
- no candidate or dirty chunk can starve under continuous movement;
- queue depth and oldest-item age remain bounded.

## 5. Priority 3: stop rebuilding visibility from every slot every frame

### 5.1 Current problem

Even without uploads, `collect()` clears the visible list, scans
`world.chunk_count`, checks initialization and geometry, and performs an
eight-corner visibility test for each drawable chunk every frame.

### 5.2 Required change

Maintain a dense list of initialized renderable chunk slots so empty cache
slots are never scanned. Add a visibility cache keyed by:

- camera chunk/cell;
- quantized camera orientation or frustum generation;
- active render distance;
- world render-set revision;
- per-chunk mesh/bounds revision.

When only a chunk mesh revision changes and bounds stay equal, update that
entry without rebuilding unrelated visibility state. When camera motion stays
inside conservative cache thresholds, reuse the prior visible set. The cache
must be conservative: false positives are acceptable, false negatives are
not.

A spatial hierarchy is only justified if dense-list plus cached-frustum work
remains material at maximum supported render distance. Start with the smaller
change and measure it.

### 5.3 Acceptance criteria

- unchanged rendered image and no edge popping during rapid camera rotation;
- collection p95 below 0.5 ms when no upload occurs;
- cache invalidation is covered for chunk add, eviction, remesh, bounds change,
  resize, render-distance change, teleport, and world reset.

## 6. Priority 4: reduce solid draw-submission overhead

Solid submission averages 0.261 ms and reaches 2.018 ms. It sets a chunk-offset
uniform and submits one draw per visible chunk. This is a real scaling cost,
but it is smaller than upload and streaming work in this run.

First add counters for visible chunks, non-empty solid/water meshes, draw calls,
triangles, VAO binds, and uploaded bytes. Then evaluate:

1. storing chunk world offsets in an instance/SSBO record;
2. multi-draw indirect or a platform-supported multi-draw path;
3. batching only chunks with compatible buffers/material state;
4. a fallback per-chunk path for platforms without the required capability.

Do not combine all geometry into one buffer that must be rebuilt whenever one
chunk changes. Prefer stable per-chunk allocations in larger buffer arenas, or
indirect draws referencing independently replaceable ranges. This preserves
incremental streaming and bounds update cost.

Acceptance target: halve solid submission CPU time at the same visible chunk
count without increasing upload spikes or changing output.

## 7. Unconfirmed spikes that need narrower measurement

The 12.7-20.5 ms exclusive `game_update` spikes are not explained by
`world_stream_update`, player motion, or block interaction. The code around the
stream call includes tick advancement, input acquisition, character-location
synchronization, settings access, render-distance strategy work, and possibly
work hidden by scope imbalance or an operating-system stall. None of those
should be rewritten based only on the current region name.

Add temporary subregions for:

- `world.advance_tick()`;
- camera-input acquisition;
- character-location synchronization;
- generation-budget calculation;
- render-distance/settings reads;
- post-stream return/error handling.

Also split currently uninstrumented frame time into `present`, frame pacing,
phase transition/loading, and residual time. Frame 1145 is entirely
uninstrumented at 18.798 ms, and several otherwise normal frames retain 1-4 ms
of residual time. `window.present()` is currently outside a dedicated region,
so swap blocking cannot yet be distinguished from application work.

This instrumentation is diagnostic only. No optimization should be accepted
until it identifies an actual function or wait responsible for the spike.

## 8. Work that should not be prioritized from this run

- Water submission, crosshair drawing, player movement, and block interaction
  are too small to matter presently.
- The overlay averages 0.322 ms when sampled, but it is optional and analytics
  or debug UI work is outside this document's requested performance scope.
- Additional RW locks will not solve the measured renderer costs. Rendering
  reads committed mesh state on the main thread; adding locks around that path
  would add overhead.
- Creating render threads per frame is prohibited. Any CPU preparation workers
  must be persistent and sleep on a condition variable when idle.
- GPU work must remain on the graphics-context owner unless a tested shared
  context design is introduced.
- The 2.676 second application-level outlier must not distort gameplay frame
  targets; loading latency requires its own benchmark and user-facing goal.

## 9. Validation plan

For every optimization, compare the ordinary non-analytics build for player
experience and use the analytics build only to explain the result.

Run at least ten fixed 10-second trials for baseline and candidate builds with:

- the same seed, camera path, resolution, render distance, and graphics mode;
- a warm-cache stationary view;
- continuous movement into newly generated chunks;
- rapid rotation with no movement;
- repeated block edits causing neighbor remeshes;
- maximum supported render distance;
- water-heavy and geometry-heavy scenes.

Report average, median, p95, p99, maximum, frames over 8.33 ms, frames over
16.67 ms, and the count/bytes/age of queued uploads and streaming commits.
Compare visual output or deterministic visible-mesh hashes where practical.

An optimization is accepted only when the non-analytics build improves frame
time or tail latency, correctness tests remain unchanged, and the gain survives
repeated trials. A lower analytics-region time alone is not sufficient.

## 10. Recommended implementation sequence

1. Add upload-specific and unexplained-update diagnostics.
2. Move index partitioning into mesh production and introduce the bounded GPU
   upload queue.
3. Add indexed candidates and a dirty-remesh queue.
4. Apply one resumable streaming commit budget.
5. Add dense renderable-slot tracking and conservative visibility reuse.
6. Record draw counts and prototype indirect/batched solid submission.
7. Re-run the full benchmark matrix after each isolated change.

This sequence targets the measured hitches first while keeping each behavioral
change small enough to verify independently.
