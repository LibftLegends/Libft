# GNU Make Build-System Redesign for Libft and ft_vox

## Purpose

This document is an implementation handoff for the Luna model. It covers the
Libft repository in this directory and the ft_vox/Minecraft project in the
parent directory. The implementation must remain based on GNU Make. Do not add
CMake, Ninja, Meson, or another build-system generator.

Before changing either repository, read and follow its `AGENTS.md`. Preserve
all existing user changes and do not mix source-code refactors into this build
work.

## Required end state

Libft's canonical build must have one dependency graph:

```text
make
  |
  v
one dependency graph
  |
  v
individual .o files scheduled across all available jobs
  |
  v
module .a archives
  |
  v
Full_Libft.a
```

The root Make process must know every object prerequisite. It must not run a
separate `make -q` pass and must not delegate canonical compilation to one
recursive Make process per module. GNU Make should schedule ready objects from
different modules across the same job pool.

ft_vox should keep its own root graph, but include Libft's flattened graph when
building the in-tree Libft variant. This gives one scheduler visibility into
both ft_vox and Libft objects. A temporary one-pass recursive compatibility
path is acceptable during migration, but it is not the final architecture.

## Audit findings

The original analysis is substantially correct for the current repositories.

### Confirmed in Libft

- The root `Makefile` creates one recursive `make -q` check for each module and
  later invokes another recursive Make process for modules classified as
  stale. Dependency graphs are therefore parsed and inspected twice.
- Progress state has session-specific initialization filenames but shared
  files such as `stale_modules`, `total`, `completion_count`, and static lock
  directories. Concurrent builds are not isolated.
- `mk/progress.sh` waits forever on a directory lock with no owner or stale-lock
  validation.
- `mk/acquire_directory_lock.sh` removes a lock after a retry limit without
  proving that its owner is dead.
- `mk/run_module_build.sh` removes the shared progress lock from its exit trap
  even when that process did not acquire the lock. This can remove another
  process's live lock.
- Module recipes count all existing `.o` files using `find | wc -l`. On an
  incremental build this reports existing objects rather than objects built by
  the current invocation.
- Most module object rules list every module `$(HEADERS)` entry while also using
  `-MMD -MP`. A change to one listed header can rebuild an entire module even
  when compiler-generated dependency files identify fewer consumers.
- The test build adds another stale-object counting and lock-based progress
  subsystem, so fixing only the root Makefile is insufficient.
- Root aggregate archives have `FORCE` prerequisites and are rebuilt on no-op
  invocations.

### Confirmed in ft_vox

- Object recipes run directory creation, compilation, dependency rewriting,
  recursive `find`, `wc`, log creation, lock acquisition, log replay, and log
  deletion per object. This is especially expensive through Git Bash on
  Windows.
- Progress counters count existing objects and therefore do not represent the
  work required by the current invocation.
- Libft archive targets depend on `FORCE` and always start another Make process.
- ft_vox already uses `-MMD -MP`, but then rewrites each dependency file using
  Perl on non-Windows platforms.

### Important qualifications

- GNU Make already supports parallel object compilation and coordinates
  recursive builds through its jobserver. The problem is duplicate scanning,
  process and shell overhead, unsafe custom coordination, and the root
  scheduler's lack of visibility into all object edges.
- Accurate progress must not be implemented by counting files in object
  directories. GNU Make does not expose a native exact “number of stale edges”
  counter suitable for these recipes. Prefer one clear line per command and
  native output synchronization over a percentage bar.
- Removing `FORCE` from a recursive archive target without exposing its real
  prerequisites would cause stale builds. Flatten the graph first, or retain a
  temporary always-invoked one-pass submake until the real prerequisites are
  visible.

## Design principles

1. Make's dependency graph is the only stale-work database.
2. Compiler-generated `.d` files are the authority for included headers.
3. One configuration has one isolated object root.
4. One compile edge launches the compiler, not a chain of bookkeeping tools.
5. Every real file target has real file prerequisites and no `FORCE` edge.
6. Parallel output uses GNU Make's `--output-sync`, not filesystem locks.
7. Module archives remain available, but they are products of the global graph,
   not independent recursive builds.
8. A no-op build performs no compilation, archiving, or linking.

## Libft implementation design

### 1. Convert module descriptions into includable manifests

The existing `mk/modules/<Module>.mk` files are close to source manifests, but
they assign generic names such as `SRCS`, `HEADERS`, `TARGET`, and
`MODULE_CFLAGS_EXTRA`. Generic variables collide when all manifests are
included by one Make process.

Convert each manifest to namespaced data. For example:

```make
BASIC_SOURCES := \
    basic_atoi.cpp \
    basic_bzero.cpp
BASIC_CPP_FLAGS :=
BASIC_MM_SOURCES :=

GAME_SOURCES := \
    game_world.cpp \
    game_voxel_chunk.cpp
GAME_CPP_FLAGS :=
GAME_MM_SOURCES :=
```

Keep source paths relative to their module and add the module directory when
instantiating the graph. Preserve special cases explicitly:

- Objective-C++ `.mm` sources in platform modules;
- vendored C sources in Lua;
- module-specific definitions such as
  `GAME_USE_VOXEL_REGION_BACKEND`;
- generated files, if any;
- modules with source files in nested directories;
- test-only definitions and sources.

Do not use `wildcard` to discover production source files. Explicit manifests
make source additions and removals reviewable and ensure Makefile changes
invalidate the correct graph.

### 2. Add a global module-rule generator

Create an includable Make fragment, for example
`mk/global/module_rules.mk`, that defines an `eval`/`call` template. The
template receives a module key, module directory, archive name, source lists,
and extra flags. It must derive:

- release objects under a configuration-specific root;
- debug objects under a separate root;
- test objects with `LIBFT_TEST_BUILD` under a separate root;
- `.d` files for every object family;
- the release, debug, and test module archives.

Illustrative shape only:

```make
define LIBFT_REGISTER_MODULE
$(1)_RELEASE_OBJECTS := ...
$(1)_DEBUG_OBJECTS := ...
$(1)_TEST_OBJECTS := ...

$$(MODULE_ARCHIVE_$(1)): $$($(1)_RELEASE_OBJECTS) $$(MODULE_MANIFEST_$(1))
	$$(AR) rcs $$@ $$($(1)_RELEASE_OBJECTS)
endef
```

Every registered module must expose `MODULE_MANIFEST_<KEY>` as the path of the
manifest that defines its source and object lists. The manifest is a real
archive prerequisite, not merely a file parsed by Make. This ensures that
adding, removing, or renaming a source makes the module archive stale even when
none of the remaining object files changed.

Do not pass unfiltered `$^` to the archiver after adding manifest or
configuration prerequisites: `$^` would include `.mk` files. Archive recipes
must use the module's exact object variable or explicitly filter prerequisites
to supported object-file suffixes.

Luna must validate the expanded database with `make -pn` while developing.
Avoid clever nested expansion that obscures targets or silently drops flags.

Use object paths that include module identity and configuration, for example:

```text
build/libft/<configuration>/Basic/basic_atoi.o
build/libft/<configuration>/Game/game_world.o
```

The configuration identity must change when ABI-affecting compiler options or
definitions change. Prefer explicit configuration directory names for the
supported modes. If arbitrary caller-supplied `COMPILE_FLAGS` must remain
supported, retain a stable fingerprint, but compute it once per invocation and
do not invoke a shell tool per object.

Configuration fragments must participate in the graph at the level they
affect. A fragment that changes compiler flags, language mode, generated-header
selection, or target definitions must invalidate the affected objects (or must
change their configuration-specific object path). A fragment that changes
only a module's archive membership must invalidate that module archive. Do not
attach every Makefile in the repository to every object; define focused lists
such as `LIBFT_COMPILE_CONFIG_INPUTS`, `LIBFT_ARCHIVE_CONFIG_INPUTS`, and
module-specific configuration inputs.

The module manifest is the focused configuration input for its module. The
global graph makes it a prerequisite of that module's release, debug, test, and
efficiency objects as well as its archives. This covers module-specific
definitions and source-list changes without launching a per-module hashing or
shell process during graph parsing.

Archive targets also depend on the focused archive configuration input
(`mk/build_config.mk` in standalone Libft, or the corresponding prefixed path
when the graph is included by ft_vox). This ensures changes to the archiver or
archive flags invalidate the archive without making those configuration files
members of the archive.

### 3. Use one global compile-rule family

Define global pattern or static-pattern rules for C++, C, and Objective-C++.
Each compile command must:

- create its target directory through an order-only directory prerequisite;
- compile exactly one source;
- emit its dependency file directly;
- identify the target explicitly with `-MT`;
- print one concise source-to-object status line.

Use flags equivalent to:

```make
-MMD -MP -MF $(@:.o=.d) -MT $@
```

This should eliminate the normal need for `FIXDEP`. First verify generated
dependency files on Windows Git Bash, Linux, and macOS, including paths with a
drive letter. If a compiler genuinely emits unusable paths, fix the flags or
path inputs at generation time. Do not run Perl over every `.d` file unless a
reproducible cross-platform test proves it unavoidable.

Include all generated dependencies once near the end of the root graph:

```make
-include $(ALL_DEPENDENCY_FILES)
```

For performance, `ALL_DEPENDENCY_FILES` may be selected from the active
configuration family. Standalone release, debug, test, and test-debug goals
must not parse unrelated configuration databases; an embedded ft_vox graph
must retain the release dependencies needed by the parent link graph.

Remove broad `$(HEADERS)` prerequisites from generic object rules. Add an
explicit header prerequisite only for a generated or force-included header
that the compiler cannot report.

### 4. Build module and aggregate archives from real prerequisites

Each module archive must depend on its exact object list and on the manifest
that defines that list. Depending on the manifest is required for source
removal: changing `Basic.mk` from `foo.cpp bar.cpp old.cpp` to
`foo.cpp bar.cpp` may not change either remaining object timestamp, but it must
still make `Basic.a` stale.

Recreate archives deterministically so removed sources do not leave stale
members. `ar rcs` does not necessarily remove members that no longer appear in
the current object list; therefore build a temporary archive from the exact
object variable and replace the target only after success, or remove the old
archive immediately before a guaranteed archive command while handling failure
correctly. Do not use raw `$^` when non-object prerequisites are present.

The final archive must depend on all module archives. Preserve the current
platform-specific aggregation behavior (`libtool -static` on macOS and MRI
`ar -M` where supported), but write to a temporary output and replace
`Full_Libft.a` only after successful completion. This prevents a failed build
from leaving a partial final archive.

Remove `FORCE` from release, debug, test, module, and aggregate real-file
targets once their true prerequisites are present.

Add an archive-integrity test that verifies:

- no object member is duplicated;
- deleting a source from a manifest removes its former archive member;
- a failed aggregation leaves the previous valid archive intact;
- two consecutive no-op builds leave archive timestamps unchanged.

The canonical implementation exposes this check as `make archive-integrity`.
It validates every module archive and `Full_Libft.a` against the current object
graph, and performs the failed-temporary-replacement check without passing the
aggregate object list through a Windows command line.

The gate first settles any legitimate downstream archive rebuild caused by the
source-removal probe, then invokes the root `all` target through `$(MAKE)` and
compares archive timestamps before and after that invocation. This keeps the
recursive check jobserver-aware and makes a no-op failure distinguishable from
the intentional probe rebuild.

### 5. Delete custom stale scanning and lock-based progress

After the global graph is active, remove the root check stamps, scan stamps,
`stale_modules`, `total`, and progress initialization targets. Remove all call
sites for:

- `mk/progress.sh`;
- `mk/run_module_build.sh`;
- `mk/count_stale_objects.sh`;
- `mk/acquire_directory_lock.sh`.

Delete those scripts only after `rg` confirms no remaining references.

Use GNU Make output synchronization. Add a documented opt-in/default policy
that works with the minimum supported GNU Make version, for example invoking
CI and developer parallel builds with:

```text
make --output-sync=target -j<jobs>
```

Do not maintain numeric per-file progress. Print the module and source on each
actual compile edge. This means a five-object incremental build prints five
compile lines, which is both stable and truthful.

### 6. Flatten the Libft test graph too

The test build currently has separate stale counting, output locks, and several
object families. Bring test objects into the same root dependency graph and
keep distinct object roots for:

- normal tests;
- debug tests;
- efficiency/performance tests;
- sanitizers or coverage modes that change compiler flags.

The test executable must depend on its exact test objects and the exact Libft
test archive. Test fixtures copied or generated during a build need explicit
file targets, not side effects hidden inside unrelated recipes.

Keep test filtering and runtime behavior unchanged. This task changes how test
binaries are built, not which tests execute.

### 7. Preserve module-local compatibility entry points

Developers may still run `make -C Modules/Basic`. Preserve this temporarily by
turning module Makefiles into thin wrappers that invoke the root graph with a
specific module target. They must not contain a second independent set of
source lists or recipes.

Example intent:

```make
all:
	$(MAKE) -C ../.. Modules/Basic/Basic.a
```

Mark wrapper targets phony, forward the jobserver by using `+$(MAKE)`, and do
not add `-j` inside wrappers. The root graph remains canonical.

## ft_vox implementation design

### 1. Simplify native ft_vox object recipes

Replace the per-object shell blocks with direct compile rules. Remove:

- `.ft_vox_build_*.log` files;
- `.ft_vox_output_lock`;
- `find | wc -l` counters;
- per-object log replay;
- dependency-file Perl rewriting if `-MF/-MT` validation succeeds.

Keep separate normal, debug, test, coverage, and other flag-changing object
roots. Make directory targets order-only prerequisites.

The parent graph must include an invocation-level configuration identity in
its native object roots. The identity covers effective compiler flags,
optimization/debug/coverage/LTO/PGO settings, and detected optional features;
it is computed once while parsing Make, not once per object. Link targets must
also depend on the focused compiler/configuration fragments so link settings
cannot silently leave an executable stale.

Preserve `.cpp` and macOS `.mm` compile rules and all existing platform link
libraries.

### 2. Include Libft's graph instead of recursively building it

Expose Libft's global graph through a fragment that can be included from either
Libft's root Makefile or ft_vox's root Makefile. The fragment must accept a
prefix/configuration interface rather than assuming `CURDIR` is Libft.

ft_vox should configure an isolated Libft variant containing its required
definitions and flags, including `GAME_USE_VOXEL_REGION_BACKEND`, and use
archive names/object roots that cannot collide with standalone Libft builds.
The module archives and aggregate archives must include the same invocation
configuration identity as their object roots (for example,
`Basic_ft_vox_cfg<fingerprint>.a` and
`Full_Libft_ft_vox_cfg<fingerprint>.a`). This prevents two optimization,
sanitizer, or feature configurations from racing over one archive pathname.
The parent link targets must consume those configuration-specific archive
names, and parent `fclean` must remove only the selected configuration's
archives and object root.

Target result:

```text
ft_vox make
  |
  +-- ft_vox .o edges --------------------+
  |                                       |
  +-- Libft .o edges across all modules --+--> Libft module archives
                                                  |
                                                  v
                                            Full_Libft.a
                                                  |
                                                  v
                                              ft_vox link
```

The same GNU Make scheduler can then compile a ready ft_vox source while other
jobs compile ready Libft sources. Remove `$(LIBFT_FULL_LIB): FORCE` and the
recursive `$(MAKE) -C Libft` recipes only after this included graph works.

Avoid passing one giant quoted `COMPILE_FLAGS` string between builds. Define
shared warning/platform flags in Make fragments and append target-specific
definitions through target-specific variables.

### 3. Preserve standalone behavior

Both commands must remain supported:

```text
cd Libft && make -j
cd Minecraft && make -j
```

The Libft fragment must therefore separate graph declaration from public root
targets. Including it in ft_vox must not redefine `all`, `clean`, `tests`, or
other parent targets.

Cleaning must be scoped. `make clean` in ft_vox may remove the ft_vox-selected
Libft build root, but must not recursively remove unrelated standalone Libft
configurations. `make clean` in Libft must not remove ft_vox objects outside
Libft.

Libft's clean targets may also remove legacy object roots and the obsolete
`Test/.libft_progress` state directory left by the pre-flattened build. They do
not remove the parent project's objects or the selected ft_vox configuration.

## Recommended file layout

The exact names may change, but keep responsibilities separated:

```text
Libft/
  Makefile                         public standalone targets
  mk/global/config.mk              tools, platforms, configurations
  mk/global/modules.mk             module registration list
  mk/global/module_rules.mk        object/archive rule generator
  mk/global/compile_rules.mk       C/C++/Objective-C++ rules
  mk/global/aggregate_rules.mk     Full_Libft archives
  mk/global/test_rules.mk          test graph
  mk/modules/*.mk                  namespaced source manifests

Minecraft/
  Makefile                         ft_vox public targets
  mk/compiler.mk                   shared flags and dependency flags
  mk/objects.mk                    ft_vox object graph
  mk/libft.mk                      configures/includes Libft graph
```

Do not duplicate source lists between module wrappers and manifests.

## Migration sequence for Luna

### Phase 0: Capture a baseline

Record on Windows, Linux, and macOS where available:

- clean standalone Libft build command and elapsed time;
- no-op standalone Libft build command and elapsed time;
- one `.cpp` touch rebuild behavior;
- one private header touch rebuild behavior;
- full Libft tester result;
- clean/no-op/one-source ft_vox build behavior;
- archive member lists and executable names;
- compiler, GNU Make, and archiver versions.

Do not use baseline timings from different CI machine types as direct
performance comparisons.

### Phase 1: Build the flattened Libft release graph alongside legacy targets

Add namespaced manifests and global rule generation under temporary target
names. Compare produced archives and symbols. Keep the old path available for
rollback.

### Phase 2: Add debug, test, sanitizer, and platform variants

Every variant with different flags must have a distinct object root. Verify C,
C++, and Objective-C++ sources. Run the complete tester, not only module tests.

### Phase 3: Switch Libft public targets

Point `all`, `debug`, `tests`, and aggregate archive targets at the global
graph. Convert module-local Makefiles to wrappers. Remove stale scanning,
progress locks, counters, logs, and obsolete scripts.

### Phase 4: Simplify ft_vox native recipes

Remove per-object bookkeeping while retaining its current recursive Libft path.
Validate ft_vox independently before combining graphs.

### Phase 5: Include the Libft graph in ft_vox

Create an isolated ft_vox Libft configuration, remove `FORCE` and recursive
Libft recipes, and verify that one Make invocation schedules objects from both
projects.

### Phase 6: CI and performance validation

Run all supported configurations on all three operating systems. Compare clean,
incremental, and no-op builds. Keep changes split into reviewable commits so a
phase can be reverted without discarding later source work.

## Validation matrix

Luna must demonstrate all of the following before declaring completion:

| Scenario | Required result |
|---|---|
| Clean Libft build with `-j1` | Successful archives and unchanged public symbols |
| Clean Libft build with `-j2` and host job count | Successful, no output corruption or lock directories |
| No-op Libft build | No compiler, archiver, linker, stale scan, or progress script |
| Touch one `.cpp` | One object rebuilt, its module archive rebuilt, final archive rebuilt |
| Touch a private header | Only objects whose `.d` files name it are rebuilt |
| Delete source from manifest | Object is absent from rebuilt module and aggregate archives |
| Edit manifest without touching remaining sources | Its module archive and aggregate archive rebuild |
| Edit compile configuration fragment | Exactly the affected object configuration rebuilds |
| Change build configuration | No incompatible object reuse |
| Interrupt and restart | Restart succeeds without deleting lock directories manually |
| Two builds in separate configuration roots | No shared progress/temp state or interference |
| Full Libft tester | All tests pass on Windows, Linux, and macOS |
| ft_vox clean build | Executable and tests build on all supported platforms |
| ft_vox no-op build | No compiler, archiver, linker, or recursive Libft process |
| ft_vox one-source touch | Only required object and downstream link rebuild |
| Archive inspection | No duplicate or stale members |

For scheduling verification, use `make --trace` or dry-run output on a small
controlled change and show that ready objects from different Libft modules can
run in the same invocation. Do not infer this only from elapsed time.

## Executable validation procedure

Run the following procedure from the repository root. Use a separate build
directory or configuration suffix for each compiler/configuration being
compared. Do not run two commands that modify the same build root
simultaneously.

### Libft build and no-op checks

On Linux and macOS:

```sh
make fclean
make --output-sync=target -j1 all
make --output-sync=target -j2 all
make --output-sync=target -j"$(getconf _NPROCESSORS_ONLN)" all
make --trace --output-sync=target -j2 all 2>&1 | tee /tmp/libft-noop.trace
FT_TEST_HIDE_SUCCESSFUL=1 make run-tests
make --output-sync=target -j2 run-debug-tests
make --output-sync=target -j2 run-asan-tests
make --output-sync=target -j2 run-ubsan-tests
```

On Windows PowerShell:

```powershell
make fclean
make --output-sync=target -j1 all
make --output-sync=target -j2 all
make --output-sync=target -j$env:NUMBER_OF_PROCESSORS all
make --trace --output-sync=target -j2 all 2>&1 | Tee-Object .\tmp\libft-noop.trace
$env:FT_TEST_HIDE_SUCCESSFUL='1'; make run-tests
```

The final no-op invocation must contain no compiler, archiver, linker, stale
scan, progress, or recursive Libft build command. If a command is printed by
`--trace`, determine whether it is a real required prerequisite or an
unintended rebuild before continuing.

### Incremental dependency checks

Record the object and archive timestamps, then touch exactly one source and
build the affected archive. Repeat with a private header named in one generated
`.d` file. The source case must rebuild that source object and all downstream
archives. The header case must rebuild every actual dependent object and no
unrelated module object. Use `make --trace` and the generated `.d` files as
the evidence; do not use object-directory counts.

For a manifest test, copy a module manifest to a temporary working branch,
remove one listed source, rebuild that module archive, and inspect it with:

```sh
ar t Modules/Basic/Basic.a
```

The removed object must not remain in the archive. Restore the manifest before
the next test. Also edit a manifest without changing its remaining sources;
the module archive must still rebuild because the manifest is an explicit
archive prerequisite.

### Archive and graph checks

Use `make -pn` to verify that every module archive lists its exact object
prerequisites and its manifest, while the aggregate archive lists the module
archives. Verify that no archive recipe passes a manifest to `ar` as an object
input. Check archive members with `ar t` and compare them against the expected
object list. Run the same checks for release, debug, and test configurations.

For a small controlled change, run:

```sh
make --trace -j2 Modules/Basic/Basic.a
```

Run the automated archive gate after the release graph has been built:

```text
make --output-sync=target -j<jobs> archive-integrity
```

The gate must report one successful check for each module archive and for
`Full_Libft.a`.

The dry-run or trace must expose independent objects from at least two modules
as prerequisites of one top-level invocation. This is the proof that the
global scheduler can share its job pool; elapsed time alone is insufficient.

### Libft tester

Build and run the complete tester through the public target, not an individual
module target. Use the repository's normal test command for the platform and
configuration, capture its exit status, and retain the complete output. A
successful archive build is not a successful test run. Run `run-tests`,
`run-debug-tests`, and the sanitizer run targets (`run-asan-tests`,
`run-ubsan-tests`, and, where supported, `run-asan-ubsan-tests`) when those
configurations are supported by the platform. These targets build the matching
executable first and execute it from `Test/`, where fixtures and Lua test files
are resolved correctly. Run the performance/efficiency target separately
through the Libft root targets (`performance_benchmarks` and
`run_performance_benchmarks`), so CI and developers exercise the same single
graph rather than entering through the compatibility `Test/Makefile` wrapper.
Repeat after a no-op build to ensure the test executable is not silently using
a stale archive.

### ft_vox/Minecraft checks

From the Minecraft root, run its normal clean build, no-op build, one-source
incremental build, and test targets. Confirm that the trace contains direct
Libft object edges and a real `Libft/Full_Libft.a` prerequisite, with no
recursive `make -C Libft` invocation. Touch one ft_vox source and confirm that
unrelated Libft objects are not rebuilt. Touch one Libft source and confirm
that the required Libft object, module archive, aggregate archive, and final
ft_vox link are rebuilt.

Run these checks separately with `-j1`, `-j2`, and the host job count. On
Windows, also confirm that generated dependency files use valid paths and that
no `.ft_vox_output_lock`, progress lock, or per-object log is created.

The parent `make test` target runs the supported validator flags on the built
executable sequentially: `--validate-camera-speed`, `--validate-collision`,
`--validate-block-edit`, `--validate-visible-distance`,
`--validate-terrain-determinism`, `--validate-world-scale`, `--validate-caves`,
and `--validate-terrain-configuration`. It is a headless regression check and
does not launch the interactive game. The interactive application remains the
separate `all`/`ft_vox` target.

### Terrain registry and persistence checks

The terrain registry is part of the Libft/Minecraft integration contract and
must be tested independently of build scheduling. Built-in block IDs are
defined by one enum whose `TERRAIN_BUILTIN_BLOCK_COUNT` sentinel is the first
runtime ID. A compile-time range assertion must keep the built-in and runtime
ranges inside the block ID type.

The registry tests must iterate every built-in ID, verify that IDs and stable
names are unique, register the complete runtime capacity, and confirm that
runtime IDs cannot collide with built-ins or shadow built-in names. Runtime
names must use a documented stable format (`namespace:name` with lowercase
ASCII letters, digits, and underscores). Built-in names are persistence ABI and
must not be renamed or reordered without an explicit migration or alias.
Duplicate runtime registration must fail without consuming an ID. Runtime
metadata must distinguish valid ore hosts from ore blocks; ore placement must
not replace an existing ore unless the rule explicitly opts in.

Terrain configuration persistence must serialize stable block names rather
than runtime numeric IDs. Loading resolves each name through the current
registry, so adding built-ins or registering runtime blocks in a different
order cannot silently reinterpret an old custom block. Missing names must
fail the load instead of falling back to an unrelated numeric ID; this case
must have a dedicated regression test. These tests must run from the test
fixture directory so runtime asset paths are resolved consistently on Windows,
Linux, and macOS.

### Failure and interruption checks

Start a parallel clean build, interrupt it during compilation, and rerun the
same command without manually deleting lock directories or progress files.
The rerun must recover from the partially completed build and produce valid
archives. Deliberately force an archive command to fail in a temporary test
configuration and verify that the previous valid archive remains intact.

For every platform, record the compiler, archiver, GNU Make version, command
line, exit status, rebuilt target list, and test result. A performance result
is valid only when the dependency and test checks above pass.

## Performance measurements

Measure at least:

- clean build wall time;
- no-op build wall time;
- one-source incremental wall time;
- one-header incremental wall time and object count;
- number of compiler processes launched;
- number of recursive Make processes launched;
- Windows results separately because Git Bash process startup and filesystem
  metadata costs are materially different.

Run each timed case enough times to reduce noise, discard or separately report
the cold-cache first run, and report median plus range. Build correctness is a
gate: do not accept a faster result that skipped required rebuilds.

### Windows validation snapshot

The current Windows workspace was measured on 2026-08-19 after the graph and
test validation completed. Three no-op runs with `make --output-sync=target
-j8 all` measured:

- standalone Libft: 6885, 6970, and 6826 ms (median 6885 ms; range 144 ms);
- Minecraft/ft_vox: 3146, 2957, and 3182 ms (median 3146 ms; range 225 ms).

These are local Windows measurements, not cross-machine baselines. Linux and
macOS timings, clean-build timing, and incremental source/header timing must
be added from the corresponding CI runners before making cross-platform
performance claims.

## Risks and safeguards

- `eval`-generated rules can silently expand variables too early. Add small
  graph-inspection targets and check `make -pn` output.
- Module-specific flags can be lost during normalization. Create a checklist
  from every existing module Makefile before deleting it.
- Archive updates can preserve removed members. Recreate archives and test
  source removal explicitly; make each archive depend on its source manifest.
- Manifest prerequisites are not archive inputs. Never pass raw `$^` to `ar`
  when it contains `.mk` or other graph-only prerequisites.
- Included Libft rules can collide with ft_vox variables. Prefix all exported
  Libft-internal variables with `LIBFT_` and module keys.
- Object names can collide when nested sources share basenames. Preserve the
  relative source path under the object root.
- Different compile definitions cannot share object files. Keep configuration
  roots isolated and never optimize by reusing objects across incompatible
  variants.
- `--output-sync` support depends on GNU Make version. Both repository roots
  enforce GNU Make 4.0 or newer rather than restoring custom output locks as a
  fallback.

## Definition of done

The redesign is complete only when:

- Libft uses one non-recursive canonical object graph;
- all module objects are visible to one root GNU Make scheduler;
- module archives and `Full_Libft.a` are real downstream file targets;
- ft_vox includes the Libft graph and no longer forces a recursive Libft build;
- compiler `.d` files provide precise header dependencies;
- custom stale scans, file-count progress, output locks, and per-object logs are
  gone;
- no-op builds perform no build work;
- interrupted builds recover without manual cleanup;
- full Libft and ft_vox test suites pass on Windows, Linux, and macOS;
- terrain built-in/runtime ID, ore-host, and stable-name persistence tests pass;
- measured incremental and Windows build performance is recorded against the
  baseline;
- documentation describing supported Make targets and configurations is
  updated in both repositories.
