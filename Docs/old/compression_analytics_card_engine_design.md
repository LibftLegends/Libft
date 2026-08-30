# Compression hardening, runtime analytics, and card-engine foundations

## Document status

This document is an implementation handoff. It describes three independent
work streams:

1. hardening generic file-descriptor compression streams;
2. adding a low-overhead runtime analytics module for frame and function timing;
3. laying the foundations for a configurable card-game rules engine.

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
- End-phase damage cleanup/healing through a configured trigger and callback.
- Temporary modifiers expiring on configured events.
- Optional and mandatory triggers.
- Multiple simultaneous triggers with deterministic ordering.
- Target minimum/maximum, uniqueness, and invalid target rejection.
- Callback failure discards uncommitted operations.
- Infinite trigger loop stopped by deterministic safety limits.

### Determinism and networking foundations

- Replay the same command log thousands of times and compare every state hash.
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

# Delivery order and review gates

Recommended order:

1. compression short-read and write-all fixes plus deterministic tests;
2. compression options lifecycle/thread-safety redesign plus TSan tests;
3. Analytics core registry, thread buffers, scopes, frames, and snapshots;
4. Analytics exporters and Minecraft integration;
5. CardGame immutable ruleset and callback registry;
6. CardGame match state and operation engine;
7. CardGame events, configurable phases, triggers, and replay foundations.

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
