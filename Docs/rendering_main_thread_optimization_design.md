# Rendering and Main-Thread Optimization Design

Status: implemented through the currently justified optimization phases;
renderer identity, mesh partition changes, visibility caching, analytics-only
upload/commit diagnostics, bounded persistent mesh-upload scheduling, startup
backpressure handling, and conservative frustum-plane culling are implemented.
GPU draw batching remains measurement-gated because current draw-submission
cost is not material. Section 11 supersedes the in-process world-ownership
assumption and is a required, not-yet-implemented client/server migration.

Reviewed branch: `agent/libft-hardening-update`

Reviewed Minecraft commit: `68cb9ef`

Reviewed Libft commit: `182f9a5c`

Date: 2026-08-28

## 1. Purpose

### Manual world-generation stall capture

Debug builds support an opt-in manual abort for a run that appears stuck during
world loading. Start the debug executable with `FT_VOX_ABORT_ON_SIGINT=1`, then
send `Ctrl+C` to its console. The handler converts that deliberate interrupt to
`abort()`, allowing the platform debugger/core-dump configuration to capture all
threads, including workers waiting on or executing generation. This is manual
only: there is no automatic loading timeout and the handler is not compiled into
release builds. The dump must be inspected together with the startup stage
markers and periodic stream diagnostics; a dump without those state lines cannot
distinguish expensive generation from a lock or result-commit stall.

This document verifies the rendering and main-thread performance review and
defines a careful implementation and validation plan. The target is smoother
interactive rendering, especially lower p95/p99 frame times, without changing
world behavior, graphics output, chunk fairness, or thread ownership.

The first implementation pass should optimize confirmed unnecessary work. More
invasive renderer changes, especially geometry batching, must be justified by
measurements from the current GPU path.

### 1.1 GPU startup/render audit (2026-09-03)

The interactive analytics executable was launched with `--auto-start` and
allowed to run through the loading transition and first gameplay frames. The
audit reached gameplay, completed the 625-slot visibility scan, listed 13
visible chunks, synchronized four meshes, issued 13 solid draws, completed the
water pass, and continued through cached visibility samples. The normal
executable also remained responsive during the same startup window. This is
the required graphics-context evidence for the startup fix; the headless
world-generation probe alone would not have detected the renderer loop hang.

The hang was caused by the empty/uninitialized-slot branch in
`GpuGeometryBatch::collect()` skipping the scan-index increment. The scan is
now a structured bounded loop, and all skip branches advance through the loop
control while updating the visited count. The first post-fix GPU audit showed
`visible=13`, `uploads=4`, `draws=13`, and completed solid and water passes.

### 1.2 Current analytics evidence (2026-09-01)

The first captured in-world session recorded ordinary frames in approximately
the 1--6 ms range, but one frame contained a roughly 2.67 second
`gpu_batch_collect` interval. A separate `world_update` sample reached roughly
3.82 seconds. The analytics bookkeeping visible in the same capture was only
approximately 0.2--0.6 ms per frame, so the capture does not support blaming
the analytics exporter for the multi-second stalls.

The follow-up capture must be interpreted carefully: frame 64 contains a
`world_update` interval of approximately 6,423 seconds while the recorded
render interval is only approximately 1.3 ms. That is consistent with the
application being paused or inactive between frames, not a valid renderer
lag sample. Analytics now supports `trace_frame_interval`; Minecraft keeps
frame summaries and region aggregates on every frame while exporting detailed
trace events and frame summaries every 120th frame. This reduces trace
formatting, file I/O, and buffer pressure without removing the data needed to
identify frame-level bottlenecks. A one-frame export/trace interval remains
available for focused capture.

The old `minecraft_analytics.jsonl` capture predates frame-export sampling and
also exposed a format defect: records were concatenated without newline
delimiters, so it was not valid JSONL and could not be parsed as a stream. Libft
now appends a newline to every JSON frame and trace record. Old logs must not be
used for quantitative aggregation unless they are split between top-level JSON
objects first.

The producer still writes only to the active in-memory export buffer. Once a
buffer is handed off, the persistent exporter thread consumes that separate
buffer and performs serialization/file I/O. Any remaining frame-time impact
must therefore be measured as producer-side bookkeeping or lock contention;
the next capture should include the duration of `analytics_end_frame()` and
the dropped-event counters. The Minecraft analytics build now emits this
diagnostic only when finalization exceeds 1 ms or an event/frame is dropped.

Detailed stderr diagnostics use an 8 ms threshold, matching the 120 Hz frame
budget, rather than printing for every ordinary 2 ms commit or render pass.
The analytics scopes retain all timings; stderr is reserved for work capable
of consuming most or all of a frame budget.

Per-cull timing and GPU chunk identity validation are sampled once every 120
visibility-collection calls in analytics builds. Timing every cull and
rechecking every uploaded chunk added repeated clock reads and validation work
to the render thread; the enclosing visibility-scan timer remains continuous.

Controlled 10-second headless checks after these changes produced 20.22 FPS
with analytics enabled and 21.62 FPS without analytics in separate trials.
Earlier trials were approximately 20.16 and 20.07 FPS respectively. Because
the generation workload varies between launches, these numbers are not a
conclusive speedup; they do show that the analytics path is no longer causing
the previously suspected catastrophic throughput collapse. Interactive tests
must still compare matched repeated trials. The analytics output from the
latest headless run contained frame summaries without diagnostic stderr spam.

The latest in-world capture provides the first useful GPU-path breakdown. It
contains 24 sampled frame summaries from frames 120 through 2880. The sampled
`world_update` region averaged approximately 1.78 ms and peaked at 4.94 ms;
`world_stream_update` averaged 1.47 ms and peaked at 4.22 ms. The nested
deferred-edit region averaged 1.06 ms and peaked at 3.09 ms. By comparison,
GPU batch collection averaged 0.05 ms, solid submission averaged 0.25 ms,
and water submission averaged 0.009 ms, with respective maxima of 0.49 ms,
0.51 ms, and 0.018 ms. This capture does not justify Phase 4 draw batching
yet. The next performance work should first separate stream drain, deferred
edits, and input/world-update spikes while retaining the GPU draw counts as a
baseline.

A fresh 10-second headless analytics sample must not be used as GPU evidence:
it selected the software renderer and reported 17.24 FPS, 58.015 ms average
frame time, 103.685 ms p95, and 153.087 ms p99. The sampled software pass saw
121 visible chunks and 197,144 visible triangles. This is useful evidence for
the conditional software-renderer phase, but it says nothing about OpenGL
draw-submission cost and therefore does not independently justify Phase 4.
GPU batching still requires an interactive GPU capture with draw counts and
GPU-path timings.

A fresh matched headless check after frame-export sampling produced 30.90 FPS
normal versus 27.87 FPS analytics without movement. A movement-enabled pair
produced 28.47 FPS normal versus 28.84 FPS analytics. The disagreement, along
with the different p99 values, shows that the workload is not deterministic
enough for one pair to identify the culprit. It does establish that reducing
file export alone does not consistently remove the observed tail latency.
The next diagnostic experiment must run the same seed, movement script,
render-distance schedule, and duration while independently toggling producer
instrumentation and the exporter thread.

The analytics executable now accepts `--analytics-no-exporter` for this
controlled experiment. It keeps producer instrumentation and in-memory
statistics enabled, but performs the final export during shutdown instead of
starting Libft's persistent exporter thread. Normal analytics runs retain the
sleeping exporter by default; this flag is diagnostic-only and must not be used
for ordinary interactive captures.

The analytics executable also accepts `--analytics-no-instrumentation`. This
leaves the analytics-capable binary and all compile-time integration present,
but does not initialize Libft's recording session, so the runtime calls return
through the disabled path. Compare normal analytics, no-exporter, and
no-instrumentation runs with the same world seed and movement. If only the
third run recovers performance, producer-side recording is responsible; if it
does not, the regression is elsewhere in the analytics build or in another
compile-gated diagnostic path. This switch is an isolation tool, not a normal
profiling mode.

The analytics file from the sampled run contained two frame summaries (frames
120 and 240) and valid newline-delimited records. Those summaries attributed
approximately 69 ms total to `software_meshes` across the two samples, while
the analytics frame-finalization and queue diagnostics showed no reported
backpressure. This makes software mesh traversal/rasterization the current
measured hotspot, but it does not prove that analytics causes the hotspot.

Three additional movement trials with the exporter disabled averaged 24.15 FPS
(24.26, 24.02, and 24.17). Three trials with the persistent exporter enabled
averaged 23.95 FPS (23.95, 24.00, and 23.91). The approximately 0.8% average
difference is within the run-to-run variation, so the sleeping exporter thread
is not the source of the catastrophic slowdown. One enabled trial reported a
9.329 ms world-commit operation for request 424, which is a more actionable
frame-budget violation than the exporter path. The next work should focus on
commit/generation spikes and software mesh traversal while retaining the
exporter switch as a repeatable isolation tool.

A post-upload-policy headless pair also produced identical framebuffer hashes:
the normal binary measured 18.98 FPS (p95 78.09 ms, p99 91.70 ms), while the
analytics-capable binary with instrumentation disabled measured 14.99 FPS
(p95 150.25 ms, p99 202.58 ms). This is not a controlled conclusion about
analytics overhead because the runs did not share a synchronized workload
schedule and the analytics-disabled binary still uses a separate build
configuration. Retain it as evidence that repeated matched trials are needed
before attributing render-distance or frame-tail changes to the upload policy.

The full Minecraft validator target has since passed after the upload-queue and
inactive-buffer cleanup: camera speed, collision, block editing,
visible-distance, terrain determinism, world scale, caves, terrain
configuration, world revision, and asynchronous world generation all report
success. This verifies the optimization changes did not alter the tested world
or movement invariants, but it does not replace matched GPU performance runs.

The validator executable now has an explicit `--validate-all` mode. It runs the
validators sequentially, reports the named validator and error code for every
failure, and exits nonzero if any validator fails. Individual flags remain
available for focused runs. No-argument execution continues to launch the game;
the validator binary must never be mistaken for the Libft test runner or for a
request to run every check implicitly. The all-validator mode is intended for
CI and repeatable local regression checks, while long world-streaming checks
should still retain their phase-progress diagnostics.
The Minecraft Makefile exposes the same workflow as `make validate-all`, and
the ordinary `make test` target uses it so new validators are not silently
omitted when another check is added.

Analytics-only commit phase diagnostics now split a slow streamed-chunk commit
into payload transfer, spatial-index registration, deferred-edit append, and
neighbor-remesh scheduling. They are emitted only when the measured phase sum
reaches 8 ms, so normal builds and ordinary analytics frames do not pay for
formatted diagnostics. A fix should target the phase that actually dominates;
the commit's overall two-result/two-millisecond admission budget is not enough
to protect against one indivisible chunk transfer exceeding a frame budget.

The first software-render breakdown implementation revealed an instrumentation
failure: the center chunk routinely exceeded 8 ms, so printing that condition
every frame flooded stderr and itself destroyed FPS. The diagnostic now samples
one frame every 120 frames, performs per-chunk timing only on that sampled
frame, and emits at most one summary. Profiling diagnostics must never
synchronously log from a hot loop on every frame.

The software mesh loop now performs the cheap face-orientation test before
transforming vertices. This avoids three camera-space transforms for every
back-facing triangle without changing the existing face-selection rule. A
follow-up analytics sample still measured approximately 119,792 visible
triangles across 69 visible chunks, with the slowest central chunk taking
approximately 10.1 ms. Therefore triangle rasterization remains the dominant
software cost; further changes should target measured raster work rather than
adding more per-triangle diagnostics.

The software depth buffer now stores interpolated inverse depth rather than
forward depth. Since the rasterizer already interpolates inverse depth for
perspective-correct texture coordinates, the per-pixel depth test no longer
performs a reciprocal division. The clear value and comparison direction were
changed together; this remains private to the software renderer and does not
affect the GPU depth path.

The follow-up renderer diagnostic pass also moved GPU cull timing and uploaded
chunk identity/Y-bound validation to one visibility collection every 120
collections. Five repeated three-second headless analytics trials averaged
approximately 26.76 FPS, while five producer-disabled trials averaged
approximately 26.57 FPS; all framebuffer hashes matched. This does not
reproduce the reported interactive slowdown and indicates that these
diagnostics are not its cause in the headless workload. The producer isolation
switch remains necessary for an interactive capture because menu/input and
window-present behavior are not exercised by this benchmark.

The analytics producer no longer rotates and wakes the exporter for every
frame. It retains multiple frame summaries in the active buffer and hands the
buffer off only when its configured capacity is reached or during shutdown.
Region statistics are merged as one batch per completed frame, and the
export-error value is read atomically. This keeps the measured thread away
from per-event mutex churn while retaining bounded memory and explicit drop
accounting.

The producer's ordinary scope path also keeps a validated thread-local pointer
to the active frame state. Scope begin/end operations therefore do not scan the
four-entry fallback state table for every nested scope. World/menu
classification is still read atomically at scope start, because the public API
allows a transition within a frame and each scope must retain the state it
started in. The fallback lookup remains available for worker and standalone
APIs, and the cached pointer is cleared at frame completion and validated
against the session before use. This is bookkeeping-only: it does not change
scope timestamps, nesting, classification, aggregation, or export buffer
ownership.

The frame publisher also previously rebuilt the rolling mean and sorted the
entire 120-frame window for p95 and p99 on every frame while holding the
analytics mutex. That made the sampled exporter policy ineffective: file
serialization was deferred, but percentile calculation still ran at frame
rate. The publisher now maintains the rolling sum incrementally and performs
percentile sorting only when the frame is selected for export. This preserves
the exact sampled percentiles while removing avoidable O(window-squared) work
from ordinary frames. Any future aggregate must follow the same rule: use an
O(1) producer update and defer sorting, formatting, and reporting to the
exporter or sampled path.

The active renderer performs a synchronous `GpuChunkMesh::sync()` and OpenGL
buffer upload while collecting visible chunks. A newly completed world result
can also perform main-thread chunk insertion, full snapshot capture for a
neighbor remesh, and deferred-edit work. These are now separately instrumented
in analytics builds. The next measurement must record the affected chunk and
mesh byte/index counts before choosing between upload chunking, a stricter
commit budget, or both. A fix is not considered validated until the p95/p99
frame data and the worst upload/commit samples are captured independently.

The collector now keeps a persistent upload cursor and admits at most two
pending visible mesh uploads per frame, with a four-megabyte aggregate transfer
budget. The first pending upload is always admitted so a large individual mesh
cannot starve indefinitely, while later uploads are deferred when either limit
is reached. This is deliberately a small fixed policy rather than a new thread
or a GPU synchronization point: it reduces the visible-world fill-in delay
without allowing a burst of generated meshes to consume an unbounded frame.
The upload count and byte total remain available to analytics diagnostics so
the policy can be tuned from matched captures. A slot-reuse identity mismatch
still invalidates the entry and cannot be bypassed by the budget.

The scheduler is shared by cache-hit and cache-miss paths. This matters while
streaming: generation and remeshing can invalidate the visibility cache on
consecutive frames, so fairness implemented only in the cache-hit path would
repeatedly upload the first entries in the spatial index and starve distant
visible chunks. The scheduler walks the complete visible list from a
persistent cursor before advancing it, so repeated full scans cannot reset
upload priority. If the cursor's chunk was evicted during recentering, the
next traversal starts at the visible chunk nearest the camera rather than at
an arbitrary storage/index entry. This makes newly streamed terrain appear
around the player first while the remainder of the visible list continues to
drain fairly.

The scheduler additionally selects the nearest pending visible chunk as the
start of each traversal. This handles the case where the cursor remains valid
but older remesh work keeps arriving ahead of a newly streamed chunk. The
nearest-first choice is only a priority hint; the complete circular traversal
and bounded upload policy remain in force, so the system does not create a
thread per upload or discard distant work.

## 2. Current frame ownership

The interactive game loop is single-threaded for input, world-result commits,
render submission, and presentation:

```text
poll input
    -> update player
    -> update world / commit worker results
    -> build optional diagnostics
    -> collect visible chunks
    -> submit GPU draws
    -> present
    -> sleep to the 120 Hz deadline
```

Terrain generation and remeshing can execute in the generation pipeline, but
completed results are committed into `World` on the main thread. OpenGL work
also remains on the main thread that owns the graphics context.

The interactive loop targets 120 frames per second in
`Application::run_game_loop()`. This is an 8.33 ms CPU and GPU frame budget,
not a 60 FPS lock. `present()` may impose an additional platform-dependent
swap interval, so actual pacing must be measured rather than inferred from the
sleep deadline alone.

## 3. Verification of the review

### 3.1 Resolved: render diagnostics are built only when enabled

`GameSession::render()` now checks whether an overlay or revision preview is
enabled before calling `build_render_debug()`. Therefore the following work is
skipped on ordinary in-game frames when diagnostics are disabled:

- Linux opens and parses `/proc/self/status` to obtain resident memory;
- macOS calls `task_info()`;
- GPU memory accounting scans every `GpuChunkMesh` slot;
- biome information is recomputed;
- seed and biome strings are copied or formatted.

This is a stronger and more actionable finding than the original description,
which treated debug cost as limited to visible overlays. The 15-frame throttle
in `VoxelRenderer::render_gpu_overlay()` only throttles rasterization and
texture upload; it does not throttle `build_render_debug()`.

When the revision preview is visible, an additional temporary vector is
created, reserved, populated, and traversed every frame.

Verdict: resolved. Keep the early check in place and retain a regression test
or profile comparison for overlay-disabled gameplay.

### 3.2 Resolved/qualified: resize and viewport calls

`ApplicationPhaseController::render_gpu_frame()` reads the GPU window
dimensions and calls `VoxelRenderer::resize_gpu()` every frame. The underlying
`GpuRenderer::resize()` returns immediately when dimensions are unchanged, so
that path does not repeatedly invoke resize work. `GpuWorldRenderer::render()`
sets `glViewport()` as part of establishing the world render state; it is not a
second resize operation and should remain unless viewport state is managed by a
strict frame-level render-state cache.

Verdict: the previously suspected duplicate resize work is not present in the
current implementation. A render-state cache may be considered later, but it
is not a current performance fix.

### 3.3 Confirmed: every frame scans and culls all chunk slots

`GpuGeometryBatch::collect()` discovers drawable chunks from the bounded world
chunk storage, checks initialization and mesh state, performs chunk AABB
visibility testing against six precomputed frustum planes, calls
`GpuChunkMesh::sync()`, and reconstructs `_visible_chunk_slots` when the
visibility cache is invalidated.

`GpuChunkMesh::sync()` correctly avoids uploads when the mesh revision is
unchanged, so the review must not characterize geometry as being uploaded every
frame. The recurring CPU work is the bounded drawable-slot scan, six-plane AABB
test, visible-list reconstruction, and subsequent draw submission.

Analytics builds additionally time the individual mesh upload nested inside
this scope. A slow-upload diagnostic reports the chunk coordinate, byte count,
vertex count, solid-index count, water-index count, and duration. World-result
commit and deferred-edit application are separate analytics scopes as well.
The collector also reports its scanned-slot count, visible-slot count, upload
count, and total duration when it exceeds two milliseconds. This distinguishes
a blocking OpenGL upload from an expensive main-thread world commit or an
unexpected collection-loop size. All of these diagnostics are compiled out of
normal builds.

On the same sampled collection cadence, the solid and water flushes now report
their effective draw counts and pass duration in analytics builds. This is the
measurement gate for Phase 4: range or multi-draw batching should only be
implemented if these counts and timings are material on the supported GPU path.

The analytics build separates cached-list synchronization and full visibility
scanning regions. Geometry change detection is now an O(1) revision read rather
than a second storage walk; this measurement is not present in normal builds.

The hot `WorldChunk` metadata is stored before the large voxel and mesh
payloads within each object. This reduces the offset to metadata when a
known chunk is visited, but does not make an array walk cache-friendly because
the object stride is still large. The fixed storage bound keeps the renderer’s
authoritative fallback bounded; the spatial index remains the important
optimization for world queries. The payload layout and rendering semantics are
unchanged.

The world spatial index remains the authoritative query acceleration structure,
but the GPU visibility collector does not rely on it as the sole drawable
enumeration. A streamed slot can become initialized between index-maintenance
steps, and making rendering depend on a later block edit to repair that index
causes visible pop-in. The collector therefore scans the bounded chunk-storage
array for drawable meshes and converts each storage slot directly to its stable
GPU slot. The analytics sample reports index validity separately from the
storage scan; the fixed 625-slot bound makes this correctness fallback explicit
until a publication-safe drawable list is implemented.

Visibility invalidation now uses a monotonic world geometry revision instead of
recomputing a hash over the loaded index every frame. World insertion, eviction,
remeshing, regeneration, and block edits advance that revision; unchanged frames
therefore perform an O(1) cache-key read while retaining explicit invalidation
for geometry changes. The old hash-based cache path has been removed; diagnostics
and validation use the monotonic geometry revision directly.

The latest capture still showed multi-second `gpu_batch_visibility_scan`
intervals even though upload and signature regions were sub-millisecond. This
change reduces index-publication correctness risk; the next analytics capture
must distinguish actual culling cost from an externally descheduled process.
The diagnostics must report index validity, storage slots visited, visible
chunks, and the slowest culling operation for any scan exceeding the threshold.

Sampled GPU identity and vertical-bound checks are only evaluated after a
mesh is current for the world revision. A mesh deferred by the upload budget
is reported through the pending-upload counters instead of being misreported
as stale geometry. This keeps diagnostics actionable without changing the
upload policy.

The subsequent capture moved the dominant spike to `world_update`: one frame
took approximately 3.53 seconds while rendering itself remained about 1 ms.
The suspected recenter path (evicting out-of-range chunks and rebuilding the
spatial index) previously sat outside the stream sub-regions, so it could not
be distinguished from the rest of the update. An analytics-only
`world_stream_recenter` region now measures that path and reports loaded
counts before and after it. This must be used to determine whether chunk
destruction is the real stall before changing eviction semantics.

The next capture recorded a 3.97-second `world_update` interval, but its
exclusive time was entirely in the outer update scope: no `game_update`,
`player_motion`, stream, commit, or deferred-edit child region was recorded.
This places the stall before `GameSession::tick_world()`, where input-device
polling was previously uninstrumented. An analytics-only `input_device_poll`
region now isolates that call. Do not change world-generation or eviction
behavior based on the earlier attribution until this region is measured.

Verdict: substantially optimized. The per-slot mutex-taking empty check has
been removed, the loaded spatial index avoids scanning unused storage slots,
and the AABB test uses precomputed frustum planes. A persistent visible-list
cache skips the cull scan and list rebuild when camera pose, viewport, render
distance, and the geometry revision are unchanged. A cache miss still performs
the full scan, and runtime captures must continue to measure its candidate
count, duration, and tail latency. Upload selection is independent of cache
validity and remains fair during streaming invalidations.

The cached path previously uploaded only one pending visible mesh per frame.
That protected frame time but could make newly generated terrain appear to have
a shorter render distance while the upload backlog drained. It now uses the
same bounded count/byte policy as the cache-miss path. The current policy is a
maximum of four meshes and 8 MiB per frame. This improves catch-up without
removing the frame-time guard; it must be validated with rapid movement and
large-mesh scenes for both pop-in distance and p95/p99 frame time.

The cache also retains every visible non-empty world mesh whose upload is still
pending. Previously, a deferred mesh could be omitted from the cached visible
list during the initial scan and then never revisited while the camera and
geometry signature remained unchanged. Retaining the pending entry makes the
upload budget a true queue instead of a one-time visibility decision; draw
submission still skips entries that have no completed GPU geometry.

Every 120th full collection now emits an analytics-only sample containing the
active distance, chunk radius, index validity, storage slots visited, visible
and listed counts, pending upload count, uploaded count, uploaded bytes, and
total collection time. These fields distinguish a genuinely short loaded world from
GPU upload backlog or an overly aggressive visibility result. The sample is
not emitted by normal builds and is not printed for ordinary collections.

The empty-mesh check in this loop must not call `ft_vector::empty()` for every
slot: that accessor takes the vector mutex. The mesh’s
`has_occupied_bounds` flag is maintained with the mesh lifecycle and is a
lock-free equivalent for this renderer-level check. The active GPU collector
and `MeshCuller` now use that flag, avoiding one synchronization operation per
chunk per frame while preserving the empty-mesh behavior.

### 3.4 Confirmed: one or two draw submissions per visible chunk

The solid pass loops over `_visible_chunk_slots`, updates the chunk-offset
uniform, binds the chunk's solid VAO, and calls `glDrawElements()`. The water
pass loops over the same list and attempts a corresponding water draw. Empty
water or solid geometry returns before drawing, but the loop and method call
still occur.

At high render distance this can produce many driver submissions and VAO
bindings. The cost is likely to become more important as lighting, shadows,
SSAO, fog, and additional render passes are added.

Verdict: confirmed architecture and credible scaling risk. Actual CPU/GPU cost
now has analytics-only draw-count and slow-pass diagnostics. A future capture
must compare the solid/water draw counts and pass durations before introducing
range batching.

### 3.5 Confirmed: the apparent mega-batch path is inactive

The former `GpuGeometryBatch` implementation contained mega-buffer vectors,
buffer objects, a geometry signature, dirty flags, and upload helpers. The
active path never invoked those helpers; rendering uses one `GpuChunkMesh` per
chunk. That unused implementation has now been removed.

This code is incomplete/dead infrastructure, not a ready optimization that can
simply be enabled. Its `Vertex` layout also differs from the compact
`chunk_mesh_vertex` format used by the active path. It must either be removed
or deliberately redesigned and tested.

Verdict: resolved by removal. The unused mega-buffer state, vertex type,
geometry-signature helper, shared-water-buffer setup/upload code, and stale
compatibility aliases were removed. The active per-chunk GPU mesh path remains
the only implementation; future batching must be designed as a new path with
its own range ownership, slot-reuse identity, water ordering, and validation.

### 3.6 Confirmed: software rendering traverses every chunk and triangle

The software path scans all chunk slots. Visible chunks traverse mesh indices
triangle by triangle and perform face checks, transformation, projection,
clipping, and rasterization on the main thread. Chunk visibility currently
transforms all eight AABB corners.

This is expected to dominate software rendering. It is separate from the
normal GPU path and should not take priority unless software mode is a supported
performance target.

Correction to the earlier review: `RenderTarget::prepare()` normally points
the render target directly at the destination framebuffer. In that case
`RenderTarget::blit_to()` detects identical pixel pointers and returns without
copying. A mandatory full-frame copy is therefore not present in the usual
software path.

Verdict: traversal cost confirmed; unconditional final copy disproved.

### 3.7 Confirmed: result commits can consume or exceed the frame budget

`World::drain_generation_results()` checks a two-result and two-millisecond
limit before polling each result. A result commit may replace mesh storage,
update chunk state, register a chunk, queue neighbor remeshes, and append
deferred edits.

The time check is not a hard upper bound:

- one commit starts before the deadline and may finish after it;
- `apply_deferred_edits()` runs after the timed loop;
- edit sorting, voxel writes, snapshots, and remesh submission are not covered
  by the two-millisecond condition.

The deferred-edit phase now has its own one-millisecond observation budget and
retains the existing maximum of 64 edits per call. It always attempts the
first pending edit so a busy queue continues making progress, then stops at the
time or count limit. The budget is deliberately separate from the result-drain
budget; it prevents a large deferred-edit burst from silently consuming the
rest of the frame while preserving deterministic ordering of the edits that
are applied.

Verdict: result commits are bounded by the existing two-result/two-millisecond
policy, and deferred edits are now separately bounded, but a single edit can
still exceed its budget. Continue recording count, queue depth, and worst
individual operation time before tightening the policy further.

### 3.8 Confirmed with qualification: deferred-edit sorting and allocation

`apply_deferred_edits()` sorts the edit vector and creates a local `pending`
vector every call. An empty vector does not allocate storage, and sorting an
empty range is trivial, so this is not an important idle-frame cost by itself.
When edits are present, `pending.push_back()` can allocate, and repeated
`queue_chunk_remesh()` calls can capture snapshots during main-thread commit
work.

Verdict: burst/frame-spike concern, not a proven steady-state bottleneck.

### 3.9 Confirmed: menu canvases are regenerated and uploaded every frame

GPU menu rendering clears the canvas, rerenders the scene, converts every
pixel to RGBA, and uploads the full texture each frame. The same happens for
the in-game settings overlay while it is open.

Verdict: confirmed. It does not affect ordinary gameplay frames, but dirty
tracking can reduce menu and paused-overlay CPU/upload work.

### 3.10 Confirmed: RW locks are not the render hot path

The new RW-lock integration protects voxel reads and bulk snapshot capture for
generation/remeshing. The renderer consumes committed `WorldChunk::mesh` data
owned by the main-thread world state and does not repeatedly lock voxel storage
while drawing.

Public `World` voxel-query wrappers now acquire a shared lock, while internal
`WorldBlockQuery` helpers remain explicitly unlocked for callers that already
own the world lock, including the raycaster worker pool.

Adding RW locks around rendering would add synchronization overhead and would
not address the confirmed render costs. Keep OpenGL context ownership and mesh
commit ownership on the main thread.

The persistent raycast worker pool now takes one shared world read lock at the
public raycast boundary while its range jobs execute. Worker lookups continue
to use the internal unlocked query path, so the pool does not recursively lock
once per voxel. World streaming or edits therefore cannot mutate chunk storage
while a multi-range raycast is reading it.

World mutation entry points now take the matching exclusive lock while
updating chunk storage, revisions, edit history, or streaming state. The lock
is not taken by internal query helpers, so a public raycast acquires one read
lock and its persistent worker ranges remain lock-free at the voxel-lookup
level.

### 3.11 Resolved: GPU mesh identity when chunk slots are reused

The active renderer stores one `GpuChunkMesh` per fixed world-array slot and
currently decides whether to upload with the stored mesh identity. A newly
streamed chunk can reuse a slot whose previous chunk was evicted. The stream
commit path assigns the current world geometry revision to the replacement, so
the renderer cannot retain the previous chunk's VAO/EBO contents when the
coordinates and voxel revision happen to be equal.

The renderer then updates the slot's world X/Z offset to the replacement
coordinate. This can display valid geometry at the wrong location and can look
like a height error while collision still reads the correct live chunk.

The implemented fix is:

- make the upload key include chunk identity, such as
  `(chunk_x, chunk_z, voxel_revision, mesh_revision)`;
- store the uploaded identity in `GpuChunkMesh`;
- force an upload when coordinate or world-generation identity changes, even
  when the local mesh revision is numerically equal;
- invalidate the GPU entry on eviction and empty-mesh transitions;
- publish/update the world offset only with matching geometry.

The slot-reuse regression now evicts and regenerates the same coordinates and
verifies that the replacement has a new mesh identity and valid geometry before
any block edit occurs. Add a
cross-layer check comparing source mesh Y, collision surface Y, and rendered
vertex Y for the same world column. Do not add a global Y offset: chunks use a
documented local Y range starting at zero.

The Libft-side portion of this invariant is now covered by
`test_chunk_mesh_height_matches_generated_chunk_height`, which compares the
highest generated solid local Y with the mesh occupied maximum Y on a
deterministic flat chunk. The remaining graphics-context check must continue
to compare the uploaded chunk identity and rendered vertex Y at runtime in the
analytics/diagnostics build only.

The analytics renderer now performs that runtime check without adding work to
normal builds: it reports only chunk X/Z offset mismatches, uploaded identity
mismatches, or uploaded-versus-source Y-bound mismatches. The upload identity
contains chunk coordinates, voxel revision, and mesh revision. Normal builds
do not compile these diagnostics or their reporting code.

GPU memory accounting also now distinguishes invalidation from destruction:
evicting a chunk invalidates its uploaded identity while retaining the byte
count for still-allocated GL buffers; destruction clears that count after the
buffers are released.

The existing `--validate-visible-distance` validator now also checks the live
loaded chunks' world origins, highest solid local Y, mesh occupied maximum Y,
and representative collision surface. A successful run reported
`visible-distance: ok` with 377 loaded chunks; any mismatch prints the slot,
chunk identity, and measured values before failing.

### 3.12 Confirmed: generated, collision, and rendered height need one invariant

Generation, `WorldBlockQuery`, player grounding, collision, mesh vertices, and
GPU offsets must use the same vertical convention. Add diagnostics that report
the chunk coordinate, local/world column, top solid block Y, mesh occupied Y
range, player feet/eye Y, collision surface Y, mesh/voxel revisions, and GPU
uploaded identity. Diagnostics must distinguish stale geometry, wrong X/Z
offsets, and actual generation errors and must remain diagnostics-build only.

The collision validator now covers all tied-boundary combinations: X/Y, X/Z,
Y/Z, and the three-axis X/Y/Z corner. In each case an edge-touching voxel is
not reported as traversed, while the voxel entered after the tied boundary is
still detected. The no-hit assertion uses the raycast API's existing
`FT_ERR_INVALID_ARGUMENT` no-hit result and initializes output coordinates
before the call so a failure cannot be mistaken for a hit. The fixtures place
the test blocks above the generated terrain and choose the origin so the
edge-touching block is never the initial cell.

Fluid generation now performs a post-terrain support check before filling a
candidate pond column. The heightfield is only a candidate: caves, terrain
layers, and clamping can change the actual surface afterward. Water and
aquatic decorations are therefore skipped when the actual column-height block
is not solid, preventing floating water over an opening and decorations in an
unsupported column.

## 4. Optimization goals and non-goals

### Goals

- remove hidden diagnostics work when the overlay is disabled;
- reduce driver calls and CPU submission work without changing pixels;
- reduce p95/p99 frame spikes caused by result commits and edit bursts;
- preserve incremental mesh uploads based on `mesh_revision`;
- preserve chunk identity across slot reuse so geometry cannot be associated
  with the wrong world coordinate;
- prove that generated, collision, and rendered Y coordinates share one
  documented convention;
- establish measurements that distinguish update, commit, culling, upload,
  draw submission, GPU execution, presentation, and sleep;
- keep the design suitable for future lighting and shadow passes.

### Non-goals

- changing RW-lock scheduling or voxel semantics;
- moving OpenGL calls to arbitrary worker threads;
- changing chunk generation order or world determinism;
- weakening frustum correctness to gain misleading benchmark results;
- optimizing only average FPS while allowing worse tail latency;
- enabling the incomplete mega-buffer code without redesign.

## 5. Proposed implementation phases

### Phase 0: establish trustworthy measurements

Add low-overhead timing counters around these main-thread phases:

1. event and input processing;
2. player update and collision queries;
3. `World::update_around()`;
4. result polling and individual result commits;
5. deferred-edit application;
6. debug-data collection;
7. visibility collection and mesh synchronization;
8. solid submission;
9. water submission;
10. overlay/menu rendering;
11. `present()`;
12. explicit frame sleep.

Record count, total time, maximum time, and histogram/percentile-compatible
samples. Also record per frame:

- loaded and visible chunks;
- solid and water draw calls;
- triangles submitted;
- mesh uploads and bytes uploaded;
- generation results committed;
- deferred edits processed;
- time spent over the intended commit budget.

CPU timers do not measure asynchronous GPU completion. Add optional GPU timer
queries around world, water, and overlay passes where supported. Never call
`glFinish()` in normal benchmarking because it changes pipeline behavior.

### Phase 1: remove unconditional diagnostics work

Change `GameSession::render()` so it determines whether diagnostics are needed
before building them.

Required behavior:

```text
overlay disabled and revision preview hidden
    -> pass null debug pointer
    -> perform no RAM query, biome query, string formatting, or GPU-byte scan

overlay enabled
    -> build cheap per-frame fields
    -> refresh expensive system fields on a slower cadence

revision preview visible
    -> refresh preview only when its inputs change or at a bounded cadence
```

Recommended split:

- `build_render_debug_frame_fields()` for FPS, camera, selected block, and
  loaded/render-distance counters;
- `refresh_render_debug_system_fields()` at approximately 2-4 Hz for RAM and
  approximate VRAM;
- `refresh_revision_preview()` when center chunk, revision state, or preview
  visibility changes.

Cache the revision preview vector as a `GameSession` member to reuse capacity.
Do not cache raw pointers into world/chunk storage.

### Phase 2: eliminate redundant resize and viewport work

Make the outer GPU resize path return immediately when width and height are
unchanged. Keep one authoritative `glViewport()` update location:

- either set it only when dimensions change if no render target changes the
  viewport; or
- set it at render start and remove the additional resize-time call.

The second option is safer if future shadow maps or offscreen passes change the
viewport. The invariant should be: the world pass explicitly establishes its
required viewport once, not twice.

### Phase 3: improve visibility collection carefully

The active implementation now uses precomputed world-space frustum planes and
tests each chunk AABB against them. Keep the six-plane construction in
`RenderCache` covered by orientation, edge, and far-distance tests.

Maintain conservative behavior near the camera and far plane. A visible chunk
may be accepted unnecessarily, but a genuinely visible chunk must never be
rejected.

Only add cross-frame visible-list caching if Phase 0 shows collection is
material. Cache invalidation must include:

- camera position, yaw, and pitch;
- viewport/FOV changes;
- active render distance;
- chunk initialization and eviction;
- mesh transitions from empty to non-empty;
- world-center/index rebuilds.

A movement threshold is only safe with an expanded guard frustum. Without that
guard region, reusing visibility after camera movement can create popping.

### Phase 4: reduce draw submissions

If driver submission remains material, introduce deliberate batching rather
than reviving the incomplete mega-buffer path unchanged.

Preferred design:

- retain CPU chunk meshes and `mesh_revision` as the source of truth;
- allocate chunk geometry ranges in one or a small number of large GPU buffers;
- update only ranges belonging to changed chunks;
- retain separate solid and water command/range metadata;
- issue grouped draws using the best portable baseline supported by the
  project, with an optional multi-draw path when available;
- encode world position in vertices or per-draw data so one uniform update is
  not required for every chunk;
- compact or recycle ranges without rebuilding all geometry every frame.

Do not merge transparent water into the opaque pass. The current water pass
disables depth writes and enables alpha blending. Future correctness may
require back-to-front chunk or surface ordering, so batching metadata must keep
water independently sortable.

The implementation must handle chunk slot reuse. A GPU range cannot be treated
as valid solely because the array slot is the same; coordinate and mesh
revision/generation identity must match.

### Phase 5: bound world-result commit spikes

Separate result polling from commit scheduling and attach an estimated work
class to each result:

- generated chunk commit;
- remesh replacement;
- regeneration replacement;
- deferred edit batch;
- neighbor-remesh scheduling.

Use both a result-count limit and a time budget, but treat elapsed time as an
observation rather than a guarantee. Before expensive follow-up work, check
whether it can be deferred safely to the next frame.

Recommended rules:

- commit at least one ready result when progress is needed;
- stop after the first over-budget commit;
- never partially expose a chunk or mesh;
- coalesce dirty/remesh requests by chunk coordinate;
- avoid capturing multiple snapshots for the same chunk in one frame;
- retain queue backpressure so deferred work cannot grow without bound;
- expose queue depth and oldest-result age in diagnostics.

The generation pipeline now timestamps results when workers publish them and
reports the age of the oldest completed result still waiting for main-thread
commit. The async validator prints this value at its progress checkpoints.
This is distinct from candidate age: a high result age with low candidate age
identifies a commit/drain bottleneck, while a high candidate age with no
completed results points toward worker throughput or submission pressure.

Do not move final `WorldChunk` mesh replacement onto a worker without designing
a safe ownership transfer. The main thread must never draw a mesh while another
thread mutates or destroys its containers.

### Phase 6: optimize deferred edits and menus

For deferred edits:

- return immediately when no edits exist;
- reuse pending storage rather than allocating a temporary vector on every
  frame;
- group edits by chunk;
- apply all edits for a chunk under one write phase;
- increment revisions and queue remesh once per affected chunk;
- preserve deterministic conflict ordering by request and sequence.

The deferred applier now retains a touched-chunk list, applies the edits in its
existing deterministic order, and schedules at most one remesh request for
each affected loaded chunk after the batch. This avoids repeated snapshot
capture and remesh submission when a generated result contains several edits
for the same chunk. The list is cleared and reused between calls; it is not
part of the serialized world state.

Streamed and regenerated chunk commits now mark the affected chunk and its
loaded neighbors dirty without capturing their snapshots during the commit.
The existing bounded dirty-remesh submitter performs those captures and queues
the remesh work afterward, preserving the one-remesh-in-flight and queue-depth
limits. Player edits retain the immediate border-remesh path because their
interactive result must remain synchronous. This keeps the expensive snapshot
copy out of the generation-result commit while preserving ownership and
deterministic remesh ordering.

The dirty-remesh submitter now propagates snapshot and pipeline errors to the
stream update instead of discarding them. Queue saturation remains an expected
backpressure result, but allocation, invalid-state, and other failures are
reported so a chunk cannot silently remain dirty forever.

The queue also retains a processing cursor. A partial budgeted pass no longer
copies the entire unprocessed suffix into scratch storage. Newly appended
edits are sorted as an incremental suffix and merged with the already ordered
remainder; processed prefixes are compacted only after they become large
enough to justify the copy. This keeps backlog handling bounded without
changing the deterministic comparator or the serialized edit data.

For menus:

- mark the canvas dirty on scene entry, selection/input changes, settings
  changes, and animation ticks;
- rerasterize and upload only when dirty;
- continue drawing the cached texture each frame when presentation requires it.

The first-launch settings menu now follows this policy as well. It compares
the selected row and the three settings values after input handling, redraws
and uploads only when one changed (or on the initial frame), and presents the
cached overlay texture on unchanged frames. This removes repeated full-canvas
rasterization and texture upload while keeping input and window presentation
responsive.

### Phase 7: software renderer, only if required

If software mode has a performance requirement, profile it separately. Likely
work includes conservative frustum planes, face-group rejection, reduced
per-triangle copying, tiled rasterization, and possibly worker-owned tiles.

Parallel software rendering must partition color/depth ownership by tile or
use another race-free design. Multiple threads must not write the same depth
and color pixels without coordination.

The first software-renderer optimization is now implemented without changing
pixel ownership: the renderer caches the visible chunk-slot list while camera
pose, render target, render distance, loaded count, stream center, and world
geometry revision remain unchanged. Cached slots use a visible-only mesh path,
so chunk frustum culling is not repeated once by the outer renderer and again
by `ChunkMeshRenderer`. Any tracked input change rebuilds the list, preserving
the conservative culling and slot-reuse invalidation rules. This is a
single-threaded optimization; tile parallelism remains a separate change that
must first define race-free depth/color ownership.

## 6. Performance traps

- Do not cache visibility without complete invalidation; stale lists cause
  missing chunks or slot-reuse corruption.
- Do not rebuild a single mega-buffer whenever one chunk changes; that trades
  draw-call cost for large CPU copies and GPU uploads.
- Do not call `glBufferData()` for unchanged geometry.
- Do not use `glFinish()` to obtain convenient timings in production.
- Do not perform occlusion queries per chunk without considering query latency
  and CPU/GPU synchronization.
- Do not combine water with opaque geometry or discard water ordering needs.
- Do not update the debug overlay less frequently while still collecting all
  expensive source data every frame.
- Do not make the generation result budget so strict that visible chunks never
  become ready or worker queues remain permanently full.
- Do not count sleeping or swap-interval blocking as renderer CPU cost.
- Do not use a slot-local mesh revision as a complete identity; slots are
  reusable and revisions may restart at the same value.
- Do not compare headless software FPS directly with interactive GPU FPS.
- Do not optimize the software fallback at the expense of the normal GPU path
  unless both are explicit product requirements.

## 7. Validation plan

### 7.1 Correctness checks

For every phase run:

```sh
make -j2 all
make -j2 tests
make test
```

Add focused validation for:

- resizing repeatedly and rendering after zero/minimized dimensions;
- camera rotation at chunk/frustum boundaries with no popping;
- chunk eviction and slot reuse while moving quickly;
- mesh revision upload after block edits and generation results;
- solid-only, water-only, and mixed chunks;
- pond columns with caves, clamped low terrain, and nearby vegetation;
- water support invariants: every generated fluid column has a solid block
  immediately below it, unless an explicitly configured fluid source rule says
  otherwise;
- underwater tint and overlay behavior;
- settings/menu redraw after every input-driven visual change;
- revision preview refresh after selection and regeneration changes;
- deterministic world/edit results before and after commit scheduling changes.

Use framebuffer or screenshot comparisons for stable scenes. Allow no pixel
difference for diagnostic gating, resize cleanup, and equivalent culling.
Batching may produce harmless floating-point differences only if vertex world
position representation changes; define and justify any tolerance before
accepting it.

### 7.2 Performance scenarios

Measure release/optimized builds after a warm-up period. Run each scenario
multiple times and report median plus variation:

1. stationary camera, overlay off;
2. stationary camera, FPS overlay on;
3. revision preview visible;
4. slow movement through already-loaded chunks;
5. boosted movement causing generation, eviction, and uploads;
6. repeated block edits causing neighbor remeshes;
7. water-heavy view;
8. maximum supported render distance;
9. menu and in-game settings overlay;
10. software fallback at fixed resolution.

For interactive GPU runs record CPU frame time and GPU timer results separately.
Report average, median, p95, p99, maximum, one-percent-low FPS, draw counts,
mesh upload bytes, and commit overruns. Averages alone are insufficient.

### 7.3 Acceptance criteria

Phase 1 and Phase 2 are accepted when:

- hidden overlays perform no system-memory query or GPU-byte scan;
- visible diagnostics remain correct at their documented refresh cadence;
- unchanged window dimensions do not enter resize work;
- only one required world-pass viewport setup occurs per frame;
- no framebuffer differences or validator failures occur.

Visibility/batching work is accepted when:

- no visible chunk popping occurs in boundary and rapid-rotation tests;
- mesh updates and slot reuse never display stale geometry;
- draw calls or measured render CPU time decrease materially in high-distance
  scenarios;
- newly generated visible meshes catch up within the bounded upload policy
  without unbounded frame spikes or persistent missing terrain;
- p95 and p99 frame times do not regress;
- GPU memory remains bounded during long movement tests.

Commit-scheduling work is accepted when:

- p99/worst update spikes improve under generation and edit load;
- queue depth and oldest-result age remain bounded;
- loading makes continuous progress;
- deterministic world and edit validators remain unchanged.

Loading readiness is intentionally a small playable-area gate rather than a
barrier on the complete render-distance envelope. The center chunk and the
four directly adjacent chunks must be committed before gameplay starts;
remaining chunks continue through the persistent generation pipeline while
the player is already in the world. Readiness must count those exact
coordinates, not only `loaded_chunk_count`, because the latter can include
chunks retained from a previous stream center. A generation failure in the
playable area must remain observable as a loading error; distant failures are
retried and reported without blocking entry into the world.

Startup seeding follows the same rule: `World::initialize()` seeds only the
minimum playable radius. The configured render distance is activated when the
loading gate transitions to gameplay. Seeding the complete configured radius
before the gate was a startup backpressure bug: it filled the persistent
worker queue with distant chunks and delayed the chunks required to enter the
world. The normal and analytics GUI paths now reach the gameplay transition
without that full-distance startup burst; the headless world-generation probe
and the full validator suite remain regression checks for the same handoff.

The first post-loading GPU render audit found a separate hard hang in
`GpuGeometryBatch::collect()`: the storage-scan branch for an uninitialized or
empty chunk invalidated its GPU slot and continued without advancing
`scan_index`. Since unused storage follows the initial loaded chunks, the first
gameplay render revisited that slot forever. The scan is now a structured
bounded `for` loop, so loop progress cannot be omitted by a skip branch; every
skipped slot also updates the visited counter. A diagnostics-build startup
audit verifies that the scan reaches all 625 bounded slots, reports the visible
list, completes mesh synchronization, and completes both solid and water
passes. This is a correctness fix, not merely a timeout or a relaxed loading
gate.

The loading update uses the selected `RenderDistanceStrategy` to choose its
per-frame generation budget. It must not hard-code a larger loading budget than
the interactive policy: adaptive strategies reduce submissions when measured
frame time is high, while fixed strategies retain their deliberately
conservative budget. This keeps loading responsive on slower systems without
changing the persistent worker ownership model.

Each worker result now carries separate monotonic-clock durations for voxel
generation and mesh construction. These values stay attached to the result
through queueing and main-thread commit, so analytics can distinguish a slow
worker from a slow result transfer or commit. Analytics builds report the
request ID, chunk coordinate, and both durations when their combined time is
at least 8 ms; normal builds retain the fields for ABI-consistent result
ownership but emit no timing diagnostics. A queue-age sample and a worker
duration sample must be interpreted together: high queue age with low worker
time indicates commit/backpressure, while high worker time with an empty
completed queue identifies generation or meshing throughput as the bottleneck.

Verification on 2026-09-03 passed the rebuilt asynchronous-generation and
visible-distance validators after this loading change. The asynchronous test
requires the complete playable startup area and validates that every required
chunk is occupied, drawable, and has valid partition indices, including a
genuinely worker-generated neighbor; the visible-distance test accepted the full 160-block
envelope with 400 loaded chunks. Terrain determinism, cave generation, and
terrain-configuration checks also passed. These results validate the loading
handoff and geometry invariants, but do not yet establish a throughput target
for filling the entire distant render envelope.

The normal `ft_vox.exe` target also remained up-to-date after these validator
changes, and `git diff --check` reported no whitespace errors. This confirms
that the stronger readiness coverage and its diagnostics remain isolated from
the release runtime path.

The follow-up regression pass also independently completed the terrain
determinism, cave-generation, terrain-configuration, and strengthened
playable-area async checks. These are separate invocations, so a passing
async-generation result is not being used as evidence for the deterministic
terrain or cave invariants.

A matched-duration headless sample on the same date measured 21.88 FPS for the
normal executable and 19.28 FPS for the analytics executable with its exporter
disabled. The framebuffer hashes matched; p95 frame times were 76.40 ms and
75.20 ms, and p99 frame times were 86.20 ms and 82.55 ms respectively. This
single trial is evidence that the analytics-capable path still needs repeated
matched trials, not proof that analytics or rendering is the cause of the FPS
difference. Draw batching remains gated until repeated GPU-path measurements
show material submission cost and no tail-latency regression.

The required three-trial, ten-second follow-up was also completed with the same
headless workload. Normal trials measured 22.45, 22.42, and 22.19 FPS, for an
average of 22.35 FPS; analytics-without-exporter trials measured 15.44, 18.71,
and 18.41 FPS, for an average of 17.52 FPS. The corresponding average frame
times were 44.73 ms and 57.50 ms, average p95 times were 72.92 ms and 86.35
ms, and average p99 times were 81.36 ms and 108.20 ms. All six framebuffer
hashes were `f530308712e4b5c2`, so the workloads produced identical output.
This confirms a reproducible analytics-build overhead in this headless path;
it does not identify which instrumentation region causes it, and therefore the
next profiling pass must measure producer-side bookkeeping and lock contention
before any renderer rewrite.

## 8. Recommended order

1. Add phase timing and counters.
2. Gate and throttle debug-data collection.
3. Remove duplicate resize/viewport work.
4. Measure again.
5. Improve the conservative frustum test if collection is material.
6. Implement GPU range batching only if draw submission is still material.
7. Bound/coalesce result-commit and edit work.
8. Add menu dirty tracking.
9. Profile and optimize software rendering only if required.

This order produces useful low-risk gains first and prevents a large renderer
rewrite from being justified by assumptions instead of measurements.

## 9. Analytics-versus-renderer investigation gate

An analytics run is not comparable with a normal run until both executables
report the same rendering backend. The analytics build must emit a one-time
startup diagnostic containing:

- selected backend (`gpu` or `software`);
- effective window dimensions;
- whether GPU initialization succeeded or the application fell back to the
  software renderer.

This diagnostic is intentionally emitted once during window setup, never from
the frame loop. A software fallback can explain a very large FPS difference by
itself and must not be attributed to analytics instrumentation.

The startup line is emitted after GPU initialization succeeds, and includes
both the selected backend and the initialization result. This prevents a
requested GPU mode from being reported as GPU before initialization has
actually completed.

The current capture demonstrates why this gate is required: the largest region
was `software_meshes`, approximately 50--70 ms per frame, while the GPU
rendering regions were absent. That is evidence of a software-renderer capture,
not evidence that the exporter thread is consuming 50--70 ms. Future captures
must compare GPU-to-GPU or software-to-software runs at the same resolution.

Each comparison should also record, separately from game timings:

- analytics frame-finalization duration;
- number of pending scopes and trace events;
- dropped scope, frame, and trace counters;
- exporter queue depth and oldest queued buffer age;
- total frame time and renderer backend.

The analytics session now exposes completed-buffer queue depth and active
frame/trace counts. Minecraft prints these only when frame finalization exceeds
1 ms, alongside the existing drop counters. This makes exporter backpressure
visible without adding a lock or formatter call to ordinary frames. Libft now
timestamps each completed buffer at handoff and exposes the age of the oldest
queued buffer. Minecraft includes that value in slow-finalization diagnostics.
A non-zero age with a growing queue identifies exporter backpressure; a zero
age and no drops keeps the investigation focused on the renderer or
world-generation path.

If frame-finalization stays below the agreed budget and no drops or queue
backpressure occur, investigate the renderer or world-generation phase rather
than adding more analytics buffering. If finalization or queue pressure spikes,
capture a matched run with trace export disabled and then with instrumentation
disabled to isolate sampling, aggregation, and file-export costs.

The isolation sequence must additionally verify the backend line printed at
startup. Run the same executable and scene in these three modes:

1. normal analytics: producer recording and persistent exporter enabled;
2. `--analytics-no-exporter`: producer recording enabled, exporter disabled;
3. `--analytics-no-instrumentation`: analytics-capable executable with runtime
   recording disabled.

Compare only runs with the same `renderer_backend`, resolution, seed, input
schedule, render-distance schedule, and duration. A software capture compared
with a GPU capture is invalid evidence. If mode 3 remains slow while mode 1
and mode 2 are similar, the cause is in analytics-build-only code outside the
Libft producer/exporter path. If mode 2 is materially faster than mode 1,
investigate exporter queue pressure and file I/O. If mode 3 recovers the frame
rate, investigate producer scope volume or aggregation cost and retain the
thread-local fast path above.

## 10. Streamed-chunk and fluid-generation correctness follow-up

World storage slots are recycled during recentering. A pending GPU upload must
not leave the previous slot's geometry drawable at the new chunk coordinates.
The geometry batch therefore invalidates stale drawable counts when a chunk
identity or revision changes, and validates the persistent upload cursor using
chunk coordinates as well as the slot index. The visibility cache also tracks
loaded-count, stream-center, and index-validity changes so a newly committed
chunk cannot be hidden behind a stale visibility snapshot.

Procedural lake candidates are tied to terrain: a lake candidate below sea
level requires a deterministic above-sea-level neighbour rim. Explicit
lowland-flood configurations retain their existing behavior. Aquatic
decoration is checked against generated blocks after fluid placement: lily
pads require water below a replaceable target and seagrass requires an actual
water target. Ordinary shrubs and trees continue to require dry support.

Required regression coverage includes slot reuse while uploads are throttled,
streaming beyond the initial area, visibility-cache topology changes, lake
rim and unsupported-water checks, vegetation exclusion from water, and
deterministic results when neighbouring chunks arrive in different orders.
The water-column regression must inspect every water voxel and require its
immediate lower voxel to be water or solid, except for the intentional rooted
aquatic-plant case: if the lower voxel is seagrass, the voxel below the
seagrass must be solid. Checking only the first water voxel in a column is
insufficient to detect internal floating sections. Lily pads must remain
surface decorations over water, while ordinary shrubs and trees must continue
to require dry solid support.

The streamed-result commit path must transfer the complete mesh payload:
vertices, the compatibility index list, solid index ranges, water index ranges,
and occupied bounds. Dropping either partition during the worker-to-world move
causes a chunk to be present for collision and generation while its GPU pass
has no corresponding draw range; an edit can then appear to “fix” it by
triggering a later remesh. This is now enforced in the result mesh move helper.

The streamed commit also assigns the world geometry revision to the mesh
revision (with a non-zero fallback). Revisions must not reset to `1` whenever a
storage slot is reused: otherwise the same chunk coordinates can be paired
with the same voxel/mesh revisions as an older occupant, causing the GPU cache
to retain stale geometry until a block edit changes the revision. Renderer
identity tests must therefore cover eviction, regeneration of the same
coordinates, and upload before and after an edit. Using the world revision
rather than a request-local counter also remains safe across generation-pipeline
shutdown and reinitialization.

The initial and synchronous stream paths now use that same world-wide geometry
revision when publishing a chunk. They no longer depend on a storage slot's
local `mesh_revision` increment. This keeps synchronous loading, asynchronous
loading, regeneration, and slot reuse on one identity contract: every
published geometry state receives a new monotonic world revision, even when a
slot is reused for the same chunk coordinates before a renderer collection.

Remesh commits now prepare and validate a complete replacement mesh before
destroying the live mesh. Allocation or payload-transfer failure therefore
leaves the previously drawable geometry intact and returns the original error
to the result drain. The final main-thread replacement remains the only point
where the chunk mesh revision advances, so the GPU identity/upload path sees a
single coherent transition.

The async streaming validator must be invoked with
`--validate-async-generation`. An unrecognized validation flag falls through to
the normal application path and can look like a stalled validator. The async
validator now reports progress every 100 frames, logs update calls exceeding
one second, and reports stream queue counts when an update returns an error.
This distinguishes a real generation/commit stall from an incorrectly invoked
test or a process that is merely waiting for a window/application loop.

The Minecraft `automated_tests.exe` target is the application binary with
validator flags, not the Libft test executable. Running it without a recognized
`--validate-*` flag launches the normal window/application loop and can appear
to hang during loading. CI and local validation must pass an explicit validator
flag; the Libft module suite must be run from its own `Test/libft_tests` target.

The validator waits briefly between polls instead of busy-spinning. This is
important because the test itself must leave scheduling time for the persistent
generation workers; otherwise a fast host can make the worker-backed test look
like a world-generation hang. The wait is test-only and does not change the
runtime streaming loop.

Debug and analytics builds also emit a sampled commit-pipeline line every 120
stream frames. It records queued requests, completed results, results committed
on the main thread, and the age of the oldest completed result. A growing queue
identifies worker throughput or scheduling pressure; a growing completed count
with no commits identifies a main-thread drain or lock problem; an empty queue
with no playable progress points to candidate admission or worker failure. The
line is compile-gated and is absent from release builds. Loading diagnostics
also report the number of actively processing generation requests, so an empty
queue is not mistaken for idle workers that are still computing a chunk.

While the loading screen is active, its periodic stream line also reports the
drawable playable-area count as `drawable=current/required`. This separates
“the chunk objects exist” from “the renderer has valid geometry” and makes a
stall at the end of loading actionable: a fixed pending count with a growing
completed-result age indicates commit pressure, while a fixed drawable count
with no pending/completed progress indicates candidate or worker failure.
The same line includes the stream `progress` frame, which advances whenever a
chunk is published; an unchanged progress value across successive reports
distinguishes a stalled pipeline from a backlog that is still making progress.
Diagnostics builds also print up to four exact playable-area coordinates that
are still missing or non-drawable, plus the remaining gap count. This keeps
loading investigation independent of a debugger: missing coordinates point to
candidate admission/publication, while non-drawable coordinates point to mesh
construction or payload transfer.

For repeatable local diagnosis, both normal and analytics executables expose
`--worldgen-probe`. It starts the real persistent generation pipeline without a
window, advances it from the origin, prints loaded/pending/ready/drawable/
active-worker counters, and exits zero only after the complete minimum
playable ring has drawable meshes. It has a bounded iteration timeout and
returns nonzero with the last queue/error counters if progress stops. The
analytics probe opens a frame and world session around each update so its
measurements exercise the same valid analytics lifecycle as the game loop;
this avoids turning invalid-scope error logging into a false world-generation
stall. Run `ft_vox.exe --worldgen-probe` and
`ft_vox_analytics.exe --worldgen-probe --analytics-no-exporter` before using
GUI reproduction or a debugger.

The loading telemetry contract is now preserved through the complete API path:
the stream-diagnostics builder computes playable-required, playable-drawable,
and active-worker counts; `World::stream_diagnostics()` copies those fields
explicitly; and the loading screen reports them only in diagnostics builds.
Leaving fields out of this forwarding step would expose uninitialized values
and could falsely suggest that generation has stopped or completed. The
diagnostics mesh test also checks partition index bounds, matching the
interactive readiness gate.

The revision validator waits for its selected loaded chunk through the normal
streaming path, then cancels stale background requests and drains completed
stale results before submitting the revision job. This prevents unrelated
startup queue saturation from producing nondeterministic revision-test
failures.

The async-generation validation also checks the committed mesh payload itself:
the compatibility index list must equal the combined solid and water ranges,
and every transferred partition index must reference a transferred vertex.
This prevents a voxel-only test from accepting a chunk that is valid for
collision but has no drawable GPU ranges.
Its progress, slow-update, and failure lines also include the active worker
count, so a nonzero in-flight count remains visible even when the queued and
completed-result counts are both zero.

The synchronous visible-distance validator now prints phase checkpoints with
elapsed time. Its full-radius stream and slot-reuse passes intentionally create
large workloads; a quiet period before `full-stream-complete` or
`slot-reuse-complete` is therefore not sufficient evidence of a deadlock. The
phase markers distinguish slow synchronous generation from failures in culling,
mesh validation, or recentering. This instrumentation is test-only and does not
run in the game executable.

The visible-distance validator applies the same drawable-payload invariant
after initial streaming and after a one-chunk recenter. An occupied streamed
chunk must contain vertices and at least one solid or water index range; an
empty payload is reported with its storage slot and chunk coordinates.

The interactive loading gate now uses that same minimum invariant for the
playable area. A chunk counts toward entering gameplay only after it has
occupied bounds, vertices, and at least one valid solid or water index range;
the compatibility index count must match the two partition counts. This keeps
the loading phase from ending in the interval where collision can see a
published chunk but the renderer still has no drawable geometry.

`WorldChunk::mesh_is_drawable()` is now the shared implementation of this
predicate for the loading gate, stream telemetry, and asynchronous validation.
The validator retains its additional failure-specific messages, but the
accept/reject decision is no longer duplicated across those paths. This is
important for diagnosing an apparent end-of-generation stall: a reported
non-drawable chunk now means the same thing everywhere, including an invalid
partition index rather than only an empty mesh.

Analytics captures now also report, once per diagnostic sample, the identities
of up to four visible chunks whose GPU upload remains deferred after the
bounded upload pass. Each record includes the storage slot, chunk coordinates,
voxel and mesh revisions, vertex count, and solid/water index counts. A
non-zero `deferred_visible` count with stable revisions identifies upload
backlog; a missing chunk from this list points instead to streaming/index or
culling admission and must be investigated in those phases. The GPU visibility
collector treats the fixed chunk storage array as the authoritative drawable
set. The spatial index remains useful for world queries, but a streamed chunk
can be published between index-maintenance steps; rendering must not require a
later block edit to make that chunk drawable.

The same sample reports up to four occupied chunks that are within the active
render-distance envelope but are rejected by the culler, together with the
`nearby_culled` count. This is separate from upload backlog: a non-zero value
identifies a false-negative visibility decision, while a zero value combined
with missing geometry points to stream indexing or GPU upload state instead.

Interactive acceptance for streamed rendering requires a GPU analytics capture
with a fixed seed and movement across at least one cache-radius boundary. The
capture must record, for the same frame, the stream center, loaded/pending
counts, committed chunk coordinates, visible-list count, deferred-visible
count, upload count/bytes, and the framebuffer/backend label. Interpret the
result in this order:

1. A committed chunk absent from the visible list is a storage, coordinate, or
   culling admission failure.
2. A visible chunk reported as deferred across successive frames is an upload
   scheduler or GPU-transfer failure.
3. A visible chunk whose upload identity matches but is absent from the image
   is a draw-state, shader, depth, or camera-transform failure.
4. Water-only anomalies with valid chunk identity and draw ranges belong to
   water mesh ordering/material or generation validation, not streaming.

The headless validators prove the first category's data invariants and the
slot-reuse identity rule, but cannot prove the second or third category
without an active graphics context. Do not mark the interactive rendering
issue resolved from headless success alone.

The visible-distance validator now performs the same admission check without
an OpenGL context. After initial streaming and after a one-chunk recenter, it
builds the production `RenderCache` and verifies that every occupied chunk in
the horizontal render envelope is accepted by `MeshCuller`. A failure reports
the storage slot, chunk coordinates, world offset, camera position, and active
render distance. This catches coordinate and frustum regressions in CI; it
does not replace an interactive GPU capture, which is still required to prove
that an admitted chunk was uploaded and drawn.

## 11. Required architectural pivot: authoritative world service

The mandatory architectural change is a logically authoritative world service
which has no renderer ownership. Analytics and interactive testing show that
generation, halo-light snapshot capture, relighting, remeshing, and result
publication remain coupled closely enough that a block edit can take an
unacceptable amount of time to become visible. The immediate correction is to
establish clean ownership, bounded background work, and asynchronous
publication. Process separation alone does not prove or create that correction.

The world service must first run headlessly in the existing process as a
transitional integration mode. This proves that renderer coupling has actually
been removed before transport, encryption, and process lifecycle add new
variables. The intended final deployment moves that exact service behind the
same client/server protocol for single-player and multiplayer:

```text
single-player launcher
        |
        +-- starts a separate local server process
        |       |
        |       +-- owns authoritative world state
        |       +-- owns world generation and persistence
        |       +-- validates and applies gameplay actions
        |       +-- owns authoritative block/light revisions
        |       +-- publishes snapshots, deltas, and hashes
        |
        +-- starts/connects the client process
                |
                +-- owns a partial replicated world view
                +-- owns rendering, meshes, GPU resources, and UI
                +-- predicts only explicitly permitted presentation state
                +-- sends intents; never commits authoritative world state
```

In the final topology, the local server and client must not share mutable world
memory. Using the same
wire protocol, validation path, revision rules, and recovery logic in
single-player is a deliberate requirement. It prevents a separate multiplayer
implementation from drifting away from the behavior exercised every time the
game is played locally.

Process separation is not by itself a performance optimization and must not be
used to hide an unbounded or incorrectly prioritized pipeline. The server can
still consume every CPU core and starve rendering, or the client can still
apply an unbounded burst of snapshots and meshes. The implementation must
therefore retain bounded work on both sides:

- server generation and lighting use persistent sleeping workers and bounded
  node/job budgets;
- server network serialization uses immutable prepared payloads and bounded
  queues;
- client packet processing, delta application, remeshing, mesh publication,
  and GPU upload each have independent per-frame budgets;
- the client never waits synchronously for generation, lighting, persistence,
  or a server response while rendering a frame;
- backpressure delays low-priority work rather than allocating or executing an
  unbounded backlog.

The transitional in-process mode is not a second gameplay architecture. It
uses the same world-service API, command/result queues, immutable publications,
revision rules, and budgets as the final server. It may replace serialized
transport with an in-memory adapter solely while proving the boundary. Once the
minimum vertical slice below passes, single-player moves to the separate local
server executable and the transitional adapter is removed.

### 11.1 Module responsibility boundary

Libft Networking and Crypto provide the transport and security primitives.
They must not contain Minecraft-specific block rules.

Libft owns:

- encrypted authenticated connections through the existing message transport;
- reliable ordered, unreliable, and unreliable-sequenced delivery;
- fragmentation/reassembly, retransmission, flow control, priority lanes, and
  connection statistics;
- X25519 handshake, HKDF-SHA-256 key derivation, ChaCha20-Poly1305 AEAD,
  HMAC-SHA-256 retry/bootstrap authentication, replay protection, and secure
  random generation;
- loopback IPv4/IPv6 transport and the persistent networking worker;
- bounded message parsing and transport-level denial-of-service protection;
- deterministic impairment simulation for tests.

Minecraft owns:

- application message schemas and protocol versioning;
- world/chunk/block/light revisions;
- player identity, permissions, reach, collision, inventory, cooldown, game
  mode, and action legality;
- interest management and which chunks each client may receive;
- deterministic authoritative action ordering;
- chunk snapshot/delta serialization and canonical hashing;
- prediction, reconciliation, remeshing, and visual publication;
- persistence and world migration policy.

The validation boundary should accept game-owned policy callbacks or a
Minecraft validator object. Libft transports an authenticated request and
identifies its connection; it does not decide whether a player may break stone,
place water, fly, or edit a protected region.

### 11.2 Single-player local-server lifecycle

Add a dedicated headless server executable, for example `ft_vox_server`, built
from the same authoritative server code used by remote multiplayer. The client
launcher performs this sequence:

```text
create inherited bootstrap pipe
generate 256-bit one-use bootstrap secret with Crypto secure RNG
start ft_vox_server with inherited pipe handle and loopback-only mode
write bootstrap secret and requested world identifier through the pipe
wait asynchronously for READY { port, server_instance_id, protocol_version }
connect Libft message transport to 127.0.0.1/::1 on the assigned port
authenticate the first application handshake with the bootstrap secret
erase the secret after successful channel binding
enter the normal client connection state machine
```

Do not place the bootstrap secret in command-line arguments, logs, environment
variables, save files, or crash reports. Command lines and environments are
observable by unrelated local processes on several supported platforms. Use
the platform-neutral CrossProcess/System_utils process and inherited-handle
abstractions; add narrowly scoped platform backends only where those modules do
not yet expose the required primitive.

The server binds only to loopback in local mode and rejects non-loopback source
addresses. It chooses an ephemeral port to avoid stale-port collisions. The
client must use a bounded asynchronous startup deadline and report server
stderr/readiness diagnostics if startup fails. Rendering/menu processing must
continue while the launcher waits.

Normal shutdown is explicit:

```text
client -> SERVER_SHUTDOWN_REQUEST(local bootstrap owner only)
server stops accepting gameplay intents
server drains accepted authoritative mutations
server flushes persistence atomically
server sends SERVER_SHUTDOWN_COMPLETE
client closes transport and waits for the child with a bounded deadline
```

If the client crashes, the local server detects connection loss and follows the
configured policy: save and exit for ordinary single-player, or remain alive
for an explicitly detached/LAN-hosted session. If the server crashes, the
client leaves the world, preserves diagnostics, and never continues mutating a
now-unowned local replica.

### 11.3 Authoritative and replicated world models

The server stores the canonical current block state, generated/player-modified
provenance, biome/generator identity, authoritative light state when lighting
affects gameplay, and monotonically increasing revisions. It does not build
render meshes or own GPU objects.

The client stores only subscribed chunks and derived rendering data. Each
client chunk contains at least:

```cpp
struct client_chunk_replica
{
    chunk_coordinate coordinate;
    uint64_t block_revision;
    uint64_t light_revision;
    uint64_t snapshot_generation;
    block_storage blocks;
    light_storage light;
    provenance_storage provenance;
    mesh_state mesh;
    ft_bool snapshot_ready;
    ft_bool delta_gap;
};
```

The client may retain generated baselines or compact overrides as described by
the server-authoritative delta design, but its copy is never evidence that an
action is legal. Collision used for local movement prediction is provisional;
the server remains authoritative for final player position and interactions.

World generation should occur only on the server. The server sends complete
chunk snapshots for first subscription and compact current-state deltas after
that. The client builds meshes from replicated block/light data on persistent
client workers. A server must never send render vertices as authoritative
state: vertex formats are renderer/backend details and are substantially larger
than canonical world data.

### 11.4 Application protocol over Libft Networking

Define a Minecraft application envelope inside the encrypted Libft message:

```cpp
struct minecraft_message_header
{
    uint16_t protocol_version;
    uint16_t message_type;
    uint32_t payload_length;
    uint64_t server_instance_id;
    uint64_t session_id;
    uint64_t message_sequence;
};
```

Serialize fields explicitly in network byte order. Never cast packet bytes to
the structure. Include the application header as authenticated payload; the
Libft transport already authenticates its own connection/packet envelope.
Reject unknown required versions and length violations before allocation.

Required application messages:

| Message | Direction | Delivery | Purpose |
| --- | --- | --- | --- |
| `CLIENT_CAPABILITIES` | client to server | reliable/control | protocol, generator, compression, and feature negotiation |
| `SERVER_SESSION` | server to client | reliable/control | session identity, tick rate, world identity, and ruleset digest |
| `INTEREST_UPDATE` | client to server | unreliable-sequenced | camera/player center and requested view radius |
| `CHUNK_MANIFEST` | server to client | reliable/world | subscribed coordinates and authoritative revisions |
| `CHUNK_SNAPSHOT` | server to client | reliable/snapshot | canonical full chunk at a declared revision |
| `CHUNK_BLOCK_DELTA` | server to client | reliable/delta | ordered current-state block/provenance changes |
| `CHUNK_LIGHT_DELTA` | server to client | reliable/delta | asynchronous light changes tied to a source block revision |
| `EDIT_INTENT` | client to server | reliable/action | requested block interaction, never a committed edit |
| `EDIT_RESULT` | server to client | reliable/action | accepted/rejected result and canonical revision/value |
| `PLAYER_INPUT` | client to server | unreliable-sequenced | movement/input sequence for server simulation |
| `PLAYER_STATE` | server to client | unreliable-sequenced | authoritative movement state and acknowledgement |
| `CHUNK_HASH_MANIFEST` | server to client | reliable/control | periodic canonical section/chunk hashes |
| `CHUNK_REPAIR_REQUEST` | client to server | reliable/control | request sections or full snapshot after mismatch/gap |
| `CHUNK_REPAIR_RESPONSE` | server to client | reliable/snapshot | transactional repair payload |
| `CHUNK_SYNC_REQUEST` | client to server | reliable/control | request bounded block/light replay from client cursors |
| `DELTA_ACK` | client to server | reliable/control | highest contiguous applied revision per chunk |
| `RESYNC_REQUIRED` | server to client | reliable/control | retained delta history cannot satisfy the client |

Recommended Libft channels/lanes:

```text
lane 0, channel 0: connection control, rejection, shutdown, hash/repair control
lane 1, channel 1: edit intents/results and authoritative gameplay deltas
lane 2, channel 2: chunk snapshots and bulk repair data
lane 3, channel 3: low-priority manifests/observability

unreliable-sequenced: player input, player transforms, interest updates
reliable-ordered: world deltas, edit results, snapshots, repair, lifecycle
```

Do not put a large snapshot ahead of edit results on the same reliable ordering
channel. Separate channels avoid application-level head-of-line blocking while
Libft lanes provide transport scheduling priority. Observe `FT_ERR_FULL` as
backpressure; retain or regenerate the immutable application message and retry
later. Never treat queue saturation as successful delivery.

The first implementation should expose interfaces with responsibilities close
to the following. Exact names may follow the repository's eventual Minecraft
module layout, but ownership and error semantics must remain intact:

```cpp
class minecraft_server_runtime
{
  public:
    int32_t initialize(const server_config &configuration) noexcept;
    int32_t listen(const networking_message_endpoint &endpoint) noexcept;
    int32_t advance_tick(uint64_t tick, const server_tick_budget &budget)
        noexcept;
    int32_t request_shutdown() noexcept;
    int32_t destroy() noexcept;
};

class minecraft_client_connection
{
  public:
    int32_t initialize(const client_connection_config &configuration) noexcept;
    int32_t connect(const networking_message_endpoint &endpoint) noexcept;
    int32_t send_interest(const interest_update &interest) noexcept;
    int32_t send_edit_intent(const edit_intent &intent,
        uint64_t *request_id) noexcept;
    int32_t poll_events(uint32_t maximum_events) noexcept;
    int32_t destroy() noexcept;
};

class server_world_replication
{
  public:
    int32_t subscribe(connection_id client, const interest_region &region)
        noexcept;
    int32_t validate_and_apply(connection_id client,
        const edit_intent &intent, edit_result &result) noexcept;
    int32_t build_pending_deltas(connection_id client,
        const replication_budget &budget) noexcept;
    int32_t acknowledge(connection_id client,
        const chunk_revision_ack &acknowledgement) noexcept;
    int32_t request_repair(connection_id client,
        const chunk_repair_request &request) noexcept;
};

class client_world_replication
{
  public:
    int32_t apply_manifest(const chunk_manifest &manifest) noexcept;
    int32_t apply_snapshot(const chunk_snapshot &snapshot) noexcept;
    int32_t apply_block_delta(const chunk_block_delta &delta) noexcept;
    int32_t apply_light_delta(const chunk_light_delta &delta) noexcept;
    int32_t apply_edit_result(const edit_result &result) noexcept;
    int32_t compare_hashes(const chunk_hash_manifest &manifest,
        ft_vector<chunk_repair_request> &requests) noexcept;
    int32_t advance_derived_work(const client_world_budget &budget) noexcept;
};
```

Every parser and state-changing API returns a meaningful Libft error code.
Receiving code first parses into a temporary validated message, then submits it
to the owning state machine. Parsing must never mutate a live world/session.
Sending APIs copy or transfer ownership of immutable payloads before returning;
callers must know whether data was accepted, rejected, or blocked by
backpressure.

The application protocol should be layered over the currently documented
Networking flow approximately as follows:

```cpp
networking_message_transport transport;
networking_message_transport_config transport_configuration;
networking_udp_datagram_io datagram_io;

FT_TRY(transport.initialize(transport_configuration, datagram_io));
FT_TRY(transport.start_worker());

// Server:
FT_TRY(transport.listen(loopback_or_public_endpoint));
// Accept only after bootstrap/ticket/application identity validation.

// Client:
networking_message_connection connection;
FT_TRY(transport.connect(server_endpoint, connection));

// Application messages use send_message with the lane/channel contract above.
// The owning thread drains deferred callbacks/events; transport workers never
// call world or renderer code directly.
```

`FT_TRY` in pseudocode means “check and propagate the return code.” It is not a
requirement to add a macro, exception, or hidden return-value discard.

### 11.5 Server tick and action-validation pipeline

All client world changes are intents. The authenticated connection determines
the acting player; a player ID carried inside an untrusted payload is never
authoritative.

```cpp
int32_t authoritative_server::tick(uint64_t tick)
{
    drain_network_commands_bounded();
    expire_disconnected_sessions();
    update_interest_sets_bounded();

    while (accepted_intent_budget_remaining())
    {
        edit_intent intent;
        authenticated_session *session;

        if (next_intent(intent, session) != FT_ERR_SUCCESS)
            break ;
        validation_result validation = validate_edit(*session, intent);
        if (validation.error_code != FT_ERR_SUCCESS)
        {
            queue_rejection(*session, intent, validation);
            continue ;
        }
        apply_edit_transactionally(*session, intent, tick);
    }

    advance_generation_workers_bounded();
    advance_lighting_workers_bounded();
    publish_completed_world_work_bounded();
    build_and_fanout_delta_batches_bounded();
    schedule_periodic_hash_manifests_bounded();
    enqueue_persistence_snapshots_bounded();
    return (FT_ERR_SUCCESS);
}
```

`validate_edit` must check at least:

- authenticated session and current player entity;
- finite, in-range coordinates with overflow-safe chunk/local conversion;
- chunk loaded and at the expected world/generator generation;
- requested block ID exists and is permitted in the current ruleset;
- authoritative current block matches any optional client precondition;
- authoritative raycast/reach and line of sight;
- player game mode, permission/claim/region restrictions;
- inventory ownership and item/tool requirements;
- cooldown, action rate, per-tick, per-client, and per-chunk limits;
- collision and placement occupancy rules;
- request ID is new, or is an exact idempotent duplicate of a retained result.

Each authenticated session keeps a bounded request/result ledger. A retry with
the same request ID is compared field-for-field with the original intent. An
exact match resends the cached result and, for an accepted edit, the cached
authoritative delta without calling world mutation again. Reusing a request ID
with different coordinates, preconditions, or requested value is rejected as
an invalid/forged request. The ledger is session-scoped, is cleared when the
session ends, and its capacity/eviction policy must be large enough for the
transport retry window; an evicted request must require a fresh client request
or snapshot reconciliation rather than being applied speculatively.

Prepare all fallible allocations before mutating the world. The authoritative
in-memory transaction commits block value, provenance, block revision,
dirty-persistence state, lighting invalidation, and owned immutable work records
for replication and persistence together. If any preparation fails, no part of
the authoritative state changes. This transaction must not perform synchronous
disk I/O while holding the chunk lock. A committed persistence record means an
owned record has entered a bounded persistence queue; durability is reported
separately after the persistence worker flushes it.

#### Staged block-edit and lighting publication

Breaking or placing a block is deliberately completed in stages. The client
does not wait for lighting to finish before learning whether the edit was
accepted:

For a block break specifically, the interaction is a request/confirmation
sequence rather than an immediate client-owned world mutation. The client may
show a lightweight predicted break state (for example, a crack animation or a
temporarily hidden block), but it must retain the previous authoritative block
value until the server result arrives. This prevents a rejected break from
permanently changing the replica and makes retries/reconciliation explicit.

```text
client sends EDIT_INTENT
        |
        v
server validates the intent against authoritative state
        |
        +-- rejected --> EDIT_RESULT(rejected, reason, current revision)
        |
        +-- accepted --> atomically commit block/provenance state
                         increment block revision exactly once
                         send EDIT_RESULT(accepted, canonical value/revision)
                         publish CHUNK_BLOCK_DELTA to the requester and peers
                         enqueue lighting invalidation
                                      |
                                      v
                         bounded server lighting worker recomputes affected area
                                      |
                                      v
                         send CHUNK_LIGHT_DELTA(source_block_revision=R)
```

The accepted result and block delta are the first authoritative publication.
The server updates its authoritative block state at commit time. The requesting
client then applies the canonical accepted value and revision, clears its
pending break/place request, and schedules the affected region for remeshing.
Other interested clients apply the same ordered block delta. If the result is
rejected, the requester removes its prediction and restores the last confirmed
block value; it must not invent a local replacement. Persistence and
replication records are prepared from that same committed state, so a failure
cannot report success while losing the edit.

The server must enter an accepted block delta into its retained journal before
depending on any individual network send succeeding. It should then attempt
the requester result, the source client's delta, and peer fanout independently,
returning the first error while retaining the authoritative delta for replay or
snapshot recovery. A full outgoing queue is therefore a delivery delay, not a
reason to roll back the world or discard the delta.

Lighting is a later derived publication. It runs under a bounded work budget,
may span multiple ticks, and never delays the edit result or block delta. A
light result records the block revision from which it was calculated. The
client applies it only when that `source_block_revision` still matches its
current block revision for the chunk; otherwise it discards the stale result
and waits for a newer light computation. The client may temporarily display
the previous light values, but it must never keep the old block logically
present merely because lighting or remeshing is incomplete.

This ordering is intentional:

```text
break request
    -> server validation
    -> authoritative block update (or rejection)
    -> EDIT_RESULT + CHUNK_BLOCK_DELTA
    -> client/server block state converges
    -> asynchronous lighting propagation
    -> CHUNK_LIGHT_DELTA tied to the accepted block revision
    -> bounded client light application and remesh publication
```

Lighting must therefore be treated as a derived, eventually consistent view
of the already-authoritative block state. A delayed, dropped, or stale light
message may cause lighting to settle later, but it may not delay or undo the
block result. If a newer block revision arrives before lighting finishes, the
server cancels or supersedes the old lighting job and emits a result tagged
with the newer source revision.

The same separation applies on the client: authoritative block deltas are
decoded and applied within the per-frame message/byte/operation budget, while
lighting application, remeshing, mesh publication, and GPU upload use their
own bounded asynchronous queues. A large light delta or complete mesh rebuild
must be deferred across frames rather than monopolising the render thread.

```cpp
int32_t authoritative_world::apply_edit_transactionally(
    const validated_edit &edit, authoritative_delta &out_delta)
{
    prepared_delta prepared;
    prepared_persistence_record persistence;

    FT_TRY(prepare_delta(edit, prepared));
    FT_TRY(prepare_persistence(edit, persistence));

    chunk_write_guard guard(edit.chunk);
    if (!precondition_still_matches(edit))
        return (FT_ERR_STALE_REVISION);

    uint64_t next_revision = checked_increment(edit.chunk.block_revision);
    commit_block_and_provenance(edit, next_revision);
    commit_prepared_persistence_work(persistence);
    commit_prepared_delta(prepared, next_revision, out_delta);
    enqueue_lighting_invalidation(edit.position, next_revision);
    return (FT_ERR_SUCCESS);
}
```

The pseudocode names behavior rather than requiring C++ exceptions, RAII, or
the shown helper macro. The implementation must follow Libft/Minecraft error
and lifecycle rules and must not discard meaningful return codes.

### 11.6 Delta model, acknowledgement, and interest management

Every authoritative chunk has independent, strictly increasing block and light
revisions. Do not combine asynchronous lighting changes with block revisions.
Block deltas declare a contiguous block-revision range:

```cpp
struct chunk_block_delta
{
    chunk_coordinate coordinate;
    uint64_t base_block_revision;
    uint64_t final_block_revision;
    uint64_t snapshot_generation;
    ft_vector<block_value_delta> block_changes;
    ft_vector<provenance_value_delta> provenance_changes;
};

struct chunk_light_delta
{
    chunk_coordinate coordinate;
    uint64_t base_light_revision;
    uint64_t final_light_revision;
    uint64_t source_block_revision;
    uint64_t snapshot_generation;
    ft_vector<light_value_delta> changes;
};
```

Applying a block delta is valid only when the client has exactly
`base_block_revision` and the snapshot generation matches. Duplicate deltas
whose final revision is already applied are acknowledged and ignored. A future
base revision creates a gap and triggers snapshot recovery; an older overlapping
delta is rejected unless an explicit overlap parser is implemented and tested.

A light delta additionally applies only when its light revision is contiguous
and its `source_block_revision` still equals the replica's block revision. A
light result calculated from obsolete blocks is discarded and the current
region remains dirty for relighting. Block replication must never wait for a
matching light delta.

The server retains a bounded per-chunk recent-delta ring until all interested
clients acknowledge or retention expires. If the requested base revision is no
longer retained, send `RESYNC_REQUIRED` followed by a fresh snapshot. Never
silently skip a gap.

Interest is server-authoritative. The client reports a desired center/radius,
but the server clamps it by rules, permissions, bandwidth, and configured
maximums. On entry, the server captures a snapshot at revision R, records the
subscription, then sends deltas R+1 onward. On exit, it stops broadcasts and
eventually tells the client to release the replica. Snapshot capture and
subscription registration must be ordered so no edit can fall between them.

Coalesce multiple edits to the same block within one unsent batch to the final
current value while preserving the final revision and required provenance.
This engine needs current-state replication, not an unbounded player-edit
history. Persistence and optional replay/audit logging are separate consumers.

### 11.7 Client application, prediction, and reconciliation

The networking worker receives/decrypts messages into bounded immutable queues.
It never calls renderer or world-replica code directly. At frame start, the
client applies a bounded amount of authoritative data:

```cpp
int32_t replicated_world::advance_frame(const replication_budget &budget)
{
    drain_control_messages(budget.control_messages);
    apply_chunk_snapshots(budget.snapshot_bytes);
    apply_block_deltas(budget.delta_operations);
    apply_light_deltas(budget.light_operations);
    schedule_dirty_light_and_mesh_regions(budget.scheduler_operations);
    publish_completed_meshes(budget.mesh_commits);
    return (FT_ERR_SUCCESS);
}
```

A block interaction may show an immediate cursor/animation/sound and optionally
a clearly tracked predicted block overlay. It must not overwrite the canonical
replica. When `EDIT_RESULT`/`CHUNK_BLOCK_DELTA` arrives:

- accepted and matching prediction: remove the overlay and apply canonical
  revision/value;
- accepted but different canonical value/revision: replace the overlay and
  schedule derived lighting/mesh work;
- rejected: remove the overlay and restore authoritative presentation;
- timeout: retain a pending indicator or cancel prediction; do not invent an
  authoritative success.

The server should return `EDIT_RESULT` immediately after committing the block,
without waiting for lighting, persistence flush, or every interested client.
The block delta can be broadcast in the same server tick. Light changes may
follow under a separate `light_revision` when the bounded server lighting job
completes. The client must never wait half an hour to learn that the block is
gone merely because derived lighting is delayed.

Block-state replication and derived visual work must be decoupled:

```text
authoritative block delta arrives
        |
        +-- update collision/query replica immediately
        +-- mark bounded light region dirty
        +-- mark bounded mesh region dirty
        +-- retain old mesh until replacement is ready
        +-- publish replacement mesh under block/light revision guards
```

This rule applies inside the client just as strictly as it applies between
the server and client. Applying a giant light delta, copying an unbounded
snapshot, or rebuilding a complete mesh on the render thread would simply
move the hitch from world generation to replication. Client networking must
therefore enqueue received bytes into bounded ownership-transferred queues;
the frame loop may apply only a configured operation/byte budget. Block data
needed for collision and immediate queries is applied first, while lighting
propagation, remeshing, mesh validation, and GPU upload remain asynchronous.
Completed meshes are published through an atomic ownership handoff and are
accepted only when their chunk coordinate, snapshot generation, block
revision, and light revision still match the replica. An old mesh may remain
visible briefly while derived work settles, but the logical block state and
collision state must update immediately. This makes delayed lighting a visual
settling effect rather than a gameplay or frame-rate stall.

The initial Libft Networking layer provides the transport-neutral envelope
and sender/decoder primitives for this boundary. It copies received payloads
out of transport-owned storage and validates the complete frame before
committing it to an application-owned buffer. Minecraft remains responsible
for bounded queues, prioritisation, replica mutation, remesh scheduling, and
render-thread publication.

The transport pump must preserve ownership whenever the selected endpoint
cannot accept a received message, whether the endpoint is the client ingress
queue or the server session. It may retain one complete received message as a
deferred pending item and retry it before receiving another transport message;
it must not destroy a reliable message merely because the frame budget,
application queue, or server send queue is temporarily exhausted. The pending
slot is role-neutral: a server-side `FT_ERR_FULL` is retained in exactly the
same way as a client-side `FT_ERR_FULL`. Once the pending item is accepted by
the endpoint, the pump may resume receiving. A persistent overflow beyond that
retained item is reported as backpressure and must be handled by the
connection/session policy, never treated as successful delivery. Server
dispatch must remain idempotent while such a message is retried; request IDs
and cached authoritative results are therefore part of the retry contract.

### 11.8 Canonical hashes and repair

Hashes are consistency and recovery tools, not permission checks. The server
never trusts a client merely because it reports the expected hash, and a client
cannot use its local hash to authorize an action.

Define a canonical byte representation independent of native structure layout,
endianness, padding, pointer values, container capacity, mesh data, and process
identity. Content hashes and synchronization metadata have different meanings
and must remain separate. The content hash must not include revision counters;
the manifest carries revisions alongside the hash. At minimum hash:

```text
domain tag: "FTVOX-CHUNK-HASH-V1"
world identity and generator/configuration digest
chunk x/z
canonical block IDs in fixed coordinate order
player-modified/provenance mask in fixed order
canonical authoritative light values when server-owned
```

Use Libft Crypto SHA-256. Do not invent a faster non-cryptographic hash for the
authoritative reconciliation contract. Cache hashes by revision so unchanged
chunks are not rehashed every verification interval.

Send `snapshot_generation`, `block_revision`, and `light_revision` as manifest
metadata. Equal content with different history can then have an equal content
hash while still requiring revision-aware protocol handling. Use section hashes
plus a chunk root:

```text
section_hash[y] = SHA256(section domain || metadata || canonical section bytes)
chunk_root = SHA256(chunk domain || ordered section_hash[0..15])
```

This allows repair of one mismatched section without resending an entire chunk.
The server periodically sends manifests for a bounded rotating subset of the
client's interest set, prioritizing recently edited chunks. The client compares
only when it has applied through the declared revision. A mismatch produces a
`CHUNK_REPAIR_REQUEST` containing coordinate, local revision, root, and the
requested section indices. The response is applied transactionally to a
temporary replica and committed only after lengths, IDs, revisions, and SHA-256
all validate.

Hash timing must be configurable by interval and maximum bytes/chunks per tick.
Never hash every loaded chunk every frame. Hash calculation belongs on server
and client workers over immutable snapshots; only revision-checked publication
touches live state.

### 11.9 Failure, security, and abuse handling

- Bound every count and byte length before allocating.
- Limit incomplete snapshot/reassembly memory per connection in addition to
  Libft transport limits.
- Authenticate before accepting world requests.
- Apply replay/idempotency checks at both transport packet and application
  request-ID levels.
- Rate-limit invalid edits separately from valid gameplay traffic.
- Disconnect repeated malformed/cryptographically invalid senders without
  allowing unlimited logs.
- Never expose save paths, memory addresses, secrets, stack traces, or private
  server diagnostics to a remote client.
- Preserve old authoritative state if snapshot/delta/hash construction fails.
- On `FT_ERR_FULL`, retain bounded pending work or explicitly resync; never drop
  a reliable authoritative delta silently.
- Ensure shutdown wakes every persistent worker and does not hold a world lock
  while joining networking, generation, lighting, persistence, or hash workers.
- Keep test failure injection and impairment hooks out of release archives.

The server should maintain connection-level and world-replication metrics:
accepted/rejected intents, rejection reason counts, delta bytes, snapshot bytes,
ack lag, revision gaps, repairs, hash mismatches, queue depth, backpressure,
generation/light work, and persistence latency. Analytics export must remain
off the authoritative mutation lock path.

### 11.10 Player-visible scheduling priority

Bounded queues are insufficient when urgent work remains behind thousands of
valid distant jobs. Every server and client scheduler must use stable priority
classes, bounded starvation prevention, and revision-based stale-work removal.
The default order is:

1. accepted local edits and directly affected neighbouring regions;
2. collision-affecting changes near any player;
3. lighting propagation near visible modified blocks;
4. meshes intersecting or immediately entering the camera view;
5. nearby generation required by current movement;
6. ordinary visible-region generation and meshes;
7. distant generation, persistence compaction, and hash verification.

Lighting begins at invalidated blocks and propagates outward incrementally until
no value changes. Proximity may determine which frontier node is processed next,
but must not alter the deterministic final light state. Promotion/aging prevents
continuous local edits from permanently starving lower classes. Completed work
must carry source revisions so publication can cheaply discard stale results.

### 11.11 Minimum vertical slice

Before implementing periodic hashes, targeted section repair, full capability
negotiation, provenance replication, or remote abuse hardening, Luna must deliver
and measure this end-to-end slice:

1. start the authoritative world service headlessly in-process;
2. connect one client through the in-memory protocol adapter;
3. subscribe to a small chunk area and receive snapshots;
4. break and place one block by sending an intent;
5. validate and commit it on the service;
6. return an authoritative result and contiguous block delta immediately;
7. update collision/query state, then remesh the affected region independently;
8. save through the asynchronous persistence queue and shut down cleanly.

The slice passes only if block-visible latency stays within a configured bound
while lighting and generation are deliberately backlogged. Next, replace the
adapter with Libft Networking, launch the same service as `ft_vox_server`, and
repeat the identical tests. Crypto bootstrap and advanced reconciliation follow
without changing world-service behavior.

### 11.12 Implementation sequence for Luna

Implement this migration in reviewable phases. Do not delete the current local
world path until the server path passes equivalent tests.

1. **Freeze service and revision invariants.** Document canonical coordinates,
   block/light revisions, generation identity, provenance, maximum sizes, and
   error codes. Add deterministic serialization fixtures.
2. **Extract authoritative services in-process.** Move generation, block mutation,
   validation, lighting invalidation, and persistence behind server-owned
   interfaces that can run headlessly without renderer headers.
3. **Pass the minimum vertical slice.** Use bounded in-memory command/result
   queues and prove prompt block publication independently of lighting.
4. **Build `ft_vox_server`.** Add explicit initialize/run/stop/destroy lifecycle,
   loopback listen mode, persistent worker ownership, bounded ticks, and clean
   shutdown. It must run independently before a client exists.
   The first owner may be a headless `WorldReplicationServerRuntime` around a
   caller-owned Libft transport; it must not include renderer headers or call
   renderer code from transport/worker paths.
5. **Add the minimum application protocol.** Implement session, interest,
   snapshots, block deltas, light deltas, edit intent/result, acknowledgements,
   and snapshot fallback over Libft message transport.
   Expose separate peer-admission paths: the early in-process path may use a
   test adapter, while production admission must require a connected Libft
   session whose authenticated peer identity has been verified.
6. **Create the client replica.** Separate replicated canonical blocks/light
   from derived meshes/GPU state. Apply snapshots and deltas transactionally
   under per-frame budgets.
   Use a client runtime owner that drains transport ingress separately from
   frame-budgeted replica application; callbacks must publish only immutable
   application data and must not execute renderer work on the transport path.
7. **Route all single-player edits through networking.** Remove direct client
   calls to authoritative `place_block_at`/`delete_block_at`; retain optional
   presentation prediction only. Prove the server receives, validates, commits,
   acknowledges, and broadcasts each accepted edit.
8. **Implement interest and fanout.** Snapshot-at-revision subscription, ordered
   post-snapshot deltas, bounded recent-delta retention, acknowledgement, and
   snapshot fallback.
9. **Add secure local bootstrap and full negotiation.** Start the child process
   through Libft process utilities, pass a one-use secret over an inherited
   pipe, bind it to the encrypted connection, and test timeout/crash cleanup.
10. **Implement SHA-256 reconciliation as resilience work.** Add canonical
    content hashes, revision metadata, targeted repair, full-snapshot fallback,
    and configurable rotating verification budgets. This must not block the
    first playable client/server path.
11. **Move and tune derived work.** Server generation/lighting and client
    light/mesh construction use persistent sleeping workers and the priority
    classes in section 11.10.
12. **Migrate menus and launch flow.** Single-player creates/connects a local
    server; multiplayer connects to a remote server through the same session
    state machine. Loading UI reports server, snapshot, replica, mesh, and GPU
    readiness separately.
13. **Remove compatibility ownership.** Only after all gates pass, remove direct
    authoritative world mutation from the client and archive obsolete in-process
    synchronization code.

### 11.13 Current implementation status

The first Libft Networking foundation is now implemented in
`Modules/Networking/networking_replication_protocol.*`. It provides:

- a versioned, length-checked, transactional application envelope;
- a transactional receive decoder that copies payloads out of transport-owned
  storage before handing them to an application;
- bounded payload validation before the transport sees a message;
- reliable control, reliable delta, reliable snapshot, and
  unreliable-sequenced sender helpers mapped to separate lanes/channels;
- a caller-owned revision tracker for one replicated stream that requires a
  snapshot before accepting deltas, enforces contiguous block/light revisions,
  and rejects light results derived from an obsolete block revision;
- a transactional per-pump message, payload-byte, and operation budget so a
  client can defer excess replication work without dropping authoritative
  state;
- a caller-owned client replica gate with snapshot, block-delta, and
  light-delta callbacks; callbacks run before revision advancement, so failed
  application and exhausted budgets remain retryable;
- a contiguous revision-retention window with acknowledgement tracking and
  explicit snapshot fallback when a client falls behind the retained range;
- a SHA-256 payload-content helper that keeps canonical content hashing
  separate from revision metadata, leaving application-specific chunk
  canonicalization in Minecraft;
- a Game-module block intent/delta coordinator with expected-revision and
  expected-block validation, bounded recent request/result idempotency,
  bounded recent delta history, interest/snapshot handoff, checksummed chunk
  snapshots, and
  transactional decoder cursor behavior;
- a Minecraft `WorldReplicationServerRuntime` that owns the replication service,
  peer registry, and transport pump with explicit listen, worker, pump, and
  shutdown lifecycle; secure peer admission is exposed separately from the
  transitional test adapter;
- a Minecraft `WorldReplicationClientRuntime` that owns client replication and
  transport pumping, while exposing a separate per-frame bounded drain so
  transport reception cannot perform unbounded world, lighting, mesh, or GPU
  work;
- transactional Minecraft codecs for `CHUNK_HASH_MANIFEST` and
  `CHUNK_REPAIR_REQUEST`, carrying content hashes separately from block/light
  revisions and allowing section-scoped repair requests;
- a transactional `CHUNK_REPAIR_RESPONSE` codec carrying the authoritative
  repaired revision metadata, section mask, content hash, and bounded opaque
  repair payload;
- a transactional `CHUNK_SYNC_REQUEST` codec carrying the client's block and
  light cursors for bounded journal replay or snapshot fallback;
- no Minecraft-specific block, light, or gameplay assumptions.

The client endpoint also provides bounded repair-request sending and a
repair-response callback boundary. The server session validates and dispatches
repair requests through an application-owned provider, then sends the response
reliably. The world service also has a default canonical block-stream repair
response. The client verifies that format's payload hash before invoking the
callback; application-owned formats must validate canonical content in the
callback, which must then validate the canonical content against its
replica and commit the repaired state atomically. Decoding alone is not an
acceptance decision.

`WorldReplicationBlockReplica` now provides the first client-owned replica
primitive. It owns one canonical block chunk, a separate light chunk, and
block/light/generation revision metadata, decodes snapshots through temporary
chunks before commit, and rejects deltas that are not contiguous for the
configured session, world, or chunk. It applies canonical section repairs
transactionally after validating their content hash. A canonical block repair
does not advance the light revision: the repair contains no light cells, so
the previous light state remains the last known light state until a matching
light delta is received. This avoids claiming that untransmitted lighting is
already synchronized. The replica still intentionally does not own meshes,
GPU objects, or render-thread scheduling. The remaining integration must
connect this state to the client callbacks and bounded application-owned mesh
publication.

This is transport plumbing and a headless ownership boundary, not the complete
server process. The canonical chunk snapshot now carries both the Game block
snapshot and a validated sparse-light snapshot, and the batched light-delta
codec is shared by snapshot creation and incremental publication. The
following items still belong to the Minecraft implementation: headless
authoritative world-service ownership, the in-process vertical slice, process
launch, gameplay validation rules, reconnect lifecycle integration, and the
adapter that publishes replica state into the
actual client world and bounded mesh queues. The transport pump, live sync
request, bounded replay/snapshot paths, canonical repair boundary, and
transactional block/light snapshot application are implemented. The existing
Game delta coordinator is a reusable authority/replication foundation, not
the complete server and not a renderer or lighting worker. The Networking
module must remain usable by those consumers without taking ownership of their
world state.

#### Handoff contract for the next implementation phase

The Minecraft integration must use the existing Game delta coordinator as the
authoritative block boundary rather than writing a second revision system in
the client. The intended call sequence is:

```cpp
// server-side, after transport decoding and gameplay validation
game_world_delta_channel::apply_request(request, delta);
game_block_delta_serialize(delta, payload);
networking_replication_sender::send_reliable_delta(connection,
    CHUNK_BLOCK_DELTA, payload, server_instance_id, session_id, sequence);

// client-side, after envelope decoding and apply-budget admission
game_block_delta_deserialize(delta, payload);
game_voxel_chunk::apply_authoritative_block_delta(delta);
```

The server is the only owner allowed to call the authoritative change path.
The client may maintain a prediction overlay, but it must not advance its
replica revision from the overlay. The client applies the accepted block
delta before waiting for derived lighting. The light-delta envelope carries
`base_light_revision`, `final_light_revision`, and `source_block_revision`; the
client discards a light result when its source
block revision no longer matches the applied block state. No light or mesh
operation may be performed synchronously on the render thread as part of
processing the block result.

Minecraft's `World::apply_authoritative_block_change` is the first concrete
world-side boundary for this flow. It validates local coordinates, delegates
expected-revision and expected-block checks to the Game chunk, returns the
canonical delta, updates world geometry/light revisions, records the edit for
history, and schedules the affected remesh region. Duplicate request IDs are
idempotent and do not increment revisions or enqueue duplicate remesh work.
`WorldReplicationService` now wraps that boundary for the in-process vertical
slice: it consumes an edit-intent message and emits an accepted result with
the canonical delta, or a rejected result carrying the current authoritative
revision when available. Minecraft also
now has a `WorldReplicationClient` boundary that submits edit intents through
the reliable control lane and applies received snapshots, block deltas, light
deltas, and edit results through Libft's bounded client gate. Its callbacks
are application-owned, so the client replica can update block state promptly
and schedule lighting/remeshing independently of message decoding. The
client validates the typed session/world identity in addition to the outer
envelope and can acknowledge the exact block/light revisions and generation
epoch it has applied. The server uses those acknowledgements to retain
unacknowledged deltas and choose snapshot repair when a client falls behind.
Remaining work is to connect these boundaries to the persistent transport
event loop, implement authenticated session startup, and connect the client
replica to its actual world/mesh publication queues.
It can also produce a revisioned chunk snapshot using the existing checksummed
Game chunk format, so snapshot admission precedes block/light delta admission.
The service also exposes reliable sender helpers for edit results, block
deltas, light deltas, and snapshots; block and light publications use the
reliable delta lane while control results use the reliable control lane. The
caller supplies the authenticated Libft connection and monotonically
increasing message sequence. These helpers do not mutate world state or retry
transport failures, so the server loop remains responsible for retaining an
unacknowledged result/delta and applying backpressure.
The service also validates incoming chunk acknowledgements against the
authenticated session, world identity, loaded chunk, and current authoritative
revision before the server updates its caller-owned retention window.
`WorldReplicationServerSession` now supplies the corresponding one-connection
dispatcher: it decodes authenticated envelopes, passes edit intents to the
service, sends the result immediately, sends the accepted block delta on the
next reliable-delta sequence, and validates chunk acknowledgements. It does
not yet start a server process; that remains an explicit next phase. Dispatch
also rechecks that the underlying Libft connection is `CONNECTED` and
authenticated, so registering a connection before handshake completion cannot
be used to process world messages accidentally.
`WorldReplicationServer` now owns a bounded table of 32
peer sessions, routes messages by connection ID, and fans out an accepted
block delta to other peers subscribed to that chunk. The fan-out rewrites the
recipient session identity while preserving the source request ID, world,
coordinate, and authoritative revision. A failed send is reported after the
server has attempted the other eligible peers; it is not treated as a reason
to roll back the already-committed world edit.
The server also keeps a bounded 4096-entry block-delta journal. It enforces
per-chunk revision continuity, exposes the next delta for contiguous replay,
and reports when an evicted gap requires a snapshot. Journal failure is
reported independently after the authoritative edit has already committed;
the server must retain or retry that publication through its outer queue.
`WorldReplicationServer::synchronize_peer` now uses that journal for a
subscribed chunk: it replays each contiguous retained delta, or sends a fresh
snapshot when the requested base is evicted, unavailable, or ahead of the
authoritative journal. Replay is intentionally explicit and bounded by the
caller’s scheduling loop; it does not block world mutation or lighting.
Session initialization binds the adapter to the service's server-instance ID,
and every received envelope must match both that ID and the negotiated session
ID before any application payload is dispatched. The first subscription/fallback
path now reuses `CHUNK_REQUEST`: the client requests one coordinate over the
reliable control lane, the server creates a revisioned snapshot, and the
session sends `CHUNK_SNAPSHOT` with a fresh server sequence. The session now
retains a bounded set of requested chunk coordinates, so later fan-out can
filter publications by interest without unbounded per-peer allocation. Region
interest updates are now supported through the reliable `CHUNK_INTEREST`
message, including idempotent subscribe and unsubscribe operations.
The server also exposes bounded-interest light-delta fan-out. A derived light
result is sent only to peers subscribed to its chunk, on the reliable delta
lane, with each recipient's session identity and the original source block
revision preserved. Retained serialized-delta replay and snapshot fallback
are now available for both block and light streams: the server journals each
stream independently, replays contiguous entries from a requested base, and
falls back to a complete chunk snapshot when an eviction gap or invalid future
base is detected. The live transport request that invokes this synchronization
operation is now `CHUNK_SYNC_REQUEST`. It carries the client's block and light
cursors over the reliable control lane; the server validates the session/world
identity and invokes the existing bounded block/light replay paths, which
independently fall back to a snapshot when a cursor is outside the retained
range. The client codec and server dispatch are transactional. Persistence of
acknowledgement cursors across reconnects remains pending, so reconnects must
force a fresh snapshot until that policy exists.

Per-peer acknowledgement bookkeeping is now part of the server session. Each
subscription records monotonic block/light revisions, generation epoch, and
whether the peer has acknowledged its snapshot; lower or regressive
acknowledgements never move that state backwards. After processing messages,
the server computes the minimum acknowledged revision across every active
subscriber for each chunk and prunes only entries at or below that minimum.
History is not pruned while any subscriber lacks a snapshot acknowledgement.
This makes retention acknowledgement-aware while preserving the bounded
journal fallback: if a disconnected or slow peer is later beyond the retained
range, synchronization sends a fresh snapshot. Removing a peer must trigger
the same pruning pass so a departed peer cannot pin history indefinitely.
The generic Networking layer now provides a fixed, transactional
`networking_replication_peer_cursor` record containing the authenticated
server/session/subscription identity, block/light revisions, generation, and
snapshot-acknowledged state. Minecraft may persist this record with its peer
metadata and restore it only after re-authentication and subscription
validation. If the application cannot prove that identity match, it must
discard the cursor and force a fresh snapshot. Minecraft's
`WorldReplicationCursorStore` now persists one validated cursor record through
`file_replace_safe`, rejects missing, truncated, or invalid records without
changing the caller's cursor, and removes stale records explicitly. The
remaining lifecycle work is connecting that store to authenticated reconnect
and subscription validation.

`WorldReplicationTransportPump` now provides the first live integration
boundary around Libft's `networking_message_transport`. It is a non-owning,
bounded pump for either a server or a client: it calls the transport poll,
drains at most the caller's message budget, dispatches each received message
to the selected replication endpoint, and leaves later messages queued for a
future tick. When Libft's transport worker owns polling, it only drains the
worker-owned receive queue and never calls `poll`, so the integration cannot
create two competing poll loops. It does
not perform world work, mesh work, or unbounded retries; those remain in the
server tick and client publication queues. The eventual process host must
choose exactly one owner for polling (this pump or Libft's persistent worker)
and route worker-produced messages into the same bounded dispatch path.

The client endpoint now adds a separate bounded ingress queue: the pump copies
transport-owned messages into that queue, and `drain_received_messages` applies
them under a caller-selected application budget. Queue limits are both
message-count and payload-byte bounded; overflow is reported as backpressure,
not silent loss. This keeps decoding and revision admission separate from the
application callbacks that update replicas and schedule lighting or meshes.
Both runtimes now expose a bounded `tick` boundary. The server tick limits
transport messages; the client tick separately limits transport ingress and
application-message draining and reports both counts. With Libft's persistent
worker, this drains the worker-owned queue without creating a competing poller.
If client ingress is full, already queued application messages are still
drained and the caller receives the backpressure result for a later retry.

Minecraft now reserves stable message identifiers for `CHUNK_BLOCK_DELTA`,
`CHUNK_LIGHT_DELTA`, `EDIT_INTENT`, `EDIT_RESULT`, snapshots,
acknowledgements, and `CHUNK_SYNC_REQUEST`. Message envelopes/codecs exist for
all implemented stages; the client and service wrappers route edit intents,
sync requests, and inbound replication through Libft Networking helpers. The
remaining integration work must provide the server-owned chunk registry, an
adapter from the now-canonical client replica store into the
actual client world, persistent generation/lighting workers, and bounded
publication queues described below. Those responsibilities do not belong in
Libft Networking.

The Minecraft light-delta envelope now defines a canonical sparse-cell codec:
little-endian cell count followed by strictly ascending linear cell indices
and packed-light bytes. The replica validates bounds, ordering, truncation,
and trailing data, copies the existing light state into a temporary chunk,
applies all records, and commits the light state and revision together. A
zero-count four-byte payload is the only valid no-cell-change revision. The
`protocol_chunk_light_payload_append` producer helper validates ordering and
bounds before constructing the payload transactionally, so the server must
use this exact format before publishing a light revision to the bounded
lighting/remesh queue. Its output is appended only after the complete encoded
payload has been prepared.

### 11.14 Cross-module persistence and parser hardening

The networking handoff depends on Libft persistence and configuration paths
being lossless and failure-safe. These audit findings are now implementation
requirements: replication must not lose a configuration field or corrupt a
saved snapshot during streaming or concurrent persistence work.

#### CSV parser

`ft_csv_document::parse_content` must model every row as having a current
field, including an empty field. A delimiter finalizes the current field and
starts the next one; EOF immediately after a delimiter must therefore
finalize one final empty field. The same rule applies to consecutive
delimiters and to a delimiter before a record terminator:

```text
a,b,   -> [a, b, ""]
,     -> ["", ""]
a,,   -> [a, "", ""]
"a",  -> [a, ""]
```

Reject delimiters that collide with the grammar (`NUL`, quote, LF, and CR)
unless a separately specified grammar supports them. Field and row metadata
updates must be transactional: if appending a row offset or length fails,
neither vector may retain a partial row entry. Large-field range appends may
be added later, but must preserve quote, CRLF, and escaped-quote semantics.

Required CSV tests cover trailing/consecutive empty fields, quoted empty
fields, embedded commas, escaped quotes, CRLF, embedded newlines in quoted
fields, unterminated quotes, characters after a closing quote, invalid
delimiters, very large fields, and allocation failure at every field/row
growth point. Failure tests must verify both the error and the documented
unchanged-or-destroyed document state.

#### Config parser and INI persistence

`config_parse` must never treat a fragment of one physical line as an
independent configuration record. Replace the fixed 512-byte assumption with
dynamic line accumulation, or append chunks until LF, CRLF, or EOF is seen
before parsing. A final line without a newline is valid when permitted by the
grammar. Allocation failure while growing a line must release the temporary
line and leave the partial configuration safely destroyable.

The writer must define a lossless INI grammar for public section, key, and
value strings. Use explicit escaping/quoting for `=`, brackets, comment
markers, quotes, tabs, leading/trailing whitespace, CR, and LF; or reject
unrepresentable values before touching the destination. It must never emit a
file that reparses differently. The required invariant is:

```text
parse(write(config)) == config
```

for representable entries, including empty strings and structural
characters. Write through a unique temporary file and commit only after the
complete output succeeds, so a failed write cannot replace a valid config.

`config_write_json` should avoid repeated O(N) group scans. At minimum keep a
tail when appending groups; preferably use a temporary section index or
sort/group once. Preserve entry order and JSON semantics.

Thread-safety preparation and teardown are lifecycle operations. The public
contract must require exclusive ownership, or the implementation must block
new operations, wait for active operations to drain, and only then destroy
mutex storage. This applies to both the table and entries. Test repeated
prepare/teardown, failed mutex creation, concurrent access at lifecycle
boundaries, and destruction after partial initialization under TSan.

#### Filesystem interaction required by persistence

Config and chunk persistence must use unique `O_EXCL` temporary names in the
target directory; a shared `<target>.tmp` is unsafe for concurrent writers.
Temporary-file creation may retry only for the platform equivalent of
`EEXIST`; other errors must be returned immediately and accurately. Security-
sensitive relative paths must distinguish lexical validation from real
filesystem containment and test symlink/junction escapes.

The APIs must document whether success means atomic visibility or crash
durability. The durable variant must surface file-sync and directory-sync
failures. Tests must run concurrent atomic writers, inject failures at every
write/rename/sync stage, and verify that the destination is always the old
complete payload or one complete new payload.

These fixes are prerequisites for server snapshots, replay files, and
configuration-driven world startup. Parsing, serialization, and snapshot
publication remain bounded background/server work and must not run on the
render thread.

The dynamic Config line reader is now implemented and covered by a long-line
regression test; a 1 KiB key/value line remains one entry and a final line
without a newline is accepted. The current INI writer now rejects
unrepresentable structural text before building output and commits the
complete document through `file_replace_safe`, so an existing destination is
preserved when serialization or replacement fails. JSON configuration output
now follows the same safe replacement path and uses a tail pointer for section
creation. Config teardown uses the documented exclusive-ownership contract:
the caller must quiesce table and entry users before destroying their mutexes.
Full INI escaping remains intentionally represented by rejection of values that
the current INI grammar cannot encode; a future escaped grammar may broaden
the accepted value set without weakening the round-trip invariant.

#### CrossProcess shared-memory transport

The shared-memory transport now treats descriptors as hostile IPC input. Its
fixed 304-byte big-endian wire representation is versioned and decoded
transactionally; raw C++ object layouts are never sent over the socket.
Receivers reject zero or undersized mappings, non-terminated names, addresses
below the advertised mapping base, and offsets that would exceed the mapping.
POSIX receivers verify the backing object's actual size before `mmap`, while
Windows receivers validate the mapped view before access. Partial socket sends
and receives are handled explicitly.

`cp_receive_memory` reports whether the payload was consumed independently from
later unlock/unmap cleanup errors. Callers must not retry when `consumed` is
true. Process-shared mutex owner death/abandonment is treated as an invalid
operation requiring the affected shared-memory state to be rebuilt; recovered
mutex ownership is released before returning. Test-only failure seams cover
post-consumption cleanup failures and are excluded from release builds.

The focused tests cover transactional wire truncation, malformed names,
below-base addresses, undersized backing objects, owner-death recovery, and
payload-preserving validation failures. POSIX process-death and cleanup-failure
cases must remain enabled in Linux/macOS CI; the Windows build validates the
portable wire and descriptor paths and requires equivalent native mapping tests
when its CI runner is available.

### 11.15 Required tests and acceptance gates

Unit tests:

- canonical encoding/decoding is transactional at every truncation boundary;
- all integer/count/coordinate overflow cases fail before allocation/mutation;
- accepted edits increment the authoritative revision exactly once;
- rejected and idempotent duplicate intents do not mutate or double-charge;
- block/provenance/persistence/delta commit is atomic under injected failures;
- client delta application accepts only contiguous revisions;
- the transport pump drains no more than its message budget, preserves later
  messages for the next pump, retains a full message for either endpoint,
  dispatches to exactly one endpoint, and never polls when Libft's transport
  worker is active;
- obsolete light deltas are rejected by `source_block_revision` without
  delaying or reverting the current block state;
- section and root SHA-256 values are deterministic across platforms;
- changing any canonical block, provenance bit, or world identity changes the
  expected content hash;
- changing only revision metadata does not change the content hash but remains
  visible to synchronization logic;
- mesh/container/pointer layout does not affect canonical hashes.

Multi-process integration tests:

- launcher starts a loopback-only server, securely bootstraps, connects, loads a
  minimum playable area, saves, and shuts down without orphaning a process;
- client A edits; server commits; A, B, and C receive the same revision/value;
- conflicting edits at one revision produce one deterministic authoritative
  order and every client converges;
- invalid reach, block ID, inventory, permission, collision, stale revision,
  oversized batch, replayed request, and rate-limit cases leave state unchanged;
- a client disconnects between snapshot R and delta R+1 and recovers by retained
  deltas or explicit snapshot fallback;
- a client joins while edits continue and cannot miss the snapshot/delta handoff;
- server/client hash mismatch repairs one section, then verifies the new root;
- repair corruption leaves the previous client replica intact;
- local server crash and client crash follow the documented cleanup policy.

Performance tests:

- repeated block breaks receive `EDIT_RESULT` and canonical block delta within a
  configured tick/latency bound independent of lighting completion;
- generation and lighting load cannot block client input/rendering;
- client applies no more than configured message bytes, delta operations, mesh
  commits, or GPU uploads per frame;
- server observes bounded queue memory and fair progress under many clients;
- hashes obey byte/chunk-per-tick budgets and do not create periodic frame/tick
  spikes;
- matched normal/analytics runs report p50/p95/p99 client frame time, server tick
  time, edit acknowledgement latency, block-visible latency, light-correct
  latency, snapshot throughput, and queue peaks.

Run protocol and state tests with Libft's seeded impairment simulator covering
latency, jitter, loss, duplication, corruption, reordering, MTU drops, and
manual time. Run process/network tests on Windows, Linux, and macOS, plus ASan,
UBSan, and TSan where supported. Release builds must contain no test-only fault
injection or verbose packet/world dumps.

This architecture is accepted only when:

- the renderer has no authoritative world ownership and the minimum vertical
  slice proves prompt updates before process migration;
- final single-player deployment uses a separate local authoritative server
  process;
- multiplayer uses the same server/session/replication path;
- the client cannot directly commit authoritative blocks;
- an accepted block edit is replicated promptly without waiting for lighting or
  remeshing;
- all interested clients converge through contiguous revisions;
- hash mismatch and delta-gap recovery are deterministic and transactional;
- generation, lighting, persistence, hashing, and networking never block the
  render thread;
- bounded queues expose backpressure rather than silently dropping state;
- server and client shutdown leave no worker threads or child processes behind;
- cross-platform, impairment, sanitizer, performance, and graphics-context
  validation gates pass.
