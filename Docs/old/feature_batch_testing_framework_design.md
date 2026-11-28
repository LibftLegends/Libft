# Feature-batch verification framework and CardGame simulation design

## Document status

This is the implementation handoff for thoroughly testing the feature batch
introduced on `agent/compression-analytics-cardgame-scripting`. It covers:

- Compression streaming and its options object;
- Analytics collection, frame summaries, worker events, flows, and exporters;
- CardGame rules, cards, decks, ordered zones, resolution stacks, snapshots,
  deltas, commands, and replay;
- the custom Scripting lexer, parser, compiler, bytecode verifier, VM, native
  boundary, and Game/Voxel bridges;
- Networking secure-channel key rotation and move transactionality;
- PThread RW-lock reader spill storage and cancelled-writer handling;
- integration between these modules.

The goal is not a larger collection of happy-path unit tests. The goal is a
framework that can prove four properties for every fallible operation:

1. it returns the documented result;
2. it leaves all observable and internal state valid;
3. it can be retried or destroyed according to its contract;
4. the same inputs produce the same result on Windows, Linux, and macOS.

All implementation must follow the repository-root `AGENTS.md`. Test-only
hooks must be compiled only under `LIBFT_TEST_BUILD` and must not add fields,
branches, symbols, mutable globals, or overhead to release archives.

## Required outcomes

The completed framework must provide:

- deterministic failure injection by named operation, not guessed byte counts;
- reusable lifecycle, transaction, serialization, and deterministic-state
  assertions;
- exact test coverage manifests for every public method and internal failure
  point introduced by the feature batch;
- reproducible random and property tests that always print and persist a seed;
- scenario simulations that show how a game embeds and uses CardGame;
- server/client simulations proving command validation and delta convergence;
- sanitizer, fuzz, stress, compile-out, archive, and cross-platform checks;
- useful diagnostics containing the last test, seed, command index, state hash,
  failure point, and minimal replay data.

## Testing architecture

### Test layers

Use five layers. A failure in a lower layer blocks reliance on higher layers.

| Layer | Purpose | Examples |
|---|---|---|
| Contract unit tests | Validate one method and every return path | invalid argument, full queue, destroyed state |
| Transaction tests | Prove failure does not partially publish state | failed key rotation, failed snapshot apply |
| Component-model tests | Compare implementation with a simple reference model | deck order, resolution order, delta application |
| Scenario simulations | Exercise public APIs in realistic sequences | two-player duel, end-turn healing |
| Stress/fuzz/system tests | Explore long sequences and hostile input | 100,000 commands, truncated bytecode, TSan contention |

Unit tests must not duplicate a production algorithm as their oracle. A model
should be deliberately simpler, such as a fixed array and explicit shifting for
a deck, even if production uses a different representation.

### Tester-only deterministic failure controller

Create a general tester-owned controller under `Test/Test/` based on the useful
parts of `networking_test_hooks` and `test_cma_failure_injection`. It must not
modify CMA and must not be linked into release archives.

Suggested files:

```text
Test/Test/test_failure_controller.hpp
Test/Test/test_failure_controller.cpp
Test/Test/test_transaction_oracles.hpp
Test/Test/test_seeded_scenario.hpp
```

The controller identifies a failure by a stable module and operation ID:

```text
module:    SCRIPTING
operation: PROGRAM_STRING_APPEND
occurrence: 3
error:     FT_ERR_NO_MEMORY
```

Required operations:

- begin a clean injection session;
- fail the next occurrence of one named point;
- fail after N successful occurrences;
- return the number of attempts and failures for a point;
- reset one point or the complete controller;
- reject unknown points and overlapping incompatible sessions;
- print the active point and occurrence in failure diagnostics;
- explicitly end the session and verify that no armed failure remains.

Do not infer failure locations from allocation sizes. Modules may expose a
small test-only call at the exact semantic operation boundary, while the
controller and all policy remain in the tester. Examples include “allocate
temporary receive backend”, “append trace event”, and “commit parsed string”.

Each hook must follow this shape:

```cpp
#ifdef LIBFT_TEST_BUILD
if (test_failure_should_fail(TEST_FAILURE_SCRIPTING_PROGRAM_STRING_APPEND)
    == FT_TRUE)
    return (FT_ERR_NO_MEMORY);
#endif
```

No hook may change normal control flow when no failure is armed. A release
symbol scan and preprocessor/build test must prove that the hook names and
controller references are absent from release objects.

### Automatic failure-point sweep

For each transactional API, provide a helper that:

1. creates a valid baseline object;
2. captures a canonical snapshot/hash of all relevant state;
3. runs the operation without failure and records how many times each named
   fallible point is reached;
4. repeats the operation once for every reachable occurrence;
5. injects failure at that exact occurrence;
6. verifies the expected error, unchanged source, documented destination state,
   no leaked allocation, and successful cleanup;
7. retries without injection and verifies success.

The sweep must fail if a declared failure point is never reached. This prevents
dead test hooks and tests that appear comprehensive without executing the
intended branch.

### Reusable transaction oracles

Add tester-only helpers for these recurring assertions:

- `expect_unchanged(before, after)` using a canonical snapshot or hash;
- `expect_destroyed_and_reusable(object)`;
- `expect_source_preserved_after_failed_move(source)`;
- `expect_retry_succeeds(operation)`;
- `expect_serialized_output_unchanged_on_failure(output)`;
- `expect_no_cma_allocation_delta(baseline_stats)`;
- `expect_equivalent_state(engine_a, engine_b)`;
- `expect_exact_error(expected_error)`.

Hashes are useful diagnostics but are not sufficient alone. For failed tests,
the oracle must report the first differing field or record index.

### Seed and replay artifacts

Every randomized test must use an explicit `uint64_t` seed. On failure, write:

- test name;
- seed;
- operation count and failing operation index;
- generated command or input record;
- state hash before and after;
- active failure point;
- a compact replay file when the tested module supports replay.

The seed must be overridable through an environment variable. CI should run a
small fixed seed corpus on every change and rotate additional seeds in a
scheduled workflow. A failure must be reproducible locally with one command.

### Coverage manifest

Create one Markdown or machine-readable manifest per module listing:

```text
public API -> lifecycle cases -> argument cases -> capacity cases
           -> failure points -> concurrency cases -> serialization cases
           -> owning test file(s)
```

CI must compare public declarations with this manifest and fail when a new
public method is added without a coverage row. This does not replace code
coverage; it prevents silent contract gaps that line coverage cannot detect.

## Common lifecycle and error matrix

Apply this matrix to every lifecycle class in Analytics, CardGame, Scripting,
Compression options, and secure Networking components.

### Construction and initialization

- Default construction never allocates, fails, or aborts.
- Initial state is exactly `FT_CLASS_STATE_UNINITIALISED`.
- Every owning member is null/empty and every counter is zero.
- Valid initialization succeeds and establishes all invariants.
- Every invalid argument returns the exact documented `FT_ERR_*` code.
- Calling `initialize` while initialized follows the repository lifecycle
  contract.
- Failure at each initialization stage cleans prior stages and leaves the
  object destroyed.
- Reinitialization from destroyed state succeeds.

Why: initialization is where partial ownership and stale state most often
enter lifecycle objects.

### Destruction

- Destroying uninitialized or already destroyed objects is successful and
  idempotent.
- Normal destruction releases every owned resource.
- Injected failure in each cleanup stage still runs later cleanup stages and
  returns the first error.
- Destructor execution never aborts and does not change thread-local error
  state.
- A test-build state inspection confirms fields are reset.

Why: cleanup failures must not turn one resource failure into several leaks.

### Move and copy-style initialization

- Self move is a no-op success.
- Valid move transfers all ownership and semantic state.
- Moving a destroyed source propagates destroyed state as documented.
- Uninitialized-source misuse aborts where required.
- Failure at every preparatory stage leaves the source usable and destination
  destroyed or unchanged according to the API contract.
- Retry after a failed move succeeds.

Why: secure-channel and container state cannot tolerate partial ownership
transfer.

### Error reporting

- Every public method sets `_last_error` on every required exit path.
- Successful calls clear a previous error.
- `get_error()` and `get_error_str()` abort on uninitialized objects and work
  on destroyed objects.
- Error state is thread-local and does not leak between two test threads.
- Positive recoverable errors and negative critical errors retain their signed
  meaning.

## Compression verification plan

### Stream state machine

Test compression and decompression with:

- empty input;
- one byte;
- exactly one input buffer;
- one byte below and above each buffer boundary;
- multi-megabyte compressible and incompressible data;
- one-byte producer fragments;
- randomized fragment boundaries;
- a short positive read followed by more data;
- repeated interrupted reads before success;
- EOF only after all fragments;
- invalid descriptors and closed descriptors;
- truncated compressed data at every byte boundary;
- trailing data and concatenated streams according to the documented policy;
- zlib initialization, processing, and cleanup failures;
- zero progress from read/write hooks;
- short writes of one byte, alternating sizes, and a final remainder;
- interrupted writes before and after a committed prefix;
- non-retryable write failure after a committed prefix;
- cancellation before input, during processing, and after partial output.

Why: generic descriptors are allowed to make partial progress. A short read or
write is not EOF and must neither truncate nor duplicate data.

For every successful case, decompress and compare every output byte. For every
failed case, verify zlib cleanup count, first returned error, no busy loop, and
the documented partial-output state.

### Compression options concurrency

Test snapshots, setters, reset, enable/disable compatibility behavior, and
destroy under:

- one reader and one writer;
- many readers and one writer;
- many writers;
- repeated policy transitions;
- destroy while access is active;
- mutex lock, unlock, initialization, and destruction failure injection;
- TSan with scheduled barriers that force each transition interleaving.

Why: the original defect class was deletion of a mutex while another thread
could still own or use it.

### Compression differential tests

For valid streams, compare decompressed output with the original input and, for
canonical test vectors, verify interoperability with a known zlib decoder.
Do not require compressed bytes to be identical across zlib versions unless the
configuration explicitly guarantees canonical output.

## Analytics verification plan

### Accounting invariants

For synthetic/manual-clock scopes, assert:

- every scope entry produces exactly one invocation;
- inclusive time equals complete scope duration;
- exclusive time equals inclusive time minus direct and indirect child time;
- siblings do not subtract from each other;
- recursion is counted once per invocation without corrupting parent time;
- disabled instrumentation records no invocation and performs no export;
- a validation/default zero sample never changes minimum timing;
- minimum is the smallest real sample and maximum is the largest;
- totals never double-count worker flushes or repeated frame queries;
- counter overflow follows a documented saturating or error policy;
- an unmatched begin/end is rejected and does not poison later scopes;
- scope depth 63, 64, and 65 exercise boundary behavior.

Why: a profiler that double-counts or records zero as its minimum gives
plausible-looking but incorrect optimization guidance.

### Rolling statistics and percentiles

Use exact sample sets with independently calculated answers:

- one sample;
- two samples;
- odd and even counts;
- all equal values;
- ascending and descending insertion;
- ring wrap at 119, 120, and 121 frame samples;
- `UINT64_MAX`-adjacent values without summation overflow;
- p50, p95, and p99 boundary ranks;
- replacement of the oldest sample;
- reset and new-session behavior.

Why: percentile rank conventions and ring-buffer wrap are common sources of
off-by-one errors.

### Frames, worker buffers, and cross-thread flows

Test:

- a complete frame with nested main-thread regions;
- worker events arriving before, during, and after frame close;
- multiple workers flushing the same region;
- event buffer capacities 127, 128, and 129;
- queue overflow accounting without memory corruption;
- a flow begun on one thread and ended on another;
- duplicate, missing, reversed, and unknown flow IDs;
- thread exit with unflushed data;
- session destroy while workers are quiescing;
- deterministic manual time across all threads;
- frame duration decomposed into measured regions and uninstrumented gap;
- no negative/underflowed gap when overlapping work exists.

Why: worker events and cross-thread flows are essential for Minecraft frame
analysis and are exactly where simple thread-local profilers lose information.

### Exporters and compile-out behavior

For JSON/trace exporters:

- parse exported data with Libft JSON rather than checking substrings;
- verify escaping, UTF-8 names, stable IDs, exact timestamp units, and required
  fields;
- verify deterministic ordering where promised;
- inject allocation failure at every append and prove caller output remains
  unchanged;
- test zero events, full queues, and queue wrap;
- export concurrently with collection under TSan;
- prove flushing consumes exactly the documented events once.

Build a tiny release translation unit with instrumentation macros disabled and
inspect symbols/disassembly to prove scopes, names, clocks, and test hooks are
absent. Add a microbenchmark with disabled and enabled instrumentation. Store a
baseline and alert on a statistically meaningful overhead regression rather
than requiring an unstable absolute nanosecond threshold.

## CardGame reference models

Build tester-only reference models for:

- ordered zones/decks using a simple fixed array;
- resolution order using a simple list sorted by priority and insertion order;
- player health/mana and board occupancy;
- command sequence acceptance;
- snapshot/delta application;
- event queues and deterministic effect operations.

After every generated command, compare all public state and canonical hashes
between the production engine and model. The model should reject unsupported
operations explicitly instead of silently approximating production behavior.

## CardGame exhaustive contract plan

### Rules and registration

Test every rule field at zero, one, normal, maximum, and one beyond maximum.
Test invalid combinations such as zero players, starting mana above maximum,
zero board spaces, and unreachable phases according to the final validator.

For card, type, phase, zone, and effect registration, cover:

- first and last valid ID;
- duplicate ID and duplicate name/config identity;
- missing referenced type/effect/phase/zone;
- invalid enum values and masks;
- capacity minus one, capacity, and capacity plus one;
- registration before and after match start;
- null callbacks and null output pointers;
- owner-scoped and shared zones;
- zero and maximum zone capacity;
- card-copy limit enforcement;
- rule hash stability and sensitivity to each field.

Why: configurations are an untrusted boundary and invalid references must be
rejected before gameplay state exists.

### Ordered zones and decks

For `card_game_ordered_zone` and every engine deck wrapper, cover:

- push/peek/pop at top and bottom;
- insert at index zero, count, middle, and count plus one;
- remove first, middle, last, missing, and duplicate instances;
- empty and full operations;
- duplicate policy enabled and disabled;
- exact order after every operation;
- clear, destroy, reinitialize, and move;
- deterministic shuffle for fixed seeds;
- different seeds producing valid permutations;
- zero random state and maximum random state;
- shuffle of zero, one, two, and maximum cards;
- no missing or duplicated cards after shuffle;
- snapshot, delta, state hash, and replay preserving exact deck order.

Why: deck order is authoritative gameplay state, not an unordered collection.

### Resolution stack

Test LIFO and FIFO independently, with equal and differing priorities. Define
and test the precise precedence between priority and LIFO/FIFO insertion order.

For each admission policy:

- `CLOSED`: pushing during resolution is rejected without mutation;
- `OPEN_CURRENT_BATCH`: a pushed entry may resolve in the current batch at the
  documented position;
- `OPEN_DEFERRED`: a pushed entry enters the deferred queue and becomes visible
  only after the current batch ends.

Also test duplicate entry IDs, zero and maximum capacity, begin twice, end when
not resolving, pop when empty, deferred overflow, current overflow, nested
effect additions, clear during/after resolution, and move with both queues
populated.

Why: stack admission semantics materially change game rules and must be
demonstrated rather than inferred from implementation order.

### Effects, events, and transactional operations

Test legacy effect functions and operation-buffer callbacks separately. For
callbacks, verify that:

- callbacks observe immutable pre-effect state;
- valid operation batches commit in order;
- invalid operation N rejects the complete batch or follows the explicitly
  documented transaction policy;
- callback failure publishes no partial state;
- operation-buffer overflow publishes no partial state;
- health and mana arithmetic cannot underflow or overflow;
- invalid player/source/target/event IDs are rejected;
- events emitted by effects resolve in deterministic sequence order;
- an effect that emits another event cannot create an unbounded loop;
- per-resolution operation and event budgets are enforced;
- user-data pointers are never serialized or included in hashes;
- two engines using equivalent callbacks produce identical state.

Why: function pointers are trusted extension code, but their requested state
changes still need validation by the engine.

### Snapshots, deltas, commands, and replay

For snapshots and deltas:

- round-trip every legal field at boundary values;
- apply to a differently initialized destination and verify full convergence;
- reject wrong format version, player count, phase, card ID, instance state,
  event count, deck count, and board count;
- reject duplicate/missing instances and cards in impossible zones;
- leave destination unchanged after every invalid snapshot/delta;
- create a delta with no changes, one player changed, all players changed,
  global-only change, deck-only change, and event-only change;
- reject a delta whose base sequence does not equal local state sequence;
- applying a delta twice is rejected without a second mutation;
- state hashes match after snapshot and delta convergence.

For commands and replay:

- accept strictly increasing command sequences;
- reject duplicate, stale, skipped if disallowed, and out-of-order sequences;
- reject stale expected-state sequences;
- reject commands from the inactive player or disallowed phase;
- record before/after hashes and rules hash exactly;
- serialize zero, one, maximum, and over-capacity command records;
- truncate serialized data at every byte boundary;
- mutate every header and record field independently;
- leave existing records unchanged after failed deserialization;
- replay into a fresh engine and match every intermediate and final hash;
- detect rules-hash or before-hash divergence at the first command;
- retry after a rejected command without poisoning sequence state.

Why: this is the foundation for an authoritative server and deterministic bug
reproduction.

## CardGame simulation harness

### Harness design

Add `Test/CardGameSimulation/` containing tester-only builders, scenario data,
and human-readable reports. Tests still register through the standard Libft
runner. Suggested files:

```text
Test/CardGameSimulation/card_game_simulation.hpp
Test/CardGameSimulation/card_game_simulation.cpp
Test/Test/test_card_game_simulation_duel.cpp
Test/Test/test_card_game_simulation_decks.cpp
Test/Test/test_card_game_simulation_resolution.cpp
Test/Test/test_card_game_simulation_authoritative_sync.cpp
Test/Test/test_card_game_simulation_soak.cpp
```

The harness should expose explicit operations, not hide the engine API. A
scenario should read like an embedding example:

```text
initialize rules
register types, zones, phases, effects, and cards
start match
construct and shuffle player decks with fixed seeds
submit commands
resolve events
capture snapshot/hash
```

Each step stores its expected error, expected state hash when fixed, and a
short explanation. On failure, print the step name and state diff. Scenario
fixtures must not use sleeps, wall-clock time, or non-deterministic randomness.

### Simulation 1: minimal two-player creature duel

Purpose: demonstrate the normal public API and basic authoritative turn flow.

Configuration:

- two players, five board spaces, 20 health;
- creature and spell card types;
- draw, main, combat, and end phases;
- a small deterministic deck per player;
- one creature, one direct-damage spell, and one healing spell.

Sequence and assertions:

1. Register configuration and assert a stable rules hash.
2. Start the match and assert initial health, mana, active player, and phase.
3. Draw known top cards and verify deck order/count.
4. Submit a creature play command and verify mana, board instance, command
   record, event sequence, and state-sequence increment.
5. Submit an illegal opponent command and prove complete state preservation.
6. Advance phases, apply damage, end the turn, and verify active-player change.
7. Play healing and assert clamping/limits.
8. Reconstruct the match from command records and compare every state hash.

### Simulation 2: damage-triggered and end-turn healing creature

Purpose: exercise the requested “creatures healing off damage/end of turn”
behavior through configured events and callbacks rather than hard-coded engine
rules.

Define event IDs such as `DAMAGE_DEALT` and `TURN_END`. Register a callback for
a creature that requests a health operation when its matching event occurs.
Run cases where:

- it heals its owner after dealing damage;
- it receives unrelated damage and does not heal;
- it heals at end of its owner's turn only;
- it is removed before the event resolves;
- multiple matching creatures trigger in deterministic order;
- healing would exceed the configured limit;
- a callback fails after proposing operations, proving transactional behavior;
- emitted events attempt a cycle and hit the resolution budget safely.

This scenario demonstrates the intended division: configuration selects event
and effect IDs; callbacks describe permitted operations; the engine validates
and applies them.

### Simulation 3: deck construction and manipulation

Purpose: demonstrate exact deck semantics.

Build a 20-card deck with repeated card definitions but unique instances where
the final API distinguishes them. Exercise:

- push top and bottom;
- insert at a selected index;
- tutor/remove a card;
- put a drawn card back on top and bottom;
- deterministic shuffle with a fixed seed;
- draw until empty;
- snapshot halfway, continue, restore, and repeat the same draws;
- reproduce the same order on a second engine using the same seed;
- transmit a deck-only delta and compare state hashes.

Store one golden order for a specified algorithm/version. If the shuffle
algorithm intentionally changes, bump the rules/format version and update the
vector explicitly.

### Simulation 4: configurable resolution behavior

Purpose: show that the stack understands configured mechanics without knowing
the rules of a specific card game.

Run the same entries under all combinations of:

- LIFO and FIFO;
- closed, current-batch, and deferred admission;
- equal priority and mixed priority;
- effects that enqueue one additional effect while resolving.

Each combination has an explicit expected resolution transcript. Assert the
transcript, queue sizes after each pop, and final resolving state.

### Simulation 5: authoritative server with two clients

Purpose: demonstrate efficient and safe multiplayer use.

Create three engines with identical configuration:

- server engine is authoritative;
- client A and client B maintain predicted/read-only local state.

Flow:

1. Server publishes an initial snapshot.
2. Client A sends a valid command with command sequence and expected server
   state sequence.
3. Server validates, executes, records, and creates deltas for both clients.
4. Both clients apply the delta and converge on the server hash.
5. Client B sends an illegal or stale command; server rejects it and emits no
   state delta.
6. Deliver a delta twice, out of order, truncated, and after a missed delta;
   clients reject it and request/apply a fresh snapshot.
7. Continue valid commands and prove all three hashes converge again.

The harness should support deterministic packet loss, duplication, and reorder
at the message level without using real sockets. A separate Networking
integration test may serialize the same messages through the networking
simulator.

### Simulation 6: long deterministic match and model soak

Purpose: find sequence-dependent corruption and prove replay determinism.

Generate legal and deliberately illegal commands from a fixed seed for at
least 100,000 steps in a scheduled/nightly suite. After every step:

- compare engine and reference model;
- assert all counts are within capacity;
- assert every board/deck instance is valid and appears in legal locations;
- assert health/mana ranges;
- assert event and command sequences are monotonic;
- periodically snapshot and restore into a second engine;
- periodically create/apply deltas to a third engine;
- periodically serialize/replay command records;
- compare hashes.

On failure, minimize the command list using prefix bisection and operation
deletion, then persist the smallest reproducer.

## Scripting verification plan

### Lexer, parser, and compiler

Test every token and operator, precedence combination, statement separator,
block form, conditional, loop, assignment, native call, string escape, and
literal boundary. Include:

- empty and whitespace-only input;
- every truncation point of valid source;
- invalid UTF-8 according to the chosen source policy;
- identifier/string/instruction/argument/depth limits at limit minus one,
  limit, and limit plus one;
- 31, 32, and 33 native arguments;
- integer minimum/maximum, overflow, underflow, and division by zero;
- comments and newline normalization used by terrain fixtures;
- stable source spans and diagnostics;
- allocation failure at every token, string, instruction, and diagnostic
  construction point;
- caller program/result unchanged on compile failure.

Why: malformed mod input must produce a bounded diagnostic, never partial
bytecode or unbounded parser recursion.

### Bytecode verifier and serialization

Generate valid programs containing every opcode, then mutate:

- magic, version, flags, lengths, counts, checksum, and reserved fields;
- every instruction opcode and operand;
- jump targets before zero, at end, and beyond end;
- stack underflow/overflow paths and inconsistent merge depths;
- string offsets and lengths;
- native IDs and argument counts;
- every truncation boundary and trailing byte.

Deserialization must be transactional. Existing output programs remain exactly
unchanged after rejection. Valid serialization must be byte-identical across
platforms and have checked golden vectors.

### VM and native boundary

Test every opcode directly and through compiled source. Cover operation limit,
loop limit, call depth, stack depth, string/memory budget, cancellation if
supported, and repeated execution after errors.

For native callbacks test null callback, duplicate registration, unknown ID,
incorrect argument type/count, callback error, result type, user data, nested
execution/re-entrancy policy, and callback attempts to retain ephemeral data.
The VM must remain usable after a callback failure.

### Game and Voxel bridge parity

Test custom scripts that:

- set, unset, and read Game variables;
- call typed Game callbacks and consume integer results;
- register terrain blocks with all 15 arguments;
- use multiline and semicolon-normalized terrain syntax;
- configure biome heights and snow policy;
- reject missing assets and invalid block metadata transactionally;
- produce deterministic terrain configuration and chunk bytes;
- hit operation and argument budgets;
- fail compilation/execution without mutating world or terrain state.

Add release/archive scans proving no Lua object, header, symbol, manifest, or
link dependency returns.

## Secure-channel verification plan

### Rotation failure sweep

For send-only, receive-only, and combined epoch updates, inject failure at
every derivation, temporary-backend initialization, previous-key preparation,
swap/commit, and cleanup point. After each failure assert:

- epoch, key, IV, nonce counters, replay windows, and previous-key flags retain
  the old coherent state;
- old packets still seal/open;
- no new epoch is reported as available;
- retry succeeds;
- temporary key material is wiped and allocations are released.

Also cover epoch zero, same epoch, skipped epoch, maximum epoch, nonce boundary,
old/current key grace windows, replayed packets, reordered packets, invalid
tags, changed associated data, and concurrent send/receive according to the
documented synchronization contract.

### Move transactionality

Inject failure at every backend transfer/commit point with empty, current-only,
and current-plus-previous-key states. Exactly one complete usable channel must
survive every failed move. Verify key material is not duplicated beyond the
temporary lifetime and is wiped after cleanup.

Run RFC/known-answer Crypto vectors separately so a secure-channel round trip
cannot hide two matching implementation errors.

## RW-lock verification plan

### Reader ownership spill

One thread must acquire 63, 64, 65, 128, and a configured stress count of
distinct read locks, then release them in normal, reverse, and mixed order.
Verify inline-to-spill transition, duplicate read ownership policy, allocation
failure while spilling, cleanup on thread exit where supported, and no false
`FT_ERR_NO_MEMORY` merely because the inline cache is full.

### Writer cancellation

Test hundreds and thousands of timed/cancelled writer tickets with readers and
successful writers interleaved. Assert:

- ticket order among non-cancelled writers;
- cancelled tickets never acquire ownership;
- queue advancement skips each cancellation exactly once;
- no reader/writer starvation beyond the documented policy;
- timeout at every boundary;
- counter/ticket wrap policy;
- destroy/disable behavior with waiters;
- TSan cleanliness and bounded completion.

Add a benchmark for cancellation-heavy queues and reader fast paths. Benchmarks
must report distributions and regression ratios; they are not correctness
tests and should not use fragile pass/fail wall-clock thresholds on shared CI.

## Cross-module integration tests

Add focused tests for:

- CardGame scenario callbacks implemented by Scripting native functions;
- CardGame command replay timed by Analytics without changing state hashes;
- Analytics trace export compressed and decompressed through Compression;
- CardGame snapshots/deltas transported through the Networking simulator and
  protected by the secure channel;
- key rotation between successive authoritative deltas;
- worker-thread CardGame simulation analytics flushed into one frame;
- failure injection in the second module after the first module has committed,
  with a clearly documented ownership/retry boundary.

Each integration test must still identify which module owns rollback. Avoid a
single giant end-to-end test as the only coverage for any behavior.

## Property, metamorphic, and fuzz testing

Required properties include:

- decompress(compress(input)) equals input;
- applying a valid CardGame snapshot produces the snapshot's state hash;
- applying a delta to its exact baseline equals the source engine;
- replaying accepted commands equals the recorded final hash;
- shuffling preserves the deck multiset;
- serialize/deserialize preserves valid Scripting programs;
- direct and bytecode Scripting execution agree;
- disabling Analytics cannot change instrumented program output;
- failed transactional operations preserve the pre-operation snapshot.

Fuzz targets:

- compressed-stream decoder input;
- Scripting source lexer/parser;
- Scripting bytecode deserializer/verifier;
- CardGame command-record deserializer;
- CardGame snapshots and deltas;
- Networking secure packet open path.

Every fuzz target needs strict memory/operation limits and a seed corpus made
from valid minimal and maximal records. Sanitizer findings must preserve the
input artifact.

## CI and platform matrix

### Per pull request

- clean debug test build;
- full unit and deterministic simulation suite;
- release build and tester-symbol compile-out scan;
- Linux ASan/UBSan;
- Linux TSan for concurrency-focused subsets;
- Windows and macOS normal suites;
- serialization golden vectors on all three platforms;
- archive integrity and stale-object checks;
- documentation coverage-manifest check.

### Scheduled

- long CardGame model soak with rotating seeds;
- cancellation-heavy RW-lock stress;
- Analytics overhead benchmark and percentile validation;
- Compression fragmented pipe/socket stress;
- fuzz corpus runs;
- CMA/failure-point exhaustive sweeps;
- deterministic cross-platform artifact comparison.

Every individual test retains the existing 120-second timeout. Stress tests
must report progress and a current operation/seed marker so cancellation or a
timeout produces a useful stack trace and reproducer.

### Sanitizer ownership

- ASan: ownership, use-after-free, overflow, and cleanup paths;
- UBSan: integer, alignment, enum, and shift boundaries;
- TSan: Analytics workers, Compression options, RW-lock, and any documented
  concurrent secure-channel paths;
- leak checks: every failure sweep and repeated initialize/destroy cycle.

Do not skip a sanitizer merely because a scenario is slow. Split heavy
scenarios into deterministic subsets and reserve extended counts for scheduled
runs.

## Diagnostics requirements

On any failure, report at minimum:

- full test and scenario name;
- platform, build mode, and sanitizer;
- seed and simulation step;
- expected and actual `FT_ERR_*` value;
- active failure point and occurrence;
- state/rules hash before and after;
- first differing field or index;
- last accepted CardGame command sequence;
- current Analytics frame/region or Scripting source span where relevant;
- paths to replay, trace, core, sanitizer, and failure logs.

Diagnostics must never print keys, plaintext secrets, or secure-channel key
material. Crypto diagnostics use epochs, packet numbers, sizes, and redacted
identifiers only.

## Test organization

Keep files focused and named according to `AGENTS.md`. Prefer separate files
for lifecycle, transactionality, serialization, concurrency, and simulations.
Do not grow the existing mixed test files indefinitely.

Suggested ownership:

```text
test_compression_stream_fragmentation.cpp
test_compression_stream_failure_sweep.cpp
test_compression_stream_options_concurrency.cpp
test_analytics_accounting.cpp
test_analytics_percentiles.cpp
test_analytics_worker_flows.cpp
test_analytics_export_transactionality.cpp
test_card_game_engine_lifecycle.cpp
test_card_game_engine_registration.cpp
test_card_game_engine_snapshot_delta.cpp
test_card_game_engine_command_replay.cpp
test_card_game_ordered_zone_model.cpp
test_card_game_resolution_stack_model.cpp
test_card_game_simulation_*.cpp
test_scripting_parser_boundaries.cpp
test_scripting_bytecode_verifier.cpp
test_scripting_vm_failure_sweep.cpp
test_networking_secure_channel_rotation_failure_sweep.cpp
test_pthread_rwlock_reader_spill.cpp
test_pthread_rwlock_writer_cancellation_stress.cpp
```

## Implementation sequence

1. Add the tester-only failure controller, state oracles, seed artifacts, and
   release compile-out checks.
2. Create coverage manifests from current public APIs and existing tests.
3. Fill lifecycle and deterministic boundary gaps before adding simulations.
4. Add reference models for ordered zones, resolution stacks, and engine state.
5. Implement the first four deterministic CardGame simulations as executable
   usage examples.
6. Add transactional failure sweeps for Scripting, Analytics exporters,
   snapshots/deltas, and secure-channel rotation.
7. Add concurrency schedules and TSan subsets.
8. Add fuzz targets and long scheduled simulations.
9. Add cross-platform golden artifact comparison.
10. Require all acceptance criteria before treating the feature batch as
    production-complete.

## Initial executable implementation

The tester-only implementation is present in
`Test/Test/test_failure_controller.*` and
`Test/Test/test_card_game_simulation.cpp`. It provides atomic failure-point
arming and accounting, verifies concurrent-safe controller lifecycle basics,
and exercises the executable usage scenarios:

- authoritative server/client snapshot and delta convergence;
- callback and operation failure rollback with state preservation;
- configured deck insertion, top/bottom operations, read-only inspection,
  exact-instance drawing, seeded shuffling, and removal;
- permanent and end-of-turn stat modifiers with snapshot/delta convergence;
- deferred resolution admission with a LIFO transcript;
- configurable phase graph observation and transactional transition failure;
- deterministic failure-point scheduling and occurrence accounting.

The Compression, Analytics, Scripting, Crypto, Networking, and RW-lock suites
are implemented in their owning module test files. The current implementation
also has focused accounting coverage for Analytics, including zero-duration
samples and percentile boundaries. The acceptance matrix below remains the
checklist for future platform-specific CI and fuzz expansion.

## CardGame deck identity and configurable turn rules

The deck API must expose both the card definition and the identity of the
physical copy in the deck. A definition ID answers “which card is this?”; a
unique instance ID answers “which copy is this?”. Two copies of the same card
therefore have the same definition ID but different instance IDs. Instance IDs
are generated or supplied by the authoritative engine, are nonzero, and are
unique within the match. They must survive shuffle, insert, draw, removal,
snapshot, delta, and replay operations.

Add these read and identity-preserving operations:

- `deck_inspect(player_id, index, output)`: read-only lookup by position;
  it must never change the deck or state sequence;
- `deck_get_instance(player_id, instance_id, output)`: read-only exact-copy
  lookup by unique instance ID;
- `deck_draw_top(player_id, output)`: the entry-returning overload removes and
  returns the current top card, preserving its definition ID and unique
  instance ID; the existing definition-ID overload remains a compatibility
  helper. An empty deck returns `FT_ERR_EMPTY` without changing state;
- `deck_draw_instance(player_id, instance_id, output)`: remove and return
  exactly that copy, preserving the relative order of all other cards;
- `shuffle_deck(player_id, random_state)`: deterministic seeded shuffle that
  moves complete entries, never just definition IDs.

The output entry contains `instance_id` and `card_id`. Inspecting an empty
deck, an out-of-range position, or an absent instance returns the documented
error and leaves the deck, output object, and state sequence unchanged. A
missing definition ID is distinct from a missing instance ID. Duplicate
instance IDs are rejected; duplicate definition IDs are allowed only when the
configured deck policy allows copies. Existing top/bottom helpers may remain
as compatibility wrappers, but they must create or preserve unique instance
IDs rather than making duplicate copies indistinguishable.

Snapshots and deltas must serialize the paired definition/instance entries,
and state hashes must include both fields and their order. Applying a malformed
entry, duplicate instance, unknown definition, stale delta, or duplicate delta
must be transactional. Tests must verify normal inspection, exact retrieval,
draw-at-top/middle/bottom, shuffle identity preservation, missing-card errors,
duplicate definitions, duplicate instance rejection, output immutability on
failure, and server/client convergence after every operation.

Card stats, turns, phases, and combat are also configurable runtime state.
Keep immutable card definitions separate from per-instance effective stats and
modifier records. A modifier record must identify its source/effect, target
instance or player, affected stat, value, creation turn and phase, stacking
policy, and lifetime. Support at least permanent modifiers and modifiers that
expire at end of turn; the model should be extensible to end of phase and
event-based expiration. Track modifiers rather than only the calculated total
so another client can expire a temporary effect without converting it into a
permanent change.

The rules configuration must define the turn/phase graph: phase IDs, ordering
or transition target, allowed commands, entry and exit events, reset hooks,
and whether transitions repeat, branch, or skip. Runtime state must expose the
turn number, active player, current phase, phase sequence, pending events, and
the next expiration boundary. The authoritative server validates all turn and
phase commands; clients request actions and apply committed deltas.

Configuration must also select when start/end turn and start/end phase effects
run, when mana/action limits reset, when temporary modifiers expire, whether
effects created during resolution enter the current or deferred batch, and
whether combat is automatic, command-driven, simultaneous, or ordered. Combat
must be a deterministic transaction recording attackers, targets, calculated
damage, modifiers used, death/retaliation events, and the resulting state
sequence. Any failed effect rolls the complete combat transaction back.

Required tests cover modifier stacking and isolation between identical card
copies; permanent versus end-of-turn changes; expiration before/after trigger
queues; custom linear, repeated, skipped, and branching phases; active-player
and stale-command rejection; reset timing; simultaneous and ordered combat;
lethal damage, retaliation, death triggers, effects added during resolution;
rollback; replay; snapshot/delta convergence; and malformed state inputs.

## Acceptance criteria

- Every public feature-batch API has a coverage-manifest row and owning tests.
- Every reachable named failure point is exercised automatically.
- Every transactional API proves state preservation at every failure point.
- CardGame has deterministic reference-model tests and all six simulations.
- The authoritative server/client simulation converges after valid updates and
  rejects stale, duplicate, reordered, and malformed updates.
- Scripting source, bytecode, native boundaries, and Game/Voxel bridges have
  boundary, failure, and fuzz coverage.
- Analytics accounting has no invocation double count, zero-poisoned minimum,
  worker loss, percentile error, or release-disabled overhead path.
- Compression preserves every fragmented input byte and handles every partial
  write contract without spinning or duplication.
- Secure-channel rotation and move failures leave one coherent usable channel.
- RW-lock ownership works beyond 64 read locks and cancellation stress remains
  correct and bounded.
- ASan, UBSan, TSan, normal Windows/Linux/macOS, clean/incremental builds, and
  archive checks pass.
- Release archives contain no tester hooks, profiling code when disabled, Lua
  dependencies, or test-controller symbols.
- A failed randomized or simulation test is reproducible from its reported
  seed and replay artifact.
