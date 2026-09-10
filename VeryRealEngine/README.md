# VeryRealEngine

A from-scratch Vulkan 3D engine for the 42 "Very Real Engine" project. This
is a **new, independent codebase** — see [`verdict.md`](verdict.md)
for why it does not build on top of `FullLibft`'s modules (circular
dependency graph, incompatible mandatory coding style, and the subject's
"core components must be implemented by you" / "no non-system library"
rules).

## What's reused from FullLibft, and how

Nothing is reused as *code*. The one thing carried over is an **approach**:
`Modules/GPGR`'s per-OS window backend shape (one small class per platform —
Xlib on Linux, Win32, Cocoa — behind a common interface, pumping native
events into a poll loop) is mirrored in `src/platform/`. The Linux backend
was rewritten from scratch against `VK_KHR_xlib_surface` instead of GLX,
with no dependency on FullLibft's `Basic`/`Errno`/`ft_bool` plumbing.

## Status

### Step 1 — minimal Vulkan renderer + windowing (done)

- Xlib window creation, event pump, resize detection (`src/platform/linux`)
- Vulkan instance + (when available) validation layer / debug messenger
- Physical/logical device selection with graphics + present queues
- Swapchain creation, with recreation on resize
- Depth buffer (`VK_FORMAT_D32_SFLOAT` or closest supported fallback)
- Render pass with color + depth attachments
- A depth-tested graphics pipeline (push-constant MVP matrix)

### Step 2 — OBJ loading + materials/textures (done)

- `src/assets/obj_loader.{hpp,cpp}` — from-scratch Wavefront .obj/.mtl
  parser: n-gon faces (fan-triangulated), vertex deduplication, `usemtl`
  submesh grouping, `Kd`/`map_Kd` materials. No third-party OBJ library.
- `src/assets/tga_loader.{hpp,cpp}` — from-scratch uncompressed-TGA decoder
  (24/32-bit true-color) for diffuse textures. No third-party image library.
- Renderer: descriptor set layout + pool, combined image sampler, device-local
  vertex/index/texture buffers uploaded via staging buffers, a
  material/texture/mesh handle system so several `.obj` files can be loaded
  and drawn in the same frame (see `assets/models/` + `Renderer::load_mesh_from_obj`).
- Demo scene: a textured checkerboard cube (`assets/models/cube.obj` +
  `cube.mtl` + `assets/textures/checker.tga`) spinning above an untextured,
  solid-tinted ground plane (`assets/models/plane.obj` + `plane.mtl`) —
  exercises both the textured and untextured material paths, and loading
  multiple OBJs at once, as the subject calls out explicitly.

### Step 3 — scene graph + JSON scene format (done)

- `src/assets/json_value.hpp` / `json_parser.{hpp,cpp}` — from-scratch JSON
  DOM + recursive-descent parser (objects, arrays, strings with escapes,
  numbers, bool/null). No third-party JSON library, no `Modules/JSon`.
- `src/scene/scene.{hpp,cpp}` — named nodes with parent-child transforms
  (`Scene::load` requires a node's parent to appear earlier in the file's
  `"objects"` array, so world transforms are one forward pass, no
  recursion needed). Visibility is combined down the parent chain in
  `collect_render_items()`, so hiding one parent hides its whole subtree.
- `assets/scenes/demo_scene.json` — a ground plane plus a "rig" cube with a
  smaller "satellite" cube parented to it (orbits with the rig's spin).
  Press **H** to toggle `rig`'s visibility; `satellite` disappears and
  reappears with it, from editing only the parent — verified directly
  against `Scene::collect_render_items()` (3 → 1 → 3 render items as `rig`
  is hidden/shown).
- Renderer: meshes are now cached by path (`_mesh_cache`), so two scene
  nodes referencing the same `.obj` upload it once.

### Step 4 — physics module (done)

- `src/physics/physics_types.hpp` — `RigidBodyDesc`/`Collider`: linear
  dynamics on axis-aligned box/sphere colliders (no rotational dynamics —
  out of scope for what the subject's mandatory part asks for).
- `src/physics/physics_world.{hpp,cpp}` — fixed-timestep `PhysicsWorld`:
  gravity integration (semi-implicit Euler), broad-phase AABB pretest +
  exact box/box, sphere/sphere, box/sphere narrow-phase collision, impulse
  resolution with restitution and Coulomb friction, and edge-triggered
  trigger-volume enter/exit callbacks (triggers detect overlap but never
  block movement). Verified standalone (no Vulkan/window needed) against
  four scenarios: gravity + resting contact, restitution bounce height,
  friction bringing a slide to a stop, and a trigger firing enter/exit
  exactly once while never blocking the falling body.
- `Scene` gained an optional `"physics"` field per JSON node — a rigid body
  is created and, each frame, overwrites that node's world position
  (`Scene::sync_from_physics`). Physics-driven nodes must be root nodes
  (physics works in world space; a parent would reinterpret that position
  as a local offset).
- Demo scene: `falling_cube` drops under gravity through the invisible
  `trigger_zone` (logged: "entered by" / "exited by", never blocked) and
  lands on the physics-enabled `ground`, settling via collision response
  and friction — alongside the still-unaffected `rig`/`satellite`
  parent-child pair from step 3.

### Step 5 — lighting + shadow mapping (done)

- `Light`/`LightType` (renderer.hpp): up to `kMaxLights` (4) directional or
  point lights, each with color + intensity, uploaded to a per-frame
  uniform buffer (`GlobalUbo`, descriptor set 1) shared by every draw call.
- Real per-fragment lighting in `shaders/mesh.frag`: ambient + per-light
  Lambertian diffuse, with distance attenuation for point lights.
- A directional-light shadow map: a depth-only render pass/pipeline
  (`shaders/shadow.vert`, no fragment stage) renders the scene from the
  light's point of view into a 2048×2048 depth image every frame, sampled
  back in `mesh.frag` (`compute_shadow`) with a slope-scaled bias plus
  front-face culling and a rasterizer depth bias in the shadow pass itself,
  to control acne. Re-rendering the map every frame (rather than caching
  it) is what makes both static geometry (the ground) and moving objects
  (the spinning rig/satellite, the falling/settling cube) cast correct,
  up-to-date shadows — step 5's "static and dynamic shadow rendering".
- `Scene` gained an optional top-level `"lights"` array and `"ambient"`
  field in the JSON scene format.
- Verified by running the app and comparing screenshots a few seconds
  apart: cube faces show real per-fragment shading gradients (not flat
  tint), every object casts a visible shadow onto the ground, and the
  orbiting satellite cube's shadow visibly moves between frames, tracking
  its motion.
- Scope line: only `lights[0]` (required to be directional) casts a
  shadow; additional lights shade but don't occlude. Extending to more
  shadow-casting lights means more shadow maps, not a redesign.

Not yet implemented (future steps): ECS, the house demo, Doxygen docs,
bonus features. See `verdict.md` for the full roadmap.

### Controls

- **H** — toggle the "rig" scene node (and its child) on/off
- Close the window to quit

## Building

Dependencies (Linux):

```sh
sudo apt install libvulkan-dev libx11-dev
# one shader compiler, either is fine:
sudo apt install glslang-tools     # provides glslangValidator
# or
sudo apt install glslc             # provides glslc (preferred if available)
```

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Run (from the build directory, so it finds `shaders/*.spv` next to the binary):

```sh
cd build && ./very_real_engine
```

If no shader compiler is installed, CMake prints a warning and skips
compiling `shaders/*.spv`; the C++ still builds and links, but the program
will abort at startup trying to load the missing SPIR-V files. Install
`glslc` or `glslangValidator` and re-run CMake to fix that.

## Layout

```
VeryRealEngine/
  CMakeLists.txt
  shaders/            GLSL sources (compiled to SPIR-V at build time)
  src/
    main.cpp          entry point / render loop
    math/              dependency-free vec3/mat4
    platform/          windowing abstraction + per-OS backends
    renderer/          Vulkan instance/device/swapchain/pipeline/frame loop
```
