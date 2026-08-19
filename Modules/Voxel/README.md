# Voxel

The `Voxel` module is compiled when `GAME_USE_VOXEL_REGION_BACKEND` is enabled. It generates biome-aware voxel terrain with seeded heightmaps and builds renderable chunk meshes from voxel data.

## Chunk Mesh

- `chunk_mesh_face` - Enum for west, east, down, up, north, and south block faces.
- `chunk_mesh_vertex` - One mesh vertex with voxel coordinates, texture coordinates, block id, and face id.
- `chunk_mesh_bounds` - Integer minimum and maximum bounds for generated mesh content.
- `chunk_mesh` - Mesh container with vertex vector, index vector, full chunk bounds, and cached occupied bounds.
- `chunk_mesh_initialize(chunk_mesh &mesh)` - Initializes the mesh vectors and bounds.
- `chunk_mesh_destroy(chunk_mesh &mesh)` - Destroys mesh-owned vectors.
- `chunk_mesh_clear(chunk_mesh &mesh)` - Clears mesh vertices/indices and resets bounds.
- `chunk_mesh_generate_from_chunk(chunk_mesh &mesh, const game_voxel_chunk &chunk)` - Generates visible faces for a voxel chunk.
- `chunk_mesh_intersects_frustum(frustum, world_origin_x, world_origin_y, world_origin_z)` - Tests whether a chunk-sized world box intersects the view frustum.
- `chunk_mesh_intersects_frustum(frustum, mesh, world_origin_x, world_origin_y, world_origin_z)` - Tests the mesh's occupied bounds against the view frustum.

## Voxel Generation

- `TERRAIN_GENERATOR_SURFACE_HEIGHT` - Default generated terrain surface height.
- `TERRAIN_GENERATOR_GRASS_BLOCK` - Block id used for grass.
- `TERRAIN_GENERATOR_DIRT_BLOCK` - Block id used for dirt.
- `TERRAIN_GENERATOR_STONE_BLOCK` - Block id used for stone.
- `TERRAIN_GENERATOR_SHRUB_BLOCK` - Block id used for shrub decorations.
- `TERRAIN_GENERATOR_OAK_LOG_BLOCK` - Block id used for tree trunks.
- `TERRAIN_GENERATOR_OAK_LEAVES_BLOCK` - Block id used for tree foliage.
- `TERRAIN_GENERATOR_CACTUS_BLOCK` - Block id used for cactus decorations.
- `terrain_builtin_block_id` - Single built-in block-id enum. The
  `TERRAIN_BUILTIN_BLOCK_COUNT` sentinel defines the first runtime id.
- `TERRAIN_BIOME_ZONE_WIDTH` - Width in world blocks used to group biome zones.
- `terrain_biome` - Biome selector enum for plains, hills, desert, snow, and mountains.
- `terrain_biome_profile` - Surface height, variation, and topsoil depth profile for a biome.
- `terrain_block_metadata` - Registry entry that describes whether a block is
  solid, transparent, liquid, replaceable, can host ore, is an ore, light-
  emitting, whether it occludes mesh faces, and how hard it is.
- `terrain_block_registration` - Runtime block definition containing metadata and one asset path per block face.
- `terrain_tree_template_block` - Relative block entry used by tree templates.
- `terrain_tree_template` - Block list wrapper for reusable tree presets.
- `terrain_generation_config` - Lifecycle-managed runtime generation policy
  containing noise, water, biome, and feature-rule settings.
- `terrain_biome_definition` - Lifecycle-managed customizable profile, block
  palette, template policy, and decoration/ridge/snow-cap policy for one biome
  slot. Use its `set_*` methods after `initialize()`.
- `terrain_feature_rule` - Lifecycle-managed seeded placement rule for caller-provided tree or
  object templates, with biome, height, water, and chance constraints.
- `terrain_ore_rule` - Lifecycle-managed optional deterministic ore policy with
  block, depth, vein, chance, enabled, and explicit ore-replacement settings.
  Existing ore blocks are never replaced unless `set_ore_replacement(FT_TRUE)`
  is selected. Coal, iron, and gold are disabled by default.
- `terrain_underground_structure_config` - Lifecycle-managed rounded cave
  rooms, optional surface entrances, and ravine generation ranges and
  densities. Cave radii, large-cave frequency, entrance frequency, and
  optional rounded cavern rooms are configurable through
  `set_cave_shape(...)`, `set_cave_entrances(...)`, and
  `set_cavern_rooms(...)`.
- `terrain_fluid_config` - Lifecycle-managed deterministic rivers and lakes.
- `terrain_layer_config` - Lifecycle-managed beaches, underwater sediment,
  and a broad height-based snowline for snow caps across compatible terrain.
- `terrain_generation_context` - Holds one validated, immutable generation
  snapshot for a Minecraft world. The world/save owner should initialize it
  once after loading its terrain policy and reuse it for every chunk.
- `terrain_world_chunk_coordinate` - Experimental coarse world-coordinate
  holder backed by two `ft_big_number` values. It keeps arbitrarily large
  signed chunk coordinates out of the per-block generation loops and exposes
  a stable coordinate hash for chunk-local generation.
- `terrain_generation_config_signature(...)` - Produces a stable signature for
  generation cache validation.
- `terrain_get_block_metadata(block_id)` - Looks up the metadata entry for a known block id.
- `terrain_block_is_known(block_id)` - Returns whether a block id exists in the registry and should be accepted by chunk storage.
- `terrain_register_block(registration, block_id_out)` - Loads six face assets
  from the supplied paths and registers a process-lifetime runtime block.
  Runtime ids are allocated after `TERRAIN_BUILTIN_BLOCK_COUNT`; names must be
  lowercase `namespace:name` identifiers containing only `a-z`, `0-9`, and
  `_`. Duplicate names, built-in-name shadowing, and invalid ore metadata are
  rejected. A failed duplicate registration does not consume an ID.
- `terrain_get_block_name(block_id)` - Returns a stable built-in or runtime
  block name.
- `terrain_find_block_id_by_name(name, block_id_out)` - Resolves a stable
  built-in or runtime name to the current numeric id.

Built-in names returned by `terrain_get_block_name()` are persistence ABI.
They must not be renamed or reordered without an explicit save migration or
alias. Runtime blocks are persisted by their stable names, never by their
temporary numeric IDs; loading fails when a referenced runtime name is absent.
- `terrain_get_block_asset_path(block_id, face)` - Returns the source path for one runtime block face.
- `terrain_get_block_asset_data(block_id, face, size_out)` - Returns the loaded bytes for one runtime block face. The Voxel module stores the bytes; a renderer or asset pipeline can decode them as appropriate.
- `terrain_block_is_solid(block_id)` - Returns whether a block is treated as a solid collision block.
- `terrain_block_is_transparent(block_id)` - Returns whether a block should be treated as visually transparent.
- `terrain_block_is_liquid(block_id)` - Returns whether a block behaves like a liquid.
- `terrain_block_is_replaceable(block_id)` - Returns whether a block can be overwritten by tree placement and similar terrain passes.
- `terrain_block_emits_light(block_id)` - Returns whether a block emits light.
- `terrain_block_occludes_faces(block_id)` - Returns whether a block should hide adjacent mesh faces.
- `terrain_block_hardness(block_id)` - Returns the block hardness value from the registry.
- `terrain_get_biome(world_block_x, world_block_z, seed_string)` - Picks a biome for a world block position and optional seed.
- `terrain_get_biome_profile(biome)` - Returns the height profile for a biome.
- `terrain_surface_block_for_biome(biome)` - Returns the surface block used for a biome.
- `terrain_biome_has_shrubs(biome)` - Returns whether a biome can spawn shrub decorations.
- `terrain_biome_has_trees(biome)` - Returns whether a biome can spawn tree decorations.
- `terrain_small_oak_tree_template(variant_index)` - Returns one of the reusable small oak presets.
- `terrain_small_pine_tree_template(variant_index)` - Returns one of the reusable small pine presets.
- `terrain_small_cactus_tree_template(variant_index)` - Returns one of the reusable small cactus presets.
- `terrain_large_oak_tree_template(variant_index)` - Returns one of the reusable large oak presets.
- `terrain_large_pine_tree_template(variant_index)` - Returns one of the reusable large pine presets.
- `terrain_generation_config_add_tree_template(...)` - Adds a reusable tree
  descriptor to a config-owned template registry.
- `terrain_generation_config_remove_tree_template(...)` and
  `terrain_generation_config_clear_tree_templates(...)` - Remove individual
  templates or reset the registry.
- `terrain_generation_config_assign_tree_template_to_biome(...)` and
  `terrain_generation_config_remove_tree_template_from_biome(...)` - Modify
  the template choices available to a biome.
- `terrain_tree_template_for_biome(biome)` - Returns the default tree preset for a biome.
- `terrain_tree_template_for_biome(biome, seed_value)` - Returns a seed-selected tree preset for a biome.
- `terrain_can_place_tree_template(chunk, local_origin_x, local_origin_y, local_origin_z, tree_template)` - Checks that a tree footprint fits in empty space before placement.
- `terrain_place_tree_template(chunk, local_origin_x, local_origin_y, local_origin_z, tree_template)` - Places a tree preset into a chunk.
- `terrain_generate_chunk(game_voxel_chunk &chunk, const char *seed_string)` - Fills a voxel chunk with biome-aware heightmap terrain based on an optional seed.
- `terrain_generate_chunk(game_voxel_chunk &chunk, int32_t world_block_origin_x, int32_t world_block_origin_z, const char *seed_string)` - Fills a voxel chunk with biome-aware heightmap terrain using a world-space chunk origin and optional seed.
- `terrain_generation_config` - Lifecycle-managed terrain policy object. Call
  `terrain_default_generation_config(config)` to initialize the built-in
  defaults, then use its setter methods and tree-template APIs to modify it.
  Call `destroy()` when the policy is no longer needed; copy and move use the
  explicit `initialize(other)` and `move(other)` methods.
- `terrain_default_generation_config(config)` - Initializes a config object
  with the built-in generation policy.
- `terrain_generation_config::set_*` - Validated mutation methods for scalar
  terrain policy values, callbacks, biome slots, feature rules, ore rules,
  underground structures, fluids, and terrain layers.
- `terrain_generation_config_is_valid(...)` - Checks biome counts, noise and
  chance ranges, block palettes, and feature templates before generation.
- `terrain_generation_context_initialize(...)` - Copies and validates a
  Minecraft-owned policy once, producing a generation context.
- `terrain_generation_config_serialize(...)` and
  `terrain_generation_config_deserialize(...)` - Encode/decode the versioned
  terrain-policy save payload using Libft buffers. Block references are stored
  by stable names and remapped to current numeric ids on load, including
  registered tree templates, biome palettes, ore rules, layer blocks, and
  feature rules.
- `terrain_generation_config_save_file(...)` and
  `terrain_generation_config_load_file(...)` - Libft-owned binary file I/O for
  the terrain policy used by a world save.
- `terrain_generate_chunk_with_context(...)` - Generates from a previously
  initialized context without revalidating the policy for every chunk.
- `terrain_generate_chunk_at_world_coordinate(...)` - Experimental generation
  entry point for a big-number chunk coordinate. The coordinate is promoted
  to the seed once per chunk while generation continues with native local
  coordinates. This is a branch experiment and does not yet provide seamless
  cross-chunk continuity for all legacy world-coordinate callbacks.
- `terrain_generate_chunk(..., const terrain_generation_config &config)` -
  Generates using caller-owned runtime settings without changing libft data.
- `terrain_generate_chunk_in_region(...)` - Generates a chunk through a
  `game_voxel_region` and routes feature blocks crossing chunk boundaries into
  neighboring region chunks.
- `terrain_generate_chunk_in_region_with_context(...)` - Region equivalent
  that consumes a previously initialized generation context.
- `terrain_cross_chunk_block_writer` - Optional callback used to route feature
  blocks outside the current chunk into neighboring chunks.
- `terrain_select_biome(...)` - Applies the configured biome selector and
  safely clamps custom selector results to the configured biome slots.
- `terrain_get_biome_index(...)` - Queries the active configured biome index
  for runtime HUD/debug integration, including custom slots.

Generation samples a shared, normalized biome-weight set for each world
column. Heights, surface/subsurface palette selection, and decoration policy
therefore use the same transition neighborhood; generation order and chunk
boundaries do not affect the result. Surface palette selection is deterministic
in world space, so a biome edge is dithered instead of becoming a hard block
line.

## World-generation settings reference

Call `terrain_default_generation_config(config)` first. It initializes all
nested settings and applies the built-in defaults below. Setters must be called
after initialization; percentages are inclusive `0`-`100` values, and boolean
arguments use `FT_TRUE` / `FT_FALSE`.

### Top-level `terrain_generation_config`

| Setting or method | Meaning | Built-in default / constraints |
| --- | --- | --- |
| `set_sea_level(value)` | Water-fill height in world blocks. | `72`; any signed height. |
| `set_noise_scales(large, detail, percent)` | Main terrain scale, detail scale, and detail contribution. Smaller scales create more frequent variation. | `32`, `8`, `50%`; scales must be positive, percent `0`-`100`. |
| `set_water_chance_percent(value)` | Per-column seeded chance for extra water in eligible low terrain, in addition to river/lake noise. This is not the percentage of the world that becomes water. | `0%`; `0`-`100`. Recommended `0`-`5%`. |
| `set_biome_count(value)` | Number of active biome slots. | `5`; `1`-`TERRAIN_MAX_CUSTOM_BIOMES` (`16`). |
| `set_biome_selector(selector, user_data)` | Optional callback that chooses the biome for a world position. | Built-in selector when callback is null. |
| `set_biome_transitions_enabled(value)` | Smooths height changes between biome zones. | Enabled. |
| `set_biome_transition_settings(scale, strength)` | Controls deterministic noisy surface/subsurface blending at biome edges. | `8`, `35%`; scale must be positive and strength `0`-`100`. |
| `set_mountain_ridges_enabled(value)` | Enables the global mountain-ridge pass. | Enabled. |
| `set_mountain_ridge_settings(scale, strength)` | Ridge frequency and height influence. | `48`, `8`; scale must be positive. |
| `set_erosion_enabled(value)` | Enables erosion noise in terrain height. | Enabled. |
| `set_erosion_settings(scale, strength)` | Erosion frequency and influence. | `24`, `3`; scale must be positive. |
| `set_cross_chunk_features_enabled(value)` | Allows features to cross chunk boundaries. | Enabled. Requires a cross-chunk writer for actual routing. |
| `set_cross_chunk_writer(writer, user_data)` | Receives generated blocks outside the current chunk. | No callback by default. |
| `set_feature_count(value)` | Number of custom feature rules used. | `0`; maximum `TERRAIN_MAX_FEATURE_RULES` (`16`). |
| `set_ore_rule_count(value)` | Number of ore rules used. | `3`; maximum `TERRAIN_MAX_ORE_RULES` (`16`). |

### Biomes: `terrain_biome_definition`

Each active biome has a height profile, three block layers, decoration policy,
and optional terrain policies. The built-in five slots are plains, hills,
desert, snow, and mountains.

| Setting or method | Meaning | Constraints / default behavior |
| --- | --- | --- |
| `set_biome_height_profile(index, surface, variation, topsoil)` | Baseline surface height, seeded height variation, and topsoil depth. | Variation and topsoil must be non-negative. |
| `set_biome_block_palette(index, surface, subsurface, deep)` | Block ids for the top, subsurface, and deep terrain layers. | Block ids must be known when the final config is validated. |
| `set_biome_decoration_policy(index, shrubs, trees, shrub_chance, tree_chance)` | On eligible surface columns, each percentage is the seeded chance that the corresponding decoration attempt is made. It is not the percentage of all blocks or all terrain. | Each chance is `0`-`100`; built-in defaults are shrubs enabled at `6%`, trees enabled at `18%`. Recommended shrubs `3`-`8%`, trees `10`-`20%`. |
| `set_biome_snow_caps_enabled(index, value)` | Allows the global snow-cap pass in this biome. | Enabled by default. |
| `set_biome_mountain_ridges_enabled(index, value)` | Allows the global ridge pass in this biome. | Enabled by default. |
| `set_biome_tree_template_override(index, template)` | Replaces the biome's normal tree choices. | Null removes the override. |

### Underground structures

`terrain_underground_structure_config` controls caves, ravines, and surface
entrances. The built-in policy is ravines and cave rooms enabled, with height
range `8`-`120`, ravine chance `4%`, cave-room chance `3%`, ravine shape
`2 x 20`, cave radii `2`-`3`, large-cave chance `20%`, entrance chance `8%`
with radius `1`, and cavern rooms disabled.

| Method | Meaning | Constraints |
| --- | --- | --- |
| `set_enabled(ravines, cave_rooms)` | Enables the original ravine and cave-room passes. | Boolean flags. |
| `set_chances(ravine, cave_room)` | `ravine` is the seeded chance for a ravine candidate at an eligible underground block; noise and the configured height/surface limits must also match. `cave_room` is the seeded chance that each nearby 12-block cave cell produces a room candidate. A successful room can carve many blocks, so the result is not a direct percentage of underground blocks. | `0`-`100%`. Recommended ravines `2`-`5%`, cave rooms `2`-`5%`; built-in defaults are `4%` and `3%`. |
| `set_height_range(minimum, maximum)` | Vertical range in which underground structures may form. | Minimum must not exceed maximum. |
| `set_shape(width, depth)` | Ravine width and depth. | Non-negative values. |
| `set_cave_shape(small_radius, large_radius, large_chance)` | Chooses the radius of ordinary cave rooms and the conditional chance that a selected room uses the larger radius. Rooms are placed in underground 12-block cells and can occur at any configured height. | Small radius must be non-zero; large radius must be at least the small radius and at most `16`; chance `0`-`100%`. Recommended `2`-`4` blocks, with large-cave chance `10`-`25%`; built-in values are radii `2`/`3` and `20%`. |
| `set_cave_entrances(chance, radius)` | After a cave-room candidate succeeds, this is the conditional chance that the room also receives a vertical surface entrance. Entrances can only occur from the room center height upward to the terrain surface and are limited by the entrance radius. | Chance `0`-`100%`; radius `1`-`8`. Recommended `5`-`12%` and radius `1`-`2`; built-in values are `8%` and radius `1`. |
| `set_cavern_rooms(enabled, chance, radius)` | After a cave-room candidate succeeds, this is the conditional chance that the room becomes a large cavern instead of a normal/large cave. Caverns are underground and respect the surface margin; they do not automatically open to the surface. | When disabled, pass `FT_FALSE, 0U, 0U`; values are normalized to zero. When enabled, radius is `5`-`32` and chance is `0`-`100%`. Recommended `2`-`8%` with radius `6`-`12`; use `0%` when a normal cave system is desired. |

### Fluids: `terrain_fluid_config`

| Method | Meaning | Built-in default / constraints |
| --- | --- | --- |
| `set_enabled(rivers, lakes)` | Enables river and lake passes independently. | Both enabled. |
| `set_river_settings(scale, width)` | River noise frequency and river width. | `96`, `3`; scale positive, width non-negative. |
| `set_lake_settings(scale, chance)` | In eligible low terrain, noise first identifies lake-shaped areas; this chance then decides whether the seeded lake candidate is filled. The final lake coverage is therefore lower than the configured percentage. | `48`, `4%`; scale positive, chance `0`-`100%`. Recommended `2`-`8%`; built-in `4%`. |

### Layers: `terrain_layer_config`

| Method | Meaning | Built-in default / constraints |
| --- | --- | --- |
| `set_enabled(beaches, snow_caps)` | Enables beach and snow-cap passes. | Both enabled. |
| `set_depths(beach, underwater, snow)` | Depth of beach, underwater sediment, and snow-cap layers. | Built-in `3`, `2`, `2`. |
| `set_snowline(minimum_height)` | Minimum surface height eligible for snow caps. | `84`; must be non-negative. |
| `set_block_palette(beach, underwater, snow)` | Block ids for the three layer passes. | Built-in palette is sand, sand, snow. |

### Ores and custom features

`terrain_ore_rule` exposes `set_range(minimum, maximum)`,
`set_vein(size, chance)`, and `set_enabled(value)`. The vein chance is checked
for each eligible solid block in the configured height range; it is not the
percentage of blocks that become ore because a successful check places a whole
vein. As a starting point, use `2`-`8%` for common ores and `1`-`3%` for rare
ores. The built-in rules reserve
coal (`8`-`120`, vein `8`, `12%`), iron (`4`-`80`, vein `6`, `8%`), and gold
(`4`-`48`, vein `4`, `4%`); all three are disabled by default. These built-in
percentages are intentionally generous starting values for the small test
worlds, not universal balance recommendations.

`terrain_feature_rule` exposes `set_template(...)`,
`set_biome_range(biome, minimum_height, maximum_height)`,
`set_chance(...)`, and `set_requires_dry_land(...)`. Its chance is checked once
per eligible feature-placement column after the biome, height, and dry-land
filters pass. Recommended values are `5`-`15%` for common trees/structures and
`1`-`5%` for rare structures. A feature is only placed when its template,
biome/height range, chance, and dry-land requirements all match. Feature rules
are disabled until included in the active feature count.

Example:

```cpp
terrain_generation_config config;
terrain_default_generation_config(config);
config.set_sea_level(68);
config.set_noise_scales(40, 10, 65);
config.underground_structures.set_cavern_rooms(FT_TRUE, 6U, 8U);
config.fluids.set_enabled(FT_FALSE, FT_TRUE);
terrain_generate_chunk(chunk, 0, 0, "world-seed", config);
config.destroy();
```

### Terrain scripting bridge

`terrain_scripting_bridge.hpp` exposes terrain generation through the Game
script callback bridge without creating a reverse `Game` dependency on
`Voxel`.

- `terrain_script_register_api(bridge)` registers the terrain callback set and
  is safe to call again when refreshing or hot-reloading bindings.
- `terrain_script_execute(...)` executes real Lua source with a chunk,
  generation config, world-block origin, and seed attached to its script
  context.

The registered callback names are `terrain_set_sea_level`,
`terrain_set_noise_scales`, `terrain_set_biome_height`,
`terrain_set_biome_blocks`, `terrain_set_biome_transitions`,
`terrain_generate_chunk`, and `terrain_write_generated_block`. Generated block
writes use `write_generated_block(...)`, so scripted generation does not mark a
chunk as protected by a player edit.

```lua
terrain_set_sea_level(64)
terrain_set_biome_height(0, 68, 12, 3)
terrain_set_biome_transitions(true, 24, 60)
terrain_generate_chunk()
terrain_write_generated_block(8, 80, 8, 13)
```

## Voxel Behavior

- Biomes are selected in world-space zones so adjacent chunks line up cleanly across region boundaries.
- Height is driven by smooth noise instead of a flat cutoff, which produces hills and terrain variation within each biome.
- Mountain terrain uses a warped low-frequency range mask, layered ridges,
  valley subtraction, and erosion detail so ranges form foothills and passes
  instead of repeating uniformly across the world.
- Mountain slopes are sampled from neighboring mathematical heights. Steep
  faces expose the biome's deep/rock block, while snow coverage is restricted
  by slope and varied in world space to avoid a perfectly horizontal snowline.
- Biome profiles control baseline elevation, height variation, and topsoil depth, which makes it easy to tune different terrain regions independently.
- Surface blocks now vary by biome, with snow using grass and mountains using stone, and post-passes place shrubs plus reusable tree templates on suitable surfaces.
- The block registry records collision, transparency, replaceability, face occlusion, and hardness so terrain code can make local decisions without hardcoding block ids everywhere.
- Unknown block ids are rejected at chunk write and deserialization boundaries so corrupted palette data does not silently degrade into air-like behavior.
- Small oak trees are used for plains and hills, small pine trees are used for snow and mountains, and small cactus templates are used for desert regions.
- Oak and pine now have both small and large reusable presets, while cactus remains small only.
- Tree species and variants are selected from the world seed plus the tree's world position, so the same seed reproduces the same trees in the same places.
- Tree placement is preflight-checked against the target footprint, so generation skips blocked locations instead of overwriting existing blocks.
- Replaceable blocks do not block tree placement, which lets shrubs and similar decorative terrain be overwritten cleanly.
- Mesh generation now uses a block's face-occlusion flag instead of raw transparency, so transparent foliage can still suppress hidden internal faces.
- Chunk rendering can now frustum-cull whole chunk bounds before drawing, which is the next layer above greedy meshing.
- The mesh container now caches occupied bounds separately from the full chunk bounds, which lets render code cull sparse chunks more tightly when it has the generated mesh available.
- Generated chunks persist metadata and are regenerated only when their seed,
  origin, configuration signature, or generator version changes. Direct block
  edits invalidate that cache.
- The generator version is bumped whenever deterministic terrain synthesis
  changes, so saved chunks produced by an older height/transition algorithm
  are regenerated instead of being reused as if they were current.
- The Minecraft world/save layer owns the terrain policy values. It should initialize
  one `terrain_generation_context` after loading the world and pass that
  context to chunk generation; Libft owns the encoding and persistence logic.
- The saved terrain policy and immutable generation context are the reusable
  inputs for later chunk regeneration; biome profiles, transition settings,
  noise scales, ridge/erosion settings, and layer policies do not need to be
  reconstructed from generated blocks.
- Libft owns the versioned binary encoding and file I/O for the saved terrain
  policy. The Minecraft layer chooses the world-save path and supplies the
  loaded policy, but does not define the on-disk format.
- The compatibility config API takes an immutable local snapshot per call, so
  later caller-side edits cannot alter an in-progress generation. The context
  API avoids repeating validation for every new chunk.
- Config-registered tree templates are copied into config-owned storage, so
  callers may release their source block arrays after registration. Template
  signatures are content-based rather than pointer-address-based.
- Snow caps use a configurable height snowline and each biome's
  `allow_snow_caps` flag; no biome enum is hardcoded as snow-cap eligible.
- Mountain ridges use each biome's `allow_mountain_ridges` flag instead of a
  hardcoded surface-height/biome threshold.
- Terrain generation now supports configurable ore, underground-structure,
  fluid, mountain-ridge, erosion, biome-transition, and terrain-layer policies.
