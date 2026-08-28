# RW-lock reader fast-path implementation record

Status: implemented and archived with the completed design records.

## Scope

The custom `t_pt_rwlock` strategy keeps its existing non-recursive ownership
checks, writer tickets, reader-phase cutoff, and reader/writer priority modes.
The change removes ordinary reader operations from the shared bookkeeping
mutex when admission is open.  The mutex and condition variables remain the
slow path for writer contention, phase transitions, queueing, failures, and
lifecycle operations.

## Required invariants

- A reader may enter the fast path only while `reader_fast_path_open` is true.
- The reader count is incremented by an atomic compare/exchange operation and
  is rechecked against the gate after incrementing.
- A writer closes the gate before it can wait for or acquire exclusive access.
  A reader that raced with the close rolls back its count and uses the slow
  path.
- Writers test both slow-path and fast-path reader counts before entering.
- A thread may hold a given lock in read mode at most once.  Read ownership is
  recorded in a bounded thread-local table, including whether the acquisition
  used the fast path.
- Read unlock validates TLS ownership before decrementing.  The last fast
  reader performs the contended wakeup through the mutex; it never signals a
  condition variable without holding that mutex.
- Reader phase admission and writer ticket order are unchanged.  The fast
  path is only an admission optimization and cannot bypass a closed phase.

## State and call flow

`fast_active_readers` counts readers admitted without the strategy mutex.
`active_readers` continues to count readers admitted by the queued slow path.
Both are included in writer admission and destruction checks.

`rdlock` performs these steps:

1. Validate lifecycle state and reject a matching TLS ownership record.
2. If the atomic gate is open, reserve a TLS record and CAS-increment the
   fast reader count.
3. Acquire-load the gate again.  If it closed, remove the TLS record,
   decrement the count, wake a waiting writer if this was the last reader, and
   continue through the queued path.
4. Otherwise return success without taking the strategy mutex.
5. The queued path retains ticket, phase-cutoff, condition-wait, and failure
   handling.  It records ownership in TLS after admission.

`rdunlock` first looks up the TLS record.  Fast records decrement the atomic
count and only enter the mutex wakeup path when the gate is closed and the
count reaches zero.  Slow records use the existing mutex-protected phase
transition logic.

Writers close the gate before publishing a queued writer in writer-priority
mode, and always close it when a writer becomes active.  On writer release,
the gate stays closed for a reader phase or a pending writer queue and opens
only when ordinary reader admission is legal.  Reader-priority mode keeps the
gate open for waiting readers until a writer actually becomes active, which
preserves its legacy admission semantics.

## Ownership and capacity

The TLS table has a fixed capacity of 64 simultaneously held read locks per
thread.  This keeps the common path allocation-free.  Exceeding that capacity
returns `FT_ERR_NO_MEMORY`; callers must release existing locks before
retrying.  The table is also used by writer acquisition and the generic
`unlock` dispatcher, so fast readers retain the same owner/error semantics as
slow readers.

The table is a performance mechanism, not lifetime protection.  The lock must
not be destroyed while any thread can still use it, as required by the
existing lock contract.

## Performance traps and correctness traps

- Do not replace the gate check and count increment with a plain load followed
  by an increment; that admits readers after a writer closes the gate.
- Do not use a relaxed load for the gate recheck or publish protected data
  without acquire/release ordering.
- Do not use the global reader count as proof of ownership; it cannot detect a
  recursive read lock or an unlock by another thread.
- Do not allocate, scan a shared owner vector, or broadcast on the ordinary
  uncontended reader path.
- The atomic count must be included in writer waits, try-write checks, phase
  transitions, and destroy checks.
- A last-reader wakeup must tolerate a writer closing the gate between the
  reader's first gate load and its decrement.
- The benchmark must distinguish lock overhead from useful protected work.
  Tiny operations magnify synchronization overhead; Minecraft should snapshot
  a chunk/section under the lock and mesh outside it.
- Native `pthread_rwlock_t` is a useful baseline but does not provide this
  lock's owner checks, ticket ordering, or diagnostic behavior; it is not a
  feature-equivalent comparison.

## Verification plan

Correctness tests cover:

- concurrent readers entering together;
- writer closure of the reader gate;
- FIFO writer tickets;
- reader-priority compatibility;
- reader phases and cutoff admission;
- recursive read rejection and foreign unlock rejection;
- try-read/try-write behavior while the gate is open or closed;
- fast readers releasing while a writer waits;
- reader bookkeeping and condition-wakeup failure injection;
- destruction rejection while either reader count is non-zero.

Stress tests should repeatedly run 1, 2, 4, 8, and 16 readers with writers
arriving at random phase boundaries.  The protected payload must be checked
after every run, and the run must complete under ThreadSanitizer and the
address/undefined sanitizers.

The efficiency probe should report repeated medians for sequential reads,
sequential writes, read-only concurrency, and 95/5 mixed traffic at several
thread counts.  Each workload should be run with tiny, medium, and realistic
snapshot critical sections, with warm-up iterations and stable CPU settings.
Report throughput and tail wait time, not just one wall-clock sample.
