# Persistent Networking Event Loop Design

Status: implemented on the current branch; retained as an archived implementation record

Scope: `Modules/Networking` event-loop registration, polling, wakeup,
thread-safety, platform backends, tests, and performance validation.

## 1. Purpose

The current `event_loop` stores persistent read and write registrations in two
descriptor arrays, then passes those arrays directly to `nw_poll()`. The
`nw_poll()` contract uses those arrays as both input and readiness output by
replacing non-ready entries with `-1`. This destroys the event loop's watch set
after any partial readiness result.

The Linux implementation also creates an epoll descriptor, registers every
socket, allocates readiness buffers, waits, and destroys everything for every
call. It holds the event-loop mutex throughout that work. A socket requested
for both reading and writing is added twice and normally fails with `EEXIST`.

This design separates persistent registration state from per-run readiness
state and gives the event loop a persistent platform poller. It preserves the
existing one-shot `nw_poll()` API for callers that depend on its destructive
readiness-array behavior.

## 2. Goals

- Registrations survive any number of calls to `event_loop_run()`.
- One socket can request read, write, or both interests.
- Linux performs registration work on add/remove/update, not every wait.
- `event_loop_run()` does not hold the registry mutex while blocking.
- Another thread can add or remove a socket while the poll thread waits.
- Structural changes can wake an infinite or long-running wait promptly.
- Readiness output is bounded, explicit, and separate from registrations.
- Destruction cannot race with an active wait.
- Linux, kqueue platforms, and poll/select fallback expose the same semantics.
- Existing direct `nw_poll()` callers remain source-compatible.

## 3. Non-goals

- Do not replace higher-level protocol state machines or message transport.
- Do not invoke arbitrary user callbacks while holding an event-loop lock.
- Do not make socket ownership belong to the event loop. The loop observes file
  descriptors; callers still open and close sockets.
- Do not silently close registered sockets during removal or destruction.
- Do not convert every current `nw_poll()` caller to the persistent event loop
  in the first change.

## 4. Required semantics

### 4.1 Registration

Each file descriptor has one logical registration with an interest mask:

- `EVENT_LOOP_INTEREST_READ`
- `EVENT_LOOP_INTEREST_WRITE`
- both flags combined

Adding an already-registered descriptor merges the requested interest. Adding
an interest already present is idempotent success. Removing one interest keeps
the descriptor registered if another interest remains. Removing the final
interest unregisters it from the backend.

Writer/read terminology from synchronization primitives does not apply here:
read and write interests are independent readiness bits and may be reported in
the same event.

### 4.2 Readiness

Polling never mutates registration state. Each returned event contains:

```cpp
struct event_loop_ready_event
{
    int32_t file_descriptor;
    uint32_t ready_mask;
    int32_t error_code;
};
```

`ready_mask` may contain read, write, hangup, and error flags. Hangup/error must
not be discarded merely because the caller requested only read or write;
callers need those states to terminate connections cleanly.

One descriptor appears at most once in a result batch. If it is ready for both
read and write, both bits are set in that one event.

### 4.3 Concurrency

Only one thread may execute the backend wait for one event-loop instance at a
time. Concurrent calls to `event_loop_run()` return `FT_ERR_THREAD_BUSY` (or an
equivalent documented error) instead of racing over a shared readiness buffer.

Add, update, and remove operations may run while another thread is blocked in
`event_loop_run()`. Destruction must wake the waiter and wait for it to leave
the backend before releasing backend state.

Socket closure follows this required order:

1. Remove all interests from the event loop.
2. Wait for/remediate any readiness event already handed to the consumer.
3. Close the socket.

Closing first risks descriptor-number reuse and stale events referring to an
unrelated newly opened socket.

## 5. Public API direction

Keep the old functions during migration, but add explicit readiness output:

```cpp
int32_t event_loop_initialize(event_loop *loop) noexcept;
int32_t event_loop_destroy(event_loop *loop) noexcept;

int32_t event_loop_add_interest(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask) noexcept;
int32_t event_loop_remove_interest(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask) noexcept;

int32_t event_loop_wait(event_loop *loop, event_loop_ready_event *events,
    uint32_t event_capacity, uint32_t *event_count,
    int32_t timeout_milliseconds) noexcept;
int32_t event_loop_interrupt(event_loop *loop) noexcept;
```

Compatibility wrappers map as follows:

- `event_loop_init()` calls/forwards to initialization while its legacy `void`
  return remains supported.
- `event_loop_clear()` calls destruction.
- `event_loop_add_socket(loop, fd, FT_FALSE)` adds read interest.
- `event_loop_add_socket(loop, fd, FT_TRUE)` adds write interest.
- `event_loop_remove_socket()` removes the corresponding interest.
- `event_loop_run()` waits into an internal bounded readiness buffer and
  returns the ready count, but does not alter registrations.

New code should call the explicit APIs. Once all in-tree users migrate, the
legacy wrappers can be deprecated separately.

Direct `nw_poll()` remains a one-shot compatibility function. Its documentation
must explicitly say that descriptor arrays are filtered in place. The event
loop must never pass persistent storage to it.

The checked-out branch's `udp_event_loop_wait_internal()` currently saves an
array index, calls `event_loop_run()`, inspects whether that array slot survived,
restores the descriptor manually, and then removes it. That restoration is a
workaround for destructive `nw_poll()` output. It must migrate in the same
change that makes `event_loop_run()` non-destructive: wait for an explicit
`event_loop_ready_event`, check its mask, and remove the temporary interest.
There must not be an intermediate commit where `event_loop_run()` changes but
the UDP helper still infers readiness from mutated registration arrays.

## 6. Internal state

The event loop should own:

```cpp
struct event_loop_registration
{
    int32_t file_descriptor;
    uint32_t interest_mask;
    uint64_t generation;
};

struct event_loop
{
    int32_t *read_file_descriptors;
    int32_t read_count;
    int32_t *write_file_descriptors;
    int32_t write_count;
    pt_mutex *mutex;
    ft_bool thread_safe_enabled;
    event_loop_registration *registrations;
    uint32_t registration_count;
    uint32_t registration_capacity;
    event_loop_ready_event *ready_events;
    uint32_t ready_capacity;
    void *backend_events;
    uint32_t backend_event_capacity;
    int32_t backend_descriptor;
    int32_t wakeup_read_descriptor;
    int32_t wakeup_write_descriptor;
    pt_mutex *wait_mutex;
    ft_bool wait_active;
    uint64_t next_generation;
    ft_bool stopping;
};
```

Exact names may be adapted to repository conventions. Important invariants:

- Registrations are unique by file descriptor.
- `interest_mask` is never zero for a stored registration.
- Generations increase whenever a descriptor is newly registered.
- Backend state and registration state are committed together while locked.
- The wait mutex serializes backend waits but is not held by add/remove.
- The registry mutex (`mutex`) is never held across `epoll_wait()`, `kevent()`, or
  `poll()`.
- Wakeup descriptors are internal and never returned to callers.

The generation protects against file-descriptor reuse. On Linux, store a token
containing descriptor identity and generation in `epoll_event.data.u64`, then
discard an event if its generation no longer matches the current registration.

## 7. Linux backend

### 7.1 Initialization

`event_loop_initialize()` performs, in order:

1. Validate the object state.
2. Allocate and initialize synchronization objects.
3. Create one epoll descriptor with `epoll_create1(EPOLL_CLOEXEC)`.
4. Create a nonblocking close-on-exec `eventfd` for wakeups. A nonblocking pipe
   is an acceptable fallback if `eventfd` is unavailable.
5. Register the wakeup descriptor with `EPOLLIN`.
6. Commit initialized state only after all steps succeed.

Every failure path closes created descriptors and frees initialized resources.

### 7.2 Add/update

Under the registry mutex:

1. Find the descriptor registration.
2. Calculate the merged mask.
3. Allocate registration capacity before changing the backend.
4. Translate the merged mask to epoll flags.
5. Use `EPOLL_CTL_ADD` for a new descriptor or `EPOLL_CTL_MOD` for an existing
   descriptor.
6. Commit the registration table only after `epoll_ctl()` succeeds.
7. Signal the wakeup descriptor so a blocked waiter observes structural change.

Include `EPOLLRDHUP` where supported. Start with level-triggered behavior.
Do not introduce `EPOLLET` until all consumers are proven to drain sockets to
`EAGAIN`; edge-triggered mode is a common source of lost wakeups.

### 7.3 Remove

Under the registry mutex:

1. Find the descriptor and calculate the remaining mask.
2. If interests remain, call `EPOLL_CTL_MOD`.
3. If none remain, call `EPOLL_CTL_DEL`.
4. Commit the table change after backend success.
5. Increment/invalidate the generation before descriptor reuse is possible.
6. Wake the waiter.

Treat `ENOENT` during final removal carefully: it may indicate backend drift.
Do not silently claim success unless registration state is reconciled and an
error policy is documented.

### 7.4 Wait

`event_loop_wait()` performs:

1. Acquire the wait mutex or return busy.
2. Check stopping state under the registry mutex, then release it.
3. Call `epoll_wait()` without the registry mutex.
4. Retry after `EINTR` while recomputing the remaining finite timeout from a
   monotonic deadline.
5. Drain the wakeup descriptor when it appears.
6. Validate each event token against current descriptor generation.
7. Merge duplicate readiness for one descriptor into one output event.
8. Return explicit count and status.
9. Release the wait mutex on every path.

If the wait was only woken for structural change and no user descriptor is
ready, return success with zero events. The caller may immediately wait again.

## 8. Other platform backends

### 8.1 kqueue

The implementation keeps one kqueue descriptor. It adds, updates, and deletes
`EVFILT_READ` and `EVFILT_WRITE` filters as interests change, and uses a
nonblocking pipe for wakeup. Generation tokens are carried through `udata`, and
read/write filters are merged into one public readiness event.

### 8.2 poll/select fallback

Maintain the canonical registration table. Before each wait, copy only the
descriptor/mask snapshot into temporary `pollfd` storage while holding the
registry mutex, then release the mutex before `poll()`. A wakeup pipe must be
part of the snapshot so add/remove interrupts the wait.

After return, validate readiness against the current registration state. The
fallback uses temporary snapshots and remains O(N), but preserves
non-destructive registrations and wakeup-driven modification semantics. Native
epoll and kqueue paths are the zero-allocation steady-state implementations.

### 8.3 Windows

The first implementation may use the existing Windows polling mechanism with
the same snapshot and wakeup rules. A later IOCP redesign is separate scope.
Do not expose epoll-specific semantics through the public API.

## 9. Locking and lifetime rules

The lock order is always:

1. wait mutex, when serializing wait lifecycle;
2. registry mutex, only for short state transitions.

Add/remove take only the registry mutex. They must never attempt to acquire the
wait mutex. This prevents add/remove from waiting behind network inactivity.

No callback, socket I/O, or blocking wait occurs while the registry mutex is
held. Registration changes may allocate the canonical table and the legacy
compatibility mirrors while holding this short-lived mutex; these allocations
are bounded by the requested registration count and never occur on the native
persistent wait hot path. High-frequency registration churn should preallocate
where possible.

`event_loop_destroy()` sets `stopping`, signals wakeup, synchronizes with the
wait mutex, unregisters/closes backend descriptors, frees registrations and
buffers, destroys mutexes, and transitions state. It must reject or safely
coordinate concurrent add/remove operations.

Thread-safety setup must happen only in initialization before publication.
Remove lazy `event_loop_prepare_thread_safety()` calls from normal operations.
If the compatibility function remains public temporarily, make it an
initialization-only helper with an atomic lifecycle gate or document that it
requires exclusive ownership.

## 10. Error handling

- Invalid descriptor, mask, timeout, capacity, or output pointer returns an
  argument error.
- Allocation failure leaves registration and backend state unchanged.
- `epoll_ctl()` failure leaves the canonical table unchanged.
- `EINTR` is retried without extending finite timeout.
- Backend hangup/error readiness is returned as an event, not as a wait failure.
- Internal wakeup saturation (`EAGAIN`) is success because a wakeup is already
  pending.
- Destroy is best-effort and returns the first cleanup error after attempting
  all cleanup.
- No public operation exposes raw `errno` as its only diagnostic; map it to the
  Libft error contract while preserving platform detail where supported.

## 11. Implementation record

The implementation completed the following work:

- Added persistent-registration, read/write merge, blocked-wait modification,
  destruction, timeout, interruption, hangup, busy-waiter, and repeated-wait
  regression tests.
- Added a direct `nw_poll()` regression test for one descriptor requested in
  both read and write arrays.

### Caller migration

- Migrated the UDP event-loop helper to explicit readiness events; it no longer
  restores descriptors mutated by `nw_poll()`.
- Kept one-shot HTTP and other isolated waits on `nw_poll()` where persistent
  state has no benefit.
- The message transport continues to own callback dispatch and its existing
  wait abstraction; the event loop never invokes transport callbacks.

### Checked-out branch integration constraints

This design is based on the current `agent/flattened-make-terrain` branch, not
only `origin/main`. That branch contains substantial message-transport,
handshake, secure-channel, simulator, NAT traversal, and owner-thread callback
work. The event-loop implementation must preserve these established contracts:

- `networking_message_transport::poll(timeout)` obtains `_io` while holding the
  transport lock, releases that lock before `wait_readable()`, and then returns
  to owner-thread `poll()`. Do not reintroduce a transport-wide lock around a
  blocking wait.
- Message-transport callbacks remain on the owner thread. The event-loop waiter
  only reports readiness; it does not directly invoke transport callbacks.
- Existing command-queue synchronization and `FT_ERR_THREAD_BUSY` behavior are
  independent of the event-loop wait mutex and must remain intact.
- Connected UDP send/close synchronization already present on the branch must
  keep its lock order. The event loop unregisters interest before socket close
  and must not acquire transport/socket locks from inside the registry lock.
- `networking_datagram_io::wait_readable()` is currently the transport's wait
  abstraction. Migration should first provide an event-loop-backed
  implementation behind that interface, then change transport code only if a
  measured benefit justifies it.
- One-shot HTTP connection waits may continue using `nw_poll()`; they must not
  be mechanically moved into shared persistent state.

For integration verification, record the branch tip and inspect the diff for every
touched Networking file. During rebase/conflict resolution, preserve branch
behavior explicitly and rerun message-transport, secure-channel, simulator,
UDP close/send, callback-owner-thread, and command-lifetime tests. A clean
event-loop test run alone is insufficient acceptance on this branch.

### Remaining cleanup scope

The legacy read/write arrays and `event_loop_run()` wrapper remain for source
compatibility. `event_loop_run()` uses a temporary readiness buffer because it
does not expose readiness events; new code should use `event_loop_wait()`.
`event_loop_prepare_thread_safety()` remains an initialization-state check and
does not lazily construct synchronization.

This document is intentionally stored under `Docs/old/` now that the design
has been implemented.

## 12. Correctness tests

### 12.1 Persistent registration

Register three socket pairs for read. Make A readable, wait, and verify only A
is reported. Consume A. Make B readable, wait again, and verify B is still
registered. Repeat with C. Verify all registrations remain removable.

### 12.2 Combined read/write interest

Register one connected socket for read and write. Verify registration succeeds
without `EEXIST`. Make incoming data available and verify one event can contain
both readiness bits where the platform reports both.

### 12.3 Interest updates

- Add read, then add write: registration becomes read+write.
- Remove read: write remains.
- Remove write: descriptor disappears.
- Repeated add of the same bit is idempotent.
- Removing a missing interest follows the documented error/idempotency policy.

### 12.4 Concurrent modification

Start an infinite wait in thread A. In thread B, add a socket and make it ready.
Verify thread A wakes promptly and reports it. Repeat for remove and interest
modification. Use bounded test deadlines so failure cannot hang CI.

### 12.5 Destruction while waiting

Start an infinite wait, destroy from another thread, and verify the waiter exits
without use-after-free, deadlock, or leaked descriptors.

### 12.6 Descriptor reuse

Register a descriptor, queue readiness, remove and close it, then create sockets
until the numeric descriptor is reused. Register the new descriptor and verify
no stale event from the old generation is delivered as readiness for the new
socket.

### 12.7 Timeout and interruption

- Zero timeout behaves as nonblocking poll.
- Finite timeout is within a reasonable monotonic tolerance.
- Negative timeout waits indefinitely until readiness or interruption.
- Repeated signals causing `EINTR` do not reset the finite timeout.
- Internal wakeup with no user readiness returns zero cleanly.

### 12.8 Error and hangup

Verify peer close produces hangup/error readiness and does not silently remove
the registration. Verify caller can then remove it explicitly.

### 12.9 Allocation and syscall failure injection

Inject failure for registration growth, readiness-buffer growth, backend
creation, wakeup creation, ADD, MOD, and DEL. After each failure, verify table
and backend remain consistent and destruction leaks nothing.

### 12.10 Cross-platform contract

Run the same semantic suite against epoll, kqueue, poll/select, and Windows
backends. Platform-specific tests may additionally inspect backend details, but
the shared suite is authoritative.

## 13. Concurrency and sanitizer tests

- Run add/remove/update/wait stress with several modifier threads and one waiter
  under ThreadSanitizer.
- Run destruction and failure-injection paths under AddressSanitizer and
  UndefinedBehaviorSanitizer.
- Use at least thousands of registration transitions and descriptor reuse
  cycles.
- Never depend on sleeps alone for ordering; use condition variables, atomics,
  pipes, or barriers to establish test phases.
- Every threaded test has a watchdog and joins all threads on success/failure.

## 14. Performance validation

Measure before and after with 1, 10, 100, 1,000, and the platform-supported
maximum practical descriptor count.

Measure separately:

- idle zero-timeout waits;
- finite-timeout waits with no readiness;
- one ready descriptor among many;
- ten percent readiness;
- add/remove churn;
- combined read/write registrations;
- wakeup latency for cross-thread add/remove.

Record:

- waits per second;
- median and p95/p99 wait-processing latency;
- allocations per wait;
- `epoll_create1`, `epoll_ctl`, `epoll_wait`, and `close` syscall counts;
- CPU time and peak memory;
- add/remove-to-wakeup latency.

Acceptance targets on Linux:

- zero heap allocations in steady-state wait when output capacity is sufficient;
- no `epoll_create1()` or epoll-descriptor `close()` per wait;
- no `epoll_ctl()` during unchanged steady-state waits;
- recurring work scales primarily with ready events, not registered sockets;
- add/remove wakes a blocked waiter within a bounded CI-safe deadline.

Benchmarks must include warmup, multiple samples, fixed compiler settings, and
raw sample output. Do not claim improvement from one timing run.

## 15. Performance and correctness traps

- Edge-triggered epoll without draining until `EAGAIN` loses notifications.
- `EPOLLOUT` is commonly continuously ready; registering it permanently can
  cause a busy loop. Enable write interest only while buffered output exists.
- Holding the registry mutex while allocating or invoking callbacks creates
  avoidable contention and reentrancy hazards.
- Closing before unregistering permits descriptor-reuse confusion.
- Treating wakeup `EAGAIN` as failure can lose progress even though a wakeup is
  already pending.
- Using wall-clock time for timeout retry makes clock changes corrupt deadlines;
  use a monotonic clock.
- Returning one event per filter can duplicate a descriptor; merge readiness.
- Fixed tiny event buffers can repeatedly delay ready sockets. Grow to a bounded
  high-water mark or let callers provide capacity.
- Unbounded buffers allow hostile connection counts to force memory growth.
- Per-wait allocation recreates allocator contention even with persistent epoll.
- A callback that removes its own socket must be safe because callbacks execute
  after locks are released and consume copied readiness events.
- Tests using ordinary files are unsuitable for some readiness semantics; use
  socket pairs or pipes appropriate to each platform.

## 16. Acceptance criteria

The redesign is complete when:

- all correctness tests above pass;
- direct `nw_poll()` compatibility tests continue to pass;
- no registration is changed by waiting;
- read+write interest works for one descriptor;
- add/remove does not wait for a long/infinite backend timeout;
- destruction safely wakes and joins an active wait;
- Linux steady-state waits allocate nothing and retain one epoll descriptor;
- TSan/ASan/UBSan targeted suites pass;
- Linux, macOS/kqueue, and Windows/fallback CI jobs pass;
- Networking documentation clearly distinguishes one-shot `nw_poll()` from the
  persistent event-loop API.
