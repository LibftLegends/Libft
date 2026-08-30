# CardGame

`card_game_engine` is a data-driven card-game foundation. The library stores
card definitions, configurable board and turn limits, and effect function
pointers; the game supplies the actual effect behavior and therefore owns the
rules that are specific to its game.

Register effects before registering cards. A card's `effect_id` selects the
callback dispatched by `play_card`. Match state is queried through explicit
status-returning methods and can be reconstructed from the rules and input
commands by the game/network layer.
