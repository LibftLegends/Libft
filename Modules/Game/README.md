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
- `game_inventory` - Inventory container for item stacks with add/remove/query behavior, revision-checked unique-instance add/remove, transfers, exact stackable-quantity removal, and value-only snapshots. Revisioned unique-item add returns the committed slot. `restore_snapshot(...)` rebuilds staged contents from a value snapshot, then replaces the map in place while preserving the inventory object and its process-local request caches; callers must exclude concurrent access and use it for compensating rollback, not ordinary save loading. Quantity removal preflights all matching untracked stacks, commits under one inventory lock and retains a bounded 4,096-entry scoped request cache for exact success/failure retry; the cache is process-local and is cleared on reinitialization/destruction. `game_item` has a separate 64-bit physical instance ID (zero for stackable/untracked items); identified instances are single-unit items and duplicate IDs are rejected within the destination inventory. The ID is a host-supplied identity token, not a global minting or persistence service.
- `game_character::get_inventory()` - Server-side access to the character-owned inventory. `game_inventory::get_snapshot(...)` returns value-only slot/item records and inventory metadata with the captured revision; inventory persistence consumes this locked snapshot and now preserves the revision through save/load. Older snapshots without the optional revision remain readable. Production builds do not expose the mutable item map. Use `clear_items()` for reset/load paths; it advances the inventory revision. Test builds retain `get_items()` for legacy fixture setup. Persistence does not yet include a recovery journal.
- Minecraft's `WorldItemStore` uses Libft `game_inventory::transfer_to` for revision-checked pickup transfers; the Minecraft store owns world entity identity, chunk indexing, reach/owner-lock checks, and request deduplication. Minecraft's single-player interaction creates configured built-in block drops with compensating cleanup on an edit failure. The store is in-memory and lacks durable atomic block/drop transactions, rendering, replication, and persistence.
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

- `game_crafting_ingredient` - Recipe ingredient with one exact item ID, up to eight unique explicit alternatives, or a stable tag ID, plus count and rarity matching. Tagged ingredients are expanded to the registered immutable alternatives when a recipe is registered.
- `game_crafting::query_recipe_at_station_snapshot(...)` - Evaluates recipe availability and ingredient deficits for a requested batch against one caller-captured `game_inventory_snapshot`, without reading or mutating a live inventory. The compatibility overload queries one batch. Hosts aggregating multiple recipes should capture once and reuse that immutable snapshot so every result row observes the same inventory revision; malformed snapshot aggregates and ingredient-count overflow are rejected.
- `game_crafting` - Recipe unlocks, non-mutating `query_recipe(...)` status/ingredient-deficit queries, recipe-owned output definitions, and two-phase `prepare_craft`/`commit_craft` transactions. Queries allocate inventory units across cumulative rarity-aware requirements and explicit alternatives without reusing stack units; rarity-specific groups are allocated before wildcard groups. Missing groups include their accepted alternative IDs. Tags are registered before recipes (maximum 64 tags, eight members each); Minecraft resolves its content tags and registers their members through this API. A prepared craft checks batch ingredients, primary and byproduct capacity, and the exact inventory revision; commit rejects stale inventory and advances the revision on success. Recipes support up to 16 immutable, value-copied byproducts, committed with their primary output and ingredient consumption. Output templates must have physical instance ID 0; the host may use `commit_craft(..., output_instance_id)` to assign a freshly minted ID to a unique one-item primary output. The commit rejects an ID already present in the source inventory. Host-side global ID uniqueness and use of this overload for unique Minecraft outputs remain host responsibilities. Existing caller-supplied-output `craft_item(...)` is compatibility-only and cannot execute recipes with authoritative bound outputs or station-constrained recipes. `set_recipe_station(...)` assigns one immutable positive station ID to a registered, still-locked recipe; register it locked, assign its station, then unlock it. Queries and preparation must use that station through `query_recipe_at_station(...)` and `prepare_craft_at_station(...)`; legacy calls use station 0 and fail closed for constrained recipes. Station assignment requires a locked recipe and is immutable before unlock. The query reports `station_available`. Minecraft still validates the real station, reach, and station revision. `prepare_processing_at_station(...)` now stages ingredient consumption without requiring output capacity and returns a value-only primary/byproduct snapshot; `commit_processing_inputs(...)` consumes the inputs while leaving products for a host-owned timed job. This is a processing transaction foundation, not a job scheduler: fuel policy, elapsed ticks, output retry, and job persistence remain host responsibilities.
- `game_recipe_book` - Stable-ID recipe catalogue and unlock state, with caller-configured initial grants, idempotent source-event grants, and transactional sparse persistence. `unlock_recipe_with_result(...)` separately reports whether the event receipt was newly recorded and whether it made the recipe newly known, so hosts can durably checkpoint a new receipt even when the recipe was already unlocked; the legacy `unlock_recipe(..., newly_unlocked)` API remains available. Unknown imported IDs remain quarantined and round-trip for future catalogues; `game_crafting` remains responsible for recipe definitions and executing crafts. `game_crafting::lock_recipe()` lets a host restore runtime access consistently when importing a prior recipe-book snapshot.
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
- `game_voxel_region::write_generated_block(...)` - Routes generator-owned
  writes to the appropriate loaded region chunk without creating player
  provenance.
- `game_voxel_chunk::apply_authoritative_block_change(...)` - Validates a
  client request against the current revision and block value, then returns a
  revisioned server delta.
- `game_voxel_chunk::apply_authoritative_block_delta(...)` - Applies one
  accepted server delta and rejects stale, duplicate, or skipped revisions.
- `game_voxel_chunk::get_player_override(...)` and
  `get_player_override_count()` - Expose the compact current-state overrides
  used to preserve the generated baseline while synchronizing player changes.
- `game_voxel_chunk::get_revision()` - Returns the monotonically increasing
  player-edit revision for the chunk.
- `game_voxel_chunk::copy_blocks(...)` - Copies a complete chunk snapshot in
  one read-locked operation using the same `z, y, x` ordering as the terrain
  snapshot pipeline.
- `game_block_change_request` and `game_block_delta` - Separate client-intent
  and server-accepted wire records containing world/chunk identity, local
  coordinates, revisions, and provenance.
- `game_world_delta_history` - Bounded recent-delta retention for recovery;
  an evicted revision returns `FT_ERR_OUT_OF_RANGE` so the caller can send a
  full snapshot instead of silently losing synchronization.
- `game_world_delta_channel` - Transport-neutral per-chunk authority
  coordinator combining request validation, revision history, snapshot
  handoff, live-client collection, and snapshot serialization.
- `game_world_delta_snapshot_serialize(...)` and
  `game_world_delta_snapshot_deserialize(...)` - Length-bounded, checksummed
  full-snapshot envelope helpers.
- `game_voxel_chunk::is_generation_protected()` - Reports whether a manual block edit protects the chunk from automatic regeneration.
- `game_voxel_generation_metadata` - Persisted seed, world origin, generator
  version, configuration signature, and completed-stage mask for validating
  cached generated chunks.
- `game_voxel_chunk::set_generation_metadata(...)`,
  `get_generation_metadata()`, `has_generation_metadata()`, and
  `generation_metadata_matches(...)` - Manage and query generation cache
  identity.
- `game_voxel_chunk` uses a heap-owned writer-priority, non-recursive RW lock
  for public block, metadata, override, revision, serialization, and lifecycle
  operations. `get_generation_metadata()` returns a thread-local snapshot;
  `get_section()` remains a borrowed reference for lifecycle-exclusive or
  caller-synchronized code and must not be retained across concurrent writes.
- `game_voxel_region::is_chunk_visible(...)` - Tests whether a loaded voxel chunk intersects a camera frustum.

## Hooks, Scripting, and Catalogs

- `ft_game_hook_context` - Hook invocation context.
- `ft_game_hook_metadata` - Hook metadata record.
- `ft_game_hook_listener_entry` - Registered listener entry.
- `game_hooks` - Hook registry/dispatcher with add/remove/emit behavior.
- `game_script_context` - Script execution context with game state, world,
  variables, and an opaque extension pointer for module-specific bindings.
- `game_script_bridge` - Bridge for registering and invoking script callbacks.
  It defaults to Libft's custom `Scripting` runtime. New code should use
  `execute_with_user_data(...)` or `execute_custom_with_user_data(...)`; typed
  callbacks are resolved through stable native IDs. A temporary line-command
  adapter preserves existing `set`, `unset`, and `call` scripts while they are
  migrated to the custom language.
- The old interpreter-specific entry points have been removed. Script sources
  are executed by Libft's bounded custom runtime; the line-command adapter is
  retained only as a temporary source-compatibility layer for existing assets.
- Registered callbacks may return one integer through
  `game_script_context::set_result_integer(...)`; the custom adapter exposes
  that result to the script. This is used by the Voxel bridge for runtime block
  ids.
- Custom-runtime execution is bounded by its configured deterministic
  operation and resource limits.
  Filesystem, operating-system, package-loading, debug, and dynamic-code
  libraries are not exposed to scripts.
- Registered callback failures are raised immediately inside Lua, so later
  callbacks in the same script do not run after a failed host operation.
- `game_data_catalog` - Registry for item definitions, recipes, loadouts, and other static catalog records.

## Serialization and Persistence

Several classes provide save/load or serialization helpers in their `.hpp` surface and corresponding `.cpp` implementations. These methods serialize gameplay state, restore records, and report errors through the module's lifecycle/error conventions.

Voxel chunk serialization is transactional: a failed serialization or
deserialization does not replace the caller's existing buffer or chunk state.
Version-4 chunk data is accepted through the compatibility reader, with its
legacy edit history consumed and discarded before the chunk is represented in
the current format.

## Common Method Groups

Across this module, public lifecycle classes consistently expose some or all of:

- Constructors/destructors plus deleted copy/move constructors and assignments.
- `initialize()`, `initialize(const T &)`, `initialize(T &&)`, `destroy()`, and `move(T &other)`.
- `enable_thread_safety()`, `disable_thread_safety()`, and `is_thread_safe()`.
- `lock(...)` / `unlock(...)` on classes that expose explicit lock helpers.
- Domain getters/setters for ids, names, values, tables, children, records, counters, and timestamps.
- `get_error()` and `get_error_str()` on classes with object-local error reporting.

Minecraft recipe-policy runtime coverage (2026-10-13): A focused direct MinGW executable compiled the current MinecraftItemCatalog.cpp and MinecraftRecipeCatalog.cpp plus current Libft game_crafting.cpp, game_inventory.cpp, and game_recipe_book.cpp, linked against the existing support archive, initialized both catalogs, and ran validate_recipe_coverage successfully (recipe-coverage=0 outputs=93 missing=0). This verifies that every current content item whose acquisition policy is recipe-backed has a catalogued recipe output, and every tracked card output satisfies its unique recipe/discovery policy checks. It does not prove all proposed recipes/features in Sections 10-12 exist or that the application-wide validators pass. The regression test enforcing deck-wide copy limits also passes a direct runtime harness (cross-zone-copy-cap=ok).
