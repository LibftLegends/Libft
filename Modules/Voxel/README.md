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
- `TERRAIN_BIOME_ZONE_WIDTH` - Width in world blocks used to group biome zones.
- `terrain_biome` - Biome selector enum for plains, hills, desert, snow, and mountains.
- `terrain_biome_profile` - Surface height, variation, and topsoil depth profile for a biome.
- `terrain_block_metadata` - Registry entry that describes whether a block is solid, transparent, liquid, replaceable, light-emitting, whether it occludes mesh faces, and how hard it is.
- `terrain_tree_template_block` - Relative block entry used by tree templates.
- `terrain_tree_template` - Block list wrapper for reusable tree presets.
- `terrain_generation_config` - Lifecycle-managed runtime generation policy
  containing noise, water, biome, and feature-rule settings.
- `terrain_biome_definition` - Lifecycle-managed customizable profile, block
  palette, template policy, and decoration/ridge/snow-cap policy for one biome
  slot. Use its `set_*` methods after `initialize()`.
- `terrain_feature_rule` - Lifecycle-managed seeded placement rule for caller-provided tree or
  object templates, with biome, height, water, and chance constraints.
- `terrain_ore_rule` - Lifecycle-managed optional deterministic ore policy with block, depth,
  vein, chance, and enabled settings. Coal, iron, and gold are disabled by
  default.
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
  terrain-policy save payload using Libft buffers, including registered tree
  templates, biome template assignments, and feature rules.
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

## Voxel Behavior

- Biomes are selected in world-space zones so adjacent chunks line up cleanly across region boundaries.
- Height is driven by smooth noise instead of a flat cutoff, which produces hills and terrain variation within each biome.
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
