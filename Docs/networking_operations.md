# Networking operations, security, and reproducibility

## Limits and tuning

The default datagram ceiling is 1,200 bytes. The default message limit is 1 MiB,
per-connection reassembly credit is 8 MiB, the queued-byte limit is 4 MiB, and
the event queue holds 256 entries. Lower these limits for constrained clients;
raise them only after measuring allocation and latency impact. A full queue is
backpressure, not permission to drop reliable messages.

Use lane 1 for latency-sensitive input, lane 2 for ordinary state, and lane 3
for bulk data. Keep unreliable snapshots below one datagram where possible.
Use `flush()` only for bounded startup or teardown work; normal gameplay should
let pacing and congestion control schedule packets.

Statistics are a cheap direct snapshot. RTT, jitter, loss, reordering,
retransmission, bytes-in-flight, congestion window, pacing rate, queue depth,
reassembly usage, authentication failures, replay rejection, path migrations,
NAT attempts, relay fallback, and progress timestamps are counters or bounded
values. Do not turn peer-controlled text into metric labels.

## Threat model

Assume an attacker can forge UDP datagrams, replay captured packets, send
oversized or fragmented input, change source addresses, and force loss or
reordering. AEAD associated data protects routing fields, replay windows reject
duplicates, retry cookies limit pre-validation amplification, and reassembly,
ACK, event, candidate, and queue collections are bounded. Secret material is
wiped before release.

The current handshake authenticates the transcript and ephemeral X25519 keys.
Long-term peer identity still requires a rendezvous verifier/application
credential; do not treat an ephemeral key as a user identity. The later Ed25519
module must receive an independent constant-time and protocol review before it
is used for production identity.

## Rendezvous, STUN, and relay runbook

Deploy a rendezvous service in at least two failure domains. It authenticates
peers, gathers host/server-reflexive/relay candidates, signs short-lived tickets
with an attempt ID and tie-breaker, and rate-limits ticket issuance. Use at least
two STUN-compatible endpoints for candidate discovery. Deploy a relay service
for symmetric NAT, UDP-blocked networks, and failed punching.

The Libft NAT layer accepts a candidate provider, validates ticket candidates,
probes every local/remote pair through `networking_nat_probe_io`, nominates the
best validated direct pair, and can open a relay through `networking_nat_relay_io`
after the deadline. Keep direct checks rate-limited after relay fallback and
migrate only after authenticated path validation.

The deterministic test model in
`Test/Test/networking_nat_test_support.hpp` covers full-cone,
address-restricted, port-restricted, symmetric, double-NAT, and UDP-blocked
behaviour without depending on a public network. Its endpoint comparisons cover
the IPv4/IPv6 family boundary; real loopback and platform namespace jobs remain
the evidence required for platform-specific address-family and firewall claims.

## Simulator and bug reports

Insert `networking_simulated_datagram_io` between the transport and a memory or
UDP IO. Set a seed, manual clock, latency/jitter, loss, duplication, corruption,
reordering, bandwidth, MTU, black-hole, and disconnect settings. Script a
specific outgoing packet ordinal with drop, duplicate, corrupt, delay,
disconnect, or MTU-drop. Advance the clock explicitly; tests must not sleep.

A reproducible report records the Libft commit, platform, configuration limits,
simulator seed and script, manual-clock timeline, connection states, event list,
statistics snapshot, and the smallest packet/message sequence that fails.

## Sanitizer fuzzing

Build the networking parser target with a compiler that supplies libFuzzer,
AddressSanitizer, and UndefinedBehaviorSanitizer:

```text
make networking-fuzz FUZZ_CXX=clang++
Test/networking_fuzz -max_total_time=60 Test/Fuzz/corpus/networking
```

`Test/Fuzz/networking_fuzz_target.cpp` exercises bounded plaintext and encrypted
datagrams through the transport parser. The target is test-only and uses the
same internal test-build boundary as the deterministic suite; it does not add
hooks or mutable test state to production archives. Keep valid packet seeds and
every minimized regression input under `Test/Fuzz/corpus/networking`. On
platforms whose default compiler lacks libFuzzer or sanitizer runtimes, install
an LLVM toolchain or set `FUZZ_CXX` to the compatible compiler; do not replace
this target with an unsanitized build.

## Compatibility and migration

The message transport is separate from legacy HTTP, WebSocket, raw UDP/TCP, and
experimental QUIC APIs. Applications should include the narrow Networking and
Crypto headers they use rather than relying on an umbrella header. Persisted
application data must not assume this wire format; connections and tickets are
re-established after an upgrade. A protocol-version change requires new packet
vectors and a migration note before release.
