# CardGame

`card_game_engine` is a data-driven card-game foundation. The library stores
card definitions, configurable board and turn limits, and effect function
pointers; the game supplies the actual effect behavior and therefore owns the
rules that are specific to its game.

Register effects before registering cards. A card's `effect_id` selects the
callback dispatched by `play_card`. Match state is queried through explicit
status-returning methods and can be reconstructed from the rules and input
commands by the game/network layer.

Card definitions are stored separately from per-match zone capacity. The
definition registry starts at 256 entries, grows through CMA as sets are
loaded, and supports up to 65,536 definitions without enlarging every engine
object's fixed board and snapshot arrays.

Effect callbacks receive an explicitly initialized operation buffer. It grows
through CMA-backed geometric reallocation from 256 entries and returns
`FT_ERR_FULL` only at the shared 16,777,216-record defensive ceiling.
Supported callback operations include player health and mana changes, event
emission, instance damage and healing, and permanent or end-of-turn instance
stat modifiers. Callback operations are prepared first and applied under the
same snapshot rollback path, so a rejected operation cannot leave a partial
effect behind.

Callbacks may also append an opaque `card_game_host_intent` with
`card_game_operation_buffer::append_host_intent()`. Before match start, the
caller registers each supported `(type_id, version)` and its ordered scalar
field schema with `card_game_engine::register_host_intent_type()`. Payloads are
limited to eight explicitly tagged `u32`, `i32`, `u64`, `i64`, or boolean
values. Libft validates the type, version, field count, field kinds, and scalar
ranges, but does not interpret or execute the intent. Host intents never apply
to combat state. They are published only after the whole event-resolution
transaction succeeds; a failed callback or operation publishes none of that
resolution's intents.

Consumers call `get_pending_host_intent_count()` and
`peek_pending_host_intent()` to read the ordered queue, then call
`acknowledge_host_intent(sequence)` only after accepting the queue head. A wrong
sequence is rejected, so callers cannot accidentally acknowledge a later
intent. Pending host intents, their sequence, and acknowledgement state are part
of the in-memory `card_game_snapshot` and `card_game_delta` contracts and the
authoritative state hash. Applying a snapshot or delta restores the exact pending
queue. The state format version is 2; callers must not treat version-1 snapshots
as compatible. This does not yet add a byte-level persistence codec or include
host intents in serialized command/replay records. Replaying commands re-runs
deterministic callbacks and can emit intents again, so a host still needs a
durable idempotency journal around consumer acceptance and replay recovery.

`card_game_script_bridge` is an optional adapter for consumers that want effect
callbacks written with Libft's bounded Scripting module. Initialize the bridge
with an initialized `card_game_engine`, register the host-intent schemas on the
engine, and then register scripts against event types. Scripts may call
`emit_host_intent(type_id, version, field_type, value, ...)`; field types use the
`card_game_host_intent_scalar_type` numeric values, and values are type-checked
before they enter the same transactional operation buffer as native callbacks.
Script integers are signed 64-bit values, so script-authored `u64` fields are
limited to `0..INT64_MAX`; native callbacks can supply the full `u64` range.
The bridge enforces an operation limit and a fixed maximum script-effect count.
`scripting_engine::get_native_manifest_hash()` fingerprints the ordered native
ABI, and `card_game_script_bridge::get_manifest_hash()` additionally includes
registered event bindings and canonical serialized script programs. Consumers
must persist and compare this value when loading session content; automatic
session-import enforcement and broader host-schema manifests are not yet wired.

Effect callbacks retain pointers into the bridge, and CardGame currently has no
callback-unregistration API. Therefore the bridge must remain alive and at a
stable address for as long as its engine can dispatch those callbacks; destroy
the engine before destroying the bridge, and do not move either object. Script
bindings are runtime registrations, not serialized state. Script source hashes,
versioned manifests, context/query natives, and persistence/replay integration
remain required before this adapter is suitable as a cross-session content
format.

`get_snapshot()` captures the authoritative mutable match state using fixed-
width fields. `create_delta()` compares a previously captured snapshot with
the current state and emits a player-scoped change mask plus base and target
state sequences. A receiver must first apply the matching snapshot and then
call `apply_delta()`; stale or mismatched deltas are rejected before any state
is changed. Snapshots and deltas contain state only and never serialize effect
function pointers or callback-owned data.

`get_rules_hash()` identifies the configured rules, cards, and phases.
`get_state_hash()` identifies the current authoritative match state. Both use
fixed-width field hashing rather than object-memory hashing, so padding and
platform byte order do not affect the result.

Zones are registered as data with a stable ID, capacity, owner-scope flag, and
allowed card-type mask. The engine does not assign game-specific meanings to
those IDs; the consuming game decides how its zones are used.

`card_game_zone_store` provides rules-neutral storage for those configured
piles. It supports owner-scoped or shared zones, stable instance/card pairs,
top insertion, inspection, removal, capacity checks, and card-type validation.
Its `move_instance()` operation validates the destination before removing the
source entry, so failed transfers leave both piles unchanged.
Bottom access and deterministic seeded shuffling are available for pile types
whose configuration permits those operations.
A profile can use it for graveyards, exile, stacks, reserves, command zones,
or custom piles without adding game-specific meanings to the kernel.

`card_game_engine` exposes the same configured-pile operations through
`zone_push_top`, `zone_push_bottom`, `zone_insert_at`, `zone_inspect`,
`zone_pop_top`, `zone_pop_bottom`, `zone_remove_instance`,
`zone_move_instance`, and `zone_shuffle`. Automatically created entries use
the engine's stable physical-instance ID allocator and share its ownership
checks with decks and hands.

Built-in card types use IDs `0` through `3`. Games may register custom type
IDs from `4` through `31` and register cards against them with
`register_card_with_type()`; custom type definitions include their allowed
zones and per-player copy limit.

When a card is played, the engine enforces the configured board-zone capacity,
the zone's allowed type mask, and the type's per-player copy limit before
charging mana or invoking an effect. Rejected placements leave match state
unchanged.

Legacy network callers may use `submit_command()` with a monotonically
increasing command sequence. The optional expected state sequence rejects
stale intents before dispatch. New retry-sensitive callers should use
`submit_action()` with schema version 1 and a nonzero stable request ID; an
exact retry returns the committed state hash without executing again, while
reusing that ID with a different command is rejected. This initial envelope
uses the existing command kinds and does not yet model session IDs, choices,
full result payloads, or durable session snapshots.

`CARD_GAME_INTENT_DRAW_CARD` is accepted through `submit_command()` and
recorded in command/replay format v5; v1 through v4 command logs remain
readable. Version 4 added request IDs; version 5 adds a bounded draw count
(1–100). A requested multi-draw resolves attempts in order: empty-deck and
full-hand outcomes are recorded per attempt, while internal failure rolls the
whole action back. The separate
`card_game_replay` event codec remains version 3. Draw rejects
nonzero unused payload fields. Without a configured draw policy it retains the
legacy one-card draw behavior. `configure_draw_policy()` binds it to
owner-scoped discard and burned zones plus two registered typed host-event
schemas. Under that policy, an empty pile reshuffles eligible discard once
using engine RNG; an empty result emits an empty-draw host intent; a full hand
burns each drawn physical card and emits an overflow host intent. Host events are
retained in the pending intent queue and included in snapshots, deltas and state
hashes. Event payloads are `(player_id, instance_id)`, with instance zero for
empty draw. The host maps those typed outcomes to its configured instability;
CardGame does not own the amount. Mandatory discard obligations, multi-action
session operations and the durable byte-level session codec remain open.

Each accepted command is retained in a dynamically growing authoritative
command log. Records include the command, optional idempotent request ID, rules
hash, and state hashes before and after execution for auditing or deterministic
replay. The log returns
`FT_ERR_FULL` only at its explicit 16,777,216-record safety ceiling, so
accepted history is never silently discarded.

`replay_command_records()` executes a validated record sequence on a fresh
started match. It checks the configured rules hash and state hash before every
command, routes execution through `submit_command()`, and checks the recorded
post-command hash. A mismatch stops replay with `FT_ERR_INVALID_STATE`.
Registered host-intent schemas contribute to the rules hash, ensuring replay
uses the same typed host contract even though emitted queue contents are not
part of replay records or match snapshots.

`card_game_ordered_zone` provides bounded ordered storage for decks and other
piles. It supports top/bottom/index insertion, peeking, removal, duplicate
policy, capacity enforcement, and deterministic seeded Fisher-Yates shuffles.
The engine also provides a match-owned RNG state through `set_random_seed`,
`get_random_state`, and the no-argument `shuffle_deck` overload. That state is
included in snapshots, deltas, and state hashes, so an authoritative shuffle
can be restored exactly. The pointer-taking overload remains available for
callers that intentionally manage an external deterministic stream.
The container stores stable card-instance IDs rather than pointers and returns
without mutation when an operation is rejected.

`card_game_engine` exposes the same deck operations per player through
`deck_push_top`, `deck_push_bottom`, `deck_insert_at`, `deck_peek_top`,
`deck_peek_bottom`, `deck_draw_top`, `deck_draw_bottom`, `deck_remove`, and
`shuffle_deck`. Deck order is part of snapshots, authoritative deltas, and
state hashes, so a replicated match preserves the exact order rather than only
the set of cards.

`submit_command()` also accepts `CARD_GAME_INTENT_DISCARD_HAND`. The command
selects a physical hand-card instance in `target_instance` and names the
configured discard destination zone in `card_id`; the selected instance is
moved transactionally and recorded with the same command sequence/state hashes
as other authoritative commands. The command record and full replay codecs
preserve both fields, so replay discards the same physical card. This is a
primitive action, not yet a persisted mandatory-discard obligation or the
policy-driven draw/overflow flow described by the integration design.

`card_game_format` provides an immutable, configuration-driven legality layer.
It validates stable profile and corpus identity, legal card rules, banlists,
copy limits, and typed card exceptions before commit. It computes a SHA-256
format identity and validates deck zones with structured diagnostics, allowing
network matches and replays to bind to one exact rules revision.

Deck entries also expose a unique physical-copy ID through `deck_inspect`,
`deck_get_instance`, `deck_draw_instance`, and the entry-returning
`deck_draw_top` overload. Copies with the same definition ID remain distinct
through shuffle, snapshots, deltas, and replay.

The engine also exposes a first-class hand zone. `draw_to_hand()` moves a
physical card instance from the deck while preserving its stable ID and
returns `FT_ERR_FULL` when the configured hand limit is reached.
`hand_inspect()` and `get_hand_count()` are read-only queries, and
`play_card_from_hand()` removes the selected instance only after the card play
has succeeded. Hand state is included in snapshots, deltas, state hashes, and
the configurable opening-hand start path.
`discard_hand_instance()` moves a selected hand instance into a configured
discard zone while retaining its physical instance ID. The destination is
validated before the hand is changed, so a full or incompatible discard zone
leaves the card in hand.
`mulligan_hand()` provides deterministic physical-instance selection, reinserts
selected cards into the deck, shuffles with caller-provided RNG state, and
draws replacements. Invalid selections and failed operations restore the hand,
deck order, RNG state, and state sequence.

`start_match(player_count, config)` treats the supplied match configuration as
the baseline. It applies starting health, starting mana, opening hand size,
first-player selection, and opening-hand dealing in one operation. Optional
class, hero, commander, or scenario extensions can register
typed, source-identified start overrides with `register_start_override()`.
Overrides are scoped to a player or all players, use deterministic priorities,
and are resolved transactionally before cards are dealt. The core engine does
not require any class/card extension; disabling one simply leaves the baseline
configuration in effect.

Set `random_first_player` to select the first player from the engine-owned
seeded random stream. With the flag disabled, `first_player` is used directly.
The same start path accepts every player count up to
`FT_CARD_GAME_MAX_PLAYERS`; turn rotation wraps over the configured player
count rather than assuming a two-player match.

`card_game_deck_encode` and `card_game_deck_decode` provide canonical,
checksum-protected Base64URL deck codes. `card_game_deck_hash` produces a
stable SHA-256 identity from the canonical deck representation. The separate
`card_game_replay` archive stores ordered events with stable hashes, explicit
match results, and can project a full-information replay into an irreversible
player-view replay. Replay event storage grows through CMA up to its explicit
safety ceiling and never silently discards older events. Authoritative command
records can be imported with `append_command_record()` and reconstructed into a
fresh engine with `replay_into()`.

Per-instance stat modifiers are tracked separately from immutable definitions.
`add_card_modifier`, `get_card_modifier`, `remove_card_modifier`, and
`get_effective_instance_stats` support permanent and end-of-turn modifiers;
the modifier records are replicated so expiration remains deterministic.

Turn and phase state is queryable with `get_turn` and `get_current_phase`.
Games register a phase graph with `register_phase`, inspect definitions with
`get_phase`, and advance it with `advance_phase`; allowed commands and entry or
exit events are enforced by the engine.

`resolve_combat` provides deterministic ordered or simultaneous creature
combat, applies effective stats, handles retaliation and deaths, and commits
only after the complete transaction succeeds.

Event callbacks may be registered with an explicit priority. Matching callbacks
are dispatched in ascending priority order, with effect ID as the deterministic
tie-breaker, so trigger ordering does not depend on host container iteration.

The usage-limit ledger is available through the engine for cards, effects,
effect groups, hero powers, summon procedures, and arbitrary configured
actions. Limits identify a stable key and subject independently and support
action, chain, phase, turn, round, match, and custom windows with explicit
attempt, activation, or resolution consumption policies.

Allowance predicates are registered by a stable predicate ID and receive the
allowance's stable predicate-context ID during evaluation. The context ID is
part of snapshots, deltas, and state hashes; the callback registry itself is
process-local and must be registered by the receiving process before a
snapshot is applied. Function pointers and user-data pointers are never part
of serialized match state.
