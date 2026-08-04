# Game

The `Game` module contains gameplay-domain lifecycle classes for characters, items, inventory, equipment, skills, upgrades, buffs/debuffs, economy, crafting, dialogue, events, behavior trees, worlds, regions, voxels, scripting, hooks, data catalogs, and telemetry. Most classes follow the shared lifecycle pattern: construct, `initialize`, use, then `destroy`; mutable classes usually expose optional thread-safety helpers and object-local error accessors.

`game_world`, the behavior-tree composite/root classes, `game_voxel_region`, and the voxel chunk/section classes require caller-side synchronization when shared between threads. They intentionally do not provide internal mutexes.

## Public Contract

The classes below are the main orchestration surfaces in the module. Their lower-level record types mostly follow the same lifecycle and error patterns, but these four are the ones that coordinate the larger subsystems.

| Class | Primary behavior | Return codes |
| --- | --- | --- |
| `game_state` | Owns the world and character collections, game variables, and hook dispatch. It is the broad state container used by higher-level game code. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_STATE`, `FT_ERR_NOT_FOUND`, `FT_ERR_NO_MEMORY`, `FT_ERR_GAME_GENERAL_ERROR`, `FT_ERR_*` from nested helpers |
| `game_world` | Owns the event scheduler, world registry, replay state, economy/crafting/dialogue tables, quest/vendor/upgrade records, persistence helpers, and route planning. It is the main world orchestration object. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_STATE`, `FT_ERR_NOT_FOUND`, `FT_ERR_IO`, `FT_ERR_NO_MEMORY`, `FT_ERR_CONFIGURATION`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_GAME_GENERAL_ERROR`, `FT_ERR_*` from nested helpers |
| `game_server` | Owns the websocket server bridge, connected clients, and the active world reference. It starts and runs the server loop. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_STATE`, `FT_ERR_NOT_FOUND`, `FT_ERR_IO`, `FT_ERR_NO_MEMORY`, `FT_ERR_CONFIGURATION`, `FT_ERR_PERMISSION_DENIED`, `FT_ERR_GAME_GENERAL_ERROR`, `FT_ERR_*` from nested helpers |
| `game_event_scheduler` | Queues, cancels, reschedules, and processes game events. It also exposes optional profiling and snapshot helpers. | `FT_ERR_SUCCESS`, `FT_ERR_INVALID_STATE`, `FT_ERR_NOT_FOUND`, `FT_ERR_NO_MEMORY`, `FT_ERR_CONFIGURATION`, `FT_ERR_GAME_GENERAL_ERROR`, `FT_ERR_*` from nested helpers |

General rules for these orchestration classes:
- `initialize()` prepares the object for use and may fail if a required sub-object cannot be created.
- `destroy()` is expected to be idempotent and to keep cleaning up even after intermediate failures.
- `move(...)` transfers state from a source object and leaves the source destroyed.
- `get_error()` and `get_error_str()` report the object-local failure state after initialization; uninitialised access follows the shared lifecycle abort contract.

## Characters, Items, Inventory, and Equipment

- `game_character` - Character record with identity, stats, inventory/equipment integration, save/load helpers, getters/setters, add/remove operations, metrics, lifecycle, error accessors, and optional thread safety.
- `game_item_modifier` - Item modifier id/value pair with lifecycle, getters/setters, error accessors, and optional thread safety.
- `game_item` - Stackable item with id, size, rarity, dimensions, four modifiers, stack operations, modifier accessors, lifecycle, error accessors, and optional thread safety.
- `game_inventory` - Inventory container for item stacks with add/remove/query behavior, lifecycle, error accessors, and optional thread safety.
- `game_equipment_slot` - Equipment slot enum.
- `game_equipment` - Equipment loadout by slot with equip/unequip/query behavior, lifecycle, error accessors, and optional thread safety.
- `game_item_definition`, `game_loadout_entry`, `game_loadout_blueprint`, and `game_recipe_blueprint` - Catalog records used to define items, starting loadouts, and crafting recipes.

## Progression, Effects, and Economy

- `game_skill` - Skill level/experience-style record with upgrade behavior, lifecycle, getters/setters, error accessors, and optional thread safety.
- `game_upgrade` - Upgrade definition/state with cost/effect data and lifecycle helpers.
- `game_experience_table` - Level-to-experience table with lookup and mutation helpers.
- `game_progress_tracker` - Tracks named progress counters and completion state.
- `game_achievement` and `game_goal` - Achievement state with goal/progress maps and completion checks.
- `game_buff` and `game_debuff` - Timed positive/negative effects with magnitude/duration/state accessors.
- `game_resistance` - Damage/type resistance record with value accessors.
- `game_reputation` - Reputation value and threshold tracking.
- `game_currency_rate`, `game_price_definition`, `game_rarity_band`, `game_vendor_profile`, and `game_economy_table` - Economy records and tables for pricing, conversion, rarity bands, vendor behavior, and lookups.

## Crafting, Dialogue, Quests, and Events

- `game_crafting_ingredient` - Recipe ingredient struct.
- `game_crafting` - Crafting system that validates ingredients and produces recipe outputs.
- `game_dialogue_line` - One dialogue line with id, speaker/text, conditions, and lifecycle/error handling.
- `game_dialogue_script` - Ordered dialogue script.
- `game_dialogue_table` - Dialogue repository/table with register/fetch behavior.
- `game_quest` - Quest state and objectives.
- `game_event` - Scheduled or emitted event payload.
- `game_event_scheduler` - Priority/timed event scheduler with register, cancel, run, telemetry, lifecycle, and optional thread safety.
- `game_event_compare_ptr`, `s_event_scheduler_profile`, and `game_event_scheduler_telemetry_state` - Scheduler ordering, profiling, and telemetry structs.

## Behavior and AI

- `game_behavior_action` - Weighted/cooldown action definition.
- `game_behavior_profile` - Profile containing behavior weights and actions.
- `game_behavior_table` - Registry of behavior profiles.
- `game_behavior_context` - Runtime context carrying a character pointer and user data.
- `game_behavior_node` - Abstract behavior-tree node with `tick`.
- `game_behavior_composite` - Base composite node with child management.
- `game_behavior_composite::initialize()` / `destroy()` - Explicitly initialize
  and release composite child storage.
- `game_behavior_selector` and `game_behavior_sequence` - Standard behavior-tree composites.
- `game_behavior_tree_action` - Leaf node wrapping an action callback.
- `game_behavior_tree` - Owns and ticks a behavior-tree root.
- `game_behavior_tree::initialize()` / `destroy()` - Explicitly initialize and
  release root storage; construction alone does not make the tree usable.

## World, Map, Regions, and Voxels

- `game_state` - Top-level game state container.
- `game_server` - Server lifecycle class for running game systems.
- `game_world` - World container with regions, registry, events, serialization/save/load/replay integration, lifecycle, error accessors, and optional thread safety.
- `game_world_region` - Region record inside a world.
- `game_world_registry` - Registry for world records and component-style data.
- `game_world_replay_session` - Lifecycle-managed replay capture/playback session with instance error accessors; callers must synchronize shared sessions.
- `game_region_definition` - Region metadata and configuration.
- `game_region_backend` - Backend abstraction for region storage.
- When `LIBFT_TEST_BUILD` is defined, `game_region_backend` also exposes `game_world_region_backend` and `game_voxel_region_backend` aliases so tests can exercise both concrete region implementations in one build.
- `game_map3d` - 3D map/grid helper.
- `game_path_step` and `game_pathfinding` - Path step record and pathfinding system. `game_path_step_test_helper` exposes test-oriented construction/access.
- `game_voxel_chunk_section`, `game_voxel_chunk`, and `game_voxel_region` - Voxel storage for chunks, sections, and regions when the voxel backend is enabled.
- `game_voxel_chunk::write_generated_block(...)` - Writes generator-owned blocks without marking the chunk as player-protected.
- `game_voxel_chunk::is_generation_protected()` - Reports whether a manual block edit protects the chunk from automatic regeneration.
- `game_voxel_generation_metadata` - Persisted seed, world origin, generator
  version, configuration signature, and completed-stage mask for validating
  cached generated chunks.
- `game_voxel_chunk::set_generation_metadata(...)`,
  `get_generation_metadata()`, `has_generation_metadata()`, and
  `generation_metadata_matches(...)` - Manage and query generation cache
  identity.
- `game_voxel_region::is_chunk_visible(...)` - Tests whether a loaded voxel chunk intersects a camera frustum.

## Hooks, Scripting, and Catalogs

- `ft_game_hook_context` - Hook invocation context.
- `ft_game_hook_metadata` - Hook metadata record.
- `ft_game_hook_listener_entry` - Registered listener entry.
- `game_hooks` - Hook registry/dispatcher with add/remove/emit behavior.
- `game_script_context` - Script execution context with game state, world,
  variables, and an opaque extension pointer for module-specific bindings.
- `game_script_bridge` - Bridge for registering and invoking script callbacks.
  `execute_with_user_data(...)` lets another module expose a typed API without
  adding that module as a dependency of `Game`. `execute_lua(...)` and
  `execute_lua_with_user_data(...)` run real Lua 5.4 source through the
  statically embedded runtime. Registered callbacks are exposed as Lua global
  functions. `get_lua_global_string(...)`,
  `get_lua_global_integer(...)`, and `get_lua_global_boolean(...)` allow a
  host to read explicitly exported primitive values from the Lua state.
- `set_lua_instruction_limit(...)` and `set_lua_memory_limit(...)` bound Lua
  execution. The default limits are 100,000 VM instructions and 16 MiB.
  Filesystem, operating-system, package-loading, debug, and dynamic-code
  libraries are not exposed to scripts.
- `game_data_catalog` - Registry for item definitions, recipes, loadouts, and other static catalog records.

## Serialization and Persistence

Several classes provide save/load or serialization helpers in their `.hpp` surface and corresponding `.cpp` implementations. These methods serialize gameplay state, restore records, and report errors through the module's lifecycle/error conventions.

## Common Method Groups

Across this module, public lifecycle classes consistently expose some or all of:

- Constructors/destructors plus deleted copy/move constructors and assignments.
- `initialize()`, `initialize(const T &)`, `initialize(T &&)`, `destroy()`, and `move(T &other)`.
- `enable_thread_safety()`, `disable_thread_safety()`, and `is_thread_safe()`.
- `lock(...)` / `unlock(...)` on classes that expose explicit lock helpers.
- Domain getters/setters for ids, names, values, tables, children, records, counters, and timestamps.
- `get_error()` and `get_error_str()` on classes with object-local error reporting.
