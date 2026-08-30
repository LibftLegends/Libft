# Game Module Modding and Scripting Extension Points

This document enumerates the supported hooks for extending the `Game` module with external content, automation, or scripting runtimes. The guidance keeps mods isolated from core engine state while preserving determinism and observability guarantees.

## High-level architecture

The game runtime exposes three integration layers:

1. **Data-driven content** – JSON/YAML manifests loaded by `ft_world` describe entities, quests, loot tables, and balance tweaks.
2. **Scripting bridges** – the callback-command bridge exposes a restricted
   host API through `game_script_bridge`. Libft's bounded custom scripting
   runtime adapts calls to this bridge; external interpreters are not required.
3. **Event pipelines** – asynchronous hooks that attach to the scheduler to react to combat, crafting, quest, and dialogue events.

Mods interact with these layers through `ft_mod_context`, which validates permissions and orchestrates lifecycle callbacks.

## Mod packaging

Mods ship as directories with the following layout:

```
mods/
  <mod-id>/
    manifest.yml
    scripts/
    data/
    assets/
```

- `manifest.yml` declares metadata (version, author, required engine features) and the entry points to register.
- `scripts/` contains files consumed by the application's selected scripting
  runtime. Libft supplies the callback bridge and terrain bindings through its
  bounded custom scripting runtime; no external interpreter is required.
- `data/` holds JSON/YAML content for new items, quests, and dialogue. The engine merges these with the base catalogs via `ft_world::merge_content_catalogues`.
- `assets/` carries optional binary resources (audio, textures) referenced by content definitions.

## Lifecycle callbacks

During startup the engine iterates through enabled mods and invokes callbacks in order:

1. `on_register(ft_mod_context &context)` – declare new content IDs, events, or services. Registration must be idempotent.
2. `on_load(ft_mod_context &context)` – hydrate state, attach event listeners, and schedule background tasks. Use the provided `ft_scripting_dispatcher` to enqueue scripts.
3. `on_unload(ft_mod_context &context)` – clean up resources and remove event handlers.

For long-running servers, mods may also implement hot-reload handlers that re-parse data without restarting the process.

## Event hooks

Mods subscribe to events through the dispatcher functions on `ft_mod_context`:

- `subscribe_combat_event(ft_combat_event_handler handler)`
- `subscribe_crafting_event(ft_crafting_event_handler handler)`
- `subscribe_dialogue_event(ft_dialogue_event_handler handler)`
- `subscribe_scheduler_event(ft_scheduler_event_handler handler)`

Handlers receive immutable snapshots of the triggering entities plus a mutable `ft_mod_action_queue` where they can enqueue follow-up actions (spawn rewards, adjust NPC behaviour, trigger cutscenes). Actions execute within the deterministic scheduler so outcomes stay reproducible across clients.

## Scripting sandbox

The scripting bridge exposes a curated API surface:

- Read-only getters for world state (`get_character_stats`, `list_active_quests`, `get_inventory_slots`).
- Commands gated by capability tokens (`grant_item`, `start_quest`, `modify_reputation`, `schedule_event`).
- Utility helpers for math, random numbers (seeded per session), and logging through the main logger with the `_api_logging` flag enabled.

The callback bridge uses deterministic operation and resource limits and does
not expose file, network, operating-system, package-loading, debug, or
native-module APIs. The legacy line-command execution path remains available
separately for compatibility.

The Voxel module adds terrain callbacks through
`terrain_script_register_api(...)`. Scripts can tune sea level, noise, biome
heights and palettes, noisy biome transitions, run deterministic chunk
generation, and perform generator-owned block writes. The terrain execution
context supplies the chunk origin and seed so callbacks remain deterministic.

## Security and isolation

To maintain stability:

- Mods cannot access raw pointers or engine internals. All interactions happen through opaque handles managed by `ft_mod_context`.
- Scripts cannot spawn threads or perform blocking I/O. Use asynchronous callbacks with timeouts where necessary.
- Sensitive APIs (economy adjustments, PvP matchmaking) require explicit server-side configuration and emit audit logs through the Observability module.

## Testing recommendations

Before publishing a mod, run the automated regression suite:

1. Execute `make test-mods` to compile scripts, validate manifests, and run sandboxed scenarios.
2. Use the deterministic replay harness to re-run combat or quest sequences and verify identical outcomes.
3. Capture Observability traces by enabling the task scheduler exporter and confirming mod-added spans include descriptive labels.

## Deployment workflow

1. Package the mod directory into a signed archive (ZIP or tarball).
2. Upload the archive to the distribution service configured in `Storage`.
3. Update the server configuration with the mod identifier and version.
4. Roll out to staging with feature flags, monitor SLO dashboards, then promote to production.

Document changelogs and compatibility notes in `manifest.yml` so operators know when to reload mods.
