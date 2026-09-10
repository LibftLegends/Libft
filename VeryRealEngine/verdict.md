# Verdict: "Very Real Engine" (v1.4) — full requirement check against `VeryRealEngine/` as it stands

**Updated 2026-09-10, after roadmap steps 1–5 (Vulkan renderer, OBJ/materials, scene graph, physics, lighting/shadows).**

**Short answer: real, working progress on the graphics/physics/scene core (steps 1–5 are genuinely implemented and verified, not just planned) — but several requirements the subject treats as mandatory are still completely unstarted, and two structural/process issues (no library target, nothing committed to git) would each independently cause a problem at evaluation if not fixed before submission.**

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
| "The final product must be a library named `VeryRealEngine`." | ❌ **Not met** | `CMakeLists.txt` only has `add_executable(very_real_engine ...)` — there is no `add_library` target at all. Everything (renderer, physics, scene, asset loaders) is compiled straight into one demo binary. This needs restructuring: a `VeryRealEngine` static/shared library target that `src/main.cpp` links against, before this can be called compliant. |
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
| Materials/textures system, incl. distinguishable types (e.g. wood-like, metallic) | ⚠️ **Partial** | A real, working material+texture system exists: `MaterialData`/`GpuMaterial`, UV-mapped diffuse textures, per-material descriptor sets, verified visually (checkerboard cube vs. solid-tinted plane). But there is no metalness/roughness/specular workflow — "wood-like" vs. "metallic" can currently only be expressed as *different diffuse textures*, not as a different shading response (a metal and a piece of wood lit by the same light would look the same modulo texture, which isn't what "different types of materials" implies). See Addendum 2 below for the fuller PBR gap analysis. |

### Chapter IV.2 — Engine Composition

| Requirement | Status | Evidence |
|---|---|---|
| Lighting: multiple light sources | ✅ **Met, verified** | Up to 4 lights (directional or point), uploaded via a per-frame UBO (`Renderer::GlobalUbo`), each independently colored/positioned/intensity-scaled. Verified visually: a warm directional "sun" and a blue point light both visibly contributing, including the point light's blue tint remaining visible inside the sun's shadow. |
| Shadow rendering, static **and** dynamic, "based on the position and intensity of these lights" (plural) | ⚠️ **Partial** | Real shape-accurate shadow mapping exists and re-renders every frame, correctly tracking both static geometry (the ground) and moving objects (the orbiting `satellite`, the falling cube) — verified by comparing two screenshots seconds apart and seeing the shadow move with the object. However, only **one** light (`lights[0]`, required to be directional) casts a shadow; the point light shades but never occludes. The subject's plural wording ("these lights") implies every light should be able to cast shadows, which is currently not the case — a deliberate, documented scope line, but a real gap against the literal requirement. |
| JSON (or similar) scene loading: scenes, object properties, light management | ✅ **Met** | `assets/scenes/demo_scene.json` drives objects (mesh, transform, physics, visibility), materials (via referenced `.mtl`), and lights (`"lights"` array) all from one file. Runtime property changes are demonstrated only for visibility (the H-key toggle) — no broader "live-edit any property" API exists, but the subject doesn't require one beyond what's demonstrated. |

### Chapter V — Demonstration (Interactive House Environment)

| Requirement | Status | Evidence |
|---|---|---|
| Rooms with varied lighting conditions | ❌ **Not started** | The current demo scene is two cubes and a ground plane — no walls, no rooms, no architecture of any kind. |
| Interactable objects (doors, light switches, etc.) | ❌ **Not started** | No door/hinge/switch objects or interaction system exist. The physics module (collision, triggers) that would drive this is ready and proven, but nothing has been built on top of it. |
| Particle-emitting objects (e.g. steam) | ❌ **Not started** | No particle system exists anywhere in the codebase — not even a minimal CPU-side one. |
| Smooth, realistic, intuitive navigation through the house | ❌ **Not started** | There is no player/first-person controller at all; the camera in `main.cpp` is a single hardcoded `look_at`. No movement input handling beyond the one-off H key exists. |

This entire chapter — explicitly called "a final requirement," not optional — is unstarted. It's step 7 in the roadmap and is, along with the material/lighting fidelity gap (Addendum 2), the largest remaining body of work.

### Chapter VI — Documentation (Doxygen)

| Requirement | Status | Evidence |
|---|---|---|
| Doxygen comments on all classes, functions, methods, variables | ❌ **Not met** | `grep` for `@brief`/`@param`/`@return` across `src/` returns zero matches. All existing comments are plain `//` prose, which is good practice but isn't what Chapter VI asks for. |
| A `Doxyfile`/generation script in the repo | ❌ **Not present** | No `Doxyfile` exists under `VeryRealEngine/` (note: `Libft`'s root `Docs/Doxyfile` belongs to the unrelated `FullLibft` project and documents that codebase, not this one). |
| Warning-free Doxygen generation | N/A | Can't be assessed until the above two exist. |

### Chapter VII — Bonus Part

| Requirement | Status |
|---|---|
| Post-processing (bloom, motion blur, DOF) | ❌ Not started |
| Sound system | ❌ Not started |
| Particle systems | ❌ Not started (also blocks the mandatory house-demo steam requirement above) |
| Skeletal animation | ❌ Not started |
| Networking (multiplayer) | ❌ Not started |

None of this is required yet, but note that a working particle system would simultaneously satisfy both the mandatory house-demo's steam requirement *and* part of the bonus — worth prioritizing over the other bonus items for that reason alone.

### Chapter VIII — Submission and Peer Evaluation

| Requirement | Status | Evidence |
|---|---|---|
| Work is inside the Git repository that will be evaluated | ❌ **Not met right now** | `git status --porcelain VeryRealEngine` reports the entire directory as `??` (untracked). Nothing under `VeryRealEngine/` has ever been `git add`ed or committed. **As it stands today, if this were submitted for evaluation, none of steps 1–5 would exist from the evaluator's point of view** — this is the single most urgent fix needed, ahead of any further feature work. (It's also unclear yet whether `VeryRealEngine` should live as a subtree of this `Libft` repository or its own separate repository — worth deciding explicitly, since the earlier dependency audit concluded it must not depend on `FullLibft`.) |
| Compiles and runs from a fresh repository clone | ⚠️ **Plausible but not actually tested this way** | The build has been verified repeatedly from a clean `build/` directory within the existing checkout, and `README.md` documents the exact dependencies (`libvulkan-dev`, `libx11-dev`, a GLSL compiler) and build commands needed. But a true fresh-clone test (a separate `git clone` into an empty directory) has not been performed — and can't be, meaningfully, until the directory is actually committed (see above). |
| Doxygen generation instructions in the repo | ❌ **Not present** | Ties directly to the Chapter VI gap above. |
| Readable, well-commented, peer-reviewable code | ✅ **Reasonably strong** | Consistent modern C++ style throughout, explanatory comments (including deliberate scope-limitation notes so a reader doesn't mistake a simplification for a bug), and clear module boundaries (`platform/`, `renderer/`, `physics/`, `scene/`, `assets/`, `math/`). |

## Priority order for what's actually missing

1. **Commit `VeryRealEngine` to git.** Nothing else matters for grading if the work isn't in the repository being evaluated.
2. **Restructure the build into an actual library target** (`add_library(VeryRealEngine ...)`) with `src/main.cpp` linking against it as the demo executable — the subject is explicit that "the final product must be a library."
3. **Chapter V's Interactive House Environment** — rooms, a door, a light switch, steam particles, and basic player navigation. This is the largest remaining body of engine-adjacent + content work and is explicitly mandatory, not optional.
4. **Frustum + occlusion culling, plus an actual FPS measurement** — currently zero culling exists and the 60 FPS requirement has never been checked against anything.
5. **Doxygen**: add `@brief`/`@param`/`@return` annotations across the public API and add a `Doxyfile` + generation instructions.
6. **Entity-component system** distinct from the current plain scene graph.
7. **Memory-leak and crash-safety verification** (Valgrind/ASan run at least once; consider whether `std::abort()` on Vulkan errors is the right failure mode for a submission that will be poked at during defense).
8. Only after the above: PBR materials, soft/multi-light shadows, and the other visual-fidelity items in Addendum 2, plus any bonus features.

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

**Short answer: no, not close — and closing that gap fully is a much bigger undertaking than the subject's mandatory checklist implies.**

### What the reference images actually are

- **V.1 / V.2** ("Architecture Demonstration A/B") — photoreal architectural-visualization renders: physically-based materials (real metal, wood grain, brushed concrete, fabric), soft area-light shadows, indirect/bounce lighting (global illumination), ambient occlusion in every crevice, screen-space or ray-traced reflections in the glass and floor, depth of field, and a full color-graded tonemap. This is the output of an offline path tracer or a real-time PBR pipeline (Unreal/Unity HDRP or an archviz renderer like Corona/V-Ray), not a hand-rolled hobby renderer.
- **V.3** ("Architecture Demonstration C") — an ornate, high-poly interior (carved gold trim, a multi-light chandelier with dozens of individual bulbs, a stylized painted sky) with the same PBR + GI + soft-shadow treatment, plus significant hand-authored high-detail modeling.
- **V.4** ("Architecture Demonstration D") — a stylized (not photoreal) game interior, but still built on a modern PBR/GI pipeline (Unreal-quality baked or real-time GI, volumetric-feeling light shafts, properly authored high-detail props: barrels, workbenches, hanging lantern with real glow/bloom).

All four assume: a large, hand-modeled/textured asset library; a PBR material model (albedo/roughness/metallic/normal maps, not a flat diffuse tint); soft, filtered shadows from area or many-point lights; some form of global illumination or baked lightmaps; and a post-processing stack (bloom, tonemapping, color grading, possibly DOF).

### What `VeryRealEngine` has right now (steps 1–5)

| Capability the images demonstrate | Current engine state |
|---|---|
| PBR materials (albedo/roughness/metallic/normal maps) | ❌ One flat diffuse texture + a solid tint color per material (`MaterialData`, `mesh.frag`). No roughness, metalness, or normal maps. |
| Soft, filtered shadows | ❌ Single hard-edged shadow map, one sample per fragment (`compute_shadow` in `mesh.frag`), no PCF/PCSS. |
| Multiple shadow-casting lights | ⚠️ Partial by design — only `lights[0]` (must be directional) casts a shadow; additional lights shade but don't occlude (documented scope line). |
| Global illumination / bounce lighting / ambient occlusion | ❌ Not implemented. Ambient is a single flat scalar added uniformly everywhere. |
| Reflections (glass, floor, metal) | ❌ Not implemented. No screen-space reflections, no reflection probes/cubemaps. |
| Post-processing (bloom, tonemapping, color grading, DOF) | ❌ Not implemented. No tonemap operator; bright lights would clip rather than bloom. |
| High-detail, hand-authored assets (furniture, trim, foliage, glass) | ❌ The only assets are `cube.obj` and `plane.obj`. No door, furniture, chandelier, or foliage models exist. |
| Textures beyond one procedural checkerboard | ❌ `assets/textures/` contains exactly one generated 64×64 TGA checkerboard. |
| Anti-aliasing | ❌ No MSAA/TAA; single-sample throughout `renderer.cpp`. |
| Interior house scene with multiple rooms | ❌ Not started (same gap as Chapter V above). |

### What is already in place that these images would also need

- A working OBJ/MTL loader handling multiple textured/untextured meshes loaded simultaneously — reusable once real house assets exist.
- A scene graph with parent-child transforms and JSON loading — reusable for door hinges, furniture placement, etc.
- Real per-fragment Lambertian lighting with multiple lights and one shadow-casting directional light — a legitimate foundation, far short of the PBR + GI + soft-shadow + post-processing stack the images show.
- A physics module with collision/gravity/friction/triggers — reusable for opening doors and detecting a player walking into a light-switch trigger.

### Verdict

**Not achievable with the engine as it stands.** Reaching anything close to Figures V.1–V.4 requires, on top of the priority list above:

1. **A PBR material/shading model** (albedo, roughness, metallic, normal maps; a Cook-Torrance or similar BRDF) — a substantial rewrite of `mesh.frag`/`MaterialData`/the material descriptor-set layout.
2. **Soft shadows** (PCF at minimum; PCSS or shadow-map cascades for more lights/quality) and support for more than one shadow-casting light.
3. **Some approximation of global illumination / ambient occlusion** — even a cheap screen-space AO pass would meaningfully close the gap; full GI is a much larger undertaking.
4. **A post-processing pipeline**: HDR render target + tonemap operator is close to mandatory just to avoid blown-out highlights; bloom is explicitly a bonus item.
5. **Real, hand-authored (or sourced) 3D assets and textures** — furniture, doors, a chandelier, foliage, wood/metal/fabric textures with proper UVs. Content-authoring work, not engine work, but a hard requirement to visually match any figure.
6. **Anti-aliasing** (MSAA is the cheapest fit for the current single-subpass forward renderer).

**None of this blocks the subject's literal mandatory-part checklist**, which asks for "realistic" shadows and a material *system*, not photorealism. But if the goal is to get anywhere near what Figures V.1–V.4 show, that gap is real, large, and mostly orthogonal to the remaining roadmap steps — it would need to be scoped as its own body of work (PBR shading, soft shadows/AO, post-processing, asset authoring) layered on top of the current renderer.
