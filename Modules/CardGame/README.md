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
queue. Every pending host intent records the originating action request ID
(zero for intents produced outside a submitted action); snapshots, deltas, and
state hashes preserve it. The byte snapshot codec writes version 2 and reads
version 1 snapshots with a zero action request ID for legacy intents. Host
intents are not included in serialized command/replay records. Replaying
commands re-runs deterministic callbacks and can emit intents again, so a host
still needs a durable idempotency journal around consumer acceptance and replay
recovery.

`card_game_script_bridge` is an optional adapter for consumers that want effect
callbacks written with Libft's bounded Scripting module. Initialize the bridge
with an initialized `card_game_engine`, register the host-intent schemas on the
engine, and then register scripts against event types. Scripts may call
`emit_host_intent(type_id, version, field_type, value, ...)` and
`random_bounded(bound)`; field types use the
`card_game_host_intent_scalar_type` numeric values, and values are type-checked
before they enter the same transactional operation buffer as native callbacks.
Script integers are signed 64-bit values, so script-authored `u64` fields are
limited to `0..INT64_MAX`; native callbacks can supply the full `u64` range.
The bridge enforces an operation limit and a fixed maximum script-effect count.
`scripting_engine::get_native_manifest_hash()` fingerprints the ordered native
ABI, and `card_game_script_bridge::get_manifest_hash()` additionally includes
registered event bindings and canonical serialized script programs, the
canonical host-intent/draw-policy schema hash, and an optional host-supplied
policy manifest hash and configured script operation limit.
`serialize_script_catalog()` emits bounded checksummed
`CGSC` version-1 bundles. `load_script_catalog()` verifies every program and
compares the computed manifest with the caller's trusted hash before accepting
the catalogue; a mismatch unregisters the staged callbacks. The caller must
obtain the trusted hash from its authenticated content manifest. Automatic
session-import enforcement remains a host integration responsibility.
During an effect invocation, scripts may read `card_actor_id()`,
`card_instance_id()`, and `session_phase()`. These return the actor, physical
source instance, and current engine phase for that invocation; they cannot be
queried outside the callback. `choice_selected_id(choice_id)` returns a selected
option only after resolution and only when the choice belongs to the active
player. These natives are registered in a fixed order and contribute to the
native manifest hash. `random_bounded(bound)` is also
available only during an effect invocation. It advances the match-owned
xorshift stream with rejection sampling, returns a uniform integer in
`[0, bound)`, and is restored with the surrounding event snapshot if script
execution or later event processing fails. Its RNG state participates in the
existing snapshot, delta, and state hash. Hosts can register up to 64 strictly increasing nonzero policy-context key IDs before registering scripts, then set or clear signed 64-bit values between engine actions. `context_value(key_id)` reads only a registered, set value during an effect; unknown keys and unset values fail. The sorted key schema contributes to the bridge manifest, while live values remain host-owned inputs and stay excluded from content identity. `serialize_context_values` and `restore_context_values` persist those inputs in a bounded checksummed payload tied to the registered key order; `get_registered_context_keys` returns the ordered schema so a restoring host can reject configuration drift. The session owner still decides when and where values are saved. Scripts can also append bounded `draw_cards(count)` and `require_discard(count)` operations during an effect. Both target the active player and are applied within the event transaction. Draws use the configured draw policy; discard requests create the same physical-card, configured-zone obligation enforced by normal draw commands. Counts must be positive and no greater than the engine limits. Native names are part of the ordered bridge manifest.

Effect callbacks retain pointers into the bridge. `bridge.destroy()` now
unregisters each callback before releasing its script program; call it while
the engine object is still alive and do not move either object. Destroying the
engine first also clears its callback registry, after which the still-live
engine object can safely be followed by `bridge.destroy()`. Do not reinitialize
the engine while a bridge still owns registrations. Pending events for an
unregistered effect no longer invoke that effect. Script bindings remain
runtime registrations and are not part of CardGame snapshots. The optional
bridge serializes its programs as a separate checksummed catalogue and can
enforce a caller-supplied trusted manifest when loading it. The host still
owns manifest authentication and session-import enforcement.

`get_snapshot()` captures the authoritative mutable match state using fixed-
width fields. `create_delta()` compares a previously captured snapshot with
the current state and emits a player-scoped change mask plus base and target
state sequences. A receiver must first apply the matching snapshot and then
call `apply_delta()`; stale or mismatched deltas are rejected before any state
is changed. Snapshots and deltas contain state only and never serialize effect
function pointers or callback-owned data.

`get_rules_hash()` identifies the configured rules, cards, and phases.
`get_script_environment_hash()` identifies the draw policy and typed
host-intent schemas in canonical type-ID order, independent of registration
order and effect callback pointers.
`get_state_hash()` identifies the current authoritative match state.
`get_rules_hash()` and `get_state_hash()` use fixed-width field hashing rather
than object-memory hashing, so padding and platform byte order do not affect
the result.

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

Host callers may use `submit_command()` with a monotonically increasing
command sequence. The optional expected state sequence rejects stale intents
before dispatch. Retry-sensitive callers use `submit_action()` with a nonzero
stable request ID; schema version 2 requires the physical source instance ID
for a play action. An exact retry returns the committed state hash without
executing again, while reusing that ID with a different command or physical
card is rejected. Schema version 1 remains readable for older callers.
Choice selection is a sequenced `CHOOSE_OPTION` command
carrying a stable choice ID and option ID. Client actions cannot supply its
clock value: `submit_action()` rejects choice commands, and the trusted host
uses `submit_host_action()` to inject the authoritative epoch. An exact retry
reuses the first committed result even if the host's current epoch has since
advanced. The broader action API still lacks session IDs, complete result
payloads, and durable session coordination.

`query_pending_choices(player_id, output)` is a read-only per-player view of
unresolved choices. It includes stable choice and option IDs, option values,
kind, and deadline, while omitting the timeout default and internal resolution
state. The query builds a staged result and preserves the caller's previous
output if allocation fails.

`query_legal_actions(player_id, output)` reports command kinds currently
available under the active phase and mandatory-discard/choice obligations,
plus player-specific and global pending-choice counts. While any unresolved
choice is open, action submission permits only its owner's choice or the
trusted expiry action; a mandatory discard takes priority until fulfilled. The
query does not execute effects, inspect targets, or advance RNG. Card-specific
target legality and unavailable/no-effect explanations remain part of the
host-facing effect-status query contract.

`CARD_GAME_INTENT_DRAW_CARD` is accepted through `submit_command()` and
recorded in command-log format v9; v1 through v8 command logs remain readable.
Version 4 added request IDs; version 5 adds a bounded draw count
(1ÃƒÂ¢Ã¢â€šÂ¬Ã¢â‚¬Å“100). A requested multi-draw resolves attempts in order: empty-deck and
full-hand outcomes are recorded per attempt, while internal failure rolls the
whole action back. The separate
`card_game_replay` event codec is version 8 and retains readers for versions
1 through 7. Version 6 carries the authoritative choice epoch; version 7
carries the action result value used by host deadline resolution; version 8
records the exact physical card instance consumed by a play. Draw rejects
nonzero unused payload fields. Without a configured draw policy it retains the
legacy one-card draw behavior. `configure_draw_policy()` binds it to
owner-scoped discard and burned zones plus two registered typed host-event
schemas. The discard destination is required independently of whether discard
reshuffling is enabled. Under that policy, an empty pile reshuffles eligible discard once
using engine RNG; an empty result emits an empty-draw host intent; a full hand
burns each drawn physical card and emits an overflow host intent. Host events are
retained in the pending intent queue and included in snapshots, deltas and state
hashes. Event payloads are `(player_id, instance_id)`, with instance zero for
empty draw. The host maps those typed outcomes to its configured instability;
CardGame does not own the amount. Mandatory discard obligations are stored in
snapshots, deltas, state hashes, command records and replay. While one is
pending, only the obligated player may discard an owned hand instance to the
configured destination; an empty resulting hand creates no impossible
obligation. Multi-action session operations and the durable byte-level session
codec remain open.

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
can be restored exactly. `random_bounded(bound, value_out)` uses rejection
sampling over that same stream for unbiased bounded values; scripts access it
through `random_bounded(bound)` during an effect callback. The pointer-taking
overload remains available for callers that intentionally manage an external
deterministic stream.
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
preserve both fields, so replay discards the same physical card. Policy-driven
draws record mandatory-discard obligations in snapshots, deltas, hashes,
command records and replay; while an obligation is pending, other actions are
rejected until its owner discards the required physical hand instance.

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

Deck-wide copy-limit enforcement (2026-10-13): card_game_format::validate_deck now totals each card definition across every submitted deck zone before applying its configured copy limit. The main and side zones can no longer each independently consume the full allowance. Added test_card_game_format_enforces_copy_limit_across_deck_zones for 4 allowed / 5 rejected, and a direct MinGW runtime harness linked the edited source and current Libft archive; it passed (cross-zone-copy-cap=ok). Both implementation and test pass strict warning-as-error syntax compilation. The full Libft test runner is still unverified because the workspace Make runner fails during Git Bash startup.

Minecraft recipe-policy runtime coverage (2026-10-13): A focused direct MinGW executable compiled the current MinecraftItemCatalog.cpp and MinecraftRecipeCatalog.cpp plus current Libft game_crafting.cpp, game_inventory.cpp, and game_recipe_book.cpp, linked against the existing support archive, initialized both catalogs, and ran validate_recipe_coverage successfully (recipe-coverage=0 outputs=93 missing=0). This verifies that every current content item whose acquisition policy is recipe-backed has a catalogued recipe output, and every tracked card output satisfies its unique recipe/discovery policy checks. It does not prove all proposed recipes/features in Sections 10-12 exist or that the application-wide validators pass. The regression test enforcing deck-wide copy limits also passes a direct runtime harness (cross-zone-copy-cap=ok).

The host-intent API now exposes `get_next_host_intent_sequence()` so adapters
can distinguish an already acknowledged sequence from one that was never
issued. GameSession uses this persisted sequence high-water mark to make a
durably saved host-output acknowledgement idempotent across retries and
snapshot restore. This change has not been compiled or runtime-validated.

Host expiry actions use `CARD_GAME_INTENT_RESOLVE_EXPIRED_CHOICES` through `submit_host_action()`. The command records the host epoch and server actor; `card_game_action_result::result_value` returns the number of defaults applied and remains stable on retry and replay.

Host-managed card adapters can use `CARD_GAME_INTENT_PLAY_EFFECT` to log an
exact physical hand instance, effect event, and discard destination as one
transaction. Its fixed 64-byte `play_payload` is copied into the effect context
and preserved by command-log and replay serialization, so typed choices and
other deterministic effect inputs are available during replay. Command-log
version 13 and replay version 12 add this payload, the action request ID, and a
dedicated discard-zone field while older versions remain readable.
For session-bound actions and replay, the Minecraft regeneration callback also
checks that the payload request ID matches the engine's active action request
ID before producing a host intent.

Command-log v8 adds the action result value; v9 adds the exact source instance ID for played cards. Replay v7 adds the action result value and v8 carries the source instance ID; older command-log and replay versions remain readable.

Registered `max_copies_per_player` limits apply to the player's complete owned
deck: draw pile, hand, and every owner-scoped zone (including discard and
burned piles). Moving an instance between those locations does not consume a
second copy. Adding a new instance to an owned location checks the combined
count, and snapshot/delta application rejects a state that exceeds the limit.
Deck setup is permitted before `start_match()`, so copy checks accept any
configured player slot during setup. Legacy untyped card IDs retain their
existing behavior. This implementation slice has not yet been compiled or
tested.
