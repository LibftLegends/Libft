# CardGame

`card_game_engine` is a data-driven card-game foundation. The library stores
card definitions, configurable board and turn limits, and effect function
pointers; the game supplies the actual effect behavior and therefore owns the
rules that are specific to its game.

Register effects before registering cards. A card's `effect_id` selects the
callback dispatched by `play_card`. Match state is queried through explicit
status-returning methods and can be reconstructed from the rules and input
commands by the game/network layer.

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

Each accepted command is retained in a bounded authoritative command log.
Records include the command, rules hash, and state hashes before and after
execution for auditing or deterministic replay. The log returns `FT_ERR_FULL`
once its fixed capacity is reached, so accepted history is never silently
discarded.

`replay_command_records()` executes a validated record sequence on a fresh
started match. It checks the configured rules hash and state hash before every
command, routes execution through `submit_command()`, and checks the recorded
post-command hash. A mismatch stops replay with `FT_ERR_INVALID_STATE`.

`card_game_ordered_zone` provides bounded ordered storage for decks and other
piles. It supports top/bottom/index insertion, peeking, removal, duplicate
policy, capacity enforcement, and deterministic seeded Fisher-Yates shuffles.
The container stores stable card-instance IDs rather than pointers and returns
without mutation when an operation is rejected.

`card_game_engine` exposes the same deck operations per player through
`deck_push_top`, `deck_push_bottom`, `deck_insert_at`, `deck_peek_top`,
`deck_peek_bottom`, `deck_draw_top`, `deck_draw_bottom`, `deck_remove`, and
`shuffle_deck`. Deck order is part of snapshots, authoritative deltas, and
state hashes, so a replicated match preserves the exact order rather than only
the set of cards.
