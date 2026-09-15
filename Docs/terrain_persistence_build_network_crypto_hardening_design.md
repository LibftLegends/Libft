# Terrain Persistence, Build Graph, Networking, and Crypto Hardening Design

Status: implementation handoff for Luna

Target repository state: post-PR #919 (`a0bc98f4` in the supplied review)

Primary implementation areas:

- `Modules/Voxel/voxel_json.cpp`
- `Modules/Voxel/voxel_types.hpp`
- `Modules/Voxel/voxel_config.hpp`
- `Test/Test/test_voxel_generator.cpp`
- root `Makefile`, `mk/global_graph.mk`, and new build-regression scripts
- `Modules/Networking` and `Modules/Crypto`
- `Test/Test`, `Test/Fuzz`, and `.github/workflows/ci.yml`

This document is an implementation plan, not evidence that the work is complete.
Every acceptance gate at the end must pass before this document can be moved to
`Docs/old`.

## 1. Required Libft conventions

All implementation and test code must follow the root `AGENTS.md`.

In particular:

- use fixed-width integer types and `ft_bool`;
- do not use `for`, `switch`, ternary expressions, or `try`/`catch`;
- use explicit error returns and lifecycle handling;
- use Libft containers instead of STL containers in production code;
- use `ft_memcpy` and `ft_memset`, never `std::memcpy` or `std::memset`;
- use the standard `std::atomic` and `std::memory_order` primitives for
  cross-thread state publication; do not introduce a second custom atomic
  implementation for this work;
- keep failure-injection code under `LIBFT_TEST_BUILD` and out of production
  archives;
- preserve the caller's state until all fallible work has succeeded;
- do not silently translate one failure into an unrelated error code.

## 2. Scope and implementation order

Implement in these grouped slices:

1. Terrain persistence: unsigned JSON conversion, transactional serialization,
   the two supported file modes, precise open/write/close errors, and tester-only
   file-I/O failure hooks/tests.
2. Build verification: adversarial incremental-build scenarios, archive checks,
   and platform CI coverage.
3. Crypto: primitive vectors, lifecycle/failure/property tests, differential and
   fuzz tests, randomness/wiping checks, and review tooling.
4. Networking: codec/state/reliability/security/NAT tests, semantic failure
   injection, fuzzing, concurrency, integration, soak, and observability.
5. CI handoff: expose the grouped suites through Make targets, retain evidence,
   and keep production archives free of tester-only code.

Do not combine behavior changes and broad test-framework rewrites into one
unreviewable commit. Each numbered area should be independently buildable and
testable.

## 3. Terrain issue 1: lossless `uint32_t` JSON serialization

### 3.1 Current defect

`voxel_json_add_u32()` in `Modules/Voxel/voxel_json.cpp` currently casts the
value to `int32_t` and delegates to `voxel_json_add_i32()`. Values in
`[2147483648, 4294967295]` are therefore emitted as negative JSON numbers.

### 3.2 Required implementation

Replace the signed cast with a real unsigned decimal conversion.

Preferred implementation:

1. Add a dedicated helper such as `voxel_json_append_u32_decimal()`.
2. Use the existing `adv_to_string(uint32_t)` overload, which formats through
   the unsigned path, or add a non-allocating Libft unsigned conversion helper
   if the team wants to avoid the temporary allocation.
3. Append the resulting digits without a sign.
4. Propagate conversion and append failures exactly.
5. Destroy/free any temporary lifecycle object on every path.

Do not use `adv_itoa()`, an `int32_t` cast, floating-point conversion, or a JSON
string value. The serialized token must remain a JSON number.

Also replace the `uint32_t`-to-`int32_t` conversion used for indexed JSON keys.
Although current array limits are small, the helper must not establish another
unsafe conversion pattern.

### 3.3 Required tests

Add table-driven tests covering:

- `0U` -> `0`;
- `1U` -> `1`;
- `2147483647U` -> `2147483647`;
- `2147483648U` -> `2147483648`;
- `4294967294U` -> `4294967294`;
- `UINT32_MAX` -> `4294967295`.

Test at least one field from each relevant configuration family:

- biome block ID;
- feature chance;
- ore block ID, size, and count;
- cave/ravine radius;
- layer block ID/depth;
- generation counts and strengths.

For each boundary value:

1. serialize the owning object;
2. assert the exact decimal token appears and no negative form appears;
3. parse/load it through the production JSON reader;
4. assert exact `uint32_t` equality after round trip;
5. serialize the loaded object again and assert stable output.

Add allocation-failure tests for the unsigned conversion and append path. On
failure, the caller's output must satisfy the transactional contract in section
4.

## 4. Terrain issue 3: transactional serialization

### 4.1 Required contract

Every `serialize_json(ft_string &output)` overload must have strong transactional
behavior:

- success replaces `output` with one complete JSON document;
- failure leaves an already-initialized `output` byte-for-byte unchanged;
- failure never exposes a partial JSON document;
- an uninitialized/destroyed destination follows the documented `ft_string`
  lifecycle contract and must not be left partially initialized.

### 4.2 Required implementation

Stop destroying `output` inside `voxel_json_prepare()`.

Build into a local initialized `ft_string staging_output`:

1. initialize `staging_output`;
2. append the complete opening, fields, closing brace, and newline;
3. if any operation fails, destroy `staging_output` and return the first error;
4. after complete success, commit with `output.move(staging_output)` or another
   ownership-transfer operation whose failure occurs before modifying `output`;
5. destroy remaining staging state on every return path.

Refactor the shared serializer flow so individual classes cannot accidentally
commit early. A recommended shape is:

- a private function that serializes a specific object into an already
  initialized staging string;
- one shared transactional wrapper that owns initialization, cleanup, and final
  commit.

Do not copy the completed JSON back through repeated `append()` operations; that
would add a second allocation-failure window and weaken the commit guarantee.

### 4.3 Required tests

For every public terrain `serialize_json()` overload:

- initialize `output` with a sentinel value;
- fail each allocation ordinal from staging initialization through final field
  append;
- assert the returned error;
- assert `output` still equals the sentinel;
- assert no allocations remain live;
- clear the failure hook and prove the same object can serialize successfully.

Also test:

- empty but initialized output;
- large pre-existing output;
- thread-safe `ft_string` output;
- final commit lock failure, if the destination has an injectable mutex path;
- repeated success replacing prior content exactly once;
- self-contained output ending in exactly one newline.

Use the existing CMA failure controller by semantic operation/ordinal. Do not
modify CMA production behavior solely for this test.

## 5. Terrain issues 2 and 4: file mode and error correctness

### 5.1 Remove append mode

The project requirement is to support only:

- `VOXEL_JSON_FILE_CREATE_ONLY`;
- `VOXEL_JSON_FILE_REPLACE`.

Delete `VOXEL_JSON_FILE_APPEND` from `voxel_json_file_mode`, remove the
`O_APPEND` branch, remove append-mode tests, and update all call sites and docs.
An out-of-range enum value must return `FT_ERR_INVALID_ARGUMENT` without opening
or changing a file.

Removing append mode resolves the concurrent record-interleaving finding by
removing the unsupported behavior. Do not retain append under another name and
do not claim atomic multi-write append semantics.

### 5.2 Correct create-only open errors

`FT_ERR_ALREADY_EXISTS` may be returned only when the underlying open failure is
`EEXIST` or the platform-equivalent error mapped by Libft.

The implementation must capture the platform error immediately after the failed
open, before close, logging, allocation, or another call can overwrite it. Map it
through `cmp_map_system_error_to_ft()` or a new System_utils open helper that
returns both the descriptor and mapped error.

Preferred shared API if a new helper is introduced:

```cpp
int32_t su_open_with_error(const char *path_name, int32_t flags, mode_t mode,
    int32_t *file_descriptor, int32_t *error_code) noexcept;
```

Required semantics:

- initialize `*file_descriptor` to `-1`;
- initialize `*error_code` to `FT_ERR_SUCCESS`;
- validate pointers and path;
- call the platform wrapper once;
- capture/map its error immediately;
- return `FT_ERR_SUCCESS` only when a valid descriptor was produced;
- never infer `ALREADY_EXISTS` from the requested mode.

If no shared helper is added, `voxel_json_write_file()` must still capture and
map `errno` immediately and use the mapped result. Avoid duplicating a partial
platform error table in Voxel.

Expected mappings include:

| Condition | Expected Libft result |
| --- | --- |
| create-only target exists | `FT_ERR_ALREADY_EXISTS` |
| missing parent | `FT_ERR_NOT_FOUND` |
| permission denied/read-only target | mapped permission/invalid-operation code |
| invalid path | `FT_ERR_INVALID_PATH` or mapped invalid-operation code |
| path too long | `FT_ERR_PATH_TOO_LONG` or mapped out-of-range code |
| process/system descriptor exhaustion | `FT_ERR_FULL` |
| memory exhaustion | `FT_ERR_NO_MEMORY` |
| other open failure | exact mapped Libft error, falling back to `FT_ERR_FILE_OPEN_FAILED` only when no more specific mapping exists |

Keep the table aligned with `cmp_map_system_error_to_ft()` and update
`Modules/Errno/ERROR_CODE_REGISTRY.md` if mappings are improved.

### 5.3 Tester-only file-I/O hooks

Real permission and descriptor-exhaustion tests are unreliable across CI
platforms. Add a Voxel/System_utils tester adapter under `LIBFT_TEST_BUILD` that
can inject an open result plus platform error at a named operation.

Required named points:

- open create-only;
- open replace;
- first write;
- partial write after N bytes;
- interrupted write;
- close failure.

Production builds must compile these hooks to no behavior and production
archives must not contain hook symbols.

### 5.4 File tests

Test create-only with:

- a new path;
- an existing regular file;
- an existing directory;
- missing parent;
- inaccessible directory where the platform allows a deterministic test;
- injected `EACCES`, `EPERM`, `EROFS`, `EINVAL`, `ENAMETOOLONG`, `EMFILE`,
  `ENFILE`, `ENOMEM`, and `EIO`;
- a null path and an invalid enum value.

Test replace with:

- a new file;
- a shorter replacement of a longer file, proving truncation;
- a longer replacement;
- injected partial writes and `EINTR` according to the chosen retry contract;
- write failure preserving the exact error;
- close failure;
- no leaked descriptor on any path.

Create-only must never alter an existing file. Failed replace must be documented
as either potentially modifying the target or upgraded to an atomic temporary
file + rename design. Do not imply transactional file replacement unless the
atomic rename implementation and platform tests exist.

## 6. Build graph adversarial regression suite

### 6.1 Purpose

Clean builds and archive-integrity checks are insufficient evidence for the
flattened Make graph. Add a repeatable script, preferably
`mk/test_incremental_build_graph.sh`, and expose it through an
`incremental-build-tests` Make target.

The script must operate in a disposable copied checkout or temporary worktree.
It must not mutate or clean the developer's active tree.

### 6.2 Required scenarios

Each scenario starts from a known successful baseline and records build-plan
events emitted by `BUILD_PLAN_MODE=1`.

1. Touch a private header used by one module. Assert only its real dependency
   closure recompiles and the required archives relink.
2. Touch a widely shared header. Assert every dependent object rebuilds and
   unrelated objects do not.
3. Delete one object file. Assert it recompiles and is restored to its module
   archive and `Full_Libft.a`.
4. Delete one archive member with `ar d`. Assert normal `make` restores it.
5. Remove a source from a temporary module manifest. Assert the stale member
   disappears from both module and aggregate archives.
6. Rename a source and update its manifest. Assert the old member disappears and
   the new member appears.
7. Change compile flags without touching source. Assert the configuration
   fingerprint selects/rebuilds the correct object set.
8. Alternate release, debug, test, test-debug, ASan, and UBSan modes without
   cleaning. Assert outputs never reuse incompatible objects.
9. Run `make -j32` after touching multiple dependency roots. Assert no missing or
   duplicate archive members and no stale progress state.
10. Interrupt a parallel build, then rerun it. Assert recovery and accurate
    progress reporting.
11. Build two parent projects concurrently against the same Libft source tree.
    Require separate output roots/configuration fingerprints and intact archives.
12. Use paths containing spaces and long paths on Windows/macOS/Linux.
13. Change a module manifest, root graph, and compiler configuration separately;
    assert each invalidates the intended targets.
14. Run the stale-source/member scanner after every scenario.

Manifest invalidation must be content-based rather than dependent solely on
filesystem timestamp resolution. The global Make graph should derive a stable
manifest fingerprint and use a fingerprinted stamp prerequisite for each
module archive. An unchanged fingerprint must remain a no-op; a changed
fingerprint must force the module and aggregate archives to be regenerated.

### 6.3 Assertions and diagnostics

The script must fail on:

- a required object not rebuilt;
- an unrelated full rebuild where a narrow closure was expected;
- stale or duplicate archive members;
- archive/member order instability when inputs did not change;
- mixed sanitizer/release objects;
- a build-progress count that omits or double-counts work;
- two concurrent builds writing the same temporary archive;
- any nonzero `git diff --check` result in generated manifests/scripts.

On failure, retain:

- build plan;
- invoked command and environment fingerprint;
- object/archive manifests before and after;
- `ar t` output;
- timestamps/hashes for affected files;
- complete compiler output.

Run the suite on Windows, Ubuntu, and macOS CI. Include at least one `-j32`
Linux run and one concurrent-parent run.

## 7. Crypto tester expansion

The current Crypto tests provide important first vectors, but comprehensive
coverage requires independent tests per primitive and per failure contract.
Split new tests into focused files rather than continuing to grow one monolithic
file.

Recommended files:

- `test_crypto_sha256.cpp`
- `test_crypto_hmac_hkdf.cpp`
- `test_crypto_chacha20.cpp`
- `test_crypto_poly1305.cpp`
- `test_crypto_aead.cpp`
- `test_crypto_x25519.cpp`
- `test_crypto_session.cpp`
- `test_crypto_random.cpp`
- `test_crypto_failure_paths.cpp`
- `test_crypto_differential.cpp`

### 7.1 SHA-256

Test:

- all applicable RFC 6234/NIST known-answer vectors;
- empty input and lengths 1, 55, 56, 63, 64, 65, 127, 128, and 129;
- one-shot versus every practical incremental split for short messages;
- one-byte incremental updates for multi-block messages;
- repeated initialize/final/destroy lifecycle paths;
- update before initialize, final twice, update after final, null + nonzero
  length, and null + zero length;
- represented bit-length overflow rejection;
- output state and secret/state wiping after failure and destroy;
- differential results against two independently built references in CI.

### 7.2 HMAC-SHA-256 and HKDF-SHA-256

Test all RFC 4231 HMAC and RFC 5869 HKDF vectors, not only one sample.

Boundary tests:

- key lengths 0, 1, 31, 32, 63, 64, 65, 127, and 128;
- empty and multi-block messages;
- absent salt versus explicit 32-byte zero salt;
- empty `info`;
- HKDF output lengths 0, 1, 31, 32, 33, 8159, and 8160;
- reject 8161 and arithmetic-overflow lengths before writing output;
- client/server labels and transcript fields are length-delimited and cannot
  collide through concatenation;
- allocation failure leaves output unchanged/cleared according to the public
  contract and wipes intermediate keys.

### 7.3 ChaCha20

Test RFC 8439 quarter-round, block, encryption, and multi-block vectors.

Also test:

- counters 0, 1, `UINT32_MAX - 1`, and `UINT32_MAX`;
- rejection before counter wrap;
- lengths around 64-byte blocks;
- zero-length input;
- in-place/overlap behavior exactly as documented;
- invalid pointers and output allocation failures;
- no output exposure after a failed operation;
- differential output over deterministic random keys/nonces/messages.

### 7.4 Poly1305

Test all RFC 8439 vectors and messages of lengths 0, 1, 15, 16, 17, 31, 32,
33, and long multi-block inputs. Include carries near limb boundaries, maximal
byte values, clamping checks, and independent differential tests.

Mutating any message or key byte must change verification outcome. Intermediate
`r`, accumulator, and one-time keys must be wiped after use.

### 7.5 ChaCha20-Poly1305 AEAD

Test every available RFC 8439 vector plus a pinned, reviewed malformed-vector
corpus. Do not download test vectors at test runtime.

Required mutation matrix:

- flip every bit of the 16-byte tag;
- mutate first/middle/last ciphertext bytes;
- mutate AAD and its encoded length;
- mutate nonce and key;
- truncate ciphertext/tag at every boundary accepted by the parser;
- empty plaintext, empty AAD, and both empty;
- AAD/plaintext lengths around 16- and 64-byte boundaries;
- maximum configured packet size;
- authentication failure always returns one generic error and releases no
  plaintext;
- every allocation failure clears ciphertext/plaintext and authentication tag;
- repeated nonce rejection is tested at the protocol layer that tracks nonce
  use.

### 7.6 X25519

Test RFC 7748 public-key and shared-secret vectors, the 1- and 1,000-iteration
tests in ordinary CI, and the 1,000,000-iteration vector in scheduled slow CI.

Include:

- scalar clamping;
- input high-bit masking;
- all standardized low-order inputs;
- all-zero shared-secret rejection;
- non-canonical encodings according to the chosen RFC policy;
- aliasing/overlap behavior;
- deterministic random differential comparisons;
- source keys unchanged on success/failure;
- intermediate field state wiped.

### 7.7 Session derivation and randomness

Session tests must prove:

- client send equals server receive and vice versa;
- send/receive keys and IVs are distinct;
- transcript, role, label, peer key, and epoch changes produce distinct output;
- key updates are deterministic for the same epoch and distinct across epochs;
- invalid or repeated epochs are rejected by Networking state;
- failure is transactional and all temporary secrets are wiped.

Randomness tests must cover every platform backend, short reads/retries where
applicable, provider failure, null arguments, zero length, and large requests.
The deterministic provider must only exist in test builds. Add an archive-symbol
test proving no `crypto_test_*` symbol is present in `crypto.a`.

### 7.8 Crypto fuzzing and constant-time review support

Add separate libFuzzer targets for:

- SHA incremental chunk sequences;
- HMAC/HKDF lengths and labels;
- AEAD seal/open round trips and malformed inputs;
- X25519 encoded points;
- session transcript/key-update parsing.

Each target needs a small checked-in seed corpus and dictionaries where useful.
Run ASan+UBSan fuzzing on Ubuntu for every PR with a bounded time and longer
scheduled runs. Preserve crash inputs as CI artifacts.

Add constant-time review gates:

- compiler inspection/static checks for tag comparison and X25519 ladder;
- no secret-indexed table access or secret-dependent early exit;
- a statistical timing harness such as a dudect-style test in scheduled CI;
- independent cryptographic review before production-security claims.

Timing tests are review evidence, not a proof of constant-time behavior.

## 8. Networking tester expansion

Split protocol tests by responsibility:

- `test_networking_message_codec.cpp`
- `test_networking_message_state_machine.cpp`
- `test_networking_message_reliability.cpp`
- `test_networking_message_fragmentation.cpp`
- `test_networking_message_flow_control.cpp`
- `test_networking_message_lanes.cpp`
- `test_networking_message_security.cpp`
- `test_networking_message_nat.cpp`
- `test_networking_message_simulation.cpp`
- `test_networking_message_thread_safety.cpp`
- `test_networking_message_failures.cpp`
- `test_networking_message_integration.cpp`

Reuse `networking_test_support.hpp`, `networking_test_hooks.hpp`, and
`networking_nat_test_support.hpp`; extend them with named semantic points rather
than byte-count assumptions.

### 8.1 Codec and parser

For every packet/frame type, test:

- minimum valid encoding and maximum configured encoding;
- exact golden bytes in network byte order;
- encode/decode/encode stability;
- unknown protocol versions, packet types, frame types, and reserved bits;
- all truncation offsets;
- length underflow/overflow and trailing data;
- duplicate/misordered control frames;
- integer boundary values and checked arithmetic;
- parser work and allocation bounded before authentication;
- no state mutation from rejected packets.

Generate structured valid packets and mutate every header field. Assert a
specific accepted/rejected outcome and bounded memory/time, not merely "no
crash".

### 8.2 Connection state-machine model

Create a small reference model covering `LISTENING`, `CONNECTING`, handshake,
connected, draining, closed, and aborted states. Generate operation sequences
and compare implementation state/events/errors to the model.

Cover:

- duplicate connect/accept/reject/close/abort;
- simultaneous open and close;
- timeout at each handshake stage;
- early data rejection;
- stale handles and connection-ID reuse;
- event ordering and exactly-once terminal events;
- reentrant callbacks calling public APIs;
- destroy/reinitialize after every state.

### 8.3 Reliable delivery and ACK/loss logic

Use deterministic seeds across loss 0-30%, duplication, corruption, and
reordering. For thousands of generated traces assert:

- every accepted reliable message is delivered exactly once and in channel
  order;
- unreliable traffic is never retransmitted;
- ACK ranges merge/split correctly and stay bounded;
- packet-number wrap/limits are handled safely;
- retransmission retains message/channel/lane identity;
- RTT estimator, PTO/RTO, congestion window, and pacing follow their specified
  invariants;
- duplicate ACKs repair loss without duplicate delivery;
- sender flow credit never goes negative or exceeds advertised limits;
- queues drain or time out without leaked records.

Persist failing seeds and print the seed, simulator configuration, and operation
trace in the assertion output.

### 8.4 Fragmentation and reassembly

Test messages at `MTU - 1`, `MTU`, `MTU + 1`, every fragment boundary, maximum
message size, and one byte above maximum.

Cover out-of-order fragments, duplicates, overlap attempts, conflicting metadata,
missing first/last fragments, timeout cleanup, connection close during
reassembly, allocation failure at every fragment ordinal, many concurrent
partial messages, and hostile fragment floods. Memory must remain within the
configured per-connection/global bounds.

### 8.5 Lanes, scheduling, congestion, and flow control

Verify exact queue accounting per lane and connection. Add deterministic
weighted-deficit tests proving configured ratios within a documented tolerance
while control traffic remains responsive and low-priority lanes do not starve.

Test runtime lane-weight changes, reserved bandwidth, flush behavior, queue-full
backpressure, connection close with queued data, retransmission fairness, pacing
under manual time, receive-credit shrink/growth, and integer saturation.

### 8.6 Handshake, secure channel, and key updates

Test:

- complete client/server transcript and directional keys;
- retry-cookie address, port, timestamp, expiry, and secret rotation binding;
- amplification bytes before address validation stay within the documented
  ratio;
- malformed and duplicate hellos/Finished messages;
- peer-key pin success/failure;
- tampered encrypted header/body/tag;
- replay window boundaries, duplicates, too-old packets, and large jumps;
- nonce uniqueness for every `(direction, epoch, packet_number)`;
- key-update request/ack loss, duplication, reordering, timeout, and simultaneous
  updates;
- previous-key expiry and rejection after expiry;
- secure close/abort authentication;
- no plaintext fallback when crypto initialization or randomness fails;
- all secret/session state wiped after failure and destroy.

Cross-check Networking outputs against the standalone Crypto APIs and an
independent reference implementation in CI.

### 8.7 NAT, rendezvous, relay, and path migration

Expand the deterministic NAT matrix to IPv4 and IPv6 combinations of:

- full cone;
- address restricted;
- port restricted;
- symmetric NAT;
- double NAT;
- hairpin supported/unsupported;
- UDP blocked;
- direct path lost after establishment;
- relay available/unavailable/flapping.

Test ticket expiry, signature/verifier rejection, attempt-ID binding, candidate
deduplication/limits, priority tie-breaking, concurrent probing, nomination,
keepalive, relay fallback, direct-path upgrade, relay-to-relay behavior, and
authenticated path migration.

No unauthenticated ticket overload may establish a path. Invalid tickets and
failed probes must not partially commit remote candidates or selected paths.

Add Linux namespace/netem integration tests for direct, restricted, symmetric,
and blocked UDP topologies. Keep the deterministic model as the fast PR suite;
run namespace tests in Linux CI/scheduled jobs.

### 8.8 Worker and thread safety

Run under ThreadSanitizer on Linux and stress:

- start/stop/destroy races;
- concurrent sends, receives, close, statistics, and lane updates;
- command-queue full and wakeup loss;
- callbacks dispatched only on the owning thread;
- callbacks reentering transport APIs;
- transport locking enabled and disabled contracts;
- failure during mutex/condition/worker creation;
- repeated initialize/start/stop/destroy cycles;
- no callback or worker access after destroy.

Tests must use synchronization primitives and manual clocks, not timing sleeps,
except in dedicated real-time integration tests with generous bounded timeouts.

### 8.9 Named failure matrix

For every point in `networking_test_failure_point`, test both `fail_next` and
`fail_after(N)` for each meaningful ordinal:

- connection allocation;
- outgoing frame allocation;
- sent-packet allocation;
- reassembly allocation;
- received-message copy;
- event enqueue;
- datagram send;
- transport mutex allocation.

Add missing semantic points for command enqueue, ACK-range growth, handshake
state, simulator queues, NAT candidates/probes, relay records, callback copies,
and worker creation/wakeup.

For every injected failure assert:

- exact error code;
- no partial queue/statistics/state commit;
- ownership is clear and no double free/leak occurs;
- connection remains usable or transitions to the documented terminal state;
- hook counters reset after the case;
- production archives contain no test-hook symbols.

### 8.10 Networking fuzzing

Keep the existing whole-datagram target and add focused targets for:

- packet/frame codec;
- handshake and retry cookies;
- encrypted packet parser;
- ACK ranges;
- fragment/reassembly sequences;
- NAT/rendezvous tickets and candidate lists;
- simulator scripts;
- state-machine operation sequences.

Fuzz both plaintext-explicit-test mode and encrypted mode. Initialize valid state
before feeding parser data so deeper paths are reachable. Add custom mutators
that preserve enough packet structure to exercise authenticated and reassembly
logic.

Required invariants include no crash, no sanitizer finding, bounded allocation,
bounded loop work, no plaintext delivery after authentication failure, no
duplicate reliable delivery, and clean destroy after every input.

### 8.11 Integration, soak, and observability

Add IPv4/IPv6 loopback and multi-process tests covering direct and relay adapters,
large reliable messages, mixed lanes, reconnect, path change, and orderly/abortive
shutdown.

Scheduled suites must include:

- multi-hour seeded impairment soak;
- connection churn;
- sustained maximum-size fragmentation;
- long packet-number/key-epoch progression;
- bounded-memory hostile input;
- throughput/latency baselines with regression thresholds.

Statistics tests must verify counters exactly against generated traces, snapshot
consistency during concurrent activity, saturation behavior, and exporter
failure isolation. Observability callbacks must never run while internal
transport locks are held.

## 9. CI and tester integration

Add explicit Make targets:

- `terrain-persistence-tests`;
- `incremental-build-tests`;
- `crypto-tests`;
- `crypto-fuzz`;
- `networking-message-tests`;
- `networking-fuzz`;
- `networking-netem-tests`;
- `networking-soak`.

Required PR jobs:

- Windows, Ubuntu, and macOS build + full tester;
- archive integrity for every module and `Full_Libft.a`;
- terrain persistence focused tests;
- incremental build graph tests;
- Ubuntu ASan+UBSan tests;
- Ubuntu bounded Crypto and Networking fuzz jobs;
- compile with OpenSSL unavailable, proving new Crypto/Networking transport does
  not depend on it.

Required scheduled jobs:

- ThreadSanitizer networking suite;
- extended fuzzing with crash artifact upload;
- X25519 million-iteration vector;
- Linux namespace/netem NAT matrix;
- networking soak and performance baselines;
- concurrent-parent and `make -j32` build stress.

Every CI failure must retain the full log, failing seed/input, build plan,
sanitizer report, and relevant archive manifests. Tests must run from `Test/`
when fixtures use paths relative to that directory.

## 10. Reviewable implementation slices

Recommended commit sequence follows the five grouped implementation slices in
section 2. Keep each slice independently buildable and testable; documentation
and the final acceptance audit are part of the CI handoff slice.

Each slice must keep the full test binary buildable and must pass
`git diff --check`.

## 11. Acceptance criteria

The work is complete only when the grouped gates below are evidenced:

- Terrain persistence is lossless and transactional; append mode is absent; the
  two remaining file modes preserve precise mapped errors and never leak handles.
- Every incremental-build scenario and archive-integrity check passes on
  Windows, Linux, and macOS, including sanitizer/release separation.
- Crypto vectors, boundaries, lifecycle/failure, mutation, differential, fuzz,
  randomness, wiping, and review gates pass; the standalone path has no OpenSSL
  dependency.
- Networking codec, state, reliability, fragmentation, flow-control, lanes,
  security, NAT/relay, worker, failure, fuzz, integration, and resource-bound
  tests pass with no sanitizer findings.
- Fuzz corpora and bounded fuzz jobs pass; production archives contain no tester
  hooks or deterministic provider; `make all`, `make tests`, archive integrity,
  the full tester, `git diff --check`, and the `AGENTS.md` scan pass.
- An independent cryptographic review is recorded before encrypted Internet
  connections are called production-ready.

Only after this evidence exists may this design be moved to `Docs/old`.
