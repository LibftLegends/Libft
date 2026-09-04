# World Generation and Water Audit

Date: 2026-09-04
Scope: current uncommitted world-generation changes in `Libft/Modules/Voxel/voxel_generation.cpp` and related terrain configuration/persistence code.
Audience: Luna implementation model.

## Executive verdict

The current implementation is not sufficient for the requested behavior. It is deterministic, and moving shrubs/trees after fluid placement is directionally correct, but surface water is still a per-column fill decision layered over an already-final heightfield. Rivers do not carve channels, lakes do not create or validate basins, and every accepted surface-water column fills to the global sea level. This explains abrupt cutoffs and implausible desert water.

Underground lakes have additional correctness and configuration defects. In particular, `enable_lakes == false` does not disable underground lakes, and the enclosure checks prove only the center floor/roof rather than the full 3x3 lake footprint.

Do not patch the current predicates by adding more neighboring-height thresholds. The correct fix is to introduce a deterministic hydrology plan that terrain shaping and later fluid placement both consume.

## Current pipeline

1. Column biome and smoothed height are prepared; a boolean `has_surface_water` is selected (`voxel_generation.cpp:178-188`).
2. Base columns and caves are generated from the unchanged heightfield (`voxel_generation.cpp:1452-1527`).
3. Every low column receives beach/underwater materials whether or not water was selected (`voxel_generation.cpp:1528-1554`).
4. Selected surface columns are filled from terrain height through global sea level (`voxel_generation.cpp:1555-1571`).
5. Snow-cap decoration is applied inline (`voxel_generation.cpp:1573-1594`).
6. Underground lakes are created/filled (`voxel_generation.cpp:1599-1677`).
7. Shrubs, trees, then configured structures are placed (`voxel_generation.cpp:1678-1797`).
8. Ores are generated last (`voxel_generation.cpp:1800-1810`).

The stage API accepts arbitrary masks and records them independently (`voxel_generation.cpp:1413-1453`, `1812-1814`); it does not enforce the dependencies implied by this sequence.

## Findings

### Critical: surface water does not shape terrain

`terrain_should_fill_water` returns one boolean based on legacy random water, an absolute-value noise band called a river, or a lake noise/chance test (`voxel_generation.cpp:1019-1055`). The final terrain height is computed independently before that result is used (`voxel_generation.cpp:178-188`, `1468-1474`). Water is allowed only where terrain already happens to be below sea level and is then filled to sea level (`voxel_generation.cpp:1555-1571`).

Consequences:

- Rivers cannot cut a channel through ordinary terrain or maintain a coherent route.
- Lakes cannot create a bowl, rim, shore, depth profile, or local water level.
- River, lake, and legacy random water lose their identity after selection, so feature-specific shaping is impossible.
- Narrow noise crossings and independent random decisions produce abrupt one-column gaps and clipped-looking edges.
- A low desert column can become a deep vertical water column up to sea level without a plausible basin.

Required change: replace `has_surface_water` with a per-column hydrology record containing at least feature kind, bed height, water-surface height, depth, bank/rim state, and stable feature identity. Compute the plan before final terrain heights are committed. Terrain generation must lower/shape the bed and banks from this plan; the fluids stage must only fill the cavities established by that same plan.

### Critical: underground lakes ignore `enable_lakes`

The underground pass runs whenever the FLUIDS stage is requested (`voxel_generation.cpp:1601`) and uses only `lake_chance_percent` to select candidates (`voxel_generation.cpp:1620-1629`). It never checks `config.fluids.enable_lakes`.

Required change: add an explicit underground-lake enable flag and use it as a hard gate. At minimum, gate the current pass on `enable_lakes`; preferably separate surface-lake and underground-lake controls because they are distinct features. Add a regression test proving zero underground water when disabled, even if chance remains nonzero.

### High: surface support is not enclosure validation

`terrain_is_supported_surface_water` checks four cardinal neighbor heights and accepts three neighbors that are either above sea level or within two blocks of the current terrain (`voxel_generation.cpp:1058-1076`). This does not prove that a pond is enclosed, that a river is connected, or that the proposed water surface cannot spill into an unfilled neighbor. Diagonals and the full shoreline are ignored.

Required change: remove this function from hydrology decisions. Validate an entire planned feature on a deterministic planning tile with a halo. For a lake, select the water level only after proving a closed rim or explicitly shaping one. For a river, generate a connected channel and banks, then verify continuity into neighboring planning tiles.

### High: all surface features use global sea level

Surface filling always ends at `config.sea_level` (`voxel_generation.cpp:1558-1565`). Inland ponds and rivers therefore cannot have local elevations. A low column may receive an implausibly tall stack of water.

Required change: carry `water_surface_y` in the hydrology plan. Reserve global sea level for ocean/coastal water. Derive inland lake levels from basin spill height and cap depth; derive river levels from the channel profile. Voxel rivers may descend in deterministic one-block steps, but must never create unsupported vertical faces or disconnected pools.

### High: underground enclosure checks are incomplete

For existing cavities, the code validates perimeter walls only at water height, accepts any non-solid interior block, and checks only the center floor and center roof (`voxel_generation.cpp:1079-1126`). It does not verify the full 3x3 floor, full roof, or upper head-space perimeter.

For newly carved cavities, perimeter walls are checked at two levels, but again only the center floor and center roof are checked (`voxel_generation.cpp:1129-1183`). The code then fills all nine lower cells and clears all nine upper cells (`voxel_generation.cpp:1634-1667`). Outer water cells may consequently sit over air, and outer head-space cells may open through the roof.

Required change: validate every cell in these sets before writing anything:

- solid floor below every water cell;
- replaceable/air cavity cells only (do not treat arbitrary non-solid blocks as air);
- solid perimeter at every occupied water/head-space level;
- solid roof above every head-space cell;
- no connection to an unplanned cave opening through sides, floor, or roof.

Perform validation first and commit atomically only after all invariants pass.

### High: stage masks permit invalid ordering and overwrites

Arbitrary stage masks are accepted (`voxel_generation.cpp:1413-1423`). A DECORATION-only call can run before FLUIDS, after which surface fluid writes unconditionally replace blocks above terrain (`voxel_generation.cpp:1555-1567`). Re-running BASE clears the chunk and resets stage metadata (`voxel_generation.cpp:1445-1450`). The API records requested stages as completed without enforcing prerequisites (`voxel_generation.cpp:1812-1814`).

Required change: define and enforce a stage dependency graph. Recommended order:

1. hydrology planning;
2. base terrain shaped by hydrology;
3. caves and underground structures;
4. ores/geology;
5. surface and underground fluids;
6. surface layers/bank materials that depend on final wetness;
7. structures;
8. vegetation and other decoration last.

Either automatically include missing prerequisites or reject an invalid mask. Fluid writes must require AIR or an explicitly fluid-replaceable block and must not overwrite structures/decorations silently.

### High: beach materials are applied to dry lowlands

Beach and underwater blocks are selected solely from `column_height < sea_level` (`voxel_generation.cpp:1528-1554`), before consulting whether the column belongs to a water feature. A low column rejected by the water predicates can still become sand/underwater substrate.

Required change: choose shore and bed materials from the hydrology feature record. Distinguish ocean beach, river bank, lake shore, wet bed, and ordinary dry lowland. Do not infer wetness from elevation alone.

### Medium: underground-lake tuning is coupled to surface lakes

`terrain_fluid_config` exposes only river enable/scale/width and lake enable/scale/chance (`terrain_config.hpp:198-229`). The same `lake_chance_percent` controls surface lake columns and underground 3x3 candidates (`voxel_generation.cpp:1041-1053`, `1623-1629`). Defaults enable both with one 4% chance (`voxel_data.cpp:2427-2429`).

Required change: add independent underground-lake settings: enable, region frequency/chance, radius range, depth range, minimum/maximum Y, minimum floor thickness, minimum roof thickness, and cave-intersection policy. Surface lakes need independent frequency, radius/depth, and basin/rim controls.

### Medium: underground generation is chunk-grid-bound and order-dependent

Candidates begin two blocks from chunk edges and use local modulo spacing (`voxel_generation.cpp:1605-1622`), so lakes cannot cross chunk boundaries and placement visibly inherits chunk coordinates. A successful write changes whether later nearby candidates pass, making output dependent on traversal order (`voxel_generation.cpp:1617-1671`).

Required change: select underground lake centers in world coordinates using deterministic region cells, then rasterize each feature into every intersecting chunk. Candidate validity must be computed from immutable planned terrain/cave data, not blocks modified earlier in the same traversal.

### Medium: avoidable generation cost

Column preparation computes the smoothed height once (`voxel_generation.cpp:178-180`), then surface support recomputes it for four neighbors (`voxel_generation.cpp:1066-1073`), and the main generation loop computes the current height again (`voxel_generation.cpp:1468-1470`). Underground code performs the expensive enclosure/create scan before cheap cadence and chance predicates (`voxel_generation.cpp:1617-1632`).

Required change: build one height/hydrology halo cache and reuse it. For underground lakes, test enablement, world-coordinate candidate cadence, and seeded chance before block-volume validation. Instrument generation time per stage to catch regressions.

### Medium: aquatic decoration was removed without replacement

Lily-pad and seagrass block IDs still exist (`terrain_types.hpp:140-141`), but there is no placement path in current generation code. Moving normal vegetation after water is correct, but aquatic decoration needs to be restored as a wet-aware final decoration step.

Required change: place normal shrubs/trees only on explicit dry-land plan cells with valid soil and air clearance. Place aquatic plants only after fluids, using feature kind, water depth, light/open-sky rules, and valid substrate.

### Medium: coverage does not test hydrology behavior

Existing searched tests cover fluid configuration persistence and disable fluids in terrain-transition tests, but do not assert river connectivity, lake enclosure, disable semantics for underground lakes, cross-chunk seams, stage-order independence, or biome-specific water behavior. Generator version was bumped to 6 (`terrain_types.hpp:84`), which is appropriate for changed output but does not substitute for behavior tests.

Required change: add the acceptance suite below before considering the redesign complete.

## Target design for Luna

### 1. Plan hydrology in world space

Generate immutable hydrology in deterministic world-coordinate planning tiles larger than a chunk (for example 64x64 or 128x128), with enough halo for the maximum lake radius, river bank width, and terrain smoothing kernel. Chunk generation must produce identical overlap data regardless of generation order.

Each planned column should carry:

```text
kind: none | ocean | river | surface_lake
feature_id
base_height
final_ground_height / bed_height
water_surface_y
water_depth
bank_or_rim_distance
wetness / substrate class
```

Rivers should come from a connected deterministic path or flow field, not independent column chance. Their distance field should shape channel width, depth, banks, and meanders. Surface lakes should be seeded as complete features with radius/shape, then assigned a valid local water level and a closed rim; reject or reshape invalid basins as a whole.

### 2. Shape terrain from the plan

Apply river channel and lake bowl profiles while calculating the final heightfield. Blend banks/rims over configurable distances. Clamp depth and gradients. This makes terrain and water one coherent feature while still allowing blocks to be emitted in separate stages.

### 3. Generate underground lakes as complete 3D features

Select complete lake volumes in world-coordinate region cells. Validate the complete shell against planned caves and terrain before any writes. Either require fully sealed lakes or deliberately connect them to caves under an explicit policy. Rasterize a feature into all intersecting chunks so chunk boundaries do not alter its shape.

### 4. Commit features in a strict order

Use the dependency order listed in the stage-mask finding. Decoration must consume final wet/dry state. Keep an immutable plan through all stages instead of re-deriving feature intent from final block IDs.

### 5. Extend configuration and persistence completely

When adding fields, update all of the following together:

- `terrain_fluid_config` declaration, lifecycle/copy code, setters, defaults, and validation;
- binary save and load (`voxel_save.cpp:627-642`, `1040-1055`, `1119-1127`) with a versioned migration path;
- JSON output (`voxel_json.cpp:489-505`) and JSON input if configuration loading is intended;
- generation configuration signature (`voxel_data.cpp:3168-3177`);
- scripting/bindings and public documentation;
- equality/copy/persistence tests;
- generator version, because terrain output changes again.

Do not reuse `water_chance_percent` as a generic hydrology switch. Deprecate it or define it narrowly as legacy scattered water, disabled by default.

## Acceptance tests

The implementation is complete only when automated tests cover these cases:

1. Generate the same multi-chunk region in forward, reverse, random, and parallel chunk order; hashes must match.
2. Every river entering a chunk edge has a matching channel/water profile in the neighboring chunk.
3. Every surface lake has a coherent feature ID, bounded depth, local water level, and closed shoreline; no isolated one-column water unless explicitly configured.
4. No surface water cell has AIR directly below it. Horizontal AIR beside water is allowed only at a validated exposed bank/waterfall rule.
5. Every sealed underground lake has a solid floor, side shell, and roof across its complete footprint; no water cell touches unplanned AIR.
6. Disabling surface lakes, rivers, or underground lakes independently yields none of that feature across a large deterministic sample.
7. DECORATION cannot precede required terrain/fluid stages; invalid masks are rejected or prerequisites are applied automatically.
8. Trees and ordinary shrubs never occupy water and are not rooted in a water bed. Aquatic plants appear only under their explicit rules.
9. Dry lowlands do not receive beach/underwater materials merely because they are below sea level.
10. Plains, hills, desert, snow, and mountains are sampled over many seeds. Hydrology obeys config consistently; desert water has a shaped channel/basin rather than clipped fills.
11. Binary round-trip, JSON output/input, configuration signature, validation, and old-save migration include every new setting.
12. Benchmarks report height planning, terrain emission, fluids, and decoration separately and remain within an agreed chunk-generation budget.

## Suggested implementation sequence

1. Add failing tests for disable semantics, underground enclosure, stage dependencies, and dry-lowland substrate.
2. Define hydrology plan structures and versioned configuration fields.
3. Implement deterministic tile/halo planning and seam/order tests.
4. Make final terrain height consume river/lake shaping data.
5. Fill surface fluids from planned local levels and materials from wetness/feature kind.
6. Replace underground candidate scanning with complete world-space lake features and shell validation.
7. Enforce stage dependencies and make writes replacement-safe.
8. Restore wet-aware aquatic decoration; keep shrubs and trees final and dry-only.
9. Complete persistence/signature/bindings/docs migration, bump generator version, and run the full test/benchmark suite.

## Definition of done

The visual symptom is not the definition of done. Completion requires coherent planned water features, terrain shaped for those features, deterministic cross-chunk output independent of generation order, full underground enclosure guarantees, independent configuration controls that actually disable their features, safe stage ordering, and the acceptance tests above.

---

# Implementation Contract Addendum: Lightweight Voxel Lighting, Blob Shadows, and Look-Down Interaction

This addendum is the next implementation workstream for the same Libft/Minecraft handoff. It targets the visual character of older Java Minecraft with discrete voxel lighting, existing directional face shading, optional cheap voxel corner AO, and small entity blob shadows. It must not become a conventional dynamic shadow-mapping system.

## Implementation status (2026-09-04)

The initial implementation is now present on the target branches. Libft provides packed sky/block light, uniform-or-materialized section storage, metadata-driven emission/attenuation, deterministic bounded propagation, and light-aware greedy mesh keys. Minecraft builds lighting in the generation/remesh worker pipeline from immutable snapshots, publishes only revision-matched results, uploads the packed light byte to OpenGL, and prepares the equivalent CPU triangle shade before rasterization. Both render paths include a cheap player receiver shadow; the GPU version is one blended quad and the software version is a depth-tested projected ellipse.

The snapshot lighting halo is 15 blocks and uses a captured 3x3 chunk neighborhood when available; unavailable cells conservatively use solid stone and are corrected by the existing neighbor remesh path. A `make validate-all` target now runs the existing validators plus the maximum-downward-pitch/normal-raycast underfoot break-and-place validator. Libft’s focused lighting tests cover packing, uniform storage, direct sky/block falloff, and greedy-gradient preservation.

At a chunk edge, mesh face sampling falls back to the nearest in-chunk boundary light rather than zero, so a correctly propagated boundary does not become an artificial dark strip while the neighboring mesh is unavailable.

The synchronous startup/fallback loader is also covered: it now initializes lighting before creating its mesh. A local core-only solver is used when no neighbors are available, while the full 15-block halo solver is retained for neighbor-aware remeshes. This both prevents black startup geometry and avoids paying the halo scan for isolated first-generation chunks.

The loader lighting callback now reads the center chunk while it is still being built (before publication), reads the complete available cardinal neighbor chunks across the halo, and treats unavailable outer-halo cells as opaque stone. This prevents the former failure where generated chunks were interpreted as air and underground/cave cells incorrectly retained full skylight. A regression test covers a roofed horizontal cave entered from a side opening and verifies stepped skylight falloff.

Lighting is not performed in the render loop: the Libft builder is synchronous on its caller’s thread, and Minecraft calls it from worker threads. The main thread only captures immutable state and publishes accepted results. No shadow-map, light-volume, or per-frame flood-fill pass was added.

Block edits now mutate authoritative voxel storage and mark the edited chunk plus its cardinal neighbors dirty; they no longer synchronously rebuild meshes or lighting. The frame scheduler gives explicitly edited chunks nearest the player priority, while ordinary post-stream dirty chunks remain behind generation work so streaming cannot starve. The edit-stress benchmark verified the change: edit calls fell from frame-scale work (roughly 62--70 ms in the baseline) to approximately 4 microseconds in the measured run, with worker costs and stale results reported separately.

Libft also exposes `voxel_light_build_stats` (`scanned_cells`, `propagated_cells`, and `queue_peak`) and the pipeline carries those counters with asynchronous results, so light-build cost can be measured independently from mesh cost. Minecraft additionally records main-thread snapshot-capture time, which is intentionally separate from worker light/mesh time. `voxel_shadow_find_receiver` and `voxel_shadow_height_fade` are reusable Libft helpers used by both Minecraft render paths; the current consumer has a player entity only, so mob submission remains an integration extension when entity rendering exists.

## Ownership

Reusable functionality belongs in Libft: packed sky/block light, metadata, light storage, deterministic propagation, light queries, light-aware meshing, packed mesh lighting, and generic downward receiver helpers. Minecraft owns chunk relight scheduling, immutable World snapshots, revision-checked publication, renderer consumption, entity shadow submission, settings, camera limits, interaction, and validators. Workers must never inspect mutable World state.

## Baseline lighting model

Each voxel has independent `sky_light` and `block_light` channels in the range `0..15`. Pack them into one byte (`bits 0..3` sky, `bits 4..7` block) with centralized pack/unpack/combined helpers. Visible light is `max(max(0, sky_light - sky_darkening), block_light)`; daytime starts with `sky_darkening = 0`, allowing future day/night changes without relighting or remeshing.

Unobstructed vertical columns receive direct skylight `15` down to the first blocker. Sideways, around-obstruction, and emitted light use six-neighbor propagation and lose at least one level per step. Sources combine by maximum contribution, never addition. Block metadata must expose validated `emitted_light_level` and `light_attenuation` (`0..15`), retaining `light_emitting` compatibility. Attenuation `15` blocks propagation; positive attenuation ends the special direct-sky path and ordinary propagation applies.

Lighting is derived state, not persisted authoritative voxel data or network payload. The deterministic Libft builder consumes an immutable block/material snapshot and a bounded target halo; level `0..15` means no global flood fill is needed. Missing neighbors use a conservative non-sky/lateral boundary and trigger bounded relight plus remesh when published. Use reusable bounded queues and integer levels; final results must be independent of generation order.

Use uniform/materialized light sections like voxel sections: one byte for uniform sections and one byte per voxel only when needed. A fully materialized `16 x 16 x 256` chunk is approximately `64 KiB`. Do not duplicate sky and block arrays without measured justification.

## Meshing and renderers

Add packed light to `chunk_mesh_vertex`, sample the cell immediately outside each visible face, and include lighting in the greedy-merge signature so equal materials with different light do not flatten a gradient. Preserve existing directional face shading. Verify/reorder fields so `sizeof(chunk_mesh_vertex)` remains unchanged where reasonably possible, with Linux/Windows/macOS layout tests.

Use a centralized monotonic 16-entry nonlinear brightness curve. The final baseline is texture color multiplied by face shade and the light brightness. GPU meshes upload the packed byte as an integer attribute; the shader unpacks both nibbles, applies the sky-darkening uniform, takes the maximum, and uses the brightness curve. Do not add shadow framebuffers, light volumes, or world lookups in the fragment shader. In the software renderer, prepare flat triangle shade before rasterization; no world-light query or extra light interpolation belongs in the per-pixel loop.

Optional AO is Phase 2 only. If measured worthwhile, quantize local corner occupancy to four levels while meshing; do not implement SSAO or another screen-space pass.

## Minecraft asynchronous integration

The worker pipeline is `immutable voxel snapshot -> bounded-halo light build -> light-aware mesh -> result -> revision-checked publication`. Results must carry chunk identity, voxel revision, world epoch, relevance epoch, and any other required identity; stale results are discarded. Chunk-neighbor arrival and lighting-relevant edits (including roofs, walls, emitters, water, and removals) mark bounded regions dirty. Coalesce overlapping edits and avoid synchronous render-thread relights.

Record sampled analytics for light snapshot bytes, build/mesh time, propagated cells, queue peak, dirty-region coalescing, and stale-result discards. No per-node logging. Static frames must perform no light flood fill, relight, or camera-driven mesh rebuild.

Synchronous chunk insertion no longer relights its four loaded neighbors inline. It marks them dirty and submits at most the currently available bounded worker work; `FT_ERR_FULL` is retained as a retryable back-pressure result, while other queue errors are propagated. This keeps relight work distributed across update frames. The performance session prints a `lighting:` record with light/mesh microseconds, scanned and propagated cells, queue peak, snapshot bytes, and stale-result count.

World startup also initializes only the center chunk synchronously. Neighboring generation, lighting, mesh publication, and neighbor relights follow the normal one-chunk-per-ten-frames acceptance policy, preventing a launch-time burst of dozens of full lighting builds.

## Entity blob/contact shadows

For active nearby entities, query downward a small fixed distance (for example eight blocks) for the nearest valid solid receiver. Submit one quad/two triangles, slightly above the receiver, with a radial alpha mask. Scale radius from the entity footprint with safe clamps. Fade opacity with entity height and remove the shadow when no receiver exists or the maximum height is exceeded. Cache receiver data while the entity cell and receiver revision remain valid, and enforce a fixed distance/count budget if populations grow. Implement the generic query in Libft only when it remains Minecraft-independent; entity policy and submission remain in Minecraft.

## Camera and interaction

Minecraft should raise the pitch limit from the current restrictive value near `1.2` radians to just below vertical, initially `1.5533430342749532` radians (about 89 degrees). Keep clamping to avoid crossing 90 degrees. Do not add underfoot interaction special cases: the ordinary raycast must target, break, and place beneath the player when normal collision and placement rules permit it. Add a deterministic validator covering maximum downward pitch, underfoot selection, ordinary break, receiving-face selection, and ordinary placement.

## Required tests and completion checks

Add Libft tests for all 256 packed-light combinations, darkening/combined maximum, metadata validation, direct skylight, roof/side opening, source falloff, occlusion, maximum-combination sources, runtime attenuation, chunk seams, deterministic generation order, edit invalidation, all visible side-face light attributes, and greedy lighting boundaries. Add CPU/GPU brightness-order consistency checks. Add Minecraft validators for stale results, neighbor relight seams, blob receiver/height fade/culling, camera look-down, underfoot break, and placement.

The implementation is complete only when open columns are level 15, caves/roofs darken, emissive gradients cross chunk seams, lighting remains off the frame hot path, CPU and GPU consume the same Libft data, edits are bounded/coalesced and revision-safe, blob shadows remain two-triangle cheap, underfoot interaction uses the normal raycast, and Libft plus `make validate-all` pass. Explicit non-goals are shadow maps, ray tracing, SSAO, RGB light, physically based GI, persistent/networked light fields, and an unbounded global light graph.

## Edit-stress performance validation

Static-world measurements are not sufficient for this feature. A representative performance run must also include repeated real block edits, because breaking or placing a block invalidates lighting and mesh data and can expose work that is accidentally being performed synchronously on the frame thread.

Minecraft now provides `--perf-edit-stress`. In perf mode it alternates deleting and restoring one generated surface block every frame through the normal `World` edit APIs. It reports successful edits, edit failures, average edit time, and maximum edit time alongside the existing light-build, mesh-build, snapshot, propagation, and stale-result diagnostics. Use it with `--perf-test-headless --perf-seconds=30`; add `--perf-move` to exercise the player blob-shadow path while edits are continuously rebuilding nearby geometry.

Compare at least these matched runs:

```text
--perf-test-headless --perf-seconds=30
--perf-test-headless --perf-edit-stress --perf-seconds=30
--perf-test-headless --perf-edit-stress --perf-move --perf-seconds=30
```

The edit-stress result is a failure if edit time creates frame-scale spikes, if failures accumulate, if stale-result discards grow without converging, or if the edit path performs unbounded synchronous relighting/remeshing. Record frame mean/p95/p99/max, edit average/max, light and mesh timings, queue depth/peak, snapshot capture time, propagated cells, and stale-result count. A future fix should coalesce edits and defer bounded relight/remesh work across frames; the benchmark must remain in place to verify that fix does not regress while preserving shadow correctness.

## Runtime lighting scheduler configuration

The incremental lighting scheduler must not compile these limits into the engine. They are tuning parameters and must be owned by a runtime configuration object that can be changed before or during a world session (with thread-safe publication if the scheduler is running):

```text
min_nodes_per_frame       default 32
target_nodes_per_frame    default 128
max_nodes_per_frame       default 512
time_budget_microseconds  default: a small configurable fraction of the frame budget
```

Configuration validation must reject zero/negative values, `min > target`, `target > max`, and unreasonable upper bounds that could turn a malformed setting into a frame stall. The scheduler reads a stable configuration snapshot at the beginning of each frame. It processes at least the configured minimum while work exists, then stops at the configured maximum or time budget, whichever comes first; the minimum is allowed to overrun the time budget so pending work cannot starve. Benchmark runs should print the active values with the lighting diagnostics so results are reproducible.

Libft now provides the validated `voxel_light_update_config` value type and default/validation helpers. The Minecraft integration must use this type when the incremental node scheduler is added; until that scheduler is integrated, the existing asynchronous full-build path remains governed by its worker/remesh back-pressure limits.

---

# Proactive Libft audit: CSV parser

This audit records the CSV findings supplied during the lighting/world-generation implementation work. There are no newer commits after `010ba901c` / PR #921 in the reviewed line, so the CSV module was audited as an older Libft module. It was originally added in commit `1fc303ea` on May 28, 2026, and its public header has not changed since June 25.

## Findings and priorities

### High: trailing empty fields are dropped

The parser finalizes a field at a delimiter and resets `field_started` to false. At end of input it only finalizes another field when `field_started == true` or `current_field.size() > 0`. Consequently, valid trailing empty fields disappear:

```text
a,b,  ->  ["a", "b"]     (incorrect; expected ["a", "b", ""])
,    ->  [""]             (incorrect; expected ["", ""])
```

Fix this by modeling the parser as always owning one current field, or by tracking that the last consumed character was a delimiter. EOF after a delimiter must finalize an empty field. The first regression cases are:

```text
"a,b,"  ->  3 fields
","      ->  2 empty fields
"a,,"    ->  3 fields
"\"a\"," ->  2 fields
",\n"     ->  2 fields
```

This is a data-correctness issue: silently removing the final optional column can shift table semantics.

### Medium: delimiter values are not validated

The public API accepts an arbitrary delimiter character. Reject at least NUL, quote, LF, and CR up front unless a deliberately different grammar is documented. A quote delimiter is structurally ambiguous because it is also the quoting marker; NUL and record terminators collide with parser boundaries.

### Medium: row metadata update is not transactional

`csv_finalize_row()` appends the row offset and then the row length. If the second append fails, the offset vector has changed while the lengths vector has not. The normal initialization path destroys the document after an error, but the helper still temporarily violates its internal invariant.

Reserve both vectors before mutation, or roll back the first append when the second append fails. Add allocation-failure coverage around both growth points.

### Medium/performance: per-character field appends

Quoted and unquoted fields currently append one character at a time. This is functionally acceptable when `ft_string` growth is amortized, but large fields pay repeated calls and bounds checks. Defer this optimization until correctness is fixed; a later implementation should scan to the next structural character and append contiguous spans where escaping rules permit.

### Medium: malformed and boundary grammar coverage

The parser manually implements quoting, so tests should cover trailing empty fields, consecutive delimiters, empty quoted fields, embedded commas, escaped quotes (`""""`), embedded CR/LF inside quoted fields, CRLF records, unterminated quotes, characters after a closing quote, huge fields, allocation failure at every field/row-vector growth point, and invalid delimiters.

## Recommended implementation sequence

1. Add failing tests for trailing delimiters and consecutive empty fields.
2. Fix EOF field finalization while preserving quoted-field and newline behavior.
3. Add delimiter validation and document the accepted delimiter grammar.
4. Make row offset/length updates transactional under allocation failure.
5. Add malformed-input and allocation-failure tests.
6. Profile large fields before considering contiguous-range append optimization.

The existing flat field storage plus row offsets/lengths is a sound lookup design, and serialization already has explicit quote/escape handling. The priority is therefore a small correctness patch followed by focused boundary tests, without changing the public storage model.

Current implementation status: trailing empty fields are preserved by both document parsing and `csv_split_line`; NUL, quote, LF and CR delimiters are rejected; and row metadata rolls back an offset append if the matching length append fails. Focused regression coverage for these fixes passes.
