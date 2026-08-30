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

`voxel.hpp` is retained as a compatibility umbrella for consumers that need
the complete Voxel API. New code should include the narrowest focused header
that provides its required declarations.
