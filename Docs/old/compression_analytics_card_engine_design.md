# Compression hardening, runtime analytics, card-engine foundations, and custom scripting

## Document status

This document is an implementation handoff. It describes four independent
work streams:

1. hardening generic file-descriptor compression streams;
2. adding a low-overhead runtime analytics module for frame and function timing;
3. laying the foundations for a configurable card-game rules engine;
4. replacing the vendored Lua runtime with a completely custom Libft scripting
   language, compiler, and virtual machine.

No implementation is part of this document. Each work stream must be delivered
in separate commits so it can be reviewed and reverted independently. All code
must follow the repository-root `AGENTS.md`, including lifecycle, error,
thread-safety, fixed-width type, Libft-container, formatting, and test-only hook
rules.

## Goals

- Prevent silent truncation when compression reads from pipes, sockets, or
  fragmented producers.
- Correctly handle short and interrupted writes without duplicating output.
- Remove unsafe optional-mutex transitions from compression options.
- Make it possible for Minecraft to report where frame time is spent, including
  nested work and work executed on worker threads.
- Keep analytics overhead bounded and measurable.
- Make analytics useful at runtime as well as through exported trace files.
- Create a card engine that understands how to execute a configured rules model
  but contains no built-in knowledge of a particular card game's rules.
- Allow games to register native effect callbacks and reference them by stable
  identifiers from configuration.
- Make board size, zones, card types, turn phases, resources, targeting,
  triggers, damage, healing, and other common concepts configurable.
- Preserve determinism so matches can be replayed, synchronized, tested, and
  validated by an authoritative server.
- Remove Lua, its vendored sources, and its ABI from Libft after a custom,
  deterministic scripting module has reached feature parity with every current
  Game and Voxel scripting entry point.

## Non-goals

- Replacing zlib.
- Automatically profiling every machine instruction or replacing an external
  sampling profiler.
- Enabling expensive analytics collection unconditionally in production.
- Embedding the rules of poker, Magic, Hearthstone, or any other specific game
  in Libft.
- Serializing raw function pointers.
- Letting effect callbacks mutate arbitrary engine internals without validation.
- Building card rendering, networking transport, matchmaking, or user-interface
  code into the card engine.
- Reproducing the complete Lua language, Lua C API, or Lua bytecode format.
- Adding a JIT compiler in the first scripting implementation.

# Part I: Compression stream hardening

## Confirmed current defects

### Short reads are treated as end-of-stream

`ft_compress_stream_with_options()` currently selects `Z_FINISH` when
`read_bytes < input_buffer_size`. A successful short read is not EOF for a
generic descriptor. This can silently discard all data produced after that
read.

The compressor must select:

- `Z_NO_FLUSH` when `su_read()` returns a positive value, regardless of its
  size;
- `Z_FINISH` only when `su_read()` returns zero;
- an error path when `su_read()` returns a non-retryable negative value.

The existing test named
`test_ft_compress_stream_uses_finish_flush_after_partial_read` encodes the
incorrect contract and must be replaced. A partial read must assert
`Z_NO_FLUSH`; the following zero-byte EOF read must assert `Z_FINISH`.

### Short writes are treated as fatal

Compression and decompression currently call `su_write()` once per produced
buffer and require the return value to equal the complete produced size. A
short write can leave a valid prefix in the output descriptor and must be
continued from that exact offset.

Add one internal helper with behavior equivalent to:

```text
compression_stream_write_all(output_descriptor, buffer, size)
    offset = 0
    while offset < size
        written = su_write(output_descriptor, buffer + offset, size - offset)
        if written > 0
            offset += written
            continue
        if written < 0 and interruption is retryable
            continue
        return mapped error
    return success
```

Requirements:

- A zero-byte write while bytes remain must not spin forever. Return a
  dedicated I/O/progress error.
- Retry only errors whose platform contract explicitly permits retry, such as
  interruption.
- For non-blocking descriptors, choose and document one contract:
  - return a would-block status while preserving the number of bytes already
    written in an operation-state object; or
  - support only blocking descriptors in the synchronous API.
- The first implementation should keep the synchronous API blocking and return
  a mapped error for would-block. A future resumable stream object may support
  non-blocking operation without losing state.
- Update progress only by bytes actually committed to the output descriptor.
- Cancellation may be checked between short-write iterations, but cancellation
  after a partial output write must be documented as producing an incomplete
  output stream.

### Optional mutex transitions are unsafe

`t_compress_stream_options` reads `_mutex` independently in accessors while
`disable_thread_safety()` can destroy and delete that object. Concurrent mode
changes can therefore destroy a mutex that is owned, awaited, or about to be
unlocked.

The preferred repair is to make synchronization permanent for the initialized
lifetime of this small configuration object:

- Allocate and initialize the class-owned `pt_recursive_mutex` during
  `initialize()`.
- Every state read and write uses that mutex.
- Do not expose mutable enable/disable transitions after initialization.
- `destroy()` waits for/obtains the mutex according to the standard Libft
  lifecycle protocol, marks the object as transitioning, prevents new access,
  releases protected state, and destroys the mutex only after active access has
  drained.

If compatibility requires keeping `enable_thread_safety()` and
`disable_thread_safety()`, they should become policy toggles protected by a
permanent lifecycle/transition mutex. The permanent mutex itself must never be
deleted by `disable_thread_safety()`. This costs one mutex per initialized
options object but removes an entire class of use-after-free bugs.

The implementation must also align the class with `AGENTS.md`:

- use shared `FT_CLASS_STATE_*` lifecycle constants;
- use `ft_bool`, fixed-width integers, and `ft_size_t` instead of `bool`, `int`,
  and `std::size_t` in new or changed public interfaces;
- use centralized lifecycle-abort helpers;
- return `FT_ERR_*` values consistently rather than generic `0`/`1` where an
  API migration is feasible;
- keep test hooks behind `LIBFT_TEST_BUILD`;
- do not use exceptions or standard containers for the repair.

## Streaming state-machine requirements

Compression must have explicit states:

```text
READ_INPUT
  positive read -> DEFLATE with Z_NO_FLUSH -> WRITE_ALL -> READ_INPUT
  zero read     -> DEFLATE with Z_FINISH   -> WRITE_ALL -> FINISHED
  error         -> FAILED
```

Decompression must have explicit states:

```text
READ_INPUT
  positive read -> INFLATE with Z_NO_FLUSH -> WRITE_ALL
  zero read     -> final validation/finish

INFLATE
  Z_STREAM_END with no trailing bytes -> FINISHED
  needs more input                     -> READ_INPUT
  invalid/truncated/dictionary/error   -> FAILED
```

Additional rules:

- A short positive read never changes the stream to a final state.
- EOF before `Z_STREAM_END` is truncated compressed input.
- Trailing bytes remain an error unless concatenated streams are added as an
  explicit option later.
- Every zlib initialization has exactly one matching cleanup call.
- Cleanup continues even if a progress callback or write fails.
- Preserve and return the first meaningful error.
- Validate all sizes before narrowing them to zlib's `uInt` fields.
- Progress and cancellation callbacks operate from an immutable options
  snapshot captured before stream processing starts. A running stream must not
  repeatedly race with live option mutation.

## Compression test plan

### Short-read correctness

- Create a pipe.
- Start the compressor on a consumer thread.
- Write payload A, where A is smaller than the configured input buffer.
- Wait until the compressor has consumed A using a deterministic test
  synchronization point, not a timing-only sleep.
- Write payload B and close the producer descriptor.
- Decompress the result and assert exact `A || B` equality.
- Repeat with one-byte fragments, fragments exactly equal to the input buffer,
  and random fragment boundaries.

### Short-write correctness

Use a `LIBFT_TEST_BUILD`-only `su_write` adapter or Compression-local write hook
that can deterministically return:

- one byte per call;
- alternating short lengths;
- interruption before eventual success;
- zero while bytes remain;
- a non-retryable error after a committed prefix;
- would-block for the documented synchronous behavior.

Assert byte-exact compressed and decompressed output and exact progress totals.

### Thread-safety and lifecycle

- Many setter and snapshot threads while an options object remains initialized.
- Concurrent calls to compatibility enable/disable helpers, if retained.
- Concurrent destroy versus active snapshot access.
- Reinitialization after destroy.
- CMA and mutex allocation failure at every allocation point.
- Mutex lock, unlock, initialization, and destruction failure injection.
- TSan execution with enough iterations to cross transitions repeatedly.
- Verify test-only hooks are absent from release archives.

### Regression matrix

- empty input;
- input smaller than, equal to, and larger than buffers;
- incompressible and highly compressible data;
- embedded zero bytes;
- very small input/output buffers;
- cancellation before read, after read, during write-all, and after final flush;
- callback failure;
- truncated input;
- trailing bytes;
- pipe, regular file, and loopback socket descriptors on Linux, macOS, and
  Windows;
- ASan, UBSan, TSan, and deterministic failure-injection builds.

# Part II: Analytics module

## Why a separate module is needed

The existing Time trace API is useful as a basic trace writer, but it is not an
appropriate frame profiler in its current form:

- it owns global session and stack state;
- it uses standard strings and vectors;
- it is not safe for simultaneous worker threads;
- every event is formatted, written, and flushed synchronously;
- all events report thread ID zero;
- it does not compute inclusive time, exclusive time, frame percentages,
  percentiles, or dropped-event counts;
- instrumentation overhead can materially distort short frame regions.

Create a new `Modules/Analytics` module. Time remains responsible for clocks and
basic tracing. Observability remains responsible for operational metrics and
external telemetry. Analytics is responsible for low-overhead local performance
measurement and aggregation.

## Required user experience

Minecraft should be able to define a frame and nested regions such as:

```text
frame 4821: 16.42 ms
  input                         0.18 ms   1.1%
  world update                 2.31 ms  14.1%
    generation commit          0.64 ms   3.9%
  visibility                   1.12 ms   6.8%
  mesh preparation             3.40 ms  20.7%
  rasterization                7.83 ms  47.7%
    worker 0                   1.96 ms
    worker 1                   1.89 ms
    worker 2                   2.01 ms
    worker 3                   1.97 ms
  presentation                 0.71 ms   4.3%
  uninstrumented               0.87 ms   5.3%
```

The game must be able to query this information while running for a debug
overlay and export it for deeper analysis.

## Instrumentation model

Use explicit instrumentation as the primary mechanism. Automatic compiler
function instrumentation may be researched later, but it is less portable,
harder to filter, and more likely to recurse into the profiler.

Provide three concepts:

- **session**: owns registry, thread buffers, aggregation, and exporters;
- **frame**: a logical unit such as one rendered frame or one server tick;
- **scope/event**: a timed region nested under a frame or another scope.

Recommended API shape:

```text
analytics_session_initialize(session, configuration)
analytics_register_region(session, "render.rasterize", category, &region_id)
analytics_register_thread(session, "render-worker-0", &thread_id)

analytics_begin_frame(session, frame_number, &frame_token)
analytics_begin_scope(session, region_id, &scope_token)
analytics_end_scope(session, scope_token)
analytics_end_frame(session, frame_token)

analytics_get_latest_frame(session, snapshot)
analytics_get_region_statistics(session, region_id, statistics)
analytics_export_trace(session, exporter)
```

Tokens carry enough information to validate balanced begin/end operations and
must not own heap allocations. Region names are registered once, outside hot
paths. Hot-path calls use integer IDs rather than hashing strings every frame.

An optional scope helper or macro may guarantee end-on-return, but it must:

- compile to nothing when `LIBFT_ENABLE_ANALYTICS` is disabled;
- evaluate user arguments at most once;
- never hide meaningful return values;
- introduce no release dependency when analytics is excluded;
- coexist with explicit begin/end APIs for C-compatible code;
- be verified by release-symbol and archive tests.

## Timing semantics

Record monotonic high-resolution timestamps. For each completed scope calculate:

- inclusive duration: end minus start;
- child duration: sum of direct child inclusive durations on the same logical
  execution chain;
- exclusive duration: inclusive minus child duration;
- invocation count;
- minimum, maximum, mean, variance/jitter;
- configurable rolling-window percentiles such as p50, p95, and p99;
- percentage of parent scope;
- percentage of frame;
- thread identity;
- frame/tick identity.

Do not sum parallel worker durations and present that sum as wall-clock frame
time. Report both:

- CPU work: sum of worker-scope durations;
- critical-path/wall time: earliest worker start to latest required worker end,
  or an explicitly linked task span.

Calculate `uninstrumented frame time` as frame duration minus the union of
top-level instrumented intervals. Do not simply subtract a sum that may contain
overlapping scopes.

## Hot-path architecture

Each registered thread receives a fixed-capacity or bounded-growth thread-local
event buffer. A begin/end operation should normally perform only:

- one enabled-state check;
- one timestamp read;
- one token/stack update;
- one append into thread-local memory.

It must not normally perform:

- heap allocation;
- file I/O;
- JSON formatting;
- global registry locking;
- string copying;
- cross-thread waiting.

The collector drains completed records at frame boundaries or on a dedicated
collector thread. Buffer overflow follows a configured policy:

- drop newest event and increment a dropped-event counter; or
- overwrite oldest completed event in continuous-capture mode.

Never block a render worker because an analytics exporter is slow.

## Cross-thread relationships

Scopes are naturally nested only within one thread. Work handed to another
thread needs an explicit flow identifier:

```text
main: begin scope "rasterization"
main: create flow token 9001
worker: begin linked scope "rasterize chunk" with flow 9001
worker: end linked scope
main: wait for flow 9001
main: end scope "rasterization"
```

This enables a trace viewer and runtime aggregator to distinguish scheduling
delay, execution time, waiting time, and parallel CPU work.

## Runtime queries and Minecraft integration

The initial Minecraft integration should instrument stable frame phases rather
than every tiny helper:

- event polling and input;
- simulation/world update;
- asynchronous generation submission;
- generation result commit;
- chunk visibility/culling;
- mesh creation/update;
- render command preparation;
- CPU rasterization, split by persistent worker;
- GPU submission or software framebuffer write;
- debug overlay;
- presentation/vsync wait;
- frame total.

Expose snapshots suitable for the existing debug overlay:

- latest frame breakdown;
- rolling 1-second and configurable N-frame averages;
- p95/p99 frame duration;
- top regions by exclusive and inclusive time;
- worker utilization and idle time;
- frame-budget overruns;
- dropped analytics events.

The game chooses names, categories, frame boundaries, and which regions are
instrumented. Libft only records and aggregates them.

## Exporters

Exporter callbacks are registered through stable function-pointer interfaces.
Required exporters:

- runtime snapshot exporter for in-process overlays;
- Chrome/Perfetto trace JSON exporter;
- compact binary capture for low-overhead long sessions;
- summary text/CSV exporter for benchmarks.

Exporters run outside producer hot paths. A failed exporter must not corrupt the
session or stop the game. Record exporter errors and dropped batches.

## Analytics configuration

Configuration should control:

- enabled state;
- event buffer capacity per thread;
- retained frame count;
- aggregation window;
- enabled categories/region masks;
- sampling ratio for very frequent scopes;
- slow-scope threshold;
- automatic capture trigger, such as frame time above 25 ms;
- pre-trigger and post-trigger frame retention;
- exporter choice and destination;
- whether names are retained or replaced with numeric IDs in restricted builds.

Runtime enable/disable must be a safe state transition. Disabling collection
stops new records, waits only at a safe collector boundary, and never destroys
thread buffers while registered producers may access them.

## Analytics performance requirements

Define budgets before implementation:

- disabled scope overhead should be close to one predictable branch;
- enabled begin/end overhead must be benchmarked in nanoseconds;
- no hot-path allocations after registration/warm-up;
- no file writes on render or worker threads;
- profiler overhead should remain below a configurable fraction of frame time;
- overhead must be reported separately so measurements can be interpreted.

Add benchmark comparisons for disabled, enabled-without-export, enabled with a
collector, and synchronous legacy Time tracing.

## Analytics test plan

- Single nested scope inclusive/exclusive arithmetic.
- Sibling scopes and uninstrumented-gap calculation.
- Overlapping worker scopes without double-counting wall time.
- Flow-token relationships across threads.
- Balanced and unbalanced begin/end detection.
- Wrong-thread token completion.
- Frame ending with open scopes.
- Buffer overflow and exact dropped-event counts.
- Runtime enable/disable while producers are active.
- Thread registration/unregistration during collection.
- Collector/exporter failure and backpressure.
- Timestamp monotonicity and injected clock wrap/error behavior.
- Region registry collisions and duplicate names.
- Rolling statistics and percentile validation against known samples.
- Allocation-failure injection outside the hot path.
- TSan stress with many producer threads and one collector.
- Release build symbol checks proving instrumentation is absent when disabled.
- A Minecraft benchmark showing measured analytics overhead with the same scene,
  seed, resolution, frame count, and CPU affinity.

# Part III: Configurable card-engine module

## Module boundary

Create `Modules/CardGame`. The module is a deterministic rules runtime and
state-transition engine. It knows generic mechanics such as entities, zones,
spaces, events, effects, targets, phases, resources, and validation. It does not
know which concrete rules a game uses.

The host game owns:

- rule configuration files;
- card definitions and assets;
- native effect callback implementations;
- player authentication and networking;
- presentation and animation;
- AI policy;
- persistence policy;
- authoritative acceptance of player commands.

Libft owns:

- validated rule schemas;
- deterministic match state;
- command validation and application;
- event/trigger ordering;
- effect dispatch;
- target selection and legality;
- configurable turn/phase progression;
- snapshots, deltas, replay records, and state hashes;
- safe callback registration and invocation.

## Terrain-style external configuration

CardGame must use the same configuration model as terrain generation. The
engine is a generic interpreter of a validated configuration, while the game
supplies the authored data that describes what the rules mean. No card,
effect, phase, zone, deck, or resolution policy should require a new Libft
code change merely because a game designer wants a different value or
combination.

The configuration pipeline is:

```text
game config files
    -> format parser (JSON/YAML/other host adapter)
    -> schema and range validation
    -> name/reference resolution
    -> callback capability validation
    -> canonical ruleset ordering and hash
    -> immutable resolved ruleset
    -> match instances consuming that ruleset
```

Configuration must be loaded transactionally, just like terrain
configuration. Parse and validate into a temporary definition set; resolve all
references and callback names; calculate the canonical configuration hash; and
only then replace the active ruleset. A malformed file, missing callback,
duplicate ID, invalid reference, capacity overflow, unsupported stack policy,
or allocation failure must leave the previously active ruleset untouched.

The authored configuration should be able to define and modify, at minimum:

- card names, stable IDs, types, tags, costs, stats, visibility, and copies;
- effect IDs, arguments, callback names, capability requirements, and emitted
  operations;
- zones, decks, hands, discard piles, capacities, ordering, ownership, and
  visibility;
- board spaces, adjacency, placement constraints, and movement policies;
- resources, stats, bounds, modifiers, and reset rules;
- phases, turn order, priority windows, automatic events, and allowed commands;
- conditions, selectors, target cardinality, and legality filters;
- trigger subscriptions, priorities, expiration, and once-only behavior;
- resolution-stack ordering, admission, deferred work, and failure policy;
- deterministic random-stream seed/version and replay limits;
- serialization visibility, snapshot/delta policy, and network authority rules.

The engine should expose typed configuration APIs for tests and embedders, with
file adapters layered above them. Runtime matches receive an immutable
resolved ruleset handle or copy; mutable match state must not mutate the
authored configuration. If hot reload is supported, it creates a new resolved
ruleset for future matches or an explicitly migrated match. It must never
silently reinterpret an active match using a partially reloaded definition.

Configuration values and callback names are part of the canonical ruleset
hash. Numeric IDs are used internally after resolution, but authored names
remain available for diagnostics. Function pointers are registered by the
host and referenced by stable callback names/capability IDs in configuration;
pointer addresses never enter configuration files, hashes, snapshots, network
messages, or replays.

This separation is intentional: terrain configuration describes how a world
generator behaves, while CardGame configuration describes how a match behaves.

The configuration contract must nevertheless be the same at the engine
boundary. CardGame must consume the same validated, transactional, canonical
configuration pipeline as terrain: the host parser may accept JSON, YAML, or
another format, but it must produce the same resolved-definition interface,
stable IDs, reference diagnostics, range checks, and canonical hash rules.
CardGame-specific fields belong in the schema; the loading, validation,
resolution, reload, and failure-rollback mechanics must not be a second
configuration system. A CardGame ruleset should therefore be expressible
without adding a C++ enum or changing Libft code for each new card, effect,
zone, board shape, trigger, or stack policy.

At minimum, the resolved configuration must expose data equivalent to:

```text
ruleset {
    cards: [{ id, type, tags, stats, abilities, triggers }]
    effects: [{ id, callback_name, argument_schema, capabilities }]
    boards: [{ id, spaces, adjacency, capacity, placement_rules }]
    zones: [{ id, ordering, visibility, capacity, access_policy }]
    decks: [{ zone_id, initial_cards, shuffle_policy, draw_policy }]
    resolution: { ordering, admission, child_effect_policy, failure_policy }
    phases: [{ id, transitions, triggers, allowed_commands }]
    limits: { actions, effects, stack_depth, serialized_bytes }
}
```

The loader must resolve every `callback_name`, card/effect/zone reference, and
capability before publishing the ruleset. Unknown fields are rejected or
explicitly versioned, duplicate IDs are rejected, and a failed load/reload
must leave the previously active ruleset byte-for-byte usable. The canonical
hash must include all behavior-affecting values, including deck order, stack
admission, visibility, trigger ordering, and limits, while excluding pointer
addresses and host-specific parser details. Tests must load the same ruleset
through the terrain and CardGame configuration adapters, compare canonical
normalization and diagnostics, and prove that changing each behavior-affecting
field changes the ruleset hash and replay compatibility as expected.
Both use the same pattern of data-driven definitions, validation, deterministic
resolution, and immutable runtime consumption.

## Core principle: understand the rule model, not the rules

The engine should not contain logic such as “creatures heal at end of turn.” It
should understand:

- a phase can emit an event;
- a rule can subscribe to an event;
- a rule has conditions, targets, and effects;
- an effect callback can modify state through validated operations.

A game then configures:

```text
event: phase_end
condition: entity has tag "creature"
condition: entity controller is active player
effect callback: restore_end_of_turn_damage
target: each matching battlefield entity
```

The callback ID resolves to a registered native function pointer at runtime.
The config never stores the pointer value.

## Stable identifiers and registries

Use stable numeric IDs for runtime work and stable string names for authored
configuration. Loading resolves names into IDs transactionally.

Required registries:

- card types;
- card definitions;
- zones;
- board layouts and spaces;
- stats and counters;
- resources;
- tags/traits;
- turn phases and steps;
- event types;
- conditions;
- target selectors;
- effect callbacks;
- validation callbacks;
- replacement/prevention callbacks;
- win/loss/draw conditions.

Duplicate IDs/names, missing references, dependency cycles, and incompatible
types fail configuration loading without partially replacing the active ruleset.

## Function-pointer effect interface

Use explicit callback signatures with context and output operations. A callback
must not receive unrestricted mutable engine internals.

Conceptual interface:

```text
card_effect_callback(
    const card_effect_context *context,
    const card_effect_arguments *arguments,
    card_operation_buffer *output_operations,
    void *user_data)
    -> FT_ERR_*
```

`card_effect_context` contains immutable views or IDs for:

- match and ruleset;
- source card/entity;
- controller/owner;
- triggering event;
- selected targets;
- current phase/step;
- deterministic random interface;
- effect-instance ID.

The callback appends requested operations such as damage, healing, movement,
draw, discard, stat modification, token creation, or event emission. The engine
validates and commits those operations. This gives native extensibility without
letting a callback bypass invariants.

Registration uses a stable callback name and function pointer:

```text
card_effect_registry_register(
    registry,
    "restore_end_of_turn_damage",
    callback,
    user_data,
    callback_capabilities)
```

The loader resolves the config name to a runtime callback ID. Match snapshots,
network messages, and replays store callback/rule IDs and configuration hashes,
never addresses.

## Configurable game model

### Board and spaces

Board configuration must support:

- zero, one, or multiple boards;
- configurable space count;
- rows, columns, lanes, or graph-based adjacency;
- per-player, shared, and neutral spaces;
- capacity per space;
- accepted card/entity types and tags;
- placement and movement constraints;
- named groups such as front row, reserve, leader slot, or objective zone;
- optional distance and line-of-sight callbacks.

Do not encode a fixed grid in the core. Represent spaces as stable IDs plus
configured adjacency and attributes. A rectangular board is one loader/helper,
not the engine's only topology.

### Zones

Zones are configurable containers such as deck, hand, discard, exile,
battlefield, command, shop, or custom zones. Configure:

- visibility by participant/observer role;
- ordering semantics: ordered, unordered, stack, queue;
- capacity;
- owner/controller scope;
- accepted card types/tags;
- entry/exit events;
- whether randomization/shuffling is allowed;
- whether contents are included in public state hashes.

#### Ordered decks and stack-like zones

An ordered zone must store an explicit sequence of stable card-instance IDs;
it must never derive order from allocation addresses or an unordered container.
The same ordered-container implementation should support decks, queues, draw
piles, discard stacks, effect stacks, and game-specific piles. A zone
definition declares:

- `ordered`, `unordered`, `LIFO`, or `FIFO` access semantics;
- maximum capacity and whether overflow is an error, a discard, or a configured
  replacement operation;
- whether `draw`, `peek`, `remove`, and `insert` are permitted while a match is
  resolving;
- whether insertion positions are top, bottom, explicit index, or callback
  selected;
- whether duplicate card instances are allowed and how instance identity is
  preserved;
- whether order is public, owner-visible, observer-redacted, or secret;
- whether the zone participates in state hashes, snapshots, deltas, and replay.

Deck operations must be explicit and deterministic:

```text
peek_top(deck)                 // no mutation
draw_top(deck)                 // remove first card in configured orientation
draw_bottom(deck)
insert_top(deck, card)
insert_bottom(deck, card)
insert_at(deck, index, card)
remove_instance(deck, instance_id)
shuffle(deck, match_random_stream)
```

The API should expose the orientation rather than making callers infer it
from an array index. `top` and `bottom` remain stable names even if the
internal representation changes. Invalid positions, duplicate instance IDs,
capacity overflow, and forbidden operations return errors without modifying
the zone.

Shuffle must consume only the match-owned deterministic random stream. Use a
specified unbiased Fisher-Yates procedure over the logical sequence, record
the stream position or random draws in the command record, and include the
resulting order in the state hash whenever the zone is hash-visible. A seed
alone is insufficient if random calls can vary between platforms or rule
paths. Tests must compare exact shuffled orders across platforms and after
serialize/load/replay.

A deck is not a special hard-coded game object. It is a configured ordered
zone with optional draw/search/reveal permissions. Search, tutor, mill,
return-to-top, return-to-bottom, and insert-random-position are operations
with separate authorization and visibility rules. Hidden order must never
appear in an unauthorized snapshot, delta, trace, diagnostic, or error
message.

#### Configurable resolution stacks

Effect resolution uses a configured stack descriptor rather than assuming one
universal ordering. A stack descriptor contains:

- `LIFO` or `FIFO` resolution order;
- maximum pending entries and maximum total entries resolved per command;
- whether entries may be added while resolution is active;
- if additions are allowed, whether they are appended to the active stack,
  inserted at the next position, or placed in a deferred queue;
- whether newly added entries may resolve in the same pass or only after the
  current batch completes;
- whether cancellation, replacement, or priority responses are allowed;
- deterministic ordering keys for equal-priority entries;
- behavior when the stack is full, a callback fails, or a loop limit is hit.

Recommended policies are represented explicitly:

```text
stack_policy {
    ordering: LIFO | FIFO;
    admission: CLOSED | OPEN_CURRENT_BATCH | OPEN_DEFERRED;
    resolve_new_entries: SAME_PASS | NEXT_PASS;
    failure_policy: ROLLBACK_BATCH | COMMIT_PRIOR_BATCHES;
}
```

`CLOSED` rejects pushes during resolution unless the current effect is
authorized to enqueue a declared child batch. `OPEN_CURRENT_BATCH` allows a
child effect to enqueue entries and makes them eligible according to the
configured ordering. `OPEN_DEFERRED` records new entries separately and
merges them only at a defined barrier. The engine must not silently choose a
policy based on the callback that happens to run.

Each pending entry carries a stable entry ID, source/event IDs, controller,
priority group, insertion sequence, effect ID, serialized arguments, and
visibility metadata. Function pointers are never stored in a stack entry.
The resolver should process entries iteratively:

```text
begin transaction and resolution context
while eligible entries exist:
    select next entry using ordering and deterministic tie-breakers
    validate admission and capability rules
    execute effect into an uncommitted operation batch
    validate and commit the batch
    enqueue child entries according to stack policy
    enforce effect, event, depth, and total-work limits
commit or roll back according to failure policy
```

A callback must not mutate the stack or match state directly. It emits a
declared operation/entry batch that the engine validates. This makes
`LIFO`/`FIFO`, same-pass/deferred behavior, and rollback testable and makes
replays independent of callback pointer identity.

### Card and entity types

Card types are data, not an enum frozen into Libft. A type definition may
declare:

- parent types or composable traits;
- allowed zones and board spaces;
- available stats/resources;
- legal commands;
- default triggers and effects;
- whether it becomes a persistent board entity;
- targeting capabilities;
- serialization visibility.

Examples such as creature, spell, equipment, land, hero, action, or reaction
belong in game configuration.

### Stats, damage, and healing

Stats are typed definitions with configurable bounds and reset behavior:

- signed/unsigned integer;
- fixed-point value if needed;
- current/base/maximum relationships;
- temporary and permanent modifiers;
- additive, multiplicative, override, minimum, and maximum layers;
- expiration event or duration.

Do not force one health representation. A game may configure:

- current health reduced by damage;
- accumulated damage compared with toughness;
- damage markers removed at a configured phase;
- a custom callback-defined model.

“Creatures heal damage at end of turn” can therefore be represented as either a
generic reset operation on a damage counter or a registered callback. The
engine executes the configured event and effect; it does not assume the rule.

### Turn and timing model

Configure a directed phase graph rather than a fixed sequence. Each phase/step
defines:

- stable ID and name;
- active participant selection;
- entry and exit events;
- allowed command categories;
- automatic actions;
- priority/pass policy;
- next-phase conditions;
- repeat/skip/extra-turn behavior;
- timeout policy supplied by the host;
- cleanup rules.

This supports alternating turns, simultaneous planning, reaction windows,
real-time rounds, and games without turns.

### Events, triggers, and effects

Every meaningful transition emits a typed event with a deterministic sequence
number. Trigger definitions include:

- event type;
- source and controller filters;
- conditions;
- target selector;
- effect list;
- once/per-turn/limited-use policy;
- priority/order group;
- optional/mandatory behavior;
- duration and expiry.

Effects may be built-in generic operations or native callback effects. Keep the
built-in set small and composable:

- set/add/remove stat or counter;
- move card/entity between zones/spaces;
- create/destroy entity or token;
- draw/reveal/shuffle;
- damage/heal;
- add/remove modifier;
- emit event;
- schedule delayed effect;
- request configured random choice;
- invoke registered callback.

### Conditions and targeting

Conditions and selectors should be declarative trees compiled into validated
runtime nodes. Support AND, OR, NOT, comparisons, tags, ownership/control,
zone/space membership, stat/resource values, event fields, and callback-backed
predicates.

Selectors produce candidate IDs, then legality filters and cardinality rules
apply. Configuration controls minimum/maximum target count, uniqueness,
ordering, optionality, and whether choices are public or hidden.

## Command and authority model

Players submit intents, never direct state mutations:

```text
play card X from hand to space Y
activate ability A with targets T
pass priority
choose option O for pending choice C
```

The engine validates an intent against the current ruleset and match state,
produces deterministic operations, commits them transactionally, and emits a
result/delta. In a networked game, the server is authoritative and clients may
only predict commands whose rollback behavior is explicitly supported.

Every accepted command records:

- match ID;
- command sequence;
- participant ID;
- command type and payload;
- ruleset/configuration hash;
- state hash before and after;
- generated deterministic operations;
- random draws/choices or the random-stream position.

## Determinism

Determinism is mandatory:

- never use wall-clock time in rule resolution;
- use a match-owned seeded random stream;
- define stable iteration order for all registries, zones, events, targets, and
  simultaneous triggers;
- do not use pointer addresses as ordering or identity;
- use fixed-width serialized values;
- callbacks receive deterministic services only;
- callback capability declarations identify whether a callback is safe for
  replay/network execution;
- hash the resolved ruleset and callback registry contract before a match.

The same initial state plus command log and random seed must produce the same
state hashes on Linux, macOS, and Windows.

## Effect resolution and safety

Use an explicit iterative resolution queue, not recursive callback execution.
This prevents stack exhaustion and makes ordering observable.

```text
validate command
produce root operations
enqueue resulting events
collect matching triggers
order triggers deterministically
resolve next effect into proposed operations
validate and commit operations
enqueue resulting events
repeat until queue empty or configured safety limit reached
```

Configure hard safety limits:

- maximum effects resolved per command;
- maximum events emitted;
- maximum queued choices;
- maximum entities/tokens created;
- maximum trigger depth/causal chain;
- callback execution budget supplied by the host if required.

Exceeding a limit returns a deterministic error and leaves the transaction in a
defined state. Prefer validate-then-commit operation batches. If callbacks can
partially build a batch and then fail, discard the uncommitted batch.

## State, snapshots, deltas, and replay

Separate:

- immutable resolved ruleset;
- mutable authoritative match state;
- private per-participant views;
- command/event log;
- network/persistence deltas.

Snapshots must support redaction so one participant does not receive another
participant's hidden cards. Deltas describe final state changes and required
public events, not arbitrary pointers or callback-local data.

Provide canonical serialization and hashing. Version every format. Loaders are
transactional and reject unknown incompatible versions, invalid counts,
duplicate IDs, missing callbacks, invalid references, and arithmetic overflow.

## Suggested module files

```text
Modules/CardGame/
  card_game.hpp
  card_game_types.hpp
  card_game_ruleset.hpp/.cpp
  card_game_config.hpp/.cpp
  card_game_registry.hpp/.cpp
  card_game_match.hpp/.cpp
  card_game_command.hpp/.cpp
  card_game_operation.hpp/.cpp
  card_game_event.hpp/.cpp
  card_game_effect.hpp/.cpp
  card_game_target.hpp/.cpp
  card_game_turn.hpp/.cpp
  card_game_zone.hpp/.cpp
  card_game_board.hpp/.cpp
  card_game_snapshot.hpp/.cpp
  card_game_delta.hpp/.cpp
  card_game_replay.hpp/.cpp
  card_game_hash.hpp/.cpp
```

Split files further when they become large. The umbrella header should expose
only public APIs; consumers include individual headers for individual needs.

## Card-engine implementation phases

### Phase 1: schemas and immutable ruleset

- Define IDs, descriptors, registries, configuration schema, validation, and
  canonical ruleset hash.
- Implement transactional loading from in-memory descriptors first.
- Add JSON/config adapters only after the core schema is stable.
- Register native callbacks by stable name.

### Phase 2: match state and generic operations

- Participants, zones, cards/entities, board spaces, stats/resources.
- Command envelope and validator.
- Transactional generic operation batches.
- Canonical snapshot and state hash.

### Phase 3: events, triggers, turns, and effects

- Configurable phase graph.
- Event queue and deterministic trigger ordering.
- Declarative conditions/targeting.
- Native effect callback invocation.
- Choices and pending decisions.

### Phase 4: deltas, replay, and authority support

- Redacted participant views.
- Command log and deterministic replay.
- Snapshot/delta versioning.
- Server-authoritative validation interfaces.
- Client prediction hooks only after authoritative behavior is proven.

### Phase 5: advanced mechanics

- Replacement/prevention effects.
- Continuous modifiers and dependency ordering.
- Simultaneous decisions.
- Configurable priority/stack systems.
- AI query interface.
- Scripting adapter as a separate optional layer.

## Card-engine test plan

### Configuration

- Empty minimal ruleset.
- Duplicate and missing IDs.
- Missing callback registrations.
- Invalid board adjacency and zone references.
- Phase cycles that are allowed versus accidental non-progress cycles.
- Invalid stat bounds and arithmetic overflow.
- Transactional failure at every allocation point.
- Canonical hash independent of insertion order where the schema declares order
  irrelevant.

### Rules execution

- Configurable board sizes including zero, one, and large sparse graphs.
- Custom card types and tags with no core code changes.
- Move cards through configured zones.
- Deck order is preserved through draw, peek, top/bottom insertion, indexed
  insertion, removal, and repeated deterministic shuffles.
- Invalid deck indexes, duplicate instance IDs, forbidden zone operations, and
  capacity overflow leave the deck byte-for-byte unchanged.
- LIFO and FIFO stacks resolve in exact configured order, including equal-
  priority tie-breaking by insertion sequence.
- Closed stacks reject in-resolution pushes; current-batch admission and
  deferred admission each resolve at their documented barrier.
- Same-pass and next-pass child effects produce distinct, deterministic traces
  and state hashes.
- End-phase damage cleanup/healing through a configured trigger and callback.
- Temporary modifiers expiring on configured events.
- Optional and mandatory triggers.
- Multiple simultaneous triggers with deterministic ordering.
- Target minimum/maximum, uniqueness, and invalid target rejection.
- Callback failure discards uncommitted operations.
- Infinite trigger loop stopped by deterministic safety limits.

### Determinism and networking foundations

- Replay the same command log thousands of times and compare every state hash.
- Replay logs containing draws, inserts, and shuffles and compare exact hidden
  and public zone views using the same random-stream positions.
- Serialize/load every ordered-zone and resolution-stack format at every byte
  truncation boundary; failed loads preserve the previous state.
- Verify that a failed effect stack transaction restores both match state and
  ordered-container contents.
- Cross-platform golden vectors for snapshots, deltas, and ruleset hashes.
- Different registration/insertion orders produce the required canonical order.
- Hidden information is absent from unauthorized participant snapshots/deltas.
- Malformed and truncated serialized input at every byte boundary.
- Old/new version compatibility according to an explicit matrix.

### Concurrency and lifecycle

- Immutable ruleset shared by many matches.
- One authoritative execution owner per match initially; document external
  synchronization instead of adding unsafe optional mutex transitions.
- Concurrent read-only snapshots only after a safe snapshot architecture is
  implemented.
- Destroy during active host callback is rejected or waits through a permanent
  transition mechanism.
- TSan for supported concurrent paths.
- CMA failure injection and callback registry failure injection.

### Fuzzing

- Ruleset descriptor/config parser.
- Command payloads.
- Snapshot/delta/replay deserializers.
- Condition and target trees.
- Event/effect queues under bounded random rulesets.
- Differential replay: direct execution versus serialize/reload/replay.

# Part IV: Custom scripting module and Lua removal

## Objective and module boundary

Create `Modules/Scripting` as a fully custom language runtime owned by Libft.
It must perform the jobs currently handled by Lua without embedding Lua or
another third-party parser, compiler, bytecode VM, JIT, garbage collector, or
standard library. The goal is behavioral replacement of Libft's scripting use
cases, not source or ABI compatibility with Lua.

Game, Voxel, CardGame, and future consumers must depend on typed Scripting
interfaces rather than VM internals. Scripts must never receive raw engine
pointers or unrestricted access to files, sockets, processes, clocks, or native
libraries. All host interaction goes through explicitly registered capabilities
and validated function calls.

The existing Lua implementation must remain available only during migration.
Do not delete it until all current scripts and bridges pass parity tests against
the custom runtime. A temporary dual-runtime comparison mode may exist in tests
and development builds, but it must not become a permanent public compatibility
layer or enter normal release builds.

## Required migration inventory

Before designing syntax around assumptions, record every current dependency:

- `Modules/Lua` and `Modules/Lua/vendor/lua-5.4.8`;
- `mk/modules/Lua.mk` and all archive/dependency-graph references;
- `game_lua_runtime.cpp` and every public or internal caller;
- `game_scripting_bridge.*`, including every native function exposed to scripts;
- `terrain_scripting_bridge.hpp` and `voxel_terrain_scripting_bridge.cpp`;
- all script files, test fixtures, documentation, configuration, and examples;
- lifecycle, allocation, error, logging, and thread assumptions made by callers.

Produce a migration table with one row per script entry point: current Lua
name, arguments, result, side effects, determinism requirements, required
capability, replacement native-function ID, and parity-test location. Lua may be
removed only when every row is complete.

## Language specification

Write and review a versioned language specification before implementing the
parser. The first version should deliberately be small and predictable:

- UTF-8 source with a precise identifier policy, comments, and source locations;
- null, `ft_bool`, signed and unsigned fixed-width integers, deterministic
  fixed-point numbers, strings, arrays, records/maps, and function references;
- constants, local variables, functions, conditionals, bounded loops, returns,
  and explicit module imports;
- typed native calls and structured values rather than a dynamically shaped C
  stack interface;
- checked arithmetic with defined overflow, divide-by-zero, shift, and conversion
  behavior;
- stable map iteration order and canonical value serialization;
- no `eval`, native dynamic loading, pointer arithmetic, implicit host globals,
  or finalizers that can execute arbitrary script code;
- recursion disabled initially or constrained by a hard call-depth budget.

Floating-point behavior differs across platforms and compiler modes. Gameplay,
terrain, card rules, save data, and replay-affecting operations should therefore
use integers or a specified fixed-point representation. If floating point is
added later, its permitted operations and canonicalization rules require
cross-platform vectors before deterministic code may use it.

Version the source language, bytecode format, native ABI, and serialized script
state separately. Never execute bytecode whose declared versions or integrity
checks are unsupported.

## Compilation pipeline

Use an explicit, independently testable pipeline:

```text
source -> lexer -> parser -> AST -> semantic validation
       -> bytecode compiler -> bytecode verifier -> immutable module
```

Compilation must be transactional. A failed lex, parse, type, allocation,
registration, or verification step leaves the previously installed module
unchanged. Diagnostics contain a stable error code, source/module path, line,
column, byte range, and human-readable message. Multiple diagnostics may be
collected within a configured bound, but allocation failure must still return a
reliable primary error.

Use a compact stack-based VM initially because its stack effects are simple to
specify and verify. Every opcode must define operand encoding, stack inputs and
outputs, failure behavior, budget cost, and control-flow rules. The verifier
must reject invalid opcodes, truncated operands, out-of-range constants,
illegal jumps, inconsistent stack depths, invalid call signatures, excessive
resource declarations, and unreachable malformed instruction streams before a
module can execute.

Serialized bytecode uses a canonical byte order and includes magic, format
version, language version, section lengths, capability requirements, source or
content hash, and integrity validation. Source compilation remains the trusted
default until bytecode loading and its verifier are fuzzed thoroughly.

## Runtime and memory model

Each `scripting_vm` owns bounded stacks, frames, temporary values, and script
heap state. Immutable verified modules may be shared; mutable execution state
may not be shared concurrently. Use Libft allocation and container facilities,
`ft_memcpy`, and `ft_memset`, following root `AGENTS.md`. Public APIs return
Libft error codes and must not rely on C++ exceptions.

Every execution receives hard limits for:

- instructions and backward branches;
- call depth and value-stack depth;
- heap bytes, object count, string length, and collection length;
- native calls, yielded operations, diagnostics, and returned data.

Prefer invocation- or VM-owned arenas plus deterministic collection at explicit
safe points. Do not allow unpredictable garbage-collection pauses in arbitrary
instructions. If cyclic object graphs are supported, use a bounded tracing pass
at documented safe points; otherwise reject cycles in the first version. All
allocation paths must work with the tester-only CMA failure-injection framework.

Budget exhaustion, script errors, and native callback errors terminate only the
current invocation and roll back its uncommitted host operations. Verifier or VM
invariant failures are distinct internal errors and must produce diagnostics
without accepting further execution from the affected module.

## Native function and capability interface

Native functions are registered before use under stable numeric IDs and names,
with declared argument and result schemas. Never serialize function pointers.
The registry resolves configuration names to IDs transactionally, then becomes
immutable and receives a canonical hash.

A callback interface should carry only validated data and an opaque host context:

```cpp
typedef int32_t (*scripting_native_callback)(
    const scripting_call_context *context,
    const scripting_value *arguments,
    ft_size_t argument_count,
    scripting_value *result,
    void *user_data) noexcept;
```

Exact ownership and lifetime rules for arguments, results, strings, collections,
and `user_data` must be documented. Callbacks return meaningful error codes;
tests must assert those returns rather than casting them to `void`.

Capabilities are granted per loaded module or execution instance. Initial
capabilities should be narrowly separated, for example terrain query, generated
terrain write, game-state query, validated game command, CardGame effect,
deterministic RNG, and bounded logging. A script requesting an undeclared or
ungranted capability fails to load or call. The default environment grants no
filesystem, network, process, wall-clock, environment-variable, or native-module
access.

Scripts should return proposed commands or operation lists for validation and
transactional application by Game, Voxel, or CardGame. They must not mutate
authoritative world or match state directly. This preserves server authority,
supports replay, and prevents partially applied effects after callback failure.

## Determinism, sandboxing, and replay

The same verified module, inputs, configuration, and seed must produce the same
result and operation sequence on Windows, Linux, and macOS. Enforce this with:

- host-supplied deterministic RNG streams, never process-global randomness;
- no wall clock, thread identity, address, locale, filesystem-order, or hash-table
  iteration as script-visible inputs;
- canonical ordering and serialization for maps, modules, operations, and state;
- explicit integer/fixed-point arithmetic semantics;
- deterministic instruction and resource budgets rather than wall-time limits.

Saved script state may contain only approved serializable values. Include the
language version, state format version, module hash, native-registry hash, and
schema hash. Reject incompatible state transactionally. Replays store stable
script/function/effect IDs, inputs, seeds, and resulting validated operations,
never pointers or process-specific handles.

## Concurrency and hot reload

One mutable VM instance has one execution owner. Run parallel scripts through
separate execution contexts sharing immutable modules. Do not solve optional
thread safety by creating and destroying mutexes while access may be active.

Do not hold registry, world, match, or module-management locks while invoking a
host callback, performing I/O, joining threads, or waiting for script work.
Asynchronous integration uses explicit yield/resume opcodes and opaque host task
handles; suspension is allowed only at verified safe points and remains subject
to lifetime and budget checks.

Hot reload compiles and verifies a new immutable module generation, validates
its capabilities and state migration, and atomically publishes it at a game,
world, or frame boundary. Existing invocations keep their old generation until
they drain. Failed reload leaves the active generation and state untouched.

## Analytics integration

The Scripting module should expose optional Analytics scopes for compile phases,
module load, VM execution, native calls, allocation/collection safe points, and
budget failures. Instrumentation must use the low-overhead worker event-buffer
design described in Part II, avoid double-counting invocations, and preserve
correct minimum, rolling-average, percentile, frame-breakdown, and cross-thread
flow semantics. It must compile out when `LIBFT_ENABLE_ANALYTICS` is disabled;
profiling macros and tester hooks must not appear in ordinary release archives.

## Suggested module layout

```text
Modules/Scripting/
    scripting.hpp
    scripting_types.hpp
    scripting_diagnostics.hpp/.cpp
    scripting_lexer.hpp/.cpp
    scripting_parser.hpp/.cpp
    scripting_ast.hpp/.cpp
    scripting_compiler.hpp/.cpp
    scripting_bytecode.hpp/.cpp
    scripting_verifier.hpp/.cpp
    scripting_value.hpp/.cpp
    scripting_vm.hpp/.cpp
    scripting_module.hpp/.cpp
    scripting_registry.hpp/.cpp
    scripting_native.hpp/.cpp
    scripting_snapshot.hpp/.cpp
    README.md
```

The umbrella header exists for compatibility, but production consumers should
include only the individual headers they require.

## Implementation and Lua-removal phases

Implementation status: the custom Scripting runtime is now the production
runtime for Game and Voxel. The legacy interpreter and its build integration
have been removed; only historical test fixtures may retain old filenames
until they are independently renamed.

1. Inventory and freeze every existing Lua entry point and script behavior.
2. Approve language, bytecode, numeric, ownership, error, and capability specs.
3. Implement lexer, parser, AST, diagnostics, and malformed-input tests.
4. Implement compiler, bytecode format, verifier, and golden vectors.
5. Implement bounded VM, values, memory management, deterministic RNG, and
   sandbox budgets.
6. Implement immutable native registry and transactional operation interface.
7. Port the terrain bridge and compare generated outputs against Lua fixtures.
8. Port Game scripting and compare state transitions and errors against Lua.
9. Integrate CardGame effects only after its native operation API is stable.
10. Migrate scripts/configuration and run custom-runtime parity tests.
11. Switch all production build dependencies to Scripting.
12. Remove `Modules/Lua`, vendored Lua, Lua build manifests, Lua symbols,
    compatibility shims, and obsolete tests/documentation. **Completed for
    production code in this implementation batch.**

The terrain migration may use a compatibility normalizer for existing
line-oriented configuration assets, but normalized source must still be
compiled and executed by `Modules/Scripting`; it must never fall back to Lua.
Add a parity fixture covering the old newline-separated form and the canonical
semicolon-separated form, and require identical resolved terrain configuration,
generated chunk bytes, diagnostics, and operation-budget behavior. The Game and
Voxel bridges now select the custom runtime directly. The line-command adapter
is retained only as a source-compatibility normalizer; it does not invoke an
external interpreter.

Each phase must be independently reviewable. Do not combine interpreter
construction, bridge migration, and Lua deletion into one change.

## Scripting test plan

- Lexer/parser tests for every token, construct, malformed input, UTF-8 edge,
  depth limit, integer boundary, and allocation failure.
- Compiler and verifier golden vectors plus rejection of every truncated bytecode
  boundary, invalid jump, stack mismatch, bad signature, overflowed section, and
  unsupported version.
- Per-opcode VM tests, checked arithmetic, branch/loop behavior, stack and call
  limits, instruction budgets, memory limits, cancellation, and rollback.
- Native registration collisions, missing functions, bad argument/result types,
  capability denial, callback failure, ownership, and re-entrant misuse.
- Deterministic repeated execution and cross-platform golden hashes for values,
  bytecode, terrain output, Game operations, snapshots, and replay records.
- Terrain and Game parity tests running canonical fixtures through the custom
  runtime, including normalized line-oriented input and typed callback results.
- Isolation tests across worlds, matches, VMs, threads, reload generations, and
  simultaneous immutable-module readers; run supported paths under TSan.
- Hot-reload success, compile failure, incompatible-state migration, active-call
  draining, and allocation failure at every transactional step.
- Fuzz the lexer, parser, bytecode loader/verifier, value deserializer, VM, and
  native-call boundary with strict budgets.
- ASan, UBSan, TSan, lifecycle-abort, CMA failure-injection, clean-build,
  incremental-build, and archive-integrity coverage on every supported platform.
- Release checks proving no Lua objects/symbols/vendor paths, tester-only hooks,
  profiling code when disabled, or unintended filesystem/network APIs remain.

## Lua-removal acceptance criteria

- Every migration-inventory row has a passing custom-runtime parity test.
- All production Game and Voxel callers use typed Scripting interfaces.
- No production include, link, archive, Makefile, configuration, or source
  dependency refers to Lua or its ABI.
- Repository searches find no Lua production source, build, include, or link
  dependency. Historical documents and legacy fixture names are isolated from
  production targets.
- Deterministic vectors match across Windows, Linux, and macOS.
- Full CI and relevant sanitizer, fuzz, failure-injection, lifecycle, clean, and
  incremental build suites pass after the Lua module is physically removed.

# Part V: Secure-channel and RW-lock hardening

This section records the follow-up review of the secure channel and optimized
custom RW-lock. These items are implementation requirements, not optional
cleanup. The common rule is that cryptographic state, ownership state, and lock
admission state must never be published partially when a fallible operation
fails.

## Findings confirmed against the current implementation

| Area | Current behavior | Required disposition |
| --- | --- | --- |
| Send key rotation | Derives the new material, destroys the live backend, then initializes the replacement | Fix transactionally before relying on rotation for recovery |
| Combined key rotation | Destroys both live backends before initializing replacements; one backend can be new while public metadata is old | Fix as one all-or-nothing commit |
| Receive key rotation | Publishes previous-key fields before the new current backend is known to be usable | Prepare both backend objects and all metadata privately |
| Channel move | Copies metadata and moves backends sequentially; a later move failure leaves partial ownership | Make move prevalidated/infallible or commit through a complete temporary state |
| RW-lock reader ownership | Fixed TLS array rejects the 65th distinct held lock with `FT_ERR_NO_MEMORY` | Document and replace with a distinct capacity result plus a spill strategy |
| Cancelled writer tickets | Repeatedly scans and erases a buffer, shifting entries | Add contention benchmarks; replace with ordered/indexed cancellation if measurable |

The current crypto backend's `destroy()` is effectively infallible, but the
rotation contract must not depend on that forever. Failure injection must cover
derive, temporary initialization, old-backend retirement, and move/ownership
steps. A retirement failure must have an explicitly defined result: either the
old backend remains owned and usable, or the operation returns success only
after ownership has been safely transferred to a deferred-retirement object.

## Transactional key rotation

All key-update APIs must preserve this invariant on every non-success return:

```text
current send backend + send key + send IV + send epoch remain usable
current receive backend + receive key + receive IV + receive epoch remain usable
previous receive backend and replay metadata remain exactly as before
packet counters and replay windows are unchanged
```

For `update_send_key_epoch(next_epoch)`:

1. Validate initialization and strictly increasing epoch without changing state.
2. Derive the next key and IV into local wiped buffers.
3. Initialize a separate temporary crypto backend with the derived key.
4. If either step fails, wipe temporary material and return while leaving the
   current backend untouched.
5. Commit the temporary backend, key, IV, epoch, and send packet-window reset as
   one internal state transition. The old backend becomes a retirement object.
6. Destroy the retired backend only after the new backend is owned by the
   channel. If retirement can report failure, retain it for safe deferred
   cleanup and report that condition without invalidating the new channel.
7. Wipe all temporary key and IV buffers on every path, including allocation,
   initialization, commit, and test-injection failures.

For `update_receive_key_epoch(next_epoch)`, prepare both of these before
publishing any previous-key fields:

```text
temporary previous backend = current receive key
temporary current backend  = derived next receive key
temporary previous metadata = current key/IV/epoch/replay window
temporary current metadata  = next key/IV/epoch/empty replay window
```

Only after both backends initialize successfully may the channel commit the
new current backend and the previous backend together. A failed update must not
set `_has_previous_receive_key`, change previous epochs, destroy the current
backend, or reset the current replay window. `clear_previous_receive_key()`
must remain independently safe after a successful rotation.

For `update_key_epoch(next_epoch)`, use one preparation object containing both
directional backends and all directional metadata. Do not implement it as two
independent public updates: the send and receive halves must either both move
to the new epoch or neither does. If send preparation succeeds but receive
preparation fails, destroy only the temporary send backend and prove that the
old channel still seals and opens a packet.

The implementation should use a private temporary-state helper or an explicit
transaction structure. It must not use exceptions for rollback. Every helper
returns a Libft error code, and tests must check each meaningful return rather
than casting it to `void`.

## Transactional channel move

`networking_secure_channel::move()` must not destroy the destination or mutate
the source until a complete destination state is available. Preferred order:

1. Validate that the source is initialized and that all required source
   backends are present and internally initialized.
2. Construct a temporary channel state and transfer/copy every key, IV, epoch,
   replay window, packet counter, and presence flag into it.
3. Transfer all required backends into the temporary state. Backend transfer
   should be made infallible after validation, or expose a non-destructive
   `prepare_move`/swap operation. A fallible operation must not consume its
   source on failure.
4. Commit the complete temporary state into the destination.
5. Wipe and destroy the old destination only after commit, then mark the source
   destroyed in one final step.

If the existing backend API cannot support this safely, add a backend swap or
non-consuming clone of initialized key state rather than attempting three
sequential destructive moves. The channel must satisfy: after any injected
failure, either the original source remains fully usable or the destination is
fully usable; never split send/receive/previous-receive ownership between them.

## Required secure-channel tests

Add test-only, release-compiled-out failure controls with named stages rather
than byte/allocation counting. Cover every derive, temporary backend initialize,
retirement/destroy, backend move, and commit stage for send-only, receive-only,
and combined rotation. For each injected failure:

- assert the exact returned error;
- assert send and receive epochs and previous-key visibility are unchanged;
- seal with the old sender and open with the old receiver;
- verify packet counters and replay acceptance are unchanged;
- retry the same rotation without failure and verify the new epoch works;
- verify all secret buffers and temporary backends are cleaned up.

For receive rotation, test old-epoch packets during the permitted previous-key
window, then test `clear_previous_receive_key()`. For moves, inject failure at
each backend transfer and test both source and destination from independent
seal/open directions. Run these tests under ASan, UBSan, TSan, and CMA failure
injection on Linux, macOS, and Windows. Release archive checks must prove that
failure hooks and test symbols are absent when `LIBFT_TEST_BUILD` is disabled.

## RW-lock ownership capacity and cancellation

The optimized reader path now stores entries in a 64-entry inline thread-local
cache and spills additional distinct locks into a Libft-backed thread-local
buffer. The inline size is therefore only a fast-path cache size, not a public
ownership ceiling. Spill allocation failure must be reported distinctly from a
cache-capacity condition, leave the lock unacquired, and preserve writer
admission invariants. `FT_ERR_RWLOCK_READER_CAPACITY` remains reserved for a
future genuinely bounded configuration; ordinary spill allocation failures
continue to use the appropriate memory error.

Required tests hold 63, 64, and 65 distinct read locks on one thread, then test
read unlock, writer progress, duplicate/nested acquisition policy, and cleanup
after a failed admission. Repeat with mixed fast-path and normal-path locks.
The implemented inline-cache-plus-spill design must be extended with explicit
spill allocation-failure injection. That failure must be reported without
incrementing the fast-reader count or leaving an ownership entry behind.

For cancelled writers, add deterministic stress tests and benchmarks with
hundreds and thousands of timed-out tickets, cancellations at the head/middle/
tail of the queue, ticket wraparound, and concurrent reader admission. Assert
that every live writer eventually progresses, cancelled tickets never acquire,
and queue state returns to its zero-ticket baseline. Measure cancellation and
unlock latency separately from ordinary uncontended read/write latency. If the
linear scan/erase becomes visible, replace it with ordered cancellation tickets
or a ticket-indexed structure while preserving FIFO writer semantics and
transactional allocation failure behavior.

## Review gates

Do not mark secure-channel rotation complete until failure-injection tests prove
old-state usability after every failed preparation step. Do not mark the RW-lock
optimization complete until the capacity contract is explicit, the 65-lock case
has defined behavior, and cancellation-heavy benchmarks show no unacceptable
queue degradation. Keep all diagnostics, fault hooks, and stress-only controls
behind test/build feature definitions so ordinary release archives contain no
tester-only instrumentation.

# Delivery order and review gates

Recommended order:

1. compression short-read and write-all fixes plus deterministic tests;
2. compression options lifecycle/thread-safety redesign plus TSan tests;
3. Analytics core registry, thread buffers, scopes, frames, and snapshots;
4. Analytics exporters and Minecraft integration;
5. CardGame immutable ruleset and callback registry;
6. CardGame match state and operation engine;
7. CardGame events, configurable phases, triggers, and replay foundations;
8. scripting inventory and approved language/bytecode specifications;
9. custom compiler, verifier, VM, sandbox, and native registry;
10. terrain and Game bridge migration with differential parity tests;
11. production cutover followed by complete Lua dependency removal.

Each stage must pass:

- repository policy checks;
- clean and incremental builds;
- release archive checks for test-only symbols;
- Linux, macOS, and Windows tests;
- sanitizer suites relevant to the stage;
- failure-injection tests;
- API and format documentation review.

Analytics and CardGame are now exposed through stable module headers and are
included by `FullLibft.hpp` for compatibility. New direct consumers should
prefer the specific headers they use. The instrumentation surface remains
controlled by the `LIBFT_ENABLE_ANALYTICS` build definition; release builds
must leave it disabled unless profiling is explicitly requested.
