# Module Overviews

Libft exposes a large surface area with many cooperating modules. This document summarizes the purpose of each subsystem, key invariants the implementation relies on, and the error-reporting conventions callers must follow. Each section points back to headers in the repository and highlights the macros or hooks that change behaviour at compile time.

## Libft (core C utilities)
- **Design goals:** Provide a drop-in replacement for common libc helpers with deterministic allocation semantics and consistent `ft_errno` integration. Emphasize portability by routing platform-specific logic through the `Compatebility` module.
- **Key invariants:**
  - Callers must initialize library global state before invoking helpers that rely on environment or locale APIs.
  - Safe string and memory variants guarantee destination buffers are zeroed on failure and never write past their bounds.
  - Configuration macros declared in `Basic/basic_config.hpp` must be honored by all translation units; disabling a feature removes the corresponding symbols from the umbrella headers.
- **Error reporting:** Functions set `ft_errno` on failure and return sentinel values (`nullptr`, `0`, or error enums). Callers should immediately capture the error via `ft_errno` or helper wrappers like `ft_get_error_string`.

## CMA (custom memory allocation)
- **Design goals:** Offer deterministic allocation primitives with tracking hooks so tests can inject failures, enforce allocation limits, and inspect outstanding allocations.
- **Key invariants:**
  - CMA must be initialised before use so allocator bookkeeping structures exist.
  - Allocation hooks run under an internal mutex; long-running callbacks should be avoided to keep contention low.
  - Thread-safety toggles affect the entire process and should be set during program startup.
- **Error reporting:** Allocation helpers return `nullptr` on failure and set `ft_errno` to codes defined in `Errno/errno.hpp` (`FT_ENOMEM`, `FT_EINTR`, etc.). Statistics helpers report errors through the `_error_code` field on the context structs.

## System_utils
- **Design goals:** Abstract platform-specific facilities such as locale handling, file descriptors, and process management behind a single facade so Libft and higher-level modules behave consistently across operating systems.
- **Key invariants:**
  - Shims in `Compatebility` encapsulate all direct OS calls; `System_utils` performs argument validation and converts errors to `ft_errno` values.
  - Locale helpers respect `LIBFT_ENABLE_LOCALE_HELPERS`; when disabled, the module must fall back to byte-wise comparisons and case folding.
  - Resource wrappers (files, sockets) own their handles exclusively and close them in destructors to avoid leaks.
  - `ft_file` and the higher-level stream wrappers treat the descriptor returned by `operator int()` as a borrowed view. Move operations transfer ownership and clear the source to `-1`; callers must either duplicate the descriptor or call `close()` before handing it to APIs that expect to assume lifetime management so the wrapper does not double-close it later.
- **Error reporting:** Each helper translates native error codes to the shared registry in `Errno/ERROR_CODE_REGISTRY.md`. Callers can rely on `ft_errno` remaining untouched when operations succeed.

## Networking
- **Design goals:** Provide synchronous and asynchronous networking primitives that share error handling conventions with the rest of the library while bridging differences between POSIX sockets and Winsock.
- **Key invariants:**
  - Socket objects manage lifecycle through RAII, guaranteeing `close`/`closesocket` executes once per descriptor.
  - Event-loop integrations respect non-blocking invariants and never invoke user callbacks while holding internal locks.
  - TLS and HTTP helpers require initialization of global networking state via `ft_socket::initialize` before use.
- **Error reporting:** Functions return boolean success/failure while storing detailed error information in `_error_code` or via `ft_errno`. Windows-specific failures must be translated through `WSAGetLastError()` before surfacing to callers.

## Logger
- **Design goals:** Deliver structured, asynchronous logging with minimal overhead while allowing users to customize sinks and formatting.
- **Key invariants:**
  - The logging queue retains ownership of messages until sinks confirm delivery or drop policies force eviction.
  - Background worker threads exit cleanly during shutdown sequences triggered by `ft_log_shutdown`.
  - Formatting helpers avoid throwing exceptions; instead they store formatting issues in `_error_code`.
- **Operational helpers:** Remote sink health probes run on a configurable interval via `ft_log_enable_remote_health` so callers can detect broken connections through `ft_log_get_remote_health` without waiting for application logs to fail. Scoped context guards (`ft_log_context_guard`) push per-thread metadata that is automatically prepended to plain logs and merged into structured payloads, keeping correlation identifiers attached without plumbing them through every call.
- **Error reporting:** All logging entry points propagate failures through `_error_code` fields on logger instances. Sinks that fail must translate backend-specific errors into the canonical registry before returning.

## Storage
- **Design goals:** Offer embeddable persistence layers (in-memory key/value, on-disk stores) that integrate with Libft error handling and threading primitives.
- **Key invariants:**
  - Storage engines guard shared state with reader/writer locks to ensure writes see consistent snapshots.
  - Background maintenance (TTL pruning, compaction) never blocks foreground read/write operations.
  - Transactions enforce atomicity guarantees even when mixing in CMA-backed allocations.
- **Error reporting:** Public APIs return boolean results and write detailed error codes to `_error_code` on the store instance. Filesystem failures bubble through `System_utils` so `ft_errno` reflects the original cause.

## Template utilities
- **Design goals:** Provide containers and algorithms mirroring the standard library while exposing explicit error channels instead of exceptions.
- **Key invariants:**
  - Containers must not leak memory when operations fail; all partial mutations roll back before returning to the caller.
  - Iterator validity follows documented rules in each header; callers must respect ownership semantics when transferring nodes.
  - Allocator-aware code paths accept CMA allocators without relying on implicit global state.
- **Error reporting:** Template helpers favor returning lightweight result objects that store success/failure flags alongside `_error_code`. When a helper cannot update `_error_code`, it sets `ft_errno` directly.

## Voxel
- **Design goals:** Generate biome-aware voxel terrain and chunk meshes behind the `GAME_USE_VOXEL_REGION_BACKEND` gate without making the rest of the game stack care about the implementation details.
- Terrain configuration classes expose `serialize_json(ft_string&)` and `save_json_file(...)`. File modes are create-only, replace/truncate, and append; append writes newline-delimited JSON records so existing records remain intact.
- **Key invariants:**
  - Biome selection and heightmap generation are deterministic for a given seed and world origin.
  - Tree templates are reusable presets and are checked for footprint safety before placement.
  - Mesh generation must preserve visible-face correctness while remaining greedy enough to keep chunk meshes compact.
- **Error reporting:** Terrain generation and chunk mesh helpers return `FT_ERR_*` codes for allocation or chunk I/O failures, and public helpers avoid leaking partially generated state when an operation aborts.

## Observability guidance
Each module integrates with the canonical error-code registry. When adding new functionality:
- Prefer reusing existing codes so cross-module tooling can reason about outcomes.
- Document any module-specific invariants or synchronization requirements in this file to keep callers aligned.
- Update relevant tests or add new ones to demonstrate the documented behaviour, especially when invariants are enforced through runtime checks.

## Layering reference
For a coarse architecture map that groups modules by responsibility, see [Docs/module_layering.md](module_layering.md).
For the exact direct dependency graph, see [Docs/module_dependency_graph.md](module_dependency_graph.md).
