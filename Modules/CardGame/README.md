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
