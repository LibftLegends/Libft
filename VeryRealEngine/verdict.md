# Verdict: "Very Real Engine" (v1.4) — full requirement check against `VeryRealEngine/` as it stands

**Updated 2026-09-10, after roadmap steps 1–5 (Vulkan renderer, OBJ/materials, scene graph, physics, lighting/shadows).**

**Short answer: real, working progress on the graphics/physics/scene core (steps 1–5 are genuinely implemented and verified, not just planned) — several requirements the subject treats as mandatory are still completely unstarted, but the two structural/process issues flagged in earlier passes (no library target, nothing committed to git) are both now fixed.**

*(The git-tracking gap flagged in an earlier pass of this check has since been fixed: `VeryRealEngine/` is now committed — commit `ff67f34d "very real engine"` on branch `very-real-engine-checkout`, already pushed to `origin`. Verified via `git status`/`git log` below.)*

> **Project directive (overrides the subject's literal wording where they diverge):** visual parity with Figures V.1–V.4 has been set as a hard target for this project, regardless of what the subject text alone would technically require. The subject's mandatory checklist only asks for "realistic" shadows and a material *system*, which the engine already satisfies in a literal sense — but that is **not** the bar this project is being held to. Addendum 2 below, previously framed as "nice to have, not required," is now read as a first-class requirement and has been folded into the priority order accordingly.

This replaces the earlier verdict (which audited the unrelated `FullLibft` repo, before `VeryRealEngine` existed). That original audit and the dependency-reuse analysis are kept below for historical context, since they explain *why* `VeryRealEngine` was built as an independent project.

## What was checked for this pass

- Every file in `VeryRealEngine/` (`src/`, `shaders/`, `assets/`, `CMakeLists.txt`, `README.md`)
- `git status`/`git ls-files` for `VeryRealEngine/` against the enclosing `Libft` repository
- Grep for Doxygen tags (`@brief`/`@param`/`@return`) and culling code (`frustum`/`occlusion`)
- The CMake target types actually produced (`add_executable` vs `add_library`)
- Cross-reference against every checklist item in Chapters III–VIII of the subject text

## Full requirement-by-requirement check

### Chapter III — General Instructions

| Requirement | Status | Evidence |
|---|---|---|
| "The final product must be a library named `VeryRealEngine`." | ✅ **Met (fixed since the previous pass)** | `CMakeLists.txt` now has `add_library(VeryRealEngine STATIC ...)` containing every engine subsystem (renderer, physics, scene, asset loaders, windowing) — everything under `src/` except `main.cpp`. A separate `very_real_engine_demo` executable links against it (`target_link_libraries(very_real_engine_demo PRIVATE VeryRealEngine)`). Verified: `ar t build/libVeryRealEngine.a` lists all 7 engine object files, and the demo still runs correctly against the library build (same visual output — spinning rig, physics-driven falling cube, shadows — as before the restructure). |
| Build system compiles + re-links only when necessary | ✅ Met | CMake/Ninja-style incremental builds work natively; verified touching a single `.cpp` only rebuilds that object and re-links (see step-1/2/3/4/5 build logs — each incremental change rebuilt only the changed translation units). |
| No unexpected termination (segfault/bus error/double free) | ⚠️ **Unverified, and a real risk as written** | Never run under a sanitizer (ASan/UBSan) or fuzzed. Separately, by design, `VK_CHECK` and several internal checks (`Renderer::pick_physical_device`, `load_shader_module`, `load_mesh_from_obj`, unsupported image-layout transitions) call `std::abort()` on the first unrecoverable error rather than returning an error to the caller. That's a clean, intentional halt with a diagnostic message — not memory corruption — but it means almost any missing asset or driver hiccup takes the whole process down rather than failing one operation gracefully. Worth hardening before a defense where the evaluator might, say, delete a texture file to see what happens. |
| No memory leaks | ⚠️ **Unverified** | Never run under Valgrind or a leak sanitizer. `vulkan-validationlayers` (which also flags several classes of Vulkan resource leak) isn't installed in this environment either (see step-5 notes) — genuinely unchecked, not just "probably fine." |
| No non-system library except Vulkan | ✅ Met | Only Xlib (a standard X11 client library, same category as the XCB the subject names) and Vulkan are linked. The OBJ/MTL parser, TGA loader, JSON parser, math, and physics are all hand-written specifically to avoid any third-party or `FullLibft` dependency (see the dependency-audit addendum below). |
| Core components (rendering, physics, ECS) implemented by you | ⚠️ **Partially met** | Renderer and physics: yes, from scratch. Entity-component system: **does not exist yet** — `Scene`/`SceneNode` is a plain hierarchical transform graph, not an ECS (no component storage, no systems iterating component arrays). This is explicitly step 6 of the roadmap and hasn't been started. |
| A demonstration game/3D animation showcasing the engine | ⚠️ **Partial** | A real, running technical demo exists (`src/main.cpp`): a physics-driven falling cube passing through a trigger volume and settling under friction/restitution, a spinning parent-child rig, real-time shadows, all loaded from a JSON scene. It is not yet a "game" with player-driven interaction (only one debug key, H, does anything), and it is **not** the specific Interactive House Environment Chapter V separately and explicitly requires — that hasn't been started (see below). |
| Extensive documentation (comments, README, docs) | ⚠️ **Partial** | Every source file has explanatory prose comments (often documenting *why*, not just *what* — e.g. the scope-limitation notes throughout `renderer.cpp`), and `README.md` documents each completed step with what to verify. But there are **zero Doxygen-format comments** (`@brief`/`@param`/`@return`) anywhere, which Chapter VI requires specifically. |

### Chapter IV.1 — Engine Basics

| Requirement | Status | Evidence |
|---|---|---|
| Scene loading via a file (custom or JSON) | ✅ **Met** | `src/assets/json_parser.*` (hand-written JSON parser) + `src/scene/scene.cpp` (`Scene::load`). `assets/scenes/demo_scene.json` is a real, working example. |
| 3D rendering, `.OBJ` loading, textured **and** untextured, multiple `.OBJ`s at once | ✅ **Met, verified** | `src/assets/obj_loader.*` handles both. Live-tested: `cube.obj` (textured, checkerboard) and `plane.obj` (untextured, falls back to the default white texture + tint) loaded and rendered together in the same frame; confirmed by screenshot and by the renderer's mesh cache logging both loads. |
| Physics: collision detection/response, gravity, friction, trigger volumes (detect, don't block) | ✅ **Met, verified** | `src/physics/physics_world.*`. Verified with four standalone tests (no Vulkan needed): resting contact under gravity, restitution bounce height, friction bringing a slide to a stop, and a trigger firing exactly one enter/exit pair while the body passes through unblocked. Also verified live: log shows `Trigger: trigger_zone entered by falling_cube` / `exited by falling_cube`, and the cube visibly lands on the ground afterward. |
| Scene hierarchy: multiple objects, transforms, parent-child; hiding a parent hides its children by editing only the parent | ✅ **Met, verified** | `Scene::collect_render_items` combines visibility down the parent chain. Directly tested: toggling `rig`'s visibility alone took the render-item count from 3 → 1 → 3, with `satellite` (its child) disappearing/reappearing purely as a side effect. |
| Optimization: Frustum Culling + Occlusion Culling; **≥60 FPS in Release** | ❌ **Not met at all** | No culling code exists anywhere — every `RenderItem` the scene produces is drawn unconditionally in both the shadow pass and the main pass, every frame, regardless of whether it's inside the camera frustum or occluded. FPS has never been measured or benchmarked; there's no instrumentation to report frame time at all. For a demo scene of 3–5 objects this doesn't matter yet, but the requirement is explicit and currently entirely unaddressed. |
| Materials/textures system, incl. distinguishable types (e.g. wood-like, metallic) | ✅ **Met (fixed since the previous pass)** | `MaterialData` now carries `roughness`/`metallic` (parsed from `.mtl`'s `Pr`/`Pm`), and `mesh.frag` implements a real Cook-Torrance BRDF (GGX distribution, Smith geometry, Schlick Fresnel) instead of flat Lambertian shading — metallic and wood-like materials now produce genuinely different shading responses, not just different textures. Verified visually: `assets/models/sphere_metal.obj` (`metal.mtl`, roughness 0.25, metallic 1.0) shows a small, sharp, tinted highlight on a mid-gray base; `sphere_wood.obj` (`wood.mtl`, roughness 0.75, metallic 0.0) shows a broad, soft, un-tinted highlight on a matte brown base — side by side in the same scene. One real bug caught and fixed in this pass: without an ambient-specular term, the metallic sphere rendered as solid black outside its direct highlight (metals have zero diffuse response, and the engine has no environment/IBL yet); fixed with a cheap `f0 * ambient` term as a stand-in, and re-verified. Remaining gap: no normal maps yet. |

### Chapter IV.2 — Engine Composition

| Requirement | Status | Evidence |
|---|---|---|
| Lighting: multiple light sources | ✅ **Met, verified** | Up to 4 lights (directional or point), uploaded via a per-frame UBO (`Renderer::GlobalUbo`), each independently colored/positioned/intensity-scaled. Verified visually: a warm directional "sun" and a blue point light both visibly contributing, including the point light's blue tint remaining visible inside the sun's shadow. |
| Shadow rendering, static **and** dynamic, "based on the position and intensity of these lights" (plural) | ✅ **Met (fixed since the previous pass)** | Shadow mapping now covers the first `kMaxShadowCasters` (2) lights in scene order, of either type — `compute_light_space_matrix` handles both directional (orthographic) and point (perspective, aimed at the scene center) casters, each with its own shadow map, re-rendered every frame. Shadows are also now soft (PCF via a hardware depth-compare sampler + a 3x3 manual tap in `mesh.frag`'s `pcf()`), not hard-edged. Verified visually: each object casts two distinct, correctly shaped shadows simultaneously (one per light), confirmed by cropping a rendered frame and seeing the visibly feathered (not aliased) shadow edge; both remain correctly tracked as the falling cube moves and the rig spins. Remaining scope line: a point light's shadow is a single perspective frustum, not a full 6-face cubemap, so it won't shadow geometry outside that cone — documented in `renderer.hpp`/`renderer.cpp`, not silently wrong. |
| JSON (or similar) scene loading: scenes, object properties, light management | ✅ **Met** | `assets/scenes/demo_scene.json` drives objects (mesh, transform, physics, visibility), materials (via referenced `.mtl`), and lights (`"lights"` array) all from one file. Runtime property changes are demonstrated only for visibility (the H-key toggle) — no broader "live-edit any property" API exists, but the subject doesn't require one beyond what's demonstrated. |

### Chapter V — Demonstration (Interactive House Environment)

| Requirement | Status | Evidence |
|---|---|---|
| Rooms with varied lighting conditions | ✅ **Done** | `assets/scenes/house_scene.json`: Room A starts bright/warm-lit, Room B starts dark/cool-lit, verified via screenshot (`README.md`'s Step 6/7 section). |
| Interactable objects (doors, light switches, etc.) | ✅ **Done** | A hinged door (`door_hinge`/`door` parent-child, **E** to toggle, smooth lerped swing) and a light switch prop (**F** to toggle Room B's light), both proximity-gated in `src/main.cpp`. |
| Particle-emitting objects (e.g. steam) | ✅ **Done** | `src/particles/particle_system.{hpp,cpp}` — CPU-side emitter/lifetime/velocity system, xorshift32 PRNG, rendered as shrinking cubes above the coffee-machine prop. |
| Smooth, realistic, intuitive navigation through the house | ✅ **Done** | First-person WASD movement + arrow-key look in `src/main.cpp`, pitch-free movement vectors so looking up/down doesn't affect walking. |

This entire chapter — explicitly called "a final requirement," not optional — is now built. See `README.md`'s "Step 6/7" section for the full writeup, including a genuine rendering bug (inverted wall normals from a winding/culling mismatch) found and fixed while building it.

### Chapter VI — Documentation (Doxygen)

| Requirement | Status | Evidence |
|---|---|---|
| Doxygen comments on all classes, functions, methods, variables | ❌ **Not met** | `grep` for `@brief`/`@param`/`@return` across `src/` returns zero matches. All existing comments are plain `//` prose, which is good practice but isn't what Chapter VI asks for. |
| A `Doxyfile`/generation script in the repo | ❌ **Not present** | No `Doxyfile` exists under `VeryRealEngine/` (note: `Libft`'s root `Docs/Doxyfile` belongs to the unrelated `FullLibft` project and documents that codebase, not this one). |
| Warning-free Doxygen generation | N/A | Can't be assessed until the above two exist. |

### Chapter VII — Bonus Part

| Requirement | Status |
|---|---|
| Post-processing (bloom, motion blur, DOF) | ⚠️ Bloom done (as part of visual-fidelity work, not originally for the bonus). ❌ Motion blur, DOF not started. |
| Sound system | ❌ Not started |
| Particle systems | ❌ Not started (also blocks the mandatory house-demo steam requirement above) |
| Skeletal animation | ❌ Not started |
| Networking (multiplayer) | ❌ Not started |

None of this is required yet, but note that a working particle system would simultaneously satisfy both the mandatory house-demo's steam requirement *and* part of the bonus — worth prioritizing over the other bonus items for that reason alone.

### Chapter VIII — Submission and Peer Evaluation

| Requirement | Status | Evidence |
|---|---|---|
| Work is inside the Git repository that will be evaluated | ✅ **Met (fixed since the previous pass)** | `git log -1 -- VeryRealEngine/verdict.md` and `git ls-files VeryRealEngine \| wc -l` (34 files) confirm the whole directory is committed as of `ff67f34d "very real engine"` on branch `very-real-engine-checkout`, and `git status` shows the branch up to date with `origin/very-real-engine-checkout` — already pushed. (Still worth deciding explicitly whether `VeryRealEngine` should remain a subtree of this `Libft` repository or move to its own separate repository, since the dependency audit below concluded it must not *depend on* `FullLibft` — but living in the same repo history is not itself a violation of anything the subject states.) |
| Compiles and runs from a fresh repository clone | ⚠️ **Plausible but not actually tested this way** | The build has been verified repeatedly from a clean `build/` directory within the existing checkout, and `README.md` documents the exact dependencies (`libvulkan-dev`, `libx11-dev`, a GLSL compiler) and build commands needed. But a true fresh-clone test (a separate `git clone` into an empty directory) has not been performed — and can't be, meaningfully, until the directory is actually committed (see above). |
| Doxygen generation instructions in the repo | ❌ **Not present** | Ties directly to the Chapter VI gap above. |
| Readable, well-commented, peer-reviewable code | ✅ **Reasonably strong** | Consistent modern C++ style throughout, explanatory comments (including deliberate scope-limitation notes so a reader doesn't mistake a simplification for a bug), and clear module boundaries (`platform/`, `renderer/`, `physics/`, `scene/`, `assets/`, `math/`). |

## Priority order for what's actually missing

Re-ordered given the project directive above: visual-fidelity work (formerly Addendum 2, "optional") is now interleaved as first-class priority, since it and Chapter V's house demo are effectively the same body of work — a house demo built with the current flat-shaded, hard-shadowed, untextured-beyond-one-checkerboard renderer cannot look like Figures V.1–V.4 no matter how well-modeled the house itself is.

1. ~~A PBR material/shading model~~ — **done.** Cook-Torrance BRDF (GGX/Smith/Schlick) in `mesh.frag`, `roughness`/`metallic` in `MaterialData` via `.mtl`'s `Pr`/`Pm`, verified with side-by-side metal/wood spheres. Normal maps are the one piece still missing from this item.
2. ~~Soft shadows and shadow casting from more than one light~~ — **done.** PCF via a hardware depth-compare sampler, 2 shadow-casting lights (directional + point) instead of 1, verified visually (feathered edges, two distinct shadow shapes per object). Remaining gap: capped at 2 casters, and a point light's shadow is a single frustum, not a full cubemap.
3. ~~Ambient occlusion~~ — **done.** Screen-space depth-only AO (no normal buffer), verified via a cropped/zoomed screenshot showing correctly-placed contact darkening. Full global illumination is still not attempted — SSAO is a real, working approximation of *part* of what GI provides (contact/crease darkening), not a substitute for bounce lighting.
4. ~~An HDR + tonemap + bloom post-processing pipeline~~ — **done.** ACES filmic tonemap + single-pass wide-kernel bloom in `post.frag`. Verified by temporarily lowering the bloom threshold and confirming a visible glow appears (the demo scene's actual light intensities don't happen to exceed the real threshold prominently, so nothing blooms by default — code path confirmed working, not just inert).
5. ~~Anti-aliasing~~ — **done**, via a single-pass edge-smoothing filter ("FXAA-lite") in `post.frag` rather than MSAA (which would need multisampled color+depth images and a depth-resolve attachment — judged not worth the added Vulkan complexity given this lower-risk alternative achieves the same visible goal). Verified by comparing cropped/zoomed screenshots before and after: visibly smoother sphere silhouette edges.
6. ~~Real hand-authored or sourced 3D assets and textures~~ — **done.** Procedural wall/wood/metal/steam textures and materials (`assets/textures/`, `assets/models/*.mtl`), all box geometry derived from the existing verified `cube.obj`.
7. ~~Chapter V's Interactive House Environment itself~~ — **done.** Two rooms with different lighting (`assets/scenes/house_scene.json`), a hinged door (**E** to open/close), a light switch (**F** to toggle Room B's light), a particle-based steam emitter above a coffee machine, first-person WASD/arrow-key navigation. See `README.md`'s "Step 6/7" section for the full writeup, including a genuine rendering bug found and fixed along the way: `frontFace` in both the main and shadow pipelines was `COUNTER_CLOCKWISE`, but `mat4::perspective`/`orthographic`'s Y-flip (for Vulkan's NDC) mirrors screen-space winding without a compensating negative-height viewport — so back-face culling was silently keeping every box's *inner*, inward-pointing-normal face instead of the correct outward one. Invisible on the demo scene's rounded shapes, glaring on the house's thin flat walls (the room-facing wall closest to its own light rendered as the darkest surface in view). Fixed by flipping both pipelines to `CLOCKWISE`; verified with debug-normal-as-color shader output and real before/after screenshots, and re-verified `demo_scene.json` shows no regression.
8. **Frustum + occlusion culling, plus an actual FPS measurement** — currently zero culling exists and the 60 FPS requirement has never been checked against anything; the house scene (two rooms, more draw calls) is exactly where this starts to matter.
9. **Doxygen**: add `@brief`/`@param`/`@return` annotations across the public API and add a `Doxyfile` + generation instructions.
10. **Entity-component system** distinct from the current plain scene graph.
11. **Memory-leak and crash-safety verification** (Valgrind/ASan run at least once; consider whether `std::abort()` on Vulkan errors is the right failure mode for a submission that will be poked at during defense).
12. Bonus features, once the above is solid — a working particle system (item 7) already covers part of the "Particle Systems" bonus for free.

(Git tracking and the library-target restructure, formerly the top two items here, are both already done — see the Chapter III/VIII rows above.)

---

## Addendum 1: historical audit of the `FullLibft` repository (pre-`VeryRealEngine`)

*(Kept for context: this was the original audit, performed before any `VeryRealEngine` code existed, of whether the pre-existing `FullLibft` repository at the root of this checkout could serve as — or be adapted into — the subject's required engine. It concluded no, which is why `VeryRealEngine` was built as an independent project from `VeryRealEngine/` down.)*

### What was checked

- Full repo tree (`Modules/`, `Demo/`, `Docs/`, `Test/`, `mk/`, `tools/`)
- Root `README.md` and `AGENTS.md`
- Grep for `vulkan`, `VeryRealEngine`, `physics`, `entity`/`component`, `scene`, doxygen config
- `Modules/GPGR`, `Modules/Game`, `Modules/Voxel` contents
- Git remotes/branches

### Findings

| Subject requirement | Status | Evidence |
|---|---|---|
| Library named `VeryRealEngine` | ❌ Missing | No file, target, or reference to `VeryRealEngine` anywhere in the `FullLibft` repo. That project is `FullLibft` / `Full_Libft.a`. |
| Vulkan-based graphics engine | ❌ Missing / wrong API | `Modules/GPGR` wraps **OpenGL** (`gpgr_gl_funcs.cpp/.hpp`), not Vulkan. Zero hits for "vulkan" in any `FullLibft` source file. |
| Scene loading from a file (JSON or custom) | ❌ Not found | No `scene*` files, no scene-graph loader distinct from the voxel world save format. |
| .OBJ model loading (textured/untextured, multiple at once) | ❌ Not found | No `.obj` parser located anywhere in `FullLibft`. |
| Physics engine (collisions, gravity, friction, triggers) | ❌ Not found | No `physic*`-named module. Voxel collision (if any) is game/world logic, not a general physics engine with trigger volumes. |
| Scene hierarchy (parent-child, hide-with-children) | ❌ Not found | No hierarchy/transform-graph module surfaced by search. |
| Frustum/Occlusion culling, 60 FPS release target | ⚠️ Partial | `Modules/Voxel/voxel_mesh_frustum.cpp` suggests frustum culling exists for voxel meshes, but this is chunk-culling for a voxel game, not a general engine optimization layer. |
| Materials/textures system | ⚠️ Partial at best | Nothing dedicated found beyond voxel texturing. |
| Lighting + dynamic shadows (static & dynamic) | ❌ Not found | No lighting/shadow module found. |
| Entity-Component System | ❌ Not found | `Modules/Game` is a bespoke RPG-style game-logic layer, not an ECS. |
| Interactive house demo (rooms, doors/switches, particle steam, smooth nav) | ❌ Not found | `Demo/` contains a card/leaderboard-style demo, not a house walkthrough. |
| Doxygen, warning-free | ⚠️ Unverified but plausible | `Docs/Doxyfile` and `tools/run_doxygen.py` exist, but document `FullLibft`, not a `VeryRealEngine` API surface. |
| Bonus (post-processing, sound, particles, skeletal animation, networking) | ❌ Not scoped for this subject | `Modules/Networking` serves `FullLibft`'s own purposes, not a game-netcode bonus. |

### Bottom line (as of that original audit)

`FullLibft` is a mature, unrelated project (a large modular C/C++ "libft-plus" library with a voxel game, networking stack, parsers, crypto, etc., built on OpenGL). It contained none of the required pieces. Original planned roadmap (now largely executed under `VeryRealEngine/`, see the requirement check above for current status):

1. Stand up a minimal Vulkan renderer + windowing (swapchain, pipeline, depth buffer). **✅ Done (step 1).**
2. Build OBJ loading + material/texture system. **✅ Done (step 2).**
3. Build a scene graph with parent-child transforms and a JSON scene format. **✅ Done (step 3).**
4. Build a physics module (broad/narrow-phase collision, gravity/friction, trigger volumes). **✅ Done (step 4).**
5. Add lighting + shadow mapping (static + dynamic). **✅ Done, with the multi-light-shadow caveat above (step 5).**
6. Build an ECS layer distinct from `FullLibft`'s `Game` module. **❌ Not started.**
7. Build the interactive house demo with doors, light switches, and a particle steam emitter. **❌ Not started.**
8. Wire up Doxygen for the new `VeryRealEngine` API and confirm zero warnings. **❌ Not started.**
9. Bonus. **❌ Not started.**

## Addendum: dependency audit — can anything be lifted out of `FullLibft` cleanly?

Checked whether "candidate reusable" `FullLibft` modules (`Math`, `Threading`, `Networking`, `JSon`, `CMA`) — or their own dependencies — are actually separable. They are not.

**Direct `#include` fan-out for the candidates:**

- `Math` → `CMA`, `Errno`, `PThread`, `Template`, `Printf`, `GetNextLine`, `RNG`, `CPP_class`, `Basic`
- `Threading` → `CMA`, `Errno`, `PThread`, `Template`, `Time`, `System_utils`, `Basic`
- `Networking` → `CMA`, `Crypto`, `Compression`, `Encryption`, `Observability`, `CPP_class`, `Threading`, `Template`, `System_utils`, `Printf`, `RNG`, `PThread`, `Compatebility`, `Basic`
- `JSon` → `CMA`, `Advanced`, `Parser`, `CPP_class`, `Template`, `System_utils`, `Printf`, `PThread`, `Compatebility`, `Basic`
- `CMA` → `Basic`, `Compatebility`, `Errno`, `PThread`, `Sink`, `System_utils`

**And the "foundational" modules these all bottom out on are not leaves either:**

- `Template` → also pulls in `JSon` **and** `YAML` (and `CPP_class`, `CMA`, `RNG`, `Printf`, `PThread`, `Errno`)
- `System_utils` → also pulls in `Networking`, `Logger`, `SCMA`, `File`, `GetNextLine`, `Observability`, `Threading`, `API`
- `PThread`, `Errno`, `Basic`, `CMA` all cross-include each other

This is a **circular, whole-tree dependency graph**, not a layered set of independent utilities. There is no module you can extract without pulling in essentially all of `FullLibft`.

**Additional reasons this blocks reuse:**

1. **House style is mandatory and non-standard.** `AGENTS.md` requires `ft_bool`, fixed-width integer types, no `for`/`switch`/ternary, `return ;`/`return (value);` spacing, Allman braces, explicit `initialize`/`destroy`/`move` lifecycle on every class, deleted copy/move by default, and thread-local error codes per class — fighting typical Vulkan/ECS/physics code idioms.
2. **Subject rule conflict.** Core components "must be implemented by you," and the subject forbids "ANY non-system library" except Vulkan. Bundling a pre-existing `FullLibft.a` runtime as the substrate for those core components is a legitimate flag risk at defense.
3. **Domain mismatch even where names look reusable.** `FullLibft`'s `Networking` is a socket/TLS/HTTP/QUIC stack for services, not game netcode; its `JSon` is bolted to `ft_string`/`Parser`/`Advanced` internals; its `Math` drags in `RNG`/`CMA`/`PThread` just for vector/matrix types.

**Conclusion, unchanged: nothing in `FullLibft` was reusable for `VeryRealEngine`.** It was built as a fully independent project with its own minimal math/containers/parsers, per this conclusion — see `src/math/vre_math.hpp`, `src/assets/obj_loader.*`, `src/assets/tga_loader.*`, and `src/assets/json_parser.*`, none of which depend on `FullLibft`.

## Addendum 2: can `VeryRealEngine` (steps 1–5) produce visuals like Figures V.1–V.4?

**Short answer: no, not close — and per the project directive at the top of this document, closing that gap is now treated as required, not optional, regardless of what the subject's literal mandatory checklist alone would demand.**

### What the reference images actually are

- **V.1 / V.2** ("Architecture Demonstration A/B") — photoreal architectural-visualization renders: physically-based materials (real metal, wood grain, brushed concrete, fabric), soft area-light shadows, indirect/bounce lighting (global illumination), ambient occlusion in every crevice, screen-space or ray-traced reflections in the glass and floor, depth of field, and a full color-graded tonemap. This is the output of an offline path tracer or a real-time PBR pipeline (Unreal/Unity HDRP or an archviz renderer like Corona/V-Ray), not a hand-rolled hobby renderer.
- **V.3** ("Architecture Demonstration C") — an ornate, high-poly interior (carved gold trim, a multi-light chandelier with dozens of individual bulbs, a stylized painted sky) with the same PBR + GI + soft-shadow treatment, plus significant hand-authored high-detail modeling.
- **V.4** ("Architecture Demonstration D") — a stylized (not photoreal) game interior, but still built on a modern PBR/GI pipeline (Unreal-quality baked or real-time GI, volumetric-feeling light shafts, properly authored high-detail props: barrels, workbenches, hanging lantern with real glow/bloom).

All four assume: a large, hand-modeled/textured asset library; a PBR material model (albedo/roughness/metallic/normal maps, not a flat diffuse tint); soft, filtered shadows from area or many-point lights; some form of global illumination or baked lightmaps; and a post-processing stack (bloom, tonemapping, color grading, possibly DOF).

### What `VeryRealEngine` has right now (steps 1–5)

| Capability the images demonstrate | Current engine state |
|---|---|
| PBR materials (albedo/roughness/metallic/normal maps) | ✅ Roughness/metallic done (Cook-Torrance BRDF in `mesh.frag`, verified with side-by-side metal/wood spheres — see the Chapter IV.1 row above). ❌ Normal maps still not implemented. |
| Soft, filtered shadows | ✅ Hardware depth-compare sampler + 3x3 PCF tap in `mesh.frag`'s `pcf()`. Verified: cropped/zoomed screenshot shows a visibly feathered shadow edge, not an aliased one. |
| Multiple shadow-casting lights | ✅ First `kMaxShadowCasters` (2) lights each get a shadow map (directional or point). ❌ Not unbounded — a 3rd+ light still shades without casting a shadow, a documented scope line. |
| Global illumination / bounce lighting / ambient occlusion | ✅ Ambient occlusion done (screen-space, depth-only, `shaders/post.frag`). ❌ Bounce lighting / full GI still not implemented — flat ambient scalar remains the base term AO multiplies against. |
| Reflections (glass, floor, metal) | ❌ Not implemented. No screen-space reflections, no reflection probes/cubemaps. |
| Post-processing (bloom, tonemapping, color grading, DOF) | ✅ Tonemapping + bloom done (`post.frag`). ❌ Color grading and depth of field still not implemented. |
| High-detail, hand-authored assets (furniture, trim, foliage, glass) | ❌ The only assets are `cube.obj` and `plane.obj`. No door, furniture, chandelier, or foliage models exist. |
| Textures beyond one procedural checkerboard | ❌ `assets/textures/` contains exactly one generated 64×64 TGA checkerboard. |
| Anti-aliasing | ✅ Done via a single-pass FXAA-lite filter (`post.frag`), not MSAA — see the priority-list rationale. |
| Interior house scene with multiple rooms | ❌ Not started (same gap as Chapter V above). |

### What is already in place that these images would also need

- A working OBJ/MTL loader handling multiple textured/untextured meshes loaded simultaneously — reusable once real house assets exist.
- A scene graph with parent-child transforms and JSON loading — reusable for door hinges, furniture placement, etc.
- Real per-fragment Lambertian lighting with multiple lights and one shadow-casting directional light — a legitimate foundation, far short of the PBR + GI + soft-shadow + post-processing stack the images show.
- A physics module with collision/gravity/friction/triggers — reusable for opening doors and detecting a player walking into a light-switch trigger.

### Verdict

**Not achievable with the engine as it stands — but now scoped as required work, not a stretch goal.** The six technical items and the content-authoring item below are exactly items 1–7 of the priority order above; they're listed here again just as the detailed rationale for why each one earns its place there:

1. ~~A PBR material/shading model~~ (albedo, roughness, metallic; a Cook-Torrance BRDF) — **done**, normal maps excepted. See the Chapter IV.1 materials row above.
2. ~~Soft shadows and support for more than one shadow-casting light~~ — **done**, capped at 2 casters. See the Chapter IV.2 shadow-rendering row above.
3. ~~Some approximation of global illumination / ambient occlusion~~ — **done** (screen-space AO). Full GI remains a much larger, unattempted undertaking.
4. ~~A post-processing pipeline~~: HDR render target + tonemap operator + bloom — **done**. Color grading and DOF remain unimplemented.
5. **Real, hand-authored (or sourced) 3D assets and textures** — furniture, doors, a chandelier, foliage, wood/metal/fabric textures with proper UVs. Content-authoring work, not engine work, but a hard requirement to visually match any figure.
6. ~~Anti-aliasing~~ — **done** (FXAA-lite, not MSAA — see item 5 above for why).

This is a real, large body of work — mostly orthogonal to the rest of the roadmap (ECS, culling, Doxygen) — and per the directive at the top of this document, it's being treated as mandatory for this project rather than deferred past everything else.
