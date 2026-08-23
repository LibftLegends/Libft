# Test-scoped failure diagnostics design

## Status

Proposed design. This document describes tester-only infrastructure and does
not change the public Libft API.

## Problem

Tests often need more information than an assertion can provide. Useful
diagnostics may include allocator state, active locks, scheduler queues,
registered resources, object lifecycle state, recent events, temporary-file
paths, or a compact dump of the input that produced a failure.

Printing this information unconditionally makes successful test runs noisy and
can make CI logs too large to use. Adding one-off failure branches to individual
tests also duplicates formatting and makes useful diagnostics difficult to
reuse when a similar failure appears later.

The tester needs a framework with these properties:

- A test explicitly attaches the diagnostics relevant to that test.
- Attached diagnostics are exposed only when that test fails.
- Diagnostic infrastructure and diagnostic access hooks are compiled only for
  the tester.
- Diagnostic providers are reusable across unrelated tests.
- A successful test produces no diagnostic output and leaves no diagnostic
  artifact behind.
- Diagnostics augment the original assertion or exception; they never replace
  it or change whether the test passes.

## Goals

1. Associate diagnostics with one currently executing `FT_TEST`.
2. Invoke lazy diagnostic providers only after that test has failed.
3. Support captured snapshots when the state may no longer exist at failure
   time.
4. Print one ordered failure report containing the assertion and every
   diagnostic attached to the failed test.
5. Allow providers to be shared from a tester-only diagnostics directory.
6. Keep production builds free of diagnostic symbols, storage, and access
   hooks.
7. Keep the failure path bounded and useful when allocation, locking, or another
   subsystem is itself broken.

## Non-goals

- Replacing `FT_ASSERT`, `FT_ASSERT_EQ`, or the existing test result protocol.
- Enabling diagnostics in production or release builds.
- Streaming every trace event to the console.
- Running diagnostics for successful tests.
- Treating a diagnostic-provider failure as a second test failure.
- Providing an unrestricted callback that may mutate the system under test.

## User-facing model

A diagnostic provider is a tester-only function that writes a named section to
the runner's diagnostic writer. A test attaches providers before the operation
that may fail. The runner remembers those attachments for the current test.

```cpp
FT_TEST(test_thread_pool_cancellation_allows_execution)
{
    ft_thread_pool pool;

    FT_TEST_DIAGNOSTIC(ft_test_diagnostic_thread_pool, &pool);
    FT_TEST_DIAGNOSTIC(ft_test_diagnostic_lock_tracking, ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, pool.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, run_cancellation_scenario(pool));
    return (1);
}
```

If the test passes, neither provider is called and nothing is printed. If the
test fails, the output is grouped under that test:

```text
FAIL 412 test_thread_pool_cancellation_allows_execution
  assertion: Test/Test/test_thread_pool.cpp:184
              FT_ERR_SUCCESS == run_cancellation_scenario(pool)
              expected: 0 | actual: -18
  diagnostic: thread_pool
    state: stopping
    worker_count: 4
    queued_tasks: 1
    active_tasks: 0
  diagnostic: lock_tracking
    current_thread_locks: 0
    tracked_mutexes: 4
```

The same provider can later be attached to any other relevant test without
copying its implementation.

## Public tester-only API

The declarations belong in
`Modules/System_utils/test_system_utils_runner.hpp` inside
`#ifdef LIBFT_TEST_BUILD`. Production code must not see them.

```cpp
typedef struct s_ft_test_diagnostic_writer ft_test_diagnostic_writer;

typedef void (*t_ft_test_diagnostic_provider)(
    ft_test_diagnostic_writer *writer,
    const void *context) noexcept;

int32_t ft_test_attach_diagnostic(
    const char *diagnostic_name,
    t_ft_test_diagnostic_provider provider,
    const void *context) noexcept;

int32_t ft_test_diagnostic_write(
    ft_test_diagnostic_writer *writer,
    const char *key,
    const char *value) noexcept;

int32_t ft_test_diagnostic_write_int64(
    ft_test_diagnostic_writer *writer,
    const char *key,
    int64_t value) noexcept;

int32_t ft_test_diagnostic_write_uint64(
    ft_test_diagnostic_writer *writer,
    const char *key,
    uint64_t value) noexcept;

int32_t ft_test_diagnostic_write_pointer(
    ft_test_diagnostic_writer *writer,
    const char *key,
    const void *value) noexcept;

#define FT_TEST_DIAGNOSTIC(provider, context) \
    do \
    { \
        FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_test_attach_diagnostic( \
            #provider, provider, context)); \
    } while (0)
```

The initial API should use a small set of typed writer functions. Providers
must not receive a raw `FILE *`; keeping output behind the writer preserves
formatting, output limits, and future JSON support.

The macro is intentionally test-only. Using it outside a test build must fail
at compile time instead of silently becoming a no-op, because a silent no-op
would hide accidental reliance on diagnostics in production code.

## Provider placement and compilation boundary

Reusable providers belong under:

```text
Test/Diagnostics/
  test_diagnostics.hpp
  test_diagnostics_allocator.cpp
  test_diagnostics_lock_tracking.cpp
  test_diagnostics_scheduler.cpp
  test_diagnostics_thread_pool.cpp
  test_diagnostics_filesystem.cpp
```

Only the test executable and test-debug executable include these translation
units. They must not be added to module archives, `Full_Libft.a`, release
archives, demos, or production targets.

When a provider needs otherwise-private module state, the module may expose a
minimal read-only hook guarded by `#ifdef LIBFT_TEST_BUILD`. The hook should
return a snapshot or write through the diagnostic writer. It must not expose a
mutable production object, alter normal behavior, or introduce a production
symbol.

The build must enforce this boundary with two checks:

1. A release archive symbol scan rejects `ft_test_diagnostic_*` and
   `ft_*_diagnostic_for_tests` symbols.
2. A test-build contract test verifies that a known provider is linked and can
   be attached.

## Lazy providers and snapshots

Most providers should be lazy: the test registers a pointer to valid context,
and the runner invokes the provider only after failure. This avoids formatting,
filesystem access, and large state copies during successful runs.

Lazy context must remain valid until the test function returns and failure
diagnostics have been emitted. The runner therefore invokes providers
immediately after `test->func()` returns failure or throws, before per-test
cleanup destroys state.

Some diagnostics cannot be read lazily because the operation under test may
destroy, move, close, or corrupt the source object. Those tests should capture a
small snapshot in test-owned storage and attach a provider for that snapshot:

```cpp
ft_test_scheduler_snapshot scheduler_snapshot;

FT_ASSERT_EQ(FT_ERR_SUCCESS,
    ft_test_capture_scheduler_snapshot(&scheduler, &scheduler_snapshot));
FT_TEST_DIAGNOSTIC(ft_test_diagnostic_scheduler_snapshot,
    &scheduler_snapshot);
```

Snapshot capture is explicit because it does execute during successful tests.
Snapshots should contain bounded plain data and fixed-size strings where
possible. A provider must never retain ownership of the context pointer.

## Runner lifecycle

The runner owns one diagnostic attachment list for the current test. The
required order is:

1. Reset the attachment list.
2. Set the current test name.
3. Redirect test stdout and stderr as today.
4. Execute the test function.
5. Restore the runner's baseline descriptors.
6. If the test failed, print the assertion or exception followed by every
   attached provider in registration order.
7. Clear the attachment list without invoking providers when the test passed.
8. Run ordinary per-test reset work.
9. Clear the current test name.

Diagnostics must run after baseline stdout and stderr are restored. Otherwise
their report would be sent to the null device by the current runner.

The original assertion remains the primary failure. `ft_test_fail()` and
`ft_test_fail_values()` should continue storing the first failure message, but
writing `test_failures.log` should move to the grouped report step so console
and file output have the same structure.

## Attachment storage

Use runner-owned bounded storage rather than allocating one object per
attachment. A practical initial limit is 32 providers per test:

```cpp
struct s_ft_test_diagnostic_attachment
{
    const char *name;
    t_ft_test_diagnostic_provider provider;
    const void *context;
};
```

The list is reset between tests. Registration order is output order. Attaching
the same `(provider, context)` pair twice should return
`FT_ERR_ALREADY_EXISTS`; exceeding capacity should return a dedicated
recoverable `FT_ERR_*` code defined in the Errno module before use.

Registration outside an active test is runner misuse and should return
`FT_ERR_INVALID_STATE`. Null names, providers, or invalid writer pointers return
the appropriate existing `FT_ERR_*` code.

The attachment registry is owned by the runner thread. Tests that spawn worker
threads may collect data in their context, but only the runner thread may
attach providers or write the final report. This keeps report ordering
deterministic and avoids introducing another synchronization system into the
failure path.

## Failure-path safety

Diagnostic code runs when the system may already be damaged. Providers must:

- be `noexcept` and return no test result;
- perform read-only inspection;
- avoid acquiring locks that may be held by the failed operation;
- avoid unbounded allocation, recursion, waits, and filesystem traversal;
- cap strings, collections, stack traces, and event histories;
- tolerate null, destroyed, and partially initialized contexts;
- label unavailable information instead of aborting;
- never call `FT_ASSERT`, `su_abort()`, or another diagnostic provider.

The runner should impose limits per diagnostic section and per failed test.
Suggested defaults are 16 KiB per provider and 128 KiB per test. Truncation must
end with an explicit line such as `diagnostic truncated after 16384 bytes`.

If a provider throws despite its contract, exceeds its limit, or reports an
internal error, the runner appends a short `diagnostic unavailable` line and
continues with the remaining providers. The original test result is unchanged.

## Reusable provider design

Providers should report one coherent subsystem rather than one specific test.
For example:

- `ft_test_diagnostic_allocator`: allocation limit, live allocation count,
  arena state, and last allocator error.
- `ft_test_diagnostic_lock_tracking`: locks held by the current test thread and
  bounded metadata for tracked mutexes.
- `ft_test_diagnostic_scheduler`: scheduler state, queued/active counts, recent
  bounded trace events, and shutdown state.
- `ft_test_diagnostic_thread_pool`: worker, queue, cancellation, and stop state.
- `ft_test_diagnostic_filesystem_path`: normalized path, existence, type,
  permissions, size, and last filesystem error.

Provider names and field keys are stable diagnostic interfaces. Tests should
not parse human-formatted lines. Contract tests may instead use a memory-backed
writer and inspect structured key/value records.

## Interaction with subprocess and abort tests

Many lifecycle tests expect `su_abort()` in a child process. Parent-owned
providers can still report the parent test context. Child-only state requires
one of two explicit mechanisms:

1. The child writes a bounded snapshot through an existing test pipe, and the
   parent attaches that snapshot.
2. The child writes a bounded tester-only artifact whose path is attached to
   the parent test and read only on failure.

The child must not print diagnostics directly because its stdout and stderr are
suppressed and concurrent child output would break report grouping.

## Output destinations

The first implementation should write the same grouped report to:

- restored runner stdout for immediate local and CI visibility; and
- `test_failures.log` for artifact preservation.

Only failed tests appear in `test_failures.log`. The existing
`FT_TEST_PRESERVE_FAILURE_LOG=1` behavior remains responsible for retaining the
file after the executable exits.

A future JSON writer may be added behind the same provider API. Providers must
therefore write typed key/value fields rather than construct JSON or terminal
escape sequences themselves.

## Test plan

The framework needs focused contract tests:

1. A passing test attaches a provider and proves it was not invoked.
2. A failing test invokes every attached provider exactly once.
3. Diagnostics from test A never appear for test B.
4. Providers are emitted in registration order.
5. Duplicate attachment is rejected without duplicate output.
6. Capacity exhaustion returns the documented recoverable error.
7. Null and outside-active-test registration return documented errors.
8. A provider failure does not suppress later providers or change the original
   assertion.
9. Output limits truncate a provider and preserve valid report structure.
10. Snapshot diagnostics remain valid after the original object is destroyed.
11. Parallel worker activity can update provider context without attaching or
    writing from worker threads.
12. Release archives contain no tester diagnostic symbols.
13. Filtering with `FT_TEST_NAME_FILTER` emits diagnostics only for the selected
    failed test.
14. `FT_TEST_HIDE_SUCCESSFUL=1` still prints the complete grouped failure
    report.

## Implementation phases

### Phase 1: runner core

- Add the guarded attachment and writer API.
- Add bounded attachment storage to the test runner.
- Move failure-log emission into one post-test grouped report path.
- Add contract tests with synthetic providers.

### Phase 2: shared providers

- Create `Test/Diagnostics/` and add allocator, lock-tracking, scheduler, and
  filesystem providers.
- Add only the provider translation units needed by the test targets to the
  flattened test graph.
- Add release symbol-boundary verification.

### Phase 3: adoption

- Replace one-off CI logging in flaky or platform-sensitive tests with attached
  providers.
- Start with scheduler, thread-pool, cross-process, SCMA, and file tests.
- Keep provider output bounded and remove obsolete unconditional diagnostics as
  each test migrates.

### Phase 4: structured artifacts

- Add an optional JSON writer without changing provider signatures.
- Teach CI to upload the structured failure report only when the test step
  fails.

## Acceptance criteria

The design is complete when a test can attach multiple reusable diagnostic
providers, successful runs invoke none of them, a failed run prints all and only
that test's providers, the report survives in `test_failures.log`, and release
archives contain no diagnostic framework symbols.
