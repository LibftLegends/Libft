# Verdict: "Very Real Engine" (v1.4) — fresh, independently-checked assessment

**Date: 2026-09-11. This replaces the prior verdict.md wholesale rather than amending it**, because the
prior file's confidence level ("Verified: ...", "confirmed via screenshot", etc.) cannot be
trusted at face value — see "What I could and couldn't verify" below. Everything in this
version is either (a) checked directly against the source in this repo just now, or (b)
explicitly marked as un-checkable in this environment.

## What I could and couldn't verify

**Update, superseding the paragraph below**: this machine originally had no build toolchain at
all (see the "as originally written" note); a Vulkan/MoltenVK toolchain has since been installed
and a macOS platform backend added specifically so this project could be built and run locally
(see the dedicated macOS section further down) — so some claims below *are* now independently
re-verified on this machine (not just re-read from source), and are marked as such. Everything
else — anything specific to Linux/Xlib behavior, and anything about runtime correctness beyond
"it renders a frame without erroring" (physics test results, exact shadow/AO visual output,
etc.) — is still unverified here and needs a real Linux run to confirm.

*As originally written, before that toolchain existed:* this machine (macOS) had no `cmake`, no
Vulkan SDK, no `glslc`/`glslangValidator`. I could not configure, build, or run the project, so I
cannot personally confirm any claim of the form "verified visually" / "confirmed via screenshot"
/ "ran the four physics tests" — those all require a build. The previous verdict.md is full of
such claims; treat them as **unverified self-report from whatever session wrote them**, not
fact, until someone actually builds and runs this on Linux and checks.

What I *could* do, and did, in that state: read every relevant source/shader file and grep for
the specific techniques and requirements claimed. That's a legitimate way to check "does this
code exist and is it structurally real" — it is not a way to check "does it actually render
correctly at runtime." Below, "confirmed in source" means the former; nothing in this document
means the latter unless someone runs it (which, for the macOS build specifically, has now
happened — see that section).

## Bottom line, up front

- The engine is a genuine, non-trivial, from-scratch Vulkan renderer + physics + scene graph.
  It is not vaporware — the shader math and CPU-side systems described are actually present in
  the files, not just described in markdown.
- **The request to match Figures V.1/V.2's photorealism is not an achievable or sensible bar for
  this project, and I'm recommending you drop it as a literal target** (see the dedicated section
  below). Chasing it will burn the remaining time on diminishing-returns rendering tricks instead
  of on requirements that are actually gradable and actually achievable.
- Several *literally mandatory, checklist-item* requirements are still simply not done and are
  cheap relative to the visual-fidelity work already sunk: culling + an actual FPS number,
  Doxygen, and an ECS. These should come before any further rendering polish.

## Requirement-by-requirement, current state (verified in source just now)

| Requirement | Status | Evidence (checked directly) |
|---|---|---|
| Library named `VeryRealEngine` | ✅ | Was `CMakeLists.txt`'s `add_library(VeryRealEngine STATIC ...)`; **the build is now a hand-written `Makefile`** (see dedicated section below) that reproduces the same split — same 7 engine source files into `libVeryRealEngine.a`, `main.cpp` only in the `very_real_engine_demo` executable. Structurally correct either way. |
| No non-system library except Vulkan | ✅ (as far as CMake shows) | Only `Vulkan::Vulkan` and `${X11_LIBRARIES}` linked. Hand-written OBJ/MTL, TGA, JSON parsers exist in `src/assets/`. |
| Scene loading from a file | ✅ | `src/assets/json_parser.*` + `src/scene/scene.cpp`; `assets/scenes/demo_scene.json` and `house_scene.json` are real, readable JSON. |
| OBJ loading, textured + untextured, multiple at once | ✅ (structurally) | `src/assets/obj_loader.cpp` exists; scene JSON references 6 distinct `.obj` files. Cannot confirm it *renders* correctly without a build. |
| Physics: gravity, collision, friction, triggers (non-blocking) | ✅ (structurally) | `src/physics/physics_world.cpp` (268+ lines) implements AABB broad-phase, box/box and sphere-based narrow-phase, impulse resolution with restitution, Coulomb friction (confirmed the tangent-impulse math is real, not stubbed), and trigger enter/exit callbacks. Real code, not a placeholder. |
| Scene hierarchy, hide-parent-hides-children | ✅ (structurally) | `Scene::collect_render_items` combines visibility down the parent chain — confirmed by reading the function. |
| Frustum + occlusion culling, ≥60 FPS in Release | ❌ **Not done, not measured** | `grep -rni "frustum\|occlusion"` across `src/` and `shaders/` returns only comments about *shadow* frustums, never camera-view culling. `grep` for `fps`/`frame_time`/`steady_clock` in `main.cpp`/`renderer.cpp` returns **zero hits** — there is no code anywhere that measures frame time. This requirement is not partially done, it is unstarted, and the "60 FPS" number has never once been produced. |
| Materials/textures, distinguishable types (wood/metal) | ✅ (structurally) | `shaders/mesh.frag` has a real Cook-Torrance BRDF: `distribution_ggx`, `geometry_smith`/`geometry_schlick_ggx`, Schlick Fresnel (`f0 = mix(vec3(0.04), albedo, metallic)`) — this is genuine PBR shader math, confirmed by reading the shader, not just claimed. `.mtl` files carry `Pr`/`Pm` (roughness/metallic). No normal maps: confirmed zero hits for `normal_map`/`tangent` anywhere in `src/` or `shaders/`. |
| Multiple lights | ✅ (structurally) | `house_scene.json` defines 2 point lights; shader/UBO code supports up to 4. |
| Static + dynamic shadows | ✅ (structurally) | `shaders/mesh.frag` implements PCF (`pcf()`, hardware depth-compare sampler) against 2 shadow maps (`shadow_maps[2]`); `renderer.cpp` computes light-space matrices for both directional and point lights. Real, non-trivial shader/CPU code. |
| JSON scene loading (objects, materials, lights) | ✅ | Confirmed directly by reading `house_scene.json` and `demo_scene.json` — both drive geometry, transforms, parent/child, and lights from data, not hardcoded C++. |
| Interactive House Environment (Chapter V) | ⚠️ **Structurally present, minimally content** | `house_scene.json` has two rooms, a `door_hinge`/`door` pair, a `light_switch`, a `coffee_machine`. `main.cpp` wires **E** (door) and **F** (switch) to real proximity checks against node positions (confirmed by reading the interaction code, not just the README's description of it). **But every single piece of geometry in this scene — walls, floor, ceiling, door, light switch, coffee machine — is the same scaled unit cube** (`cube_wall.obj`, `cube_wood.obj`, `cube_metal.obj`). There is no modeled door handle, no hinge geometry, no switch plate, no coffee-machine shape — it's a box standing in for a coffee machine. This satisfies the *mechanic* the subject asks for (interactable objects, particle emitter, varied per-room lighting) but not with any visual quality; it will read as literally a room of gray/brown boxes if run. |
| Particle system (steam) | ✅ (structurally) | `src/particles/particle_system.cpp` is a real emitter/lifetime/velocity system (confirmed reading it, not a stub), instantiated in `main.cpp` above `coffee_machine`'s position. Renders as small shrinking cubes, not a billboard/sprite system — functionally a particle system, visually primitive. |
| Doxygen (`@brief`/`@param`/`@return`), `Doxyfile` | ❌ | `grep -rn "@brief\|@param\|@return" src/` → **0 matches**. No `Doxyfile` anywhere in the repo. This is a flat zero, unstarted, and it's an explicit, separately-graded checklist item (Ch. VI, Ch. VIII.2) — not covered by the plain `//` comments that do exist. |
| Entity-Component System | ❌ | `grep -rni "entity.*component\|ecs" src/` → 0 matches. `Scene`/`SceneNode` is a plain transform tree, not a component-store architecture. The subject requires the ECS specifically as one of "the core components of your engine" (Ch. III). |
| No crashes / no memory leaks | ❓ Unverified either way | 59 combined `abort`/`VK_CHECK` call sites in `renderer.cpp` — the design intentionally hard-aborts on any Vulkan error or missing asset rather than degrading gracefully. Never run under ASan/UBSan/Valgrind (can't be, in this environment). This is a real defense risk: an evaluator deleting/renaming one texture file will crash the whole demo, not just cause a visible glitch. |
| Bonus (post-processing, sound, particles, skeletal anim, networking) | ⚠️ Partial | Bloom + ACES tonemap + a screen-space AO pass + an FXAA-style pass all have real shader code in `shaders/post.frag` (confirmed reading it — `aces_tonemap`, `sample_bloom`, `fxaa_lite` all exist and are wired into the composite, not dead code). No sound, no skeletal animation, no networking. Particle system above already covers part of the bonus's "Particle Systems" line. |

## On matching Figures V.1–V.4: project directive, not a suggestion

**Decision (per direct instruction): getting visually close to Figures V.1/V.2 is a real
requirement for this project's demo, not something to deprioritize.** It does not need to be a
pixel-identical match — "recreate and get close" is the actual bar — but it is not optional and
it is not satisfied by the current engine. Recording that plainly so priority order below reflects
it:

1. **What V.1/V.2 actually are, technically** (context for why this is hard, not a reason to
   drop it): the exact signature of offline-path-traced architectural visualization or real
   photography — true bounced global illumination, soft area-light shadow penumbras,
   centimeter-scale physically accurate material detail (concrete, tile, wood grain), correctly
   refracting glass, and camera-realistic grain/DOF. V.3/V.4 read as real-time-plausible
   (Unreal-quality baked lighting + licensed high-poly assets). None of this changes the goal —
   it changes what has to be built to approach it.
2. **What closing the gap actually requires, concretely**, ranked by leverage (biggest visible
   improvement per unit of effort first):
   - **Real hand-modeled/sourced assets with proper UVs** — currently every object in the house
     demo (floor, walls, ceiling, door, light switch, coffee machine) is the same stretched unit
     cube (confirmed: `cube_wall.obj`/`cube_wood.obj`/`cube_metal.obj` reused everywhere in
     `house_scene.json`). This is the single highest-leverage gap: no amount of shader work
     makes a box look like a paneled door or a chandelier. Even simple, low-poly but
     *distinctly-shaped* props (door with a handle, a recognizable coffee-machine silhouette,
     a switch plate, some furniture) close more of the visual gap than any lighting feature
     below.
   - **Texture resolution and variety** — `assets/textures/` currently holds 4 small procedural
     TGAs (checker/metal/wall/wood). V.1/V.2-grade surfaces need higher-resolution,
     photographic or hand-painted albedo textures, plus roughness/AO maps per material (only
     flat `Pr`/`Pm` scalars exist right now, confirmed in the `.mtl` files).
   - **Normal maps** — confirmed zero implementation (no `tangent`/`normal_map` anywhere in
     `src/` or `shaders/`). Needed for any material to read as having real surface detail
     (grout lines, wood grain, brushed metal) instead of a flat-shaded box.
   - **Reflections** — confirmed no SSR/cubemap code anywhere. Even a cheap screen-space
     reflection pass or a static reflection cubemap for glass/floor would move the needle a lot
     for a scene like V.2 (glass door, reflective floor).
   - **Some form of baked or approximate GI beyond the current screen-space AO** — the current
     ambient term is one flat scalar plus depth-only SSAO (confirmed in `post.frag`); a cheap
     light-probe or baked-lightmap approximation would read as "the room bounces light" instead
     of "the room has one ambient constant."
   - **Depth of field + color grading** — confirmed absent from `post.frag`; comparatively cheap
     post-process additions once the above exist, and they do a lot of work in V.1/V.2's actual
     "look."
3. **This is genuinely large scope**, larger than everything already built for steps 1–5
   combined, and it is content-authoring work (assets/textures) as much as it is engine work
   (normal maps, reflections, GI approximation, DOF/grading). Budget time accordingly — this
   should be treated as its own major phase, not a polish pass tacked onto the end.

## What's actually missing, in priority order

Visual-fidelity work toward V.1/V.2 (above) and the remaining checklist-mandatory items below
are both real priorities; they're listed separately because they're different *kinds* of work
(content/rendering vs. checklist/process), not because one is optional.

**Checklist-mandatory, currently zero, cheap relative to the value:**

1. **Frustum culling + a real FPS counter.** Currently zero culling code exists and frame time
   has never been measured anywhere in the codebase (confirmed: zero hits for any timing API in
   `main.cpp`/`renderer.cpp`). This is a hard checklist requirement ("You must achieve at least
   60 FPS") that literally cannot be claimed as met right now — there's no number to point to.
   Cheap to add (a per-object AABB-vs-frustum-plane test, a `std::chrono` frame timer), and it's
   graded.
2. **Doxygen.** Zero `@brief`/`@param`/`@return` tags exist anywhere, zero `Doxyfile`. This is a
   separately, explicitly graded chapter (VI) and submission checklist item (VIII.2) that is
   currently a flat unmet zero — not partial credit, none.
3. **An actual ECS**, distinct from the current plain `Scene`/`SceneNode` transform tree. The
   subject names this specifically as one of "the core components ... implemented by you."
4. **Crash-safety hardening + one real leak/sanitizer pass.** 59 `abort()`/`VK_CHECK` call sites
   currently take the whole process down on any missing asset or driver hiccup. At minimum,
   run once under ASan/UBSan and once under Valgrind (or an equivalent) on a real Linux box —
   neither has ever been done, and it's explicitly called out as forbidden behavior ("under no
   circumstances should your program terminate unexpectedly").

**Visual-fidelity toward V.1/V.2, large scope, own phase:** real assets/textures, normal maps,
reflections, a GI approximation, DOF/color grading — see the ranked list above.

## Build system: now a plain Makefile, not CMake

Per instruction, the build has been converted from CMake to a hand-written `Makefile` so the
project builds with a plain `make` (a 42-standard expectation this subject's Chapter III also
implies: "Your Makefile or equivalent... must recompile and re-link the library only when
necessary").

- `CMakeLists.txt`, `CMakeCache.txt`, `CMakeFiles/`, and `cmake_install.cmake` are deleted and
  untracked from git — those `CMakeFiles`/`CMakeCache.txt` files were, oddly, committed in this
  repo (the result of an in-source `cmake -S . -B .` at some point); that's gone now.
- The new root `Makefile` builds `libVeryRealEngine.a` from every engine source and links
  `very_real_engine_demo` against it, mirroring exactly what `CMakeLists.txt` used to declare
  (same source lists, same "library excludes `main.cpp`" split). Object files land under `obj/`
  with `-MMD -MP` dependency tracking, so `make` only recompiles/re-links what changed — the
  same incremental-build guarantee CMake gave, now via plain `make`.
  Shaders compile in place (`shaders/*.vert`/`*.frag` → `shaders/*.spv`) via `glslc` (falling
  back to `glslangValidator`), and the demo runs directly from the repo root (`./very_real_engine_demo`) since it already finds `shaders/` and `assets/` there — no separate build/copy
  directory needed anymore.
- Standard rules: `make` (or `make all`), `make clean`, `make fclean`, `make re`.
- **This has now actually been built and run** (superseding the "not verified" note this section
  previously had — this machine has since had a real toolchain installed, see the macOS section
  below): a full `make` completed with zero warnings, and `./very_real_engine_demo` stayed in a
  live render loop for 3+ seconds (`Renderer: using physical device "Apple M1"`, meshes/scene
  loaded, no `VK_CHECK` abort). Not yet confirmed on Linux, which is the actual submission
  target — the Linux branch of the Makefile hasn't changed from what was dry-run-checked before,
  so re-verify there too before submitting.
- `README.md` build instructions updated to match (`make` instead of `cmake -S . -B build`).

## macOS support (new): the engine now genuinely builds and runs here, not just Linux

Per instruction, added so the rest of the engine can be developed/iterated on locally on macOS,
then fine-tuned/finally verified on Linux (the actual grading target) — not a replacement for
that Linux verification, an addition for faster local iteration.

- **New platform backend**: `src/platform/macos/window_macos.{hpp,mm}` — a `WindowMacOS` class
  implementing the same abstract `Window` interface `WindowLinux` does (Cocoa/`NSWindow` +
  `VK_EXT_metal_surface` instead of Xlib + `VK_KHR_xlib_surface`). `window.hpp`'s interface
  needed zero changes — it was already a clean enough abstraction (native window handle,
  poll/resize/key state, "give me your required instance extensions," "give me a
  `VkSurfaceKHR`") that a second backend just slots in.
- **Renderer changes, both `#ifdef __APPLE__`-guarded and each checked for actual support before
  use (not assumed)**: `create_instance()` now conditionally requests
  `VK_KHR_portability_enumeration` (only if `vkEnumerateInstanceExtensionProperties` actually
  lists it — MoltenVK, linked directly rather than through the generic Vulkan Loader, doesn't
  advertise it, so requesting it unconditionally hard-fails instance creation with
  `VK_ERROR_EXTENSION_NOT_PRESENT`, which is exactly the failure hit and fixed while wiring this
  up). `create_logical_device()` now conditionally enables `VK_KHR_portability_subset` if the
  physical device advertises it (the Vulkan spec requires enabling it when present; checked via
  `vkEnumerateDeviceExtensionProperties`, not assumed either way). Neither change affects Linux:
  both are `#ifdef __APPLE__`, and even on Apple only fire if the extension is actually there.
- **Makefile**: a `Darwin` branch alongside the existing `Linux` one. Homebrew:
  `brew install vulkan-headers vulkan-loader molten-vk shaderc` (installed and confirmed present
  on this machine). Links directly against `libMoltenVK.dylib` (found via `brew --prefix
  molten-vk`) rather than the generic Vulkan loader, since Homebrew's `molten-vk` formula doesn't
  ship an ICD manifest for the loader to discover it by — simpler for local dev, one less moving
  part. A dedicated `%.o: %.mm` pattern rule compiles the new Objective-C++ backend with
  `-fobjc-arc` scoped to just that file (the rest of the engine is plain C++, unaffected).
- **A real, unrelated toolchain bug hit and worked around**: this machine's `clang`/`ld`
  defaults to resolving a beta `MacOSX27.2.sdk` under `/Library/Developer/CommandLineTools/SDKs/`
  instead of the stable SDK bundled with the installed Xcode (26.2) — that beta SDK's `.tbd`
  stub libraries (`Cocoa.tbd`, `libSystem.tbd`, `libc++.tbd`) list an architecture slice
  (`arm64e.x1-macos`) the installed linker doesn't recognize, so any link touching those
  frameworks fails with `tapi error: malformed file` / `unknown architecture` — nothing to do
  with this project's code. Fixed by pinning both the compile and link steps to
  `` `xcrun --sdk macosx --show-sdk-path` `` (which correctly resolves to the Xcode-bundled SDK,
  bypassing whatever makes the ambient/CommandLineTools default resolve to the broken beta one).
  Documented in both the Makefile and `README.md`'s macOS section in case this is something about
  this specific machine's toolchain state rather than something universal.
- **Verified end-to-end, not just "compiles"**: `make` completes with zero warnings; the binary
  is a valid `arm64` Mach-O; running it selects the Apple M1 GPU, loads all OBJ meshes and the
  house JSON scene, and stays alive in the render loop for 3+ seconds with no `VK_CHECK` abort or
  Vulkan error — this is a real, functioning Vulkan (via MoltenVK) instance + device + swapchain
  + render pipeline on this machine, not just a successful `make`.
- **What this does *not* establish**: MoltenVK is a translation layer, not a conformant Vulkan
  implementation — feature/extension support, performance characteristics, and edge-case
  behavior (especially around the shadow-mapping and post-processing passes, which push more
  unusual Vulkan usage than a basic triangle) can differ from a real Linux driver. Nothing here
  changes the Linux-first advice already in this document: use macOS for day-to-day iteration,
  but do the actual 60 FPS measurement, crash/leak verification, and final visual check on Linux
  before submitting.

## What I did not re-litigate

The dependency-audit conclusion in the prior version of this file (that `FullLibft` was
correctly judged unreusable, and `VeryRealEngine` correctly built as an independent project) is
a judgment call about a different, unrelated codebase and isn't something this pass re-checked;
nothing here contradicts it. If you want that re-verified too, say so explicitly — it wasn't the
question asked this time.
