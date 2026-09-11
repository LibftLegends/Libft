# VeryRealEngine

A from-scratch Vulkan 3D engine for the 42 "Very Real Engine" project. This
is a **new, independent codebase** — see <a href="verdict.md"><code>verdict.md</code></a>
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

### Visual-fidelity pass 1 — PBR (roughness/metallic) shading (done)

- `MaterialData` gained `roughness`/`metallic` fields, parsed from the `Pr`/`Pm`
  extension to `.mtl` (the same convention Blender's OBJ exporter uses).
- `shaders/mesh.frag` replaced flat Lambertian shading with a Cook-Torrance
  BRDF: GGX normal distribution, Smith geometry term, Schlick Fresnel —
  standard real-time PBR, not a shortcut approximation.
- A cheap ambient-specular term (`f0 * ambient`) stands in for the
  environment/IBL reflection this engine doesn't have yet — without it, a
  smooth metal with no diffuse term renders as solid black outside its
  direct specular highlight, which is a real artifact caught and fixed
  during this pass (see `assets/models/sphere_metal.obj`'s before/after).
- Demo assets: a procedurally generated UV sphere
  (`assets/models/sphere_{metal,wood}.obj`) plus `metal.mtl`/`wood.mtl`
  and matching procedural textures, added to `demo_scene.json` specifically
  to make the metallic-vs-wood-like distinction visible side by side —
  verified visually: the metal sphere shows a small, sharp, tinted
  highlight on a mid-gray base; the wood sphere shows a broad, soft,
  un-tinted highlight on a matte brown base.
- Scope line: no normal maps yet (would need per-vertex tangents), no
  environment map/IBL (hence the ambient-specular stand-in above).

### Visual-fidelity pass 2 — soft, multi-light shadows (done)

- Shadow mapping extended from one hard-edged caster to `kMaxShadowCasters`
  (2) — the first two lights in scene order each get their own shadow map,
  rendered fresh every frame, regardless of light type.
- `compute_light_space_matrix` now handles both light types: directional
  keeps its orthographic projection; point lights get a perspective
  frustum aimed from the light's position at the scene center. A point
  light's shadow is a single frustum, not a full 6-face cubemap — exact
  for whatever it covers, but a real, documented limitation for geometry
  outside that cone.
- Shadows are now soft: the shadow sampler uses hardware depth-compare
  (`compareOp = VK_COMPARE_OP_LESS`) with linear filtering, and
  `mesh.frag`'s `pcf()` takes a 3x3 tap around each sample — visibly
  softened shadow edges (feathered, not aliased), confirmed by cropping
  and zooming into a rendered frame.
- Verified visually: each object now casts two distinct shadow shapes (one
  per light) simultaneously, and both remain correctly shaped/positioned
  as the falling cube moves and the rig spins.
- Descriptor set 1's shadow-map binding became a `descriptorCount = 2`
  array (`sampler2DShadow shadow_maps[2]` in GLSL); `GlobalUbo` gained
  `light_space_matrices[2]` and `shadow_caster_count` alongside the
  existing per-light arrays.

### Visual-fidelity pass 3 — screen-space ambient occlusion (done)

- The renderer is now two render passes per frame instead of one: a
  geometry pass renders into an intermediate HDR-capable color target
  (`_scene_color_image`, `R16G16B16A16_SFLOAT` — headroom for a future
  tonemap pass) plus the depth buffer, both created with `SAMPLED_BIT` so
  a second pass can read them back as regular textures. (An earlier
  attempt used a single render pass with two subpasses and Vulkan input
  attachments — that doesn't work for SSAO, since input attachments only
  let a fragment shader read its own pixel, not the neighboring ones the
  AO kernel needs; caught before shipping, redone as two passes.)
- `shaders/post.vert`/`post.frag`: a fullscreen-triangle post-process pass
  computing depth-only (no normal buffer) screen-space ambient occlusion —
  an 8-tap kernel reconstructs/reprojects view-space positions using just
  the camera projection's 4 nonzero entries (`vre_math.hpp`'s general
  `mat4::inverse` was added but ultimately not needed for this — the
  analytic approach is cheaper and avoids a matrix multiply per fragment).
- Verified iteratively, not just once: the first pass at this produced
  solid black discs over each object (a real self-occlusion artifact from
  having no normal buffer to orient the kernel away from convex surfaces).
  Retuned radius/bias/strength and re-verified via a cropped, zoomed
  screenshot showing correctly-placed, subtle contact-shadow darkening at
  each object's contact with the ground — not a uniform tint, not a
  black-disc artifact.
- Known limitation, documented in `post.frag`: a fixed (non-rotated)
  kernel with no blur pass afterward shows some banding/noise rather than
  a perfectly smooth gradient — a per-pixel rotation texture + bilateral
  blur would clean this up as a natural follow-up.

### Visual-fidelity pass 4 — HDR tonemapping + bloom (done)

- `shaders/post.frag` gained an ACES filmic tonemap operator (Narkowicz's
  fit) applied to the final HDR color, replacing the swapchain format's
  implicit hard clip at 1.0 with a soft highlight rolloff.
- Bloom: rather than the usual downsample/blur-pyramid (which would need
  several more images and passes), this samples a wide Gaussian-weighted
  25-tap kernel directly on the full-resolution HDR scene-color buffer
  already bound for AO, keeping only each tap's above-threshold remainder,
  and adds it back before tonemapping — a deliberate single-pass scope
  trade-off (a real blur pyramid would give a wider, cheaper glow for the
  same tap count), documented in the shader.
- Verified in two steps: first with the scene's actual light intensities
  (no visible bloom — nothing in the demo scene happens to exceed the
  threshold prominently, so nothing to show), then by temporarily lowering
  the threshold and re-rendering, which produced a clearly visible soft
  glow around the point-light-lit areas — confirming the code path
  actually fires and works, not just "silently does nothing," before
  restoring the real HDR-only threshold.

### Visual-fidelity pass 5 — anti-aliasing (done)

- Rather than full MSAA (which needs multisampled color+depth images and a
  depth-resolve attachment — more Vulkan plumbing for comparatively lower
  payoff here), `post.frag` gained a compact single-pass edge-smoothing
  filter ("FXAA-lite"): detects contrast edges by luma across a 5-tap
  cross pattern and blurs one tap along the detected edge direction. A
  real, shipped-style AA technique, not a placeholder.
- Applied to the pre-tonemap HDR color — the only point in this
  single-pass architecture with access to neighboring texels — rather
  than the conventional post-tonemap LDR image; smooths the same geometry
  silhouette edges either way, since the aliasing comes from geometry
  coverage, not tonemapping.
- Verified by cropping/zooming the same region as an earlier AO
  screenshot and comparing: visibly smoother sphere silhouette edges
  (less staircase pixelation) with AA than without.

Not yet implemented (future steps, see `verdict.md`'s priority order for
the full list): frustum/occlusion culling + FPS measurement, Doxygen, ECS,
memory-leak verification, bonus features.

### Step 6/7 — the Interactive House Environment (done)

Two rooms (`assets/scenes/house_scene.json`, the demo's default scene) with
different lighting: Room A starts bright and warm-lit, Room B starts dark
and cool-lit. A hinged door (`door_hinge` parent + `door` child, so rotating
the parent pivots the door around its edge, not its center) opens/closes
with **E** near it. A light switch prop toggles Room B's light on/off with
**F** near it. A coffee machine emits a continuous stream of shrinking
steam-cube particles above it (`src/particles/particle_system.*`). First-
person **WASD**-move / arrow-key-look navigation, gravity-free (the player
is a floating camera, not a physics body — physics still runs for the
step-4 falling-cube/trigger demo objects in `demo_scene.json`).

- **A genuine bug, found and fixed here**: every wall in the house rendered
  visibly, but with its **outward-facing normal effectively pointing back
  into the wall** — the room-facing side of `wall_a_south`, dead ahead of
  the player and only ~1.5 units from Room A's own light, rendered as the
  darkest thing on screen instead of the brightest. Shadows were the first
  suspect (three separate shadow-frustum/bias fixes were tried and reverted
  — none of them touched it, because none of them were the actual cause).
  The real fix came from bisecting with throwaway shader debug output:
  outputting `frag_normal` as color showed every wall's normal pointing the
  *wrong* direction — not a per-mesh asset problem (the raw `.obj` files
  have correct outward normals, confirmed by inspection), and not the
  model-matrix math (confirmed algebraically: identity rotation + positive
  diagonal scale cannot flip an axis-aligned normal's sign, and a CPU-side
  `fprintf` of the light/wall/camera positions matched the JSON exactly).
  The actual cause: `mat4::perspective`/`orthographic` (`vre_math.hpp`)
  negate their Y row to flip into Vulkan's NDC (+Y down) convention, but
  neither the main mesh pass nor the shadow pass compensated with a
  negative-height viewport — so every CCW-authored triangle's winding
  mirrors to CW by the time it reaches the rasterizer. Both pipelines had
  `frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE`, so back-face culling was
  silently keeping each box's *inner* (wrong, inward-pointing-normal) face
  and discarding the correct outward one — invisible on rounded/complex
  shapes like the demo scene's spheres (still just "some face" facing you,
  shaded plausibly-if-subtly-wrong), glaring on thin architectural walls
  viewed from inside a room. Fixed by flipping `frontFace` to
  `VK_FRONT_FACE_CLOCKWISE` in both pipelines (`src/renderer/renderer.cpp`).
  Verified with the same debug-normal-as-color technique (walls now show
  six distinct, correctly outward-pointing colors) and with real screenshots
  before/after (`/tmp/vre_fix_normal.png`, `/tmp/vre_final_verified.png`)
  showing the room going from uniformly dim/gray to properly bright and
  shadowed. Also re-verified `demo_scene.json` afterward to confirm no
  regression — the checkered cube and both spheres still render correctly
  (and the metal sphere's specular highlight is, if anything, more correct
  now).

### Bonus — Sound System (done)

A real, from-scratch playback path, not a stub: `src/audio/wav_loader.{hpp,cpp}`
hand-parses uncompressed PCM WAVE files (8-bit or 16-bit, mono or stereo —
same "no third-party library" rule as the OBJ/TGA/JSON loaders), and
`src/audio/mixer.{hpp,cpp}` is a platform-independent software mixer: a
fixed pool of 32 voices, each independently resampled (linear interpolation)
to the output device's rate and mono-duplicated-to-stereo as needed, summed
in a float accumulator and clipped once at the end so multiple loud sounds
overlapping doesn't double-saturate.

- **Backend**: `src/platform/linux/audio_linux.{hpp,cpp}` — direct ALSA
  (`libasound`) PCM playback, the same category of system library the
  subject names XCB as an example of. A dedicated writer thread wakes up
  once per ~20ms ALSA period, asks the mixer for that many frames, and
  writes them — so a slow render frame never causes an audio dropout, and
  the sound card's timing never blocks the render loop.
- **Assets**: `assets/sounds/` — a soft looping ambient room hum, a UI
  click, and a door creak, all procedurally generated (matching this
  project's existing pattern for the house demo's textures) rather than
  sourced third-party audio.
- **Wired into the house demo**: the ambient hum loops continuously from
  startup; the door creak plays on every open/close (**E**); the click
  plays on every light-switch toggle (**F**).
- **Verified live**, not just "it compiles": running the demo shows a real,
  correctly-formatted (`s16le 2ch 44100Hz`, matching the mixer's own output
  exactly) active audio stream registered with the system's audio server —
  confirmed via `pactl list sink-inputs` showing an uncorked, unmuted
  `very_real_engine_demo` stream for the full duration of a run. (Directly
  recording and inspecting the stream's actual sample data wasn't possible
  in the environment this was developed in — no audio-capture permission —
  so "does the OS accept and route it exactly as intended" is the strongest
  check available there; the format, volume, and mute state all being
  exactly right and the stream staying open for the whole session is strong
  evidence the mixer is producing real, ongoing audio, not silence.)
- **No macOS backend yet**: `src/audio/null_audio_system.cpp` provides a
  silent, always-succeeds-at-loading-but-never-plays fallback so a macOS
  build still links and runs correctly (see the Makefile) rather than
  failing to link at all. A real CoreAudio backend would follow the same
  `AudioSystem` interface — the mixer and WAV loader are already fully
  platform-independent and need no changes to support one.
- A degraded/no-device environment (no sound card, a sandboxed session with
  no audio permission) is handled the same way the renderer handles a
  missing GPU feature: `AudioSystem::initialize()` returns `false` and the
  demo runs on, silently, rather than aborting — see `main.cpp`.

### Controls

- **WASD** — move, **arrow keys** — look (house scene)
- **E** near the door — open/close it (also plays a door-creak sound)
- **F** near the light switch — toggle Room B's light (also plays a click)
- **H** — toggle the "rig" scene node (and its child) on/off (demo scene)
- Close the window to quit

## Building

**Linux is the actual submission target** (this is what gets graded). macOS
is also supported, purely so the engine can be developed/iterated on without
a Linux box — see the macOS section below and its caveats.

Dependencies (Linux):

```sh
sudo apt install libvulkan-dev libx11-dev
# one shader compiler, either is fine:
sudo apt install glslang-tools     # provides glslangValidator
# or
sudo apt install glslc             # provides glslc (preferred if available)
```

Dependencies (macOS, via Homebrew):

```sh
brew install vulkan-headers vulkan-loader molten-vk shaderc
```

Build (either platform):

```sh
make
```

This produces two things at the repo root: the `VeryRealEngine` static
library (`libVeryRealEngine.a`) and a `very_real_engine_demo` executable that
links against it — the subject requires the final product to be a library,
so everything except `src/main.cpp` lives in the library, not the demo
binary. `make` also compiles `shaders/*.vert`/`*.frag` to `shaders/*.spv` in
place, and only rebuilds/re-links what actually changed (object files carry
`-MMD` header dependencies, tracked under `obj/`).

Other rules: `make clean` (remove object files), `make fclean` (also remove
the library, the demo binary, and compiled shaders), `make re` (`fclean` +
`all`).

Run (from the repo root, so it finds `shaders/*.spv` and `assets/` next to
the binary):

```sh
./very_real_engine_demo
```

If no shader compiler is installed, `make` fails on the shader-compilation
step with an explicit error. Install `glslc` or `glslangValidator` and
re-run `make` to fix that.

### macOS notes

There is no native Vulkan driver on macOS, so this uses
[MoltenVK](https://github.com/KhronosGroup/MoltenVK) (a Vulkan-over-Metal
translation layer) and a Cocoa windowing backend
(`src/platform/macos/window_macos.mm`), instead of Xlib. The Makefile links
directly against MoltenVK's own `libMoltenVK.dylib` (which implements the
full Vulkan API itself) rather than going through the generic Vulkan
loader, since Homebrew's `molten-vk` formula doesn't ship an ICD manifest
for the loader to discover it by — one less moving part for local dev.

This has been verified to actually build and run (not just compile): it
picks the Apple Silicon GPU (`Renderer: using physical device "Apple M1"`),
loads the OBJ meshes and JSON scene, and stays in a live render loop rather
than aborting.

Known gaps/caveats, since MoltenVK is not a fully conformant Vulkan
implementation:

- If your toolchain resolves a beta/mismatched macOS SDK by default (check
  with `xcrun --show-sdk-path`; symptom: linker errors like `tapi error:
  malformed file` / `unknown architecture` on framework `.tbd` files), the
  Makefile already pins both compile and link steps to
  `` `xcrun --sdk macosx --show-sdk-path` `` to route around it — this is a
  toolchain quirk, not something in this engine's control.
  `VK_KHR_portability_subset` and `VK_KHR_portability_enumeration` are
  checked for availability at runtime before being requested (MoltenVK,
  linked directly as here, doesn't advertise either — requesting them
  unconditionally would fail instance/device creation outright).
- Validation layers (`VK_LAYER_KHRONOS_validation`) aren't installed by the
  packages above; the engine runs without them (same fallback path Linux
  uses when they're absent there too), just with less Vulkan-side error
  checking.
- This is a development convenience, not the graded target. Fine-tune and
  do the final verification (memory/crash checks, the 60 FPS requirement,
  visual correctness) on the actual Linux submission environment before
  submitting.

### Generating the Doxygen documentation

```sh
doxygen Doxyfile
```

Open `docs/html/index.html` in a browser. `Doxyfile` has
`EXTRACT_ALL`/`EXTRACT_PRIVATE`/`EXTRACT_STATIC` enabled, and every public
class, struct, method, and field across `src/**/*.hpp` carries a real
`@brief`/`@param`/`@return` comment (not just relying on those flags to
paper over gaps) — confirmed generating with **zero warnings on a real,
from-scratch run** (`doxygen` 1.9.8, the current Ubuntu/apt version — the
one actually verified, not just the one this file happened to be authored
against). Only `src/platform/macos/window_macos.mm` is excluded from the
Doxygen input (Objective-C++; Doxygen's C++ parser doesn't handle
`@interface`/`@property` syntax, and that file is macOS-dev-only, not part
of the graded Linux build).

`OUTPUT_DIRECTORY` is a single-level `docs` (not `docs/doxygen`) deliberately:
this Doxygen version can create one missing directory level on its own but
not two nested ones, so on a truly fresh clone (where `docs/` doesn't exist
yet — it's gitignored) a nested `docs/doxygen` output path made the
documented one-line `doxygen Doxyfile` command fail outright with "Output
directory 'docs/doxygen' does not exist and cannot be created". Caught by
actually re-running the documented command from a clean clone rather than
trusting that it still worked after the `Doxyfile` was last touched.

### Verifying memory safety / crash safety

```sh
make SANITIZE=1 -j        # separate obj-sanitize/ + *-sanitize binary, doesn't touch the normal build
LSAN_OPTIONS=suppressions=lsan_suppressions.txt ./very_real_engine_demo-sanitize
```

Builds an AddressSanitizer + UndefinedBehaviorSanitizer-instrumented copy
of the demo, to verify Chapter III's "no unexpected termination" / "no
memory leaks" requirements with real tooling rather than by inspection.

**`lsan_suppressions.txt`** exists for one specific, verified reason: opening
the sound system's ALSA "default" device (`src/platform/linux/audio_linux.cpp`)
internally routes through PipeWire's ALSA compatibility plugin on this kind
of system, which does its own one-time plugin/config loading inside
`snd_pcm_open` — allocations this project has no API to free (confirmed by
reading every leak's full stack trace before adding the suppression: every
one terminates inside `snd_pcm_open`, never reaching any function this
project defines). `AudioLinux::destroy()` already calls
`snd_config_update_free_global()` (ALSA's own documented cleanup call) for
the part of this that *is* reachable; the suppression file covers the
remainder, which lives inside PipeWire's own internals. Without it, a real
run reports leaks whose stacks are 100% third-party library code — see the
file's own header comment for the full accounting. This is a well-known
category of ALSA/PulseAudio/PipeWire "leak" under valgrind/ASan on any
application that opens a "default" PCM device on a PipeWire-managed system,
not something specific to this codebase.

**macOS-specific caveats** (this is a dev-convenience build; do the real
verification on Linux, the actual submission target):
- AddressSanitizer's leak detector (LSan) isn't supported on macOS at all
  (`detect_leaks is not supported on this platform` — an ASan platform
  limitation, not specific to this project). Leak verification needs Linux.
- On this machine, the full ASan+UBSan combination hangs/spins at ~100% CPU
  before producing any output, specifically when both are enabled together
  against the MoltenVK/Metal backend. Isolated by testing UBSan alone
  (`-fsanitize=undefined`, no ASan): that runs cleanly end to end — stable
  ~60 FPS, zero undefined-behavior findings over a full run — so this is an
  ASan-plus-Metal/MoltenVK interaction specific to this platform, not a
  defect in the engine. Linux's native Vulkan ICD doesn't have MoltenVK in
  the picture, so this is not expected to reproduce there.

### Debug/verification environment variables

None of these affect normal interactive use (every one is optional, read
once at startup) — they exist so the demo's rendering and interaction logic
can be exercised and screenshotted without a live keyboard/mouse (see
`verdict.md` for why: a real-world case of verifying this engine in an
environment with no window-permission/input access).

| Variable | Effect |
|---|---|
| `VRE_SCREENSHOT_PATH` | Dumps the presented frame to this path as a PPM once `VRE_SCREENSHOT_FRAME` (default 60) frames have rendered. See `Renderer::capture_screenshot()`. |
| `VRE_SCREENSHOT_FRAME` | Which frame to capture (see above). |
| `VRE_CAMERA_POS` | `"x,y,z"` — overrides the starting camera position. |
| `VRE_CAMERA_YAW` / `VRE_CAMERA_PITCH` | Overrides the starting camera orientation (radians). |
| `VRE_FORCE_DOOR_OPEN` | House scene: starts with the door already fully open (skips the lerp animation). |
| `VRE_FORCE_LIGHT_ON` | House scene: starts with Room B's light already on. |
| `VRE_FORCE_HIDE_NODE` | Calls `Scene::set_visible(name, false)` at startup — the same API the H-key binding uses, for verifying hide-parent-hides-children without a keypress. |
| `VRE_AUTO_YAW_SPEED` | Continuously pans the camera at this many radians/second — lets the motion-blur post-process effect (which needs real camera movement) be exercised and screenshotted. |
| `VRE_EXIT_AFTER_FRAME` | Exits cleanly (same shutdown path as pressing Escape) after this many frames, instead of running until the window closes — needed for tools like `leaks --atExit` that require a normal process exit. |
| `VRE_PRESENT_MODE=immediate` | Forces `VK_PRESENT_MODE_IMMEDIATE_KHR` (bypasses vsync) — used to measure the engine's actual FPS ceiling above the display's refresh rate. |

## Layout

```
VeryRealEngine/
  Makefile
  Doxyfile
  shaders/            GLSL sources (compiled to SPIR-V at build time)
  src/
    main.cpp          demo entry point / render loop (NOT part of the library)
    math/              dependency-free vec3/mat4/AABB/Frustum
    ecs/               generic entity-component-system core (registry.hpp)
    platform/          windowing abstraction + per-OS backends
    renderer/          Vulkan instance/device/swapchain/pipeline/frame loop
    physics/           rigid bodies, collision, gravity/friction, triggers
    scene/             JSON-loaded scene, implemented on top of ecs/ (components + systems)
    particles/         CPU particle emitter/simulation
    assets/            OBJ/MTL, TGA, and JSON loaders
```

Everything under `src/` except `main.cpp` is compiled into the
`VeryRealEngine` library target; `main.cpp` is the one file that's specific
to this demo rather than the engine itself.
