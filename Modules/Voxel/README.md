# Voxel public headers

The Voxel module exposes focused headers so callers can include only the API
they need:

- `voxel_types.hpp` contains shared constants, enums, callbacks, and plain
  data structures.
- `voxel_config.hpp` contains biome, feature, ore, fluid, layer, and terrain
  generation configuration classes.
- `voxel_generation.hpp` contains generation contexts and world chunk
  coordinate classes.
- `voxel_api.hpp` contains module-level generation, biome, block registry,
  tree-template, and chunk-generation functions.
- `voxel_mesh.hpp` contains chunk mesh types and mesh functions.
- `voxel_lighting.hpp` contains packed sky/block light, deterministic light
  builds, and `voxel_light_build_operation`. The operation API allows a
  worker to pause and resume scanning, propagation, and finalization according
  to `voxel_light_update_config` without blocking the caller for one monolithic
  solve.
- `voxel_shadow.hpp` contains the renderer-independent blob-shadow helpers:
  `voxel_shadow_find_receiver(...)` searches downward through solid voxel
  space for an entity's receiver surface, while
  `voxel_shadow_height_fade(...)` computes the bounded alpha fade from the
  entity height above that surface. Both APIs use a caller-provided lookup
  callback and do not access world storage directly, so renderers can use them
  with an immutable snapshot or another application-owned query source.
- `voxel_scripting_bridge.hpp` contains the terrain scripting bridge API.
  Terrain configuration scripts are normalized and executed by Libft's custom
  Scripting runtime; the bridge no longer routes terrain execution through Lua.

Runtime block assets are loaded only after path validation and are capped at
`VOXEL_RUNTIME_MAX_ASSET_SIZE`, currently 4 MiB per face asset, to bound
filesystem reads and memory use.

Runtime block registry entries can be acquired through
`voxel_acquire_block(...)` into a `voxel_runtime_block_handle`. The handle
keeps the block and its loaded asset bytes alive while a registry entry is
unregistered. Legacy raw-pointer accessors remain borrowed views and must not
outlive the registry entry.

## Fluid generation contract

`voxel_fluid_config` exposes `surface_river_depth` and
`surface_lake_depth`. Both values are validated in the range `1..16` and
default to `1`, so surface water is shallow unless a caller explicitly opts
into deeper depressions with `set_surface_water_depths(...)`. The existing
two-argument river and lake settings remain source-compatible.

Surface fluids never reshape the heightfield. The generated terrain and caves
are completed first; a valid river, lake, or legacy water candidate may only
fill a natural depression whose water depth is nonzero and within the
kind-specific configured maximum. A candidate must have a same-feature
cardinal neighbor, and every cardinal neighbor must either be the same valid
feature or provide a natural terrain bank at or above the water level. Border
decisions use world coordinates and do not recurse into generation.

Generation stages are ordered as:

```text
base terrain and caves
surface and underground fluids
snow, aquatic features, shrubs, trees, and other decoration
```

The fluid stage requires both `VOXEL_STAGE_BASE_TERRAIN` and
`VOXEL_STAGE_CAVES`. Underground `underground_lake_depth` means the number of
water layers. Exactly one air headroom layer is retained above those layers;
the configured maximum Y bounds the final water layer, while the floor, walls,
headroom, roof, and chunk-height checks must all fit before a lake is written.
