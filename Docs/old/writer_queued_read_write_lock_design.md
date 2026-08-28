# Writer-Queued Non-Recursive Read/Write Lock Design

Status: implemented design; Linux-focused verification complete, platform gates pending

Primary implementation area: `Modules/PThread`

Primary consumers: read-heavy Libft state and selected Minecraft world data

This document records the implemented design and its verification contract.
Production adoption still requires the platform-specific acceptance gates at
the end of this document.

## 1. Summary

Implement a non-recursive, FIFO-like read/write lock in Libft. Any number of
threads may hold the lock for reading at the same time. Readers enter freely
while no writer is waiting. The first waiting writer closes free reader
admission. Requests accumulated behind that boundary are serviced in phases:
writers run one at a time in writer-ticket order, while a reader phase admits
all readers that were waiting when that phase's cutoff was established. Reader
requests that arrive after the cutoff wait for a later phase.

The required sequence is:

```text
reader A enters
reader B enters concurrently
writer W1 queues and closes free reader admission
reader C queues behind W1
writer W2 queues behind W1
reader A leaves
reader B leaves
writer W1 enters and leaves
reader C enters as part of the bounded reader phase
writer W2 enters after that reader phase
```

For a queue observed at a writer boundary such as `W1 R1 W2 R2 R3 W3`, the
reader cutoff is taken after `R3` has queued. The execution is `W1`, then
`R1 + R2 + R3` concurrently, then `W2`, then `W3`. A reader `R4` arriving after
the cutoff cannot join that reader phase and waits until the next one.

This is a queued phase-fair behavior. It prevents a writer from being bypassed
by readers that arrive after its gate closes, while preventing a later writer
from bypassing readers already waiting for the next reader phase. The cutoff is
required: readers arriving during a phase do not join it, so a continuous read
stream cannot starve the next writer.

## 2. Existing Libft state

Libft already has two reader/writer lock layers in
`Modules/PThread/pthread_rwlock.cpp`:

- `pt_rwlock_*` directly wraps the platform `pthread_rwlock_t`. Its fairness is
  platform-dependent and therefore cannot guarantee this design.
- `t_pt_rwlock` plus `pt_rwlock_strategy_*` implements reader- and
  writer-priority strategies with a mutex, two condition variables, and active
  and waiting counters.

The writer-priority strategy already blocks free reader admission when
`waiting_writers > 0`, which is the central behavior required here. It is
strengthened with reader tickets and a bounded phase cutoff rather than creating
a third unrelated lock implementation.

The writer-priority strategy now addresses those gaps with explicit writer
tickets, reader ownership records, reader phase cutoffs, mode-specific owner
checks, lifecycle state, cancellation handling, and focused queue tests. The
native wrapper remains separate because platform `pthread_rwlock_t` fairness is
not deterministic.

## 3. Required semantics

### 3.1 Read acquisition

A writer-queued read acquisition succeeds freely when no writer is waiting and
no writer is active. Once a writer queues, a reader succeeds only when all of
these are true:

- no writer is active;
- it belongs to the currently open reader phase, or free reader admission has
  not yet closed;
- the calling thread does not already own this lock in either mode;
- the lock is initialized and not being destroyed.

The legacy reader-priority strategy intentionally retains its older admission
rule and may admit readers while writers wait. Multiple successful readers may
execute concurrently. A reader that was already
active before the first writer queued may finish normally.

### 3.2 Write acquisition

Every writer obtains a monotonically increasing ticket. It may enter only when:

- no readers are active;
- no writer is active; and
- no reader phase is open; and
- its ticket equals the current serving ticket.

Ticket order makes the writer queue deterministic. Do not rely on
`pthread_cond_signal()` wake order because POSIX does not promise FIFO ordering.

### 3.3 Writer gate and reader phases

Free reader admission is closed whenever:

```text
next_writer_ticket != serving_writer_ticket
```

This single invariant covers both a waiting writer and an active writer. After a
writer completes, the lock establishes a reader-phase cutoff from the readers
already waiting at that instant. Every such reader may enter concurrently,
including readers that arrived after a later writer ticket. Writers remain in
their ticket order and wait until the complete reader phase leaves. Readers
arriving after the cutoff receive a later reader ticket and cannot extend the
current phase.

If no writer is waiting when the last reader of a phase leaves, a later queued
reader phase may be opened. If a writer is waiting, the next writer ticket is
served first. Thus the queue is FIFO for writers, FIFO-like across phase
boundaries, and batches compatible reader work without allowing unbounded
reader admission.

### 3.4 Unlock behavior

Use mode-specific unlock functions:

- `pt_rwlock_strategy_rdunlock()` may be called only by an active reader;
- `pt_rwlock_strategy_wrunlock()` may be called only by the active writer.

Keep `pt_rwlock_strategy_unlock()` only as a compatibility shim if an audit finds
existing external callers. New code must not use it. If the compatibility shim
cannot identify the caller's ownership mode without ambiguity, deprecate it and
return `FT_ERR_INVALID_OPERATION` rather than guessing.

The last active reader closes its current reader phase. If a writer is waiting,
it broadcasts the writer condition so the next ticket can proceed. Otherwise it
opens the next bounded reader phase and broadcasts all reader waiters. A writer
unlock advances `serving_writer_ticket`; if readers are waiting it opens a
reader phase using the current reader-ticket cutoff, even when later writers
are already queued. If no readers are waiting, it broadcasts the writer
condition or the unrestricted reader condition as appropriate.

Broadcasting writers avoids the deadlock where `pthread_cond_signal()` wakes a
writer whose ticket is not being served. The extra wakeups are acceptable; each
thread rechecks the predicate in a `while` loop.

### 3.5 Non-recursive contract

The following calls must fail immediately with
`FT_ERR_MUTEX_ALREADY_LOCKED` and must never wait:

- read lock while the same thread already owns a read lock;
- read lock while the same thread owns the write lock;
- write lock while the same thread already owns the write lock;
- write lock while the same thread owns a read lock.

This primitive does not support recursive locking, read-to-write upgrade, or
write-to-read downgrade. Those operations otherwise create self-deadlocks or
weaken the writer queue. If upgrade behavior is needed later, design it as a
separate primitive with an explicit single-upgrader policy.

Unlock by a non-owner returns `FT_ERR_MUTEX_NOT_OWNER`. Unlocking an unlocked
lock returns `FT_ERR_INVALID_STATE`.

### 3.6 Memory visibility

Successful lock acquisition must have acquire semantics and successful unlock
must have release semantics through the internal mutex. A reader admitted after
a writer releases must observe the writer's completed changes. Callers must not
access protected data before lock success or after unlock begins.

## 4. Public API

Preserve the existing `t_pt_rwlock` name for compatibility, but make the
writer-queued behavior the recommended mode.

Required API:

```cpp
int32_t pt_rwlock_strategy_init(t_pt_rwlock *rwlock,
    t_pt_rwlock_strategy strategy);
int32_t pt_rwlock_strategy_rdlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_try_rdlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_rdunlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_wrlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_try_wrlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_wrunlock(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_destroy(t_pt_rwlock *rwlock);
int32_t pt_rwlock_strategy_get_error(const t_pt_rwlock *rwlock);
const char *pt_rwlock_strategy_get_error_str(const t_pt_rwlock *rwlock);
```

Keep `PT_RWLOCK_STRATEGY_WRITER_PRIORITY` as the explicit strategy used by this
design. Do not silently change `PT_RWLOCK_STRATEGY_READER_PRIORITY` behavior for
legacy callers. The PThread README should recommend writer priority and clearly
state that reader priority can starve writers.

The native `pthread_rwlock_t` wrappers remain compatibility wrappers. Their docs
must state that they do not provide Libft's deterministic writer-queued
guarantee.

## 5. Internal state

Extend `t_pt_rwlock` with state equivalent to:

```cpp
pthread_mutex_t mutex;
pthread_cond_t reader_condition;
pthread_cond_t writer_condition;
size_t active_readers;
size_t waiting_readers;
size_t waiting_writers;
uint64_t next_writer_ticket;
uint64_t serving_writer_ticket;
uint64_t next_reader_ticket;
uint64_t reader_phase_cutoff;
pt_thread_id_type active_writer_thread;
ft_bool writer_active;
ft_bool active_writer_has_ticket;
ft_bool reader_phase_open;
pt_buffer<pt_thread_id_type> active_reader_threads;
t_pt_rwlock_strategy strategy;
std::atomic<uint8_t> initialised_state;
std::atomic<int32_t> error_code;
```

`active_writers` becomes unnecessary because `active_writer_thread != 0`
represents the single active writer. If zero is not a portable invalid thread ID
on every supported platform, keep a separate `ft_bool writer_active` flag.

The active-reader thread buffer is required to enforce non-recursion and owner
checked read unlock. Use a PThread-internal/system allocator so lock bookkeeping
does not recurse into CMA or another subsystem that may itself use this lock.
Reader admission may fail with `FT_ERR_NO_MEMORY` if ownership bookkeeping cannot
grow; in that case counters and waiting state must be rolled back before return.

Ticket counters must not wrap silently. If `next_writer_ticket` reaches
`UINT64_MAX`, normalize both counters to zero only while no writer is active and
no writer is waiting. Otherwise return `FT_ERR_OUT_OF_RANGE` without joining the
queue.

Scheduling and ownership fields are protected by the internal mutex. Lifecycle
state and the last-error value are atomic because callers must reject an
uninitialized or destroyed lock before attempting to use its mutex. Do not add
unlocked checks for the scheduling fields; one state model is easier to prove
and test.
`next_reader_ticket` identifies queued reader requests and
`reader_phase_cutoff` is an exclusive snapshot boundary. A reader with a ticket
below the cutoff belongs to the current phase; a reader at or above it belongs
to a later phase.

## 6. Acquisition algorithms

### 6.1 Read lock

Under the internal mutex:

1. Validate lifecycle state.
2. Search active reader owners and compare against the active writer owner.
   Reject recursive/cross-mode ownership immediately.
3. If free reader admission is closed, reserve a reader ticket and increment
   `waiting_readers`.
4. Wait while a writer is active or the reader's ticket is outside the current
   phase. A phase admits every reader below its cutoff, even when later writer
   tickets are queued.
5. Add the current thread to `active_reader_threads`. If allocation fails,
   decrement `waiting_readers`, unlock, and return the allocation error.
6. Decrement `waiting_readers` and increment `active_readers`.
7. Unlock the internal mutex and return success.

The active reader counter and owner-buffer size must always agree.

### 6.2 Write lock

Under the internal mutex:

1. Validate lifecycle state and reject ownership by the calling thread.
2. Reserve `writer_ticket = next_writer_ticket`, then increment
   `next_writer_ticket` and `waiting_writers`.
3. Wait while readers are active, another writer is active, a reader phase is
   open, or `writer_ticket != serving_writer_ticket`.
4. Decrement `waiting_writers`, mark the writer active, and store its thread ID.
5. Unlock the internal mutex and return success.

If condition waiting fails, ticket cancellation must not leave a hole that
blocks every later writer. Implement a small cancelled-ticket set or advance
over cancelled tickets while holding the mutex. Do not merely decrement
`waiting_writers`; the serving counter must remain able to reach the next live
ticket.

### 6.3 Try lock

Try functions take the internal mutex but never wait on a condition variable.
They return `FT_ERR_THREAD_BUSY` when their full admission predicate is false.
A failed try-write must not consume a writer ticket or close the reader gate.

## 7. Lifecycle and failure handling

Follow `AGENTS.md`, especially the PThread low-level exemptions and error
mapping rules.

- Initialization creates the mutex and both conditions transactionally. On
  failure, destroy every resource already created and leave destroyed state.
- `destroy()` succeeds as a no-op for an uninitialized or already destroyed
  lock if this matches the final PThread compatibility contract.
- `destroy()` returns `FT_ERR_THREAD_BUSY` while any owner or waiter exists and
  must leave the live lock unchanged.
- Successful destruction releases reader ownership storage, both conditions,
  and the mutex, and marks destroyed state.
- Every `pthread_*` failure is mapped through
  `cmp_map_system_error_to_ft()`.
- Preserve the first cleanup error while still attempting remaining cleanup
  where safe.
- Never destroy a condition variable that can still have a waiter.
- Every condition wait uses a `while` predicate to handle spurious wakeups.

The implementation must update `Modules/PThread/README.md`. No dependency-graph
change is expected if the implementation stays within PThread and uses existing
PThread-internal storage. If a new inter-module dependency is introduced,
update `Docs/module_dependency_graph.md` as required by `AGENTS.md`.

## 8. File placement

Keep the primitive with the other mutexes in `Modules/PThread`:

- declarations and `t_pt_rwlock` state in `Modules/PThread/pthread.hpp`, or a
  dedicated public `Modules/PThread/read_write_lock.hpp` included by
  `pthread.hpp` if the header becomes too large;
- implementation split into focused module-prefixed files such as
  `pthread_rwlock_initialize.cpp`, `pthread_rwlock_read.cpp`,
  `pthread_rwlock_write.cpp`, and `pthread_rwlock_destroy.cpp`;
- private owner/ticket helpers in `Modules/PThread/pthread_internal.hpp` or a
  private `pthread_rwlock_internal.hpp`;
- focused tests in `Test/Test/test_pthread_rwlock_writer_queue.cpp`,
  `test_pthread_rwlock_non_recursive.cpp`, and
  `test_pthread_rwlock_lifecycle.cpp`;
- public contract documentation in `Modules/PThread/README.md`.

Do not put this in `Modules/Threading`. That module supplies higher-level thread,
pool, and generic guard utilities; the synchronization primitive and its
platform conditions belong beside `pt_mutex` and `pt_recursive_mutex`.

## 9. `AGENTS.md` amendment required for adoption

The current thread-safety contract says class-owned mutexes must always be
heap-allocated `pt_recursive_mutex` objects. That rule conflicts with using a
non-recursive read/write lock as a class-owned synchronization primitive.

Before migrating a Libft class, add a narrow authoritative exception:

```text
Classes explicitly documented as read-mostly may own one heap-allocated
t_pt_rwlock configured with PT_RWLOCK_STRATEGY_WRITER_PRIORITY. Such classes
must use mode-specific read/write lock and unlock helpers, must not call a
public method that reacquires the same lock while already holding it, and must
not expose references or pointers whose validity depends on the lock after it
is released.
```

Do not weaken the recursive-mutex default for every class. Recursive locks are
still needed by existing call chains that deliberately nest public methods.
RW-lock migration requires an explicit call-graph audit first because this new
lock rejects nested acquisition.

Also add nullable safe helpers analogous to the existing mutex helpers:

```cpp
int32_t pt_rwlock_rdlock_if_not_null(t_pt_rwlock *rwlock);
int32_t pt_rwlock_rdunlock_if_not_null(t_pt_rwlock *rwlock);
int32_t pt_rwlock_wrlock_if_not_null(t_pt_rwlock *rwlock);
int32_t pt_rwlock_wrunlock_if_not_null(t_pt_rwlock *rwlock);
```

Higher-level Libft callers must explicitly ignore unlock returns in the same
style as the existing locking rules, after the primitive's owner checks have
been fully tested.

## 10. Required tests

Tests must use explicit synchronization barriers/conditions instead of timing
alone wherever possible.

### 10.1 Core concurrency

- Two readers enter concurrently and both remain active.
- A writer cannot enter while either reader is active.
- The last reader leaving wakes the first writer.
- No reader overlaps an active writer.
- No two writers overlap.

### 10.2 Writer gate and ordering

- Reader A is active; W1 queues; reader B arrives; prove B does not enter before
  W1 completes.
- W1, W2, and W3 queue in known ticket order; prove acquisition order is exactly
  W1, W2, W3.
- Readers queued behind W1 enter as one phase after W1, even when W2 is already
  queued behind them.
- Readers arriving after a reader-phase cutoff remain blocked until the phase
  completes; they enter in a later phase.
- All readers selected for one phase can enter concurrently.
- A later writer cannot overtake an earlier writer.

### 10.3 Non-recursion and ownership

- read then read on the same thread fails immediately;
- read then write fails immediately;
- write then write fails immediately;
- write then read fails immediately;
- read unlock by a non-owner fails;
- write unlock by a non-owner fails;
- wrong-mode unlock fails without changing lock state;
- double unlock fails.

### 10.4 Lifecycle and failures

- null, uninitialized, destroyed, and double-destroy behavior;
- destroy while read-owned, write-owned, or with waiters returns busy;
- mutex and each condition initialization failure cleans up correctly;
- reader-owner bookkeeping allocation failure rolls back all counters;
- condition wait and wake failure preserves a usable state or reports a clearly
  documented terminal state;
- cancelled writer tickets cannot strand later writers;
- ticket normalization near `UINT64_MAX`;
- repeated initialize/destroy cycles;
- ThreadSanitizer stress with mixed readers and writers.

### 10.5 Performance

Add an efficiency probe comparing:

- `pt_mutex` protecting the same read-heavy payload;
- native `pthread_rwlock_t` wrappers;
- writer-queued `t_pt_rwlock`.

Measure uncontended read, concurrent read scaling, uncontended write, and mixed
95/5 read/write contention. Performance is not allowed to weaken correctness,
but the new lock should show a measurable benefit before broad migration.

## 11. Call patterns and exact functions to use

### 11.1 Primitive initialization and destruction

For a directly owned `t_pt_rwlock`, call:

```cpp
error_code = pt_rwlock_strategy_init(&lock,
    PT_RWLOCK_STRATEGY_WRITER_PRIORITY);
```

Do this during the owning object's explicit `initialize()`, after all
non-lock state has been reset but before publishing the object as initialized.
If later initialization fails, call `pt_rwlock_strategy_destroy(&lock)` during
rollback.

During normal destruction:

```cpp
error_code = pt_rwlock_strategy_destroy(&lock);
```

Call this only after the object has stopped accepting operations and all worker
threads that can access it have joined. `FT_ERR_THREAD_BUSY` means shutdown order
is wrong; do not loop and retry while new callers can still enter.

For optional class thread safety, allocate the lock on the heap in
`enable_thread_safety()` and initialize it with writer priority. In
`disable_thread_safety()`, first prevent new object operations, then destroy the
lock, deallocate it, and set the owning pointer to `ft_nullptr` according to the
final `AGENTS.md` exception.

### 11.2 Read-only public method

Use `pt_rwlock_strategy_rdlock()` immediately before the first protected field
read and `pt_rwlock_strategy_rdunlock()` immediately after the last read:

```cpp
int32_t example::read_value(uint32_t *value) const noexcept
{
    int32_t error_code;

    if (value == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    error_code = pt_rwlock_strategy_rdlock(this->_rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    *value = this->_value;
    (void)pt_rwlock_strategy_rdunlock(this->_rwlock);
    return (FT_ERR_SUCCESS);
}
```

Validate arguments that do not depend on protected state before locking. Validate
protected state only after lock acquisition. Never return a pointer, reference,
iterator, span, or proxy into protected storage after releasing the read lock.

### 11.3 Mutating public method

Use `pt_rwlock_strategy_wrlock()` around the complete invariant-changing
transaction:

```cpp
int32_t example::set_value(uint32_t value) noexcept
{
    int32_t error_code;

    error_code = pt_rwlock_strategy_wrlock(this->_rwlock);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = this->set_value_locked(value);
    (void)pt_rwlock_strategy_wrunlock(this->_rwlock);
    return (error_code);
}
```

Do not unlock between related writes merely to shorten the critical section. If
readers can observe half of an invariant update, the lock is incorrectly placed.
Instead, perform expensive preparation outside the lock and keep only validation
and the non-failing commit inside it.

### 11.4 Public-to-private call structure

Because the lock is non-recursive, a locked public method must never call another
public method that acquires the same lock. Split shared logic into `_locked`
helpers:

```text
read_block()
  -> rdlock()
  -> read_block_locked()
  -> rdunlock()

apply_block_edit()
  -> wrlock()
  -> find_player_override_locked()
  -> apply_block_edit_locked()
  -> wrunlock()
```

Private `_locked` helpers:

- never acquire or release the owning lock;
- state the required mode in a comment at the declaration;
- are called only while that mode is held;
- do not call public methods on the same object;
- use another `_locked` helper when sharing work.

Do not use a name ending in `_locked` for a function that conditionally acquires
the lock. The suffix must mean the caller already owns it.

### 11.5 Try-lock calls

Use `pt_rwlock_strategy_try_rdlock()` or
`pt_rwlock_strategy_try_wrlock()` only when skipping or deferring the operation
is semantically valid. Examples include optional diagnostics, a best-effort
metrics snapshot, or rescheduling non-urgent remesh capture.

Do not spin in a loop around a try-lock. A loop such as
`while (try_lock() == FT_ERR_THREAD_BUSY)` consumes CPU, defeats the condition
queue, and can delay the owner. Either use the blocking function or return/defer
with bounded backoff owned by the caller.

### 11.6 Multi-object operations

When one operation needs two or more read/write locks, sort lock addresses and
acquire them in ascending address order, matching the existing `AGENTS.md`
multi-object rule. Mode does not change ordering: a read lock and a write lock
must still follow the same global address order.

If lock A must be upgraded from read to write, release all acquired locks and
restart the operation in address order with A requested as write. Revalidate all
observations after reacquisition. Never retain a read lock while waiting for a
write lock on the same object.

For Minecraft neighbor snapshots, prefer one world/index read lock protecting
the lookup and copying phase over taking five chunk locks in arbitrary spatial
order. If individual chunk locks are required, place their addresses into a
fixed local array, sort them, acquire all reads, copy the borders, and release in
reverse order.

### 11.7 Error and cleanup pattern

Every successful acquisition must have exactly one matching mode-specific
unlock on every return path. Prefer one cleanup label in non-RAII Libft code when
the body has many failures:

```cpp
error_code = pt_rwlock_strategy_wrlock(this->_rwlock);
if (error_code != FT_ERR_SUCCESS)
    return (error_code);
error_code = this->perform_locked_work();
(void)pt_rwlock_strategy_wrunlock(this->_rwlock);
return (error_code);
```

Do not overwrite the operation error with an unlock result at higher-level call
sites. The low-level primitive tests must prove unlock ownership and state
handling. If a platform unlock failure is considered unrecoverable, centralize
that policy inside PThread rather than leaving protected data exposed with an
ambiguous state.

### 11.8 Function mapping for `game_voxel_chunk`

The implementation audit should start with this mapping. Exact classifications
may change if a function calls another method or returns borrowed storage.

| Function family | Lock call | Unlock call | Notes |
| --- | --- | --- | --- |
| `read_block`, `get_generated_block` | `pt_rwlock_strategy_rdlock` | `pt_rwlock_strategy_rdunlock` | Move shared indexing into read-locked helpers. |
| `is_dirty`, `is_generation_protected`, `has_generation_metadata`, `generation_metadata_matches`, `get_biome_id`, `is_block_player_modified`, `get_player_override_count`, `get_player_override`, `get_revision` | read lock | read unlock | Copy scalar/result data before unlock. |
| `serialize` | read lock | read unlock | Prefer snapshot-then-serialize if serialization allocates or is slow. |
| `write_block`, `apply_block_edit`, `apply_authoritative_block_change`, `apply_authoritative_block_delta`, `write_generated_block` | `pt_rwlock_strategy_wrlock` | `pt_rwlock_strategy_wrunlock` | Entire block, override, dirty, metadata, and revision update is one transaction. |
| `clear_dirty`, `clear_persistence_dirty`, `clear_generation_metadata`, `set_generation_metadata`, `set_biome_id` | write lock | write unlock | Convert nested calls to `_locked` helpers. |
| `deserialize`, `move` | write lock | write unlock | Build temporary state first where possible; commit under write lock. Multi-object move follows address order; moving a source with queued waiters returns busy. |
| `get_section`, `get_generation_metadata` | no safe internal lock-only fix | n/a | Replace concurrent usage with copied snapshots or scoped handles. |
| `initialize`, `destroy` | lifecycle-exclusive access | n/a | Call only before publication/after quiescence; do not race lifecycle with normal operations. |

`game_voxel_chunk_section` may also need locking only if callers can bypass the
chunk and mutate a section directly. The preferred fix is to stop exposing
mutable sections to concurrent callers rather than adding one lock per section.

## 12. Performance traps

### 12.1 Critical sections that are too large

Do not hold a read lock while generating a mesh, writing a file, formatting
JSON, logging, allocating a large buffer, waiting on I/O, or calling user code.
Copy a bounded snapshot under the lock and perform expensive work afterward.

Do not hold the world/index lock for the full lifetime of a worker request. The
current Minecraft snapshot model is preferable because lock duration is bounded
by copying, not terrain generation time.

### 12.2 Critical sections that are too small

Locking once per voxel in a three-dimensional loop can cost more than the work
it protects. Add bulk APIs such as `copy_section_snapshot()` or
`copy_chunk_snapshot()` that acquire one read lock for the whole bounded copy.
Likewise, apply a batch of edits under one write acquisition when they form one
revision transaction.

### 12.3 Reader ownership lookup cost

Scanning `active_reader_threads` is O(number of active readers). This is
acceptable for a small worker pool but should be measured. Keep the vector
capacity after readers leave to avoid allocation churn. If reader counts become
large, replace linear lookup with PThread-owned thread-local ownership records or
a fixed-capacity fast path plus overflow storage; do not remove ownership checks.

### 12.4 Writer broadcast herd

Strict ticket ordering with one writer condition uses broadcast so the correct
ticket wakes. Under many queued writers this creates a thundering herd. Measure
context switches and wakeups. If it is material, add per-ticket wait nodes or a
small queue of waiter conditions, but keep lifetime and cancellation safe. Do
not switch back to arbitrary `signal()` and claim FIFO.

### 12.5 Writer preference under write bursts

Readers arriving after a phase cutoff can be delayed by a continuous write
stream. Keep writes short and batch related mutations so the queue drains. Do
not allow new readers to join an already-open phase, because that would let a
continuous reader stream starve the next writer.

### 12.6 Cache-line contention

Counters and ticket fields are written on every acquisition. Keep hot queue
state together when it benefits the internal-mutex critical section, but keep
unrelated diagnostics out of the same cache line where possible. Do not add
atomics to every field; the internal mutex already serializes state and extra
atomics can increase cache traffic without adding correctness.

### 12.7 False gains from uncontended benchmarks

An RW lock is normally slower than a simple mutex when there is no contention or
when critical sections are tiny. Require mixed and concurrent benchmarks. Do not
migrate a type merely because it has more getter methods than setter methods;
measure simultaneous read demand and lock hold time.

### 12.8 Nested lock amplification

A read method that calls ten other read methods must not acquire and release the
same lock ten times. Besides failing the non-recursive contract, this inflates
queue traffic. Acquire once at the public boundary and call `_locked` helpers.

### 12.9 Allocation while locked

Allocation can be slow, fail, or acquire unrelated allocator locks. Prepare new
buffers and serialized output before the write lock when possible, then
revalidate version/state and perform a non-failing swap or move under the lock.
The active-reader bookkeeping allocation is an unavoidable primitive-internal
exception and must use PThread-owned allocation.

### 12.10 Logging and instrumentation

Do not log synchronously while holding the internal mutex or protected object
lock. Record compact counters/timestamps, release the lock, then emit logs.
Otherwise logger locks can invert lock order or turn contention diagnostics into
the contention source.

## 13. Correctness and integration traps

### 13.1 Treating `volatile` or atomics as lock replacement

`volatile` does not synchronize threads. Individual atomic fields do not make a
multi-field chunk revision transactional. Use the read/write lock for compound
invariants and atomics only for deliberately independent publication fields.

### 13.2 Checking state before acquiring the lock

Code such as `if (!dirty) return; wrlock();` races with writers. Any decision
based on protected mutable state must be made after acquisition and rechecked
after try-lock retry or lock-set reacquisition.

### 13.3 Borrowed data escaping the lock

Returning `WorldChunk *`, a section reference, metadata reference, iterator, or
raw buffer pointer is not made safe by locking only inside the getter. The data
can change or be destroyed immediately after return. Return a copy, retain the
lock through a scoped handle, or keep access on the owning thread.

### 13.4 Calling unknown code while locked

Callbacks, virtual methods, allocators, logger sinks, and renderer APIs can take
other locks or call back into the same object. Do not invoke them while holding
the RW lock unless their lock behavior is fully documented and included in the
global lock order.

### 13.5 Upgrade and downgrade assumptions

Releasing read and acquiring write is not an atomic upgrade. Another writer may
change state between operations, so revalidate a revision or generation counter.
Never manually modify owner counters to simulate upgrade/downgrade.

### 13.6 Cancellation leaving ticket holes

A writer that exits after taking a ticket but before acquisition can permanently
block the queue if its ticket is never served. Every error, cancellation, thread
shutdown, and condition-wait failure path must mark the ticket cancelled and
advance over cancelled tickets while holding the internal mutex.

### 13.7 Condition-variable lost wakeups

Change predicates and issue the corresponding signal/broadcast while holding the
internal mutex. Wait only in `while` loops that test the complete predicate.
Never check the predicate, unlock manually, and then wait.

### 13.8 Counter and owner disagreement

Update the owner record and counter as one mutex-protected transaction. Tests
must assert `active_readers == active_reader_threads.size`. Do not decrement a
counter before proving the calling thread owns that mode.

### 13.9 Destroy racing with admission

A boolean `destroying` flag checked outside the internal mutex is insufficient.
Transition to a closing state under the mutex, reject new acquisitions, wait for
callers at the owning subsystem level, then destroy only when owner/waiter counts
are zero. The primitive itself should return busy rather than waiting during
destroy, because waiting can deadlock shutdown.

### 13.10 Copying or moving the lock bytes

Never copy `t_pt_rwlock` with `ft_memcpy`, structure assignment, container
relocation, or serialization. Native mutex and condition objects are not
relocatable. Store the lock at a stable address, normally through a heap pointer,
and explicitly delete copying/moving in any C++ owner.

### 13.11 Fork and process sharing

This design is process-local unless initialization explicitly supports
process-shared mutex and condition attributes and all ownership bookkeeping is
placed in shared memory. Do not use it across `fork()` boundaries or shared
memory merely because native pthread primitives can sometimes be configured for
that purpose.

### 13.12 Exceptions and early exits

Libft production code should follow its explicit error-return conventions. In
Minecraft code that can throw, use a verified scoped adapter so exceptions do
not leak a lock. Do not mix manual unlock and a scoped guard for the same
acquisition.

### 13.13 Lock-order inversion

Document the order between world lock, chunk lock, registry lock, pipeline mutex,
renderer resources, logger, and allocator internals. A suggested Minecraft order
is:

```text
world/index -> chunk -> registry snapshot only
```

The pipeline queue mutex should never be held while acquiring a world or chunk
lock. Renderer and logger calls occur after all world/chunk locks are released.
Confirm this order against actual call graphs before implementation.

### 13.14 Assuming readers make unsafe code safe

Concurrent readers are safe only when they perform no hidden mutation. Audit
`const` methods for lazy initialization, caches, error-state writes, metrics,
reference counts, and mutable members. Libft's thread-local `_last_error` is
compatible, but shared diagnostic fields are writes and require suitable
handling.

### 13.15 Using the lock where ownership is better

Prefer immutable snapshots, message passing, and single-thread ownership when
they already fit the architecture. Minecraft's worker result publication is a
good example: workers build private data and the main thread commits it. Do not
replace that model with long-lived shared mutable world access.

## 14. Minecraft adoption plan

Minecraft currently keeps live `World` state on the main thread and gives world
generation workers private requests, snapshots, and results. Preserve that
ownership model. A read/write lock is not permission for workers to retain raw
pointers into live world state.

### 14.1 Do not replace `WorldGenerationPipeline::mutex_`

`WorldGenerationPipeline::mutex_` protects request/result queue mutation and is
used with `std::condition_variable`. Its operations are writes, and condition
waiting requires a compatible exclusive lock. Replacing it with an RW lock adds
complexity without increasing parallelism. Keep the existing mutex there.

### 14.2 First useful target: read-mostly chunk payloads

`game_voxel_chunk` is the strongest eventual candidate:

- meshing and visibility perform many block reads;
- player edits, generation commits, deserialization, and metadata changes are
  writes;
- simultaneous readers can safely scale if they operate on one stable chunk;
- once a write arrives, later mesh/query readers should wait until the edit is
  fully committed.

This migration must be done in Libft, not by wrapping calls only in Minecraft,
because the mutable sections, override table, dirty flags, metadata, and revision
belong to `game_voxel_chunk`.

Classify methods before changing code:

- read lock: `read_block`, `get_generated_block`, dirty/protection/revision
  queries, metadata value queries, override queries, and serialization;
- write lock: block writes/edits/deltas, deserialize, move, metadata setters,
  dirty clearing, biome setters, initialize, and destroy;
- audit required: methods returning borrowed references such as `get_section()`.
`get_generation_metadata()` now returns a thread-local value snapshot.

The Minecraft `WorldChunk` wrapper keeps its lock at a stable heap address for
the lifetime of the slot. Eviction destroys the payload without relocating the
wrapper, and every wrapper read/write/revision/snapshot operation rechecks the
slot's initialized state while holding that lock. A stale slot therefore
returns `FT_ERR_INVALID_STATE` instead of entering a destroyed
`game_voxel_chunk`; neighbor lookup maps that result to an air block. The
wrapper explicitly deletes copy and move operations so the lock cannot be
relocated by C++ object copying.

Reference-returning APIs cannot safely acquire and release an internal lock
inside the getter because the caller uses the reference after unlock. Replace
concurrent call paths with copy/snapshot APIs or an explicitly scoped callback
that keeps the read lock for the entire access. Do not claim these methods are
thread-safe until their lifetime contract is fixed.

Avoid nested lock acquisition by splitting methods into:

- public methods that acquire exactly one read or write lock; and
- private `_locked` helpers that assume the caller already owns the correct
  mode and never acquire it again.

### 14.3 Second target: block registry snapshots

The voxel block registry is read frequently during generation and meshing and
changes comparatively rarely at runtime. Use a writer-queued lock around
registry lookup versus register/remove/reset operations only if profiling shows
its existing synchronization is a bottleneck. Prefer immutable registry
snapshots for worker jobs when possible; snapshots remove lock hold time from
the hot inner voxel loops.

### 14.4 World chunk index

`World::find_chunk()` and the chunk index are read-heavy, while loading,
eviction, recentering, and result publication are writes. They appear suitable
at first glance, but both find methods return raw `WorldChunk *` pointers. A lock
held only during lookup does not protect later pointer use.

Do not add a superficial lock around `find_chunk()`. Choose one of these safe
interfaces first:

1. copy the required chunk/border data into a snapshot while holding a read
   lock, then release it;
2. execute a bounded callback while the read lock remains held; or
3. return a scoped read handle whose lifetime owns the read lock and which
   cannot outlive `World`.

World load, eviction, index rebuild, revision commit, and destruction take the
write lock. Rendering and long-running generation must not hold a world read
lock across expensive work; snapshot quickly and release.

### 14.5 Terrain configuration

`World::terrain_generation_settings()` currently returns a reference. If terrain
configuration becomes mutable while workers run, replace this with a copied,
versioned configuration snapshot under a read lock. Configuration replacement
takes the write lock, increments the world/configuration epoch, and causes stale
worker results to be rejected. Do not expose a reference beyond the lock.

### 14.6 Migration order

1. Implement and verify the PThread primitive without changing Minecraft.
2. Add PThread README documentation and the narrow `AGENTS.md` exception.
3. Benchmark `game_voxel_chunk` reads and registry lookups.
4. Migrate one Libft read-mostly type, using `_locked` helpers and snapshot-safe
   APIs.
5. Run Libft's focused tests, full suite, ThreadSanitizer, and efficiency probe.
6. Update Minecraft's Libft submodule pointer.
7. Convert one Minecraft consumer at a time, beginning with snapshot capture or
   registry access rather than the pipeline queue.
8. Add Minecraft stress tests for edits racing with snapshot/mesh reads and for
   writer progress under continuous reads.
9. Profile frame time and lock contention before migrating additional state.

## 15. Minecraft acceptance tests

- Concurrent snapshot readers observe one complete chunk revision, never a mix
  of pre-edit and post-edit fields.
- A queued block edit prevents later snapshot readers from entering until the
  edit, override bookkeeping, dirty flags, and revision increment are complete.
- Existing readers can finish after an edit queues; the edit then makes forward
  progress under continuous attempted reads.
- Mesh generation uses an immutable snapshot and does not hold a live chunk or
  world lock during expensive mesh construction.
- World destruction and chunk eviction cannot race with a scoped reader.
- Terrain configuration replacement invalidates old worker requests/results.
- Main-thread frame time does not regress under normal streaming.
- ThreadSanitizer reports no races in chunk lookup, snapshot capture, editing,
  revision publication, or shutdown tests.

## 16. Acceptance gates

Implementation is complete only when:

- writer FIFO order and the reader gate are proven by deterministic tests;
- every recursive and cross-mode reacquisition fails without blocking;
- non-owner and wrong-mode unlocks cannot corrupt counters;
- no active or waiting lock can be destroyed;
- all injected initialization, allocation, wait, and wake failures are covered;
- the public PThread README distinguishes native and deterministic semantics;
- `AGENTS.md` contains the narrow class-owned RW-lock exception before any Libft
  class adopts it;
- Linux, macOS, and Windows builds and focused tests pass;
- ThreadSanitizer stress passes;
- Minecraft retains snapshot ownership and does not expose unprotected live
  pointers merely because the lock exists;
- profiling demonstrates a benefit at the first migrated read-heavy call site.
