# Analytics build and configurable CardGame formats design

## Document status

This is an implementation plan. It does not authorize changing the build graph
or CardGame runtime by itself. Implementation must follow `AGENTS.md`, preserve
transactional failure behavior, use Libft containers and error codes, and keep
tester-only controls out of release archives.

This document extends the analytics and CardGame plans archived under
`Docs/old`. Where those older documents disagree with this document about build
variants or format legality, this document is authoritative.

## Goals

- Build a fully instrumented Libft variant for Minecraft performance analysis.
- Build an ordinary gameplay variant with no analytics storage, registration,
  timing, exporter, or instrumentation overhead.
- Make the root `all` target produce both variants automatically.
- Let downstream games explicitly link the variant they intend to ship or
  profile.
- Measure complete Minecraft frames, nested work, persistent render workers,
  asynchronous world generation, waiting, and uninstrumented time without
  double-counting parallel work.
- Add data-driven CardGame formats that define legal sets, individually legal
  cards, copy limits, banlists, and card-specific rule-text exceptions.
- Make format validation deterministic, transactional, replay-compatible, and
  safe for authoritative servers.

## Non-goals

- Do not make analytics mandatory for normal gameplay.
- Do not depend on an external sampling profiler for the core runtime overlay.
- Do not infer card legality only from whether a card exists in the corpus.
- Do not store raw function pointers, process addresses, or unversioned rule
  text in format files, deck codes, saves, network messages, or replays.
- Do not let a format mutate immutable card definitions globally. Format
  exceptions are resolved into a format-scoped rules view.

# Part I: separate normal and analytics builds

## 1. Required artifacts and targets

The build must produce two independent release artifacts:

```text
normal gameplay build
  Modules/*/*.a without analytics-only objects
  libft.a
  LIBFT_ENABLE_ANALYTICS is not defined

analytics build
  Modules/*/*_analytics.a from separately compiled objects
  libft_analytics.a
  LIBFT_ENABLE_ANALYTICS=1 is defined for every compiled consumer
```

Recommended root targets:

```text
make normal          -> build only libft.a
make analytics       -> build only libft_analytics.a
make all             -> build libft.a and libft_analytics.a
make debug           -> build ordinary debug artifact
make analytics-debug -> build analytics debug artifact
```

`all` producing both archives is a convenience for development and CI. A game
must still link exactly one variant. The normal Minecraft target links
`libft.a`; a profiling target such as `minecraft-analytics` links
`libft_analytics.a` and compiles Minecraft with `LIBFT_ENABLE_ANALYTICS=1`.

The ordinary archive name must remain stable for existing consumers. The
analytics archive requires a distinct name so a stale or accidental link cannot
silently add profiling baggage to a normal build.

## 2. Why the complete analytics variant must be recompiled

`analytics_instrumentation.hpp` compiles instrumentation macros conditionally.
Defining `LIBFT_ENABLE_ANALYTICS` only while compiling `Modules/Analytics` is
insufficient: every Libft or Minecraft translation unit containing an
instrumented call site must see the same definition.

Therefore the analytics variant must use:

- a separate configuration fingerprint;
- a separate object root;
- separate dependency files;
- separate per-module archives;
- a separate final archive;
- the analytics preprocessor definition on all C, C++, and Objective-C++
  compilation commands in that variant.

Normal and analytics objects must never share paths. Parallel `make -j` builds
must not overwrite each other's objects, dependency files, temporary archives,
or progress records.

Suggested graph variables:

```text
LIBFT_GLOBAL_NORMAL_ROOT
LIBFT_GLOBAL_ANALYTICS_ROOT
LIBFT_GLOBAL_ANALYTICS_CPP_FLAGS += -DLIBFT_ENABLE_ANALYTICS=1
LIBFT_GLOBAL_ANALYTICS_ARCHIVE_SUFFIX := _analytics
LIBFT_GLOBAL_ANALYTICS_TARGET := libft_analytics.a
```

The exact names may follow the current graph conventions, but the variant and
definition must participate in `LIBFT_GLOBAL_CONFIG_FINGERPRINT`. Changing
analytics flags must invalidate only the affected variant.

## 3. Normal-build exclusion contract

The normal build must contain no analytics baggage. Compiling a macro into a
runtime `if (false)` is not enough. Disabled instrumentation must preprocess to
an expression that:

- emits no function call;
- evaluates no instrumentation-only argument;
- creates no scope/token object;
- references no Analytics symbol or region-name string;
- adds no static constructor or registration table;
- does not require linking the Analytics archive.

The normal global archive graph should exclude `Analytics` from its module
archive list unless a non-instrumentation compatibility stub is strictly
required. Prefer complete exclusion. Headers may remain available so shared
source compiles, but their disabled surface must be header-only and empty.

Minecraft must apply the same split. Its normal objects are compiled without
the definition. Its analytics objects use a separate object directory and link
the analytics Libft artifact. Reusing normal Minecraft objects in the analytics
executable would silently omit measurements.

## 4. Instrumentation interface

Use explicit regions and stable numeric IDs. Registration happens during
analytics-session initialization, never in a frame hot path.

Required concepts:

- `analytics_session`: registry, configuration, collector, retained samples,
  exporters, and diagnostics;
- `analytics_thread_context`: persistent per-thread event and scope storage;
- `analytics_frame_token`: one logical rendered frame or server tick;
- `analytics_scope_token`: a balanced timed region;
- `analytics_flow_token`: causal work crossing thread boundaries;
- `analytics_snapshot`: immutable runtime-query result for overlays.

The enabled hot path should normally perform one enabled/category check, a
monotonic timestamp read, a bounded thread-local stack update, and an append to
thread-owned memory. It must not allocate, format JSON, write files, acquire a
global registry lock, or wait for an exporter.

All meaningful return values must be checked by explicit APIs and tests.
Convenience macros may deliberately discard internal diagnostics only when the
macro contract documents that the game cannot recover at that call site and
the session increments an observable error counter.

## 5. Correct timing and accounting semantics

For every region retain:

- invocation count;
- inclusive and exclusive time;
- minimum and maximum duration;
- total and arithmetic mean;
- variance or a numerically stable jitter measure;
- rolling p50, p95, and p99;
- percentage of parent and frame;
- thread and frame identities;
- dropped-event and invalid-token counts.

Invocation accounting must happen once per successfully completed scope. A
sample value of zero is valid and must not poison minimum timing. Validation
samples must not enter production statistics.

Nested exclusive time is `inclusive - direct child union`. Top-level
instrumented frame time is the union of top-level intervals, not their sum.
This prevents overlapping scopes or parallel workers from producing a negative
uninstrumented time.

Parallel work needs both measurements:

- CPU work: sum of worker durations;
- wall/critical-path time: interval from dispatch or earliest required start to
  the last required completion.

Never report summed worker CPU time as frame wall time.

## 6. Persistent worker integration

Minecraft render and generation workers are persistent threads. Each registers
one analytics thread context after startup and keeps it until orderly shutdown.
Do not create analytics threads, contexts, or buffers per frame or task.

Cross-thread work uses stable flow IDs:

```text
main thread: begin mesh dispatch scope
main thread: create flow for chunk 42
worker: begin linked mesh-build scope
worker: finish linked scope
main thread: observe completion/wait
main thread: end mesh dispatch scope
```

The collector must distinguish queue delay, execution, main-thread wait, and
worker idle time. A worker that is sleeping because no tasks exist is not a
slow render task.

Thread contexts use bounded growth or a configured fixed capacity. Overflow
drops or overwrites completed analytics events according to configuration and
increments an exact counter. It must never block gameplay.

## 7. Minecraft instrumentation map

Start with stable coarse regions, then refine only measured hotspots:

```text
frame
  platform events/input
  simulation
    entity update
    world update
    generation submission
    generation result commit
  visibility
    chunk selection
    frustum/occlusion work
  mesh maintenance
    dirty selection
    worker queue delay
    worker mesh generation
    mesh commit/upload
  render preparation
  raycasting
    dispatch
    worker traversal
    result merge/wait
  rasterization or GPU submission
  debug overlay
  presentation/vsync wait
  uninstrumented
```

Instrument asynchronous world generation as a flow from request through worker
execution to main-thread commit. Keep computation time separate from lock-wait
and commit time. Instrument persistent raycast workers individually without
summing them into frame wall time.

The runtime overlay must expose:

- latest frame and rolling frame average;
- p50/p95/p99 frame time and FPS;
- inclusive and exclusive top regions;
- parent/child breakdown;
- worker utilization, queue delay, and idle time;
- lock wait where explicitly instrumented;
- world-generation request, execution, and commit timing;
- dropped events and profiler error state;
- estimated analytics overhead.

## 8. Collection, snapshots, and exporters

Producers write only thread-owned records. The collector drains completed
batches at safe frame boundaries or on a persistent collector thread. Runtime
queries consume immutable snapshots so the overlay never traverses producer
buffers while they are changing.

Required outputs:

- latest-frame and rolling snapshot API;
- compact text/CSV benchmark summary;
- Chrome/Perfetto-compatible trace JSON;
- compact versioned binary capture for long sessions.

Formatting and file I/O occur outside producer threads. Export failures leave
the session usable, preserve already committed statistics, and report an error
and dropped-batch count.

Configuration controls enabled categories, capacities, retention, rolling
window, sampling, slow-frame triggers, pre/post-trigger capture, exporter, and
whether names are retained. Runtime disabling closes admission for new events
but does not free a thread context until producers are quiescent.

## 9. Build-system implementation plan

### Phase A: variant-aware graph

1. Add normal and analytics variant descriptors to the global graph.
2. Put the variant name and all variant flags in the configuration fingerprint.
3. Generate separate release/debug/test object and dependency roots.
4. Generate `_analytics.a` module archives from analytics objects.
5. Build `libft_analytics.a` from those archives.
6. Make `normal` and `analytics` explicit targets.
7. Make root `all` depend on both without serializing independent work.
8. Extend progress/build-plan output with the variant so duplicate-looking
   messages remain diagnosable.

### Phase B: downstream integration

1. Add normal and analytics Libft selections to the embedded parent graph.
2. Add separate normal and analytics Minecraft object roots.
3. Add a Minecraft analytics executable/target with the definition enabled.
4. Keep ordinary Minecraft linking only the normal archive.
5. Reject linking both final Libft variants into one executable.

### Phase C: archive hardening

1. Verify stale members disappear when a source leaves a variant.
2. Verify changing analytics flags rebuilds analytics objects, not normal ones.
3. Verify parallel `make all -j32` cannot cross-contaminate variants.
4. Verify incremental parent builds select the correct archive.

## 10. Analytics build and runtime tests

### Build identity tests

- `make normal` produces the normal archive only.
- `make analytics` produces the analytics archive and its required modules.
- `make all` produces both.
- Normal and analytics object/dependency paths never overlap.
- Touching one shared source rebuilds that source in each requested variant.
- Changing only analytics flags leaves the normal archive unchanged.
- Removing/renaming a source removes stale members from both archives.
- Clean and dirty parallel builds work on Linux, macOS, and Windows.

### Proof that normal gameplay has no baggage

- Inspect normal archive symbols: no `analytics_*` implementation references.
- Inspect strings: registered region names must not survive solely because of
  disabled instrumentation.
- Link a minimal normal consumer without `Analytics.a`.
- Compare normal archive and Minecraft executable size against a baseline.
- Compare generated assembly for representative disabled macros.
- Benchmark normal versus a source-identical build before instrumentation was
  added; noise-bounded performance must not regress.

### Accounting tests

- Nested, sibling, recursive, and zero-duration scopes.
- Exactly one invocation per completed scope.
- Inclusive/exclusive arithmetic and interval unions.
- Overlapping worker scopes and critical-path computation.
- Rolling averages and p50/p95/p99 against independent reference values.
- Open, duplicate-ended, stale, wrong-session, and wrong-thread tokens.
- Clock errors, equal timestamps, and monotonicity validation.
- Buffer overflow with exact drop counts.
- Dynamic enable/disable with active persistent workers.
- Export backpressure and failure without producer blocking.
- TSan stress, CMA failure injection outside hot paths, ASan, and UBSan.

### Minecraft acceptance benchmark

Use the same seed, world, camera path, resolution, worker count, frame count,
CPU affinity, and warm-up for every comparison. Record at least:

```text
normal gameplay build
analytics build, collection disabled at runtime
analytics build, collection enabled without exporter
analytics build, collection and trace exporter enabled
```

Report median and p95/p99 frame time, FPS, enabled-scope overhead, event drops,
binary size, and CPU utilization. Do not claim a performance gain from a run
where another workload starved the benchmark.

# Part II: configurable CardGame formats

## 11. Format identity and scope

A format is a schema-versioned rule object layered over a ruleset profile and
card corpus. It determines what may be registered for a match and what decks
are legal. It does not replace the ruleset profile's turn, timing, combat, or
resource model.

Required stable identity:

```text
format_id
format_schema_version
format_revision
profile_id and compatible profile revision range
corpus_id and compatible corpus revision/hash
effective_from and optional effective_until
canonical format hash
```

The canonical hash covers the fully resolved format, included sets/cards,
banlist, copy-limit policy, exceptions, referenced program hashes, and
precedence policy. Matches, deck codes, tournament records, saves, and replays
store this hash. A server rejects mismatched clients before hidden information
or deck contents are exchanged.

Formats are immutable after a match starts. Updating a live format creates a
new revision and hash; it does not mutate existing matches or replays.

## 12. Configuration model

Use terrain-style configuration: parse into temporary unresolved records,
validate all references and conflicts, resolve inheritance/includes, build an
immutable format, canonicalize it, hash it, then commit it.

Suggested record:

```text
card_game_format_definition
  format_id
  schema_version
  revision
  profile_id
  corpus_constraints
  parent_format_ids[]
  legal_set_rules[]
  individual_card_rules[]
  global_copy_limit_policy
  banlist_entries[]
  rules_text_exceptions[]
  deck_zone_constraints[]
  sideboard/reserve constraints
  start/procedure overrides, if explicitly allowed
  precedence_policy
  metadata and source hash
```

Unknown fields, duplicate stable IDs, missing sets/cards, incompatible profile
or corpus references, inheritance cycles, contradictory active rules, invalid
dates, and unsupported schemas fail before authoritative state changes.

## 13. Set and individual-card legality

Formats may include cards through several selectors:

- every legal gameplay definition printed in a set;
- selected subsets or tags from a set;
- explicitly listed logical card definitions;
- explicitly listed printings when printing identity matters;
- exclusions applied to otherwise legal sets;
- rebalanced/rules-revision definitions selected for this format;
- date or season windows resolved before match creation.

Set membership and printing identity remain separate from gameplay definition.
Normally, any accepted printing of a legal definition is usable. A format may
restrict exact printings only for a documented gameplay reason; cosmetic art,
language, finish, or border should not change legality by accident.

Every selector resolves to a canonical sorted set of legal definition IDs plus
an optional printing policy. Deck validation operates on that resolved view,
not by repeatedly walking configuration files.

Recommended legality result:

```text
legal
illegal_unknown_card
illegal_wrong_corpus
illegal_not_in_format
illegal_printing
illegal_banned
illegal_copy_count
illegal_zone
illegal_deck_size
illegal_required_companion_or_leader
illegal_format_exception_conflict
```

Return a structured diagnostic containing the offending stable ID, zone,
observed count, permitted count, and rule/banlist entry that decided it.

## 14. Banlists and copy limits

A banlist entry targets a logical card definition by default and may optionally
target a format-specific functional revision or exact printing. Supported
policies include:

- forbidden: zero copies across configured deck zones;
- restricted: an explicit maximum, commonly one;
- semi-restricted or arbitrary maximum `N`;
- zone-specific limit, such as main/extra/side/reserve totals;
- combined-group limit shared by several related cards;
- conditional limit selected by leader/class/faction or another public deck
  property;
- point-budget list where listed cards consume a configured deck budget;
- an explicit legal override for a card excluded by a broad set rule.

Avoid hard-coded names such as `limited` or `semi_limited` in the kernel. Store
the actual numeric and scope policy; profiles may expose familiar labels.

Copy counting must define whether it uses definition ID, functional-equivalence
group, alias group, printing ID, or another stable count key. This is necessary
for alternate names, reprints, transformed faces, and games where differently
named cards share a deck-construction limit.

Banlists are versioned independently but become part of the resolved format
hash. Effective dates are metadata used to choose a revision before match
creation; validation never depends on the machine's current wall clock after a
format has been selected.

## 15. Precedence for legality decisions

Resolve legality in a fixed documented order:

```text
engine safety/schema constraints
  -> selected profile constraints
  -> resolved format set/card inclusion
  -> explicit format exclusions
  -> banlist/group/copy limits
  -> explicit format legal overrides that are allowed by policy
  -> deck-zone and deck-size constraints
  -> final invariant validation
```

Engine safety and profile invariants cannot be bypassed by configuration.
Whether a legal override may defeat a banlist entry must be explicit. The safe
default is that a banlist wins; a format author must use one reviewed entry
that replaces the conflicting rule rather than stacking contradictory allow
and ban records.

Inherited formats resolve parent rules first and child changes second, but
duplicate child changes to the same key are rejected unless the schema defines
an unambiguous replacement operation. Canonical resolution must not depend on
file order, hash-map order, or callback registration order.

## 16. Card-specific rule-text exceptions

Some formats require a card to behave differently from its corpus definition:
errata, digital rebalancing, a format-specific ruling, or an exception that
overrides a general rule. Do not edit the shared card definition in place.

Represent each exception as a stable, versioned patch:

```text
card_game_format_rule_exception
  exception_id
  target_definition_id
  optional target_face/effect/ability_id
  applicability predicate ID
  exception kind
  replacement or patch program ID
  priority and dependency IDs
  source/ruling reference
  revision and content hash
```

Supported exception kinds should be narrow and typed:

- replace an effect program;
- replace one ability or face rule;
- add/remove a keyword or characteristic;
- replace a numeric rules parameter;
- replace a target, timing, cost, or usage-limit policy;
- exempt the card/effect from one identified general rule;
- bind a card to a format-specific functional definition revision.

Plain display text is never executable. Human-readable rule text is generated
from or attached to the resolved structured rule. A free-form string cannot
override legality or runtime behavior.

Exception programs use stable IDs resolved through the immutable scripting or
native callback registry. Raw pointers and `void *` context are not serialized.
Callbacks receive a read-only state view and operation builder and cannot
directly mutate authoritative state.

## 17. Runtime rule precedence

Card text overriding a general game rule must be explicit and bounded. Use this
default precedence:

```text
engine safety and determinism constraints
  -> match-start invariants that cannot be overridden
  -> format/profile rule baseline
  -> continuous/replacement effects under profile ordering
  -> resolved card rule
  -> resolved format-specific card exception
  -> final legality and invariant validation
```

This does not mean every card exception automatically wins. The targeted rule
must declare itself overrideable, and the exception must identify that rule or
ability. Non-overrideable limits include memory/instruction budgets, stable
identity, hidden-information access, transaction boundaries, and authoritative
server validation.

When two active exceptions touch the same field/effect, resolution uses typed
priority and dependency rules. Equal-priority conflicting replacements fail
format loading. Additive patches may compose only when their schema says they
are commutative or gives a canonical order.

The engine exposes a resolved read-only card view for the selected format:

```text
base definition
  + selected functional revision
  + format exception patches
  = resolved format card definition
```

Deck validation and match execution must use the same resolved view so a deck
cannot validate one definition and execute another.

## 18. Deck codes, replays, and networking

Deck-code headers include `format_id`, format revision/hash, profile ID, and
corpus version/hash. Decoding is transactional. A code may be decoded for
inspection when its format is unavailable, but it cannot be declared legal or
used to start a match until the exact compatible format is resolved.

Canonical deck hashes include semantic card counts/zones and the selected
format hash. Two identical card lists under different formats are distinct
match inputs even if both happen to be legal.

Replays record the resolved format hash, banlist hash, exception-program hashes,
and canonical resolved-rules hash. Reconstruction requires compatible content
or fails explicitly. It never silently substitutes the newest banlist or
errata.

Authoritative servers accept player intents and deck submissions, then perform
format validation locally. Clients do not provide authoritative legality
results, patched rule programs, or state deltas. Public match metadata may
identify the format revision without revealing hidden deck contents.

## 19. Suggested CardGame APIs

Exact C++ signatures must follow existing CardGame lifecycle patterns, but the
capabilities should include:

```text
format initialize/destroy/move
format load unresolved configuration
format resolve against profile and corpus
format canonical serialize/hash
format inspect metadata and legal-card count

format query definition/printing legality
format query effective copy limit and deciding rule
format validate complete deck with structured diagnostics
format enumerate legal definitions for deck-building tools

format resolve card definition/effect for runtime
format inspect applied exception IDs
format inspect banlist entry and source revision
```

Read-only inspection returns immutable snapshots or caller-owned output data.
No API returns internal mutable arrays or pointers whose lifetime changes after
another query.

## 20. Format implementation phases

### Phase 1: schema and immutable resolution

- Define format, selector, banlist, limit-group, and exception schemas.
- Implement transactional parsing, reference validation, inheritance, canonical
  sorting, serialization, and hashing.
- Build efficient resolved legal-definition and copy-limit indexes.

### Phase 2: deck validation

- Validate all configured deck zones and aggregate count groups.
- Return deterministic structured diagnostics.
- Integrate format identity into deck codes and deck hashes.

### Phase 3: runtime exceptions

- Add typed rule/effect patch points.
- Resolve exception programs through stable registries.
- Produce immutable format-scoped card views.
- Ensure legality and execution consume the same resolved definition.

### Phase 4: authority, replay, and tooling

- Enforce exact format hashes at match setup and network handshake.
- Persist format/banlist/exception identities in replays and results.
- Add legal-card enumeration and explanation APIs for deck builders.

## 21. Format test plan

### Configuration and resolution

- Empty, minimal, inherited, and large formats.
- Set-only, card-only, and mixed inclusion.
- Explicit exclusion and legal override behavior.
- Duplicate IDs, missing references, wrong profile/corpus, inheritance cycles,
  conflicting rules, and unsupported schemas.
- Canonical equality under input reordering.
- Stable cross-platform serialization and hashes.
- Failure injection at every allocation/resolve/commit stage with old format
  state unchanged.

### Banlists and deck validation

- Forbidden, arbitrary maximum, group limit, zone limit, conditional limit,
  and point-budget entries.
- Reprints and alternate printings sharing definition-level limits.
- Alias/equivalence groups and exact-printing exceptions.
- Counts spread over main, side, extra, commander/leader, and custom zones.
- Boundary tests at zero, one below, exact, and one above every limit.
- Multiple simultaneous violations with deterministic diagnostic ordering.
- Format revision changes that make a formerly legal deck illegal and vice
  versa without mutating the prior revision.

### Rules-text exceptions

- Each typed exception kind in isolation.
- Exception targeting one face/effect without modifying siblings.
- Card exception overriding an explicitly overrideable general rule.
- Rejection when the target rule is non-overrideable.
- Priority/dependency ordering and equal-priority conflict rejection.
- Base definitions remain byte/hash-identical after resolving a format view.
- Two concurrent matches using different format exceptions do not contaminate
  each other.
- Snapshot, network, and replay behavior use the resolved rule consistently.
- Script/native callback failure leaves authoritative state and RNG unchanged.

### Security and hostile input

- Forged stable IDs, integer overflow in copy aggregation, huge selector lists,
  recursive inheritance, duplicate patches, malformed hashes, and truncated
  configuration.
- Clients claiming a different format or legality decision are rejected.
- Hidden deck information does not appear in legality errors sent to opponents.
- Instruction, recursion, operation, event, and allocation budgets remain
  non-overrideable.

### Full simulations

Run deterministic full-game fixtures with at least:

- one rotating set-based format;
- one eternal format with individual additions and a banlist;
- one format with definition-level reprints;
- one format with a card-specific erratum/rebalanced effect;
- one format where a card explicitly overrides an overrideable general rule;
- two simultaneous matches using different revisions of the same format.

Reconstruct every fixture from replay and verify identical authoritative state
hashes, per-viewer redacted hashes, legality decisions, random draws, and
results.

# Part III: delivery and acceptance

## 22. Delivery order

1. Freeze variant names, artifact names, and analytics compile-out contract.
2. Implement and test the variant-aware build graph.
3. Add Minecraft analytics target and coarse frame instrumentation.
4. Prove normal archives and gameplay binaries contain no analytics baggage.
5. Add thread buffers, flows, snapshots, statistics, and exporters.
6. Freeze CardGame format/banlist/exception schemas.
7. Implement immutable format resolution and deck validation.
8. Add typed runtime exceptions and replay/network identity.
9. Run sanitizer, incremental-build, archive, simulation, and benchmark gates.

## 23. Acceptance criteria

- `make all` builds both normal and analytics Libft artifacts.
- Normal Minecraft links the normal artifact and has no analytics symbols,
  region strings, storage, timers, exporter code, or runtime branch overhead.
- Analytics Minecraft records nested and cross-thread frame work without
  double-counting parallel durations.
- Runtime snapshots provide rolling averages, p50/p95/p99, frame breakdown,
  worker/flow timing, uninstrumented time, and exact drop/error diagnostics.
- Normal and analytics builds remain incrementally correct and independent on
  Linux, macOS, and Windows.
- A CardGame format can include sets and individual cards, exclude cards,
  enforce versioned banlists and arbitrary scoped copy limits, and explain every
  legality decision.
- Card-specific format exceptions use typed, stable, hashed programs and never
  mutate the shared corpus definition.
- Conflicting or unsafe exceptions fail during format resolution.
- Deck codes, authoritative match setup, networking, saves, and replays bind to
  the exact resolved format and reject incompatible revisions.
- All fallible preparation is transactional and all tester/fuzz hooks remain
  absent from ordinary release artifacts.
