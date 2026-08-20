# GNU Make stale-work reporting redesign

## Purpose

This document is an implementation handoff for adding accurate, readable build
progress to Libft and the parent Minecraft project without restoring the old
stale-object database, recursive per-module builds, or shared progress locks.

The implementation must continue to use regular GNU Make. It must support GNU
Make 3.81 and newer, retain the flattened Libft object graph, and allow one Make
scheduler to run Libft and Minecraft compilation jobs from the same job pool.

The desired user experience is:

```text
[BUILD PLAN] 23 source files require rebuilding

Minecraft:  6
Libft:     17

Libft/Basic:  3
Libft/Lua:    8
Libft/Voxel:  4
Libft/Game:   2

[LIBFT][Basic] Compiling basic_memcpy.cpp (1/3 files completed)
[LIBFT][Lua] Compiling lapi.c (1/8 files completed)
[MINECRAFT] Compiling terrain_generator.cpp (1/6 files completed)
[LIBFT][Basic] Archive ready: Modules/Basic/Basic.a
[BUILD PROGRESS][archives] 1/4 completed
```

The summary reports the work Make determined was stale at the start of the
invocation. The actual build also reports session-local completion counters for
each module and for archives. These counters are seeded from the same dry-run
plan; they must never be based on how many object files happen to exist in an
object directory.

## Non-negotiable requirements

1. Make remains the dependency authority. A helper must not independently
   compare source, header, object, or dependency-file timestamps.
2. The actual build remains one flattened dependency graph. Individual modules
   must not be rebuilt through recursive module Makefiles.
3. GNU Make 3.81 remains supported. Do not use `$(file ...)`, grouped targets,
   `--output-sync`, or other Make 4-only features as requirements.
4. `--output-sync=target` may remain an optional user flag on Make 4 or newer,
   but progress reporting must work without it.
5. No process-global progress file, fixed lock directory, or repository-global
   counter may be introduced. Live counters must use a unique per-invocation
   session directory.
6. Planning must not modify archives, objects, dependency files, generated
   source files, or configuration state.
7. Libft and Minecraft must use the same marker format and parser rules.
8. A no-op build must report zero compile work and execute no compiler,
   archiver, or linker command.

## Why the previous counters failed

The former implementation counted `.o` files found in an object directory.
That number represented objects already present, not objects rebuilt by the
current invocation. If a module contained sixty sources, fifty-five valid
objects, and five stale objects, the first completed compile could be displayed
as `56/60` instead of `1/5`.

Other versions attempted to discover stale modules through separate `make -q`
processes and stored totals in shared files. That duplicated Make's dependency
evaluation, performed it once per module, added substantial process-launch
overhead on Windows, and introduced lock ownership and concurrent-build races.

The replacement must obtain the plan from the same flattened Make graph that
performs the real build.

## Architecture

Use a two-phase front-end:

```text
public command
    |
    +-- planning Make invocation (dry run, emits machine-readable markers)
    |       |
    |       +-- one flattened dependency graph
    |       +-- no filesystem mutation
    |       +-- summary parser counts markers and writes session totals
    |
    +-- actual Make invocation
            |
            +-- one flattened dependency graph
            +-- all available jobs share one scheduler
            +-- concise human-readable compile/archive/link messages
            +-- successful operations update session-local counters
```

The planning phase evaluates the graph a second time, but it does not perform
one query per module. One dry-run covers the complete selected target set.

### Public and internal targets

Separate user-facing targets from internal graph targets. Suggested naming:

```text
all             -> plan `internal-all`, then build `internal-all`
tests           -> plan `internal-tests`, then build `internal-tests`
debug           -> plan `internal-debug`, then build `internal-debug`
performance     -> plan `internal-performance`, then build it

internal-all
internal-tests
internal-debug
internal-performance
```

The exact names may follow existing project conventions, but internal targets
must bypass the planning wrapper so the planning and build subprocesses cannot
recursively plan themselves.

When Libft is included by Minecraft, Minecraft owns the public wrapper. Its
internal target includes both Minecraft objects and the selected Libft archive,
so one planning pass and one actual build cover both repositories.

Standalone Libft commands use Libft's own wrapper around its internal targets.

## Machine-readable planning markers

Do not parse compiler command lines. Compiler executables, flags, quoting, and
response files differ across platforms. Add a planning variable such as:

```make
BUILD_PLAN_MODE ?= 0
```

Every compile recipe must have a marker command in a stable format. For
example:

```text
__BUILD_PLAN__|compile|libft|Basic|Modules/Basic/basic_memcpy.cpp
__BUILD_PLAN__|compile|libft|Lua|Modules/Lua/vendor/lua-5.4.8/lapi.c
__BUILD_PLAN__|compile|minecraft|Minecraft|src/terrain_generator.cpp
__BUILD_PLAN__|archive|libft|Basic|Modules/Basic/Basic.a
__BUILD_PLAN__|link|minecraft|Minecraft|ft_vox
```

Fields are:

1. fixed marker prefix;
2. operation type: `compile`, `archive`, or `link`;
3. project: `libft` or `minecraft`;
4. module name;
5. source or output path.

Paths must not contain the marker delimiter. If that cannot be guaranteed,
encode or escape the path in one documented portable way.

The dry-run parser counts only `compile` markers when reporting source files
that require rebuilding. Archive and link markers may be reported separately.

### Marker recipe design

The marker must be emitted by the same target recipe as the real operation. It
must therefore appear in `make -n` output only when Make schedules that target.
Do not generate markers by iterating over source manifests independently.

Keep planning markers distinct from human output. The actual build should print
messages such as:

```text
[LIBFT][Lua] Compiling vendor/lua-5.4.8/lapi.c
```

The planning parser consumes only lines beginning with `__BUILD_PLAN__|`.

## Planning command

Add a portable script, for example:

```text
mk/print_build_plan.sh
```

The script receives:

- the Make executable;
- the internal target;
- configuration variables required by that target;
- optionally the requested job configuration for display only.

It invokes a dry run similar to:

```text
make --no-print-directory -n BUILD_PLAN_MODE=1 internal-all
```

The implementation must preserve relevant command-line variable assignments
such as `DEBUG=1`, sanitizer modes, archive suffixes, optimization level, and
the Minecraft-selected Libft configuration. The plan is invalid if it evaluates
a different configuration root from the actual build.

The parser should use POSIX `awk`, which is available in the Unix environments
and Git Bash used by the Windows build. It should:

1. accept only marker lines;
2. count all compile operations;
3. group compile operations by project;
4. group Libft operations by module;
5. count archive and link operations separately;
6. print modules in manifest order or another deterministic order;
7. return non-zero if the dry-run Make invocation fails.

The existing public `plan` target is the dry-run interface for this work.
`make plan` must remain useful on its own: it prints the stale summary and
does not start a build. The normal public build wrapper should reuse the same
parser immediately before invoking the corresponding internal target. When a
session directory is supplied, the parser also writes machine-readable totals
for the current invocation, including one total per project/module and one
archive total. The actual recipes must consume these totals rather than
rediscovering staleness.

Do not hide errors from the planning Make invocation. A missing prerequisite or
invalid graph must stop the public build before the actual build begins.

Use a unique temporary file created with `mktemp` when available. On Windows
Git Bash, verify that `mktemp` is present in CI. If a fallback is necessary,
place the file under the selected configuration build root with a name that
contains the current process ID and remove it through an ownership-safe trap.
Never use a fixed filename such as `stale_modules` or `total`.

## Actual build output

Prefix compiler, archiver, and linker commands with `@` and print one concise
status line for each real operation. Wrapped compile recipes print the source
and its completion count together after the compiler succeeds.

Libft compile output:

```text
[LIBFT][Basic] Compiling basic_memcpy.cpp (1/3 files completed)
[LIBFT][Lua] Compiling vendor/lua-5.4.8/lapi.c (1/8 files completed)
```

Minecraft compile output:

```text
[MINECRAFT] Compiling src/terrain_generator.cpp (1/6 files completed)
```

Archive output:

```text
[LIBFT][Basic] Archiving Modules/Basic/Basic.a
[LIBFT][Basic] Archive ready: Modules/Basic/Basic.a
[BUILD PROGRESS][archives] 1/4 completed
```

Link output:

```text
[LIBFT] Linking Test/libft_tests
[MINECRAFT] Linking ft_vox
```

Do not suppress compiler diagnostics. Prefixing the compiler command with `@`
hides the command itself, but warnings and errors still reach the terminal.

Because Make 3.81 has no output synchronization, parallel status and diagnostic
lines may interleave. Each status message must be produced by one `printf`
call so individual status lines remain as coherent as the host pipe permits.
Make 4 users may add `--output-sync=target` for grouped recipe output.

### Live counters seeded by the plan

The public wrapper creates a unique temporary session directory, runs the same
dry-run plan used by `make plan`, and passes that directory to the actual
internal Make invocation. Every successful compile recipe calls a small
portable progress helper with its project, module name, and source path. Every
successful archive recipe calls the helper with the archive operation. The
helper reads the immutable total written by the planning phase, obtains a lock
private to the session, increments the matching completed count, prints the
source and its combined `completed/total` line, and releases the lock.

Counter updates occur only after the compiler or archiver succeeds. A failed
or interrupted counter update must not fail the build; the compiler,
archiver, or linker remains authoritative for the build result. The wrapper
removes the session directory with an ownership-safe cleanup trap when the
internal Make process ends.

The output is completion-ordered rather than source-ordered. With parallel
Make, a module may print `2/8` before another module prints `1/3`, and that is
correct. The totals are fixed by the plan while completed counts reflect
successful operations in this invocation.

## Counts and semantics

The initial number means:

> The number of compile targets that Make considered out of date when the
> planning phase evaluated the selected graph.

It does not mean the number of source files in the repository or the number of
objects currently present.

If a file changes between the planning and build phases, the actual build is
authoritative. It may compile more or fewer targets than the initial plan. This
race is unavoidable without freezing the source tree. Document it, but do not
add locking around source files.

Live `completed/total` counts are part of the normal public build output
because the project uses them to show stale-work progress. They are
informational only: they must never participate in dependency decisions or
determine whether a target is up to date. Parallel recipes require
synchronized mutation, so the state must be private to the current wrapper
invocation and the lock must be created inside that session directory. There
must be no fixed repository-wide counter or reusable lock directory.

If files change between planning and the actual build, Make remains
authoritative. The wrapper may report an operation outside the original total
as unplanned or document the short plan/build race; it must not silently fall
back to counting every object in a directory.

## Libft implementation plan

1. Add internal targets in the root Makefile for release, debug, test,
   test-debug, performance, and archive-integrity graph entry points.
2. Keep `mk/global_graph.mk` as the source of object and archive prerequisites.
3. Extend the generated C++, C, and Objective-C++ compile rules with stable
   project/module/source marker commands.
4. Add concise actual-build status lines and hide raw compiler commands.
5. Add archive marker/status lines to every generated archive recipe.
6. Add link marker/status lines to test and efficiency executable recipes.
7. Implement the single-pass plan parser in `mk/print_build_plan.sh`, with an
   optional session-total output mode used by public wrappers.
8. Add a session-local progress-update helper and call it only after
   successful compile and archive recipes.
9. Route public targets through one wrapper that runs the planning script,
   stores per-module and archive totals in a unique session directory, and
   then invokes exactly one internal build target with that session directory.
10. Ensure direct developer targets such as `Modules/Basic/Basic.a` continue to
   work without requiring the wrapper. Direct targets may print operation
   messages without an initial aggregate summary.
11. Ensure archive-integrity helper invocations use internal targets where
    appropriate so they do not recursively print plans.

## Minecraft implementation plan

1. Keep including `Libft/mk/global_graph.mk`; do not return to `make -C Libft`.
2. Add marker and concise output lines to Minecraft C++ and Objective-C++
   compile recipes, followed by session-counter updates after success.
3. Add marker and status lines to Minecraft link recipes. Compile and archive
   counters must include the Libft recipes when the combined graph is built.
4. Create internal `all`, test, debug, and validation graph targets.
5. Make the Minecraft planning wrapper evaluate the combined internal graph
   and seed the same session-local counter files used by its build recipes.
6. Group plan results into `Minecraft` and `Libft`, with Libft further grouped
   by module.
7. Pass the exact same Libft configuration suffix, compiler flags, debug mode,
   and archive selection to planning and actual build phases.
8. Ensure `make test` plans/builds once and then runs validators without
   triggering another build-plan pass for each validator.
9. Ensure submodule update targets remain independent of progress planning.

## Avoiding accidental recursive planning

Target separation is the primary recursion guard. An explicit variable may be
retained for diagnostics, but it is not a substitute for keeping internal
targets free of public wrapper dependencies:

```make
BUILD_WRAPPER_ACTIVE ?= 0
```

If this variable is retained, the public wrapper sets it only for the child
invocation. The implementation may omit it when the target graph itself
provides the guard.

Internal targets never call public targets. Document the allowed call direction:

```text
public target -> plan internal target -> build internal target

internal target -X-> public target
```

Recursive Make remains acceptable only as the front-end boundary that performs
one planning pass and one actual flattened build. It must not divide scheduling
by module.

## Incremental-build correctness

The planning pass must use all included `.d` files selected for the requested
configuration. Validate at least these causes of staleness:

- a `.cpp`, `.c`, or `.mm` source changed;
- a private header changed;
- a public header used by several modules changed;
- a module manifest gained a source;
- a module manifest removed a source;
- compile flags changed and selected a different configuration root;
- release, debug, test, and test-debug configurations do not share objects;
- a Lua vendor C source changed;
- a Minecraft source changed while Libft remained current;
- a Libft source changed while Minecraft sources remained current;
- both projects had stale objects in the same combined invocation.

Module archives must continue to depend on the manifest that defines their
object list. Archives must be recreated from a temporary archive and renamed so
removed sources cannot leave stale archive members behind.

## Validation matrix

### Plan accuracy

For each test, compare the printed compile count with the compile status lines
from the actual build.

1. Clean build: the plan lists every required compile target.
2. Immediate no-op: the plan reports zero and no compile line appears.
3. Touch one source: exactly one compile is planned and executed, plus its
   downstream archive/link operations.
4. Touch one private header: only consumers from its `.d` dependencies rebuild.
5. Touch one widely used header: every real consumer is counted.
6. Add a source to a module manifest: the new object is counted and archived.
7. Remove a source: zero replacement compile may be needed, but the archive is
   planned and the removed member is absent afterward.
8. Change configuration flags: the correct separate configuration root is
   planned.

### Parallel behavior

Run the clean and incremental cases with:

```text
make -j1
make -j2
make -j8
```

On Make 4 or newer, additionally run one case with optional
`--output-sync=target`. Counts must not depend on job count or output sync.

Run two builds concurrently with different configuration roots. Their planning
output and temporary files must not interfere.

### Platform matrix

Run on:

- Ubuntu with the CI Make version;
- macOS with Apple-provided GNU Make 3.81;
- Windows through the project's Git Bash shell and installed GNU Make;
- standalone Libft;
- Minecraft with the embedded Libft graph.

### Failure behavior

Validate that:

- planning exits non-zero for a missing source prerequisite;
- planning exits non-zero for malformed Make input;
- compilation failure preserves compiler diagnostics;
- an interrupted planning pass removes only its own temporary file;
- an interrupted build leaves no shared progress lock;
- a failed plan never starts the actual build;
- a failed actual build returns the compiler/archiver/linker failure code.

## CI changes

CI should invoke the same public targets developers use unless a job explicitly
tests an internal target. Do not add mandatory `--output-sync=target`.

Add the lightweight `mk/test_build_progress.sh` contract script (and run it
from CI) that:

1. builds a small selected target;
2. confirms the next plan reports zero;
3. touches or copies a controlled fixture source;
4. confirms the plan reports one stale compile;
5. builds it and confirms the matching module prints `1/1 completed`;
6. confirms an archive is reported through the archive `completed/total`
   counter;
7. restores the fixture through a temporary workspace or disposable checkout.

Do not modify tracked files in the primary CI checkout without restoring them.
Prefer a temporary copied fixture graph for exact plan-parser unit tests and use
the real graph for end-to-end no-op and incremental checks.

## Documentation updates

Update both READMEs with:

- the meaning of the initial stale source count;
- the fact that archive and link counts are separate;
- the two-phase graph evaluation;
- the small planning overhead, especially on Windows;
- the Make 3.81 compatibility guarantee;
- optional Make 4 output synchronization;
- the race caveat when files change between planning and compilation;
- a command for bypassing the summary and invoking an internal target for
  profiling or build-system debugging.

## Acceptance criteria

The work is complete only when all of the following are true:

- a no-op build prints zero stale compile targets;
- touching one source prints one stale compile target;
- each active module prints a `completed/total` counter based on the plan;
- archive recipes print a session-wide `completed/total` counter;
- header-triggered rebuild counts match the `.d` dependency graph;
- Libft standalone output identifies every compiling module;
- Minecraft output distinguishes Minecraft and Libft work;
- raw successful compiler commands are hidden while diagnostics remain visible;
- all Lua objects remain explicit prerequisites of Lua archives;
- clean, incremental, source-addition, and source-removal builds are correct;
- no fixed progress file or shared progress lock is created;
- Make 3.81, current GNU Make, Linux, macOS, and Windows builds pass;
- the full Libft and Minecraft test suites pass independently of progress mode;
- the actual build still uses one flattened scheduler across all selected
  Minecraft and Libft object targets.

## Recommended implementation order

1. Add markers and concise operation output without changing public targets.
2. Unit-test the plan parser with fixed marker fixtures.
3. Add session-total output and the session-local counter helper; test
   concurrent counter updates.
4. Add standalone Libft internal targets and wrapper.
5. Validate Libft clean/no-op/incremental behavior on all platforms.
6. Add Minecraft internal targets and its combined wrapper.
7. Validate cross-project scheduling and combined plan grouping.
8. Add CI progress-contract checks.
9. Update documentation.
10. Remove any remaining raw-command output or obsolete progress variables only
   after repository-wide reference checks.

This ordering keeps build correctness changes separate from output formatting
and makes regressions easier to isolate.
