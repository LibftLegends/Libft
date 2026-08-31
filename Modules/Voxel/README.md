# Voxel public headers

The Voxel module exposes focused headers so callers can include only the API
they need:

- `terrain_types.hpp` contains shared constants, enums, callbacks, and plain
  data structures.
- `terrain_config.hpp` contains biome, feature, ore, fluid, layer, and terrain
  generation configuration classes.
- `terrain_generation.hpp` contains generation contexts and world chunk
  coordinate classes.
- `terrain_api.hpp` contains module-level generation, biome, block registry,
  tree-template, and chunk-generation functions.
- `voxel_mesh.hpp` contains chunk mesh types and mesh functions.
- `terrain_scripting_bridge.hpp` contains the terrain scripting bridge API.
  Terrain configuration scripts are normalized and executed by Libft's custom
  Scripting runtime; the bridge no longer routes terrain execution through Lua.

Runtime block assets are loaded only after path validation and are capped at
`TERRAIN_RUNTIME_MAX_ASSET_SIZE`, currently 4 MiB per face asset, to bound
filesystem reads and memory use.

Runtime block registry entries can be acquired through
`terrain_acquire_block(...)` into a `terrain_runtime_block_handle`. The handle
keeps the block and its loaded asset bytes alive while a registry entry is
unregistered. Legacy raw-pointer accessors remain borrowed views and must not
outlive the registry entry.
