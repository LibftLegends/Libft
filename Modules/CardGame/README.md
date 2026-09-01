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

Network-facing callers should use `submit_command()` with a monotonically
increasing command sequence. The optional expected state sequence rejects
stale client intents before dispatch; accepted commands then enter the same
transactional play, turn, and phase paths used internally.

Each accepted command is retained in a dynamically growing authoritative
command log. Records include the command, rules hash, and state hashes before
and after execution for auditing or deterministic replay. The log returns
`FT_ERR_FULL` only at its explicit 16,777,216-record safety ceiling, so
accepted history is never silently discarded.

`replay_command_records()` executes a validated record sequence on a fresh
started match. It checks the configured rules hash and state hash before every
command, routes execution through `submit_command()`, and checks the recorded
post-command hash. A mismatch stops replay with `FT_ERR_INVALID_STATE`.

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
