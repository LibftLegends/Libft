# Steam-Style Message Transport Design

Status: Proposed implementation design
Target modules: `Modules/Networking` and the standalone module named `crypto`
Primary transport: UDP over IPv4 and IPv6
Audience: Libft implementers and reviewers

The cryptographic module name is intentionally exactly `crypto` (lowercase).
Networking is a consumer of that module, not its owner: the primitive source
files, public primitive APIs, test vectors, secure-random implementation, and
secret-wiping code must be maintained under the `crypto` module boundary.

## 1. Purpose

Libft needs a game-oriented networking layer with the usability properties that
make Steam GameNetworkingSockets valuable: callers exchange complete messages,
choose reliability per message, and receive secure connections, loss recovery,
fragmentation, prioritisation, diagnostics, and adverse-network testing without
building those systems around raw UDP themselves.

The required feature set is:

- reliable and unreliable message delivery selected per message;
- reliable, ordered delivery over UDP with acknowledgements and retransmission;
- automatic fragmentation and reassembly of large messages;
- NAT traversal and peer-to-peer connection establishment;
- encrypted, authenticated connections with replay protection;
- connection-quality statistics;
- deterministic bad-network simulation;
- priority lanes/channels;
- a message-based public API;
- IPv4, IPv6, Windows, Linux, and macOS support.

The first implementation milestone must prioritise the four highest-value
capabilities: message delivery modes, reliable UDP, fragmentation/reassembly,
and NAT traversal. Security is nevertheless a release blocker: no Internet P2P
mode may be called production-ready before authentication and encryption are
complete.

This is not intended to be wire-compatible with Steam. It emulates the useful
behaviour with a Libft-owned, versioned protocol.

## 2. Existing Libft foundation

The design builds on, but does not silently change, the existing APIs:

- `udp_socket` provides lifecycle-managed UDP bind, connect, send, and receive;
- `event_loop` and platform backends provide readiness waiting;
- DNS and `networking_resolved_address` provide IPv4/IPv6 resolution;
- legacy OpenSSL feature detection and TLS helpers exist, but they are not
  dependencies of the new message transport or its `crypto` module;
- `quic_experimental_session` contains experimental key-export and datagram AEAD
  work, but is not a complete transport and must not become an accidental
  dependency of the new protocol;
- Networking observability already has an exporter path that can be extended;
- `LIBFT_TEST_BUILD` already supports tester-only hooks.

The new implementation belongs in a separate message-transport layer above
`udp_socket`. HTTP, WebSocket, raw TCP/UDP, and experimental QUIC behaviour must
remain compatible.

## 3. Scope and explicit non-goals

### In scope

- client/server and peer-to-peer sessions;
- one UDP socket serving multiple logical connections;
- reliable ordered and unreliable unordered messages;
- optional unreliable sequenced messages for replaceable state updates;
- congestion-aware sending, pacing, MTU-safe packetisation, and path migration;
- direct NAT hole punching with rendezvous coordination;
- relay fallback through a separately deployable relay service;
- authenticated encryption and key rotation;
- bounded queues and explicit backpressure;
- deterministic, tester-only network and allocation failure injection;
- stable connection events, statistics snapshots, and close reasons.

### Not in the first production release

- Steam protocol compatibility or use of Steam identity/services;
- guaranteed direct connectivity through every NAT. Symmetric NAT and restrictive
  firewalls require a relay;
- voice codecs, entity replication, rollback, lobby/matchmaking, or game state
  serialisation;
- reliable-unordered delivery. It can be added later if a real use case appears;
- new or unreviewed cryptographic primitives. Required standards are implemented
  in the dedicated `crypto` module as specified in section 13;
- kernel-bypass networking or lock-free internals.

## 4. Architectural boundaries

The implementation is divided into layers so protocol logic can be tested
without real sockets.

1. `message_transport` owns sockets, polling, connection lookup, and dispatch.
2. `message_connection` owns one connection state machine, queues, congestion
   controller, key epochs, reassembly, and statistics.
3. `packet_codec` serialises and validates versioned packet headers and frames.
4. `reliability_engine` tracks packet numbers, acknowledgement ranges, RTT,
   retransmittable frames, loss, and flow control.
5. `message_fragmenter` and `message_reassembler` convert messages to/from frames.
6. `traffic_scheduler` selects frames from priority lanes and applies pacing.
7. `secure_channel` performs handshake orchestration, calls the Networking
   crypto adapter, and handles replay rejection and rekeying. The algorithms
   themselves are never implemented in Networking; they live in the deliberately
   lowercase module named `crypto` (`Modules/Crypto`, archive `crypto.a`).
8. The standalone `crypto` module is built and tested independently of
   Networking. Its only Networking-facing integration is the narrow
   `networking_crypto_backend` adapter; packet code must not include primitive
   headers directly.
9. `nat_traversal` coordinates candidate gathering, hole punching, nomination,
   migration, and relay fallback.
10. `network_simulator` is an optional datagram adapter enabled only in tests or
   by an explicit development build option.
11. A `datagram_io` interface separates protocol code from `udp_socket`; production
   and deterministic in-memory implementations share the same contract.

No connection method may directly call platform socket APIs. Platform access
stays under `udp_socket`/`datagram_io`. No test should need timing guesses or a
public Internet service.

## 5. Proposed public API

Names are provisional, but implementation should preserve these semantics.

```cpp
enum class message_delivery
{
    RELIABLE_ORDERED,
    UNRELIABLE,
    UNRELIABLE_SEQUENCED
};

struct message_send_options
{
    message_delivery delivery;
    uint8_t lane;
    uint32_t channel;
    uint64_t expiry_milliseconds;
};

struct received_message
{
    uint64_t connection_id;
    uint32_t channel;
    message_delivery delivery;
    const uint8_t *data;
    ft_size_t size;
};

class message_transport;
class message_connection;
```

Required transport operations:

- `initialize(const message_transport_config&)` and `destroy()`;
- `listen(const networking_endpoint&)`;
- `connect(const networking_endpoint&, const connection_identity&)`;
- `connect_peer(const peer_connect_ticket&)`;
- `poll(int32_t timeout_milliseconds)` for externally driven operation;
- optional `start_worker()`/`stop_worker()` for an internally driven mode;
- `accept(connection_handle)` and `reject(connection_handle, reason)`;
- `receive_messages(received_message*, ft_size_t capacity)`;
- `get_connection(handle, message_connection**)` without returning lifecycle
  objects by value.

Required connection operations:

- `send_message(data, size, options)` copies or takes explicitly documented
  ownership of caller data;
- `close(close_reason, const char *debug_text)` performs a graceful close;
- `abort(close_reason)` drops queued data and sends a best-effort close;
- `get_state(...)`, `get_statistics(...)`, and `get_remote_identity(...)`;
- `configure_lane(lane, priority_weight, reserved_bandwidth)`;
- `set_queue_limits(...)` and `flush()`.

Calls return Libft error codes. Queue-full/backpressure, message-too-large,
expired-message, authentication, protocol, and connection-state errors need
dedicated error codes. Sending must never report success and silently discard a
reliable message.

All new owning classes follow root `AGENTS.md`: default construction is
non-fallible, setup occurs in `initialize`, lifecycle state is explicit,
copy/move constructors and assignments are deleted, `move` is explicit,
destruction is idempotent, and optional thread safety uses `pt_recursive_mutex`.
Public width-sensitive fields use fixed-width or project types and `ft_bool`.

## 6. Execution and threading model

Support two modes with identical protocol behaviour:

- externally driven: the application calls `poll()` regularly;
- worker driven: one long-lived worker blocks on socket readiness and a wakeup
  primitive, then advances timers and queues.

The worker must block when idle, not spin or repeatedly sleep. Public operations
enqueue commands and wake it. Callbacks must not execute while internal locks are
held. The default event delivery model is polling an event/message queue so game
code controls the thread on which handlers run. Optional callback delivery is
also owned by the application: externally driven `poll()` drains callbacks, and
worker-driven applications call `dispatch_callbacks()` from their owning thread;
the worker never invokes user callbacks.

The current implementation provides the opt-in worker lifecycle, Libft's
wakeable socket-readiness wait while idle for UDP plus a timed fallback for
custom datagram providers, an explicit timeout overload for external `poll`,
an opt-in Libft recursive-mutex transport guard,
a bounded polled event queue, deferred callback delivery outside internal locks
with owning-thread dispatch,
and explicit logical listen/connect plus accept/reject handling for incoming
authenticated requests. Mutating public operations use a bounded Libft command
queue while the worker is active: caller-owned payloads are copied before
enqueue, the caller waits for the worker's explicit result, and pending commands
are completed with a thread error during shutdown. The worker records its native
thread identity separately from the pthread handle so worker-side execution does
not accidentally re-enqueue itself.

Thread safety is opt-in according to Libft policy. Without it, the owning thread
must perform all calls. With it, lock ordering is transport, connection, lane;
code must never acquire these in reverse order. Socket I/O and user callbacks
must occur outside connection locks.

## 7. Connection state machine

States:

`UNINITIALISED -> IDLE -> RESOLVING -> PROBING -> HANDSHAKING -> CONNECTED ->
DRAINING -> CLOSED`

`FAILED` may be entered from any active state. Each transition emits at most one
event. Invalid packets cannot revive a closed connection.

Handshake requirements:

1. Initiator sends a stateless client hello containing protocol versions, random
   nonce, connection identifier, supported cipher suites, and identity mode.
2. Responder returns a retry cookie when the source is unvalidated. The cookie is
   an authenticated, short-lived token bound to source address and hello digest;
   this prevents amplification and avoids allocating connection state.
3. Initiator repeats the hello with the cookie and ephemeral key share.
4. Both sides authenticate the transcript and derive directional traffic keys.
5. Finished messages prove key possession; application messages are accepted only
   after finished verification.

Connection identifiers, rather than only address tuples, route established
packets and permit validated path migration. A peer changing address must pass a
challenge/response before the new path becomes primary.

Handshake retransmission uses exponential backoff with jitter and a total
deadline. Simultaneous-open rules compare stable connection identifiers to choose
one role deterministically.

## 8. Wire protocol

All multi-byte integers use network byte order. Parsing must use bounds-checked
reads and never cast untrusted byte buffers to structs. Version 1 packet headers
contain:

- magic/version and packet kind;
- destination connection identifier;
- truncated or full packet number whose reconstruction is unambiguous in the
  receive window;
- key epoch and flags;
- encrypted frame payload;
- AEAD authentication tag.

Handshake packets have a separately versioned, minimally parseable envelope.
Application packet contents are encrypted. Header fields needed for routing are
authenticated as AEAD associated data.

Frames include:

- `ACK`: compact inclusive packet-number ranges and acknowledgement delay;
- `MESSAGE_FRAGMENT`: message/channel/lane identifiers, delivery mode, total
  length, fragment offset, fragment length, and bytes;
- `PING`, `PATH_CHALLENGE`, `PATH_RESPONSE`;
- `FLOW_CONTROL`, `KEY_UPDATE`, `CLOSE`, and optional `PADDING`;
- NAT probe and rendezvous frames only in their appropriate packet context.

Unknown optional frame types are length-delimited and skipped. Unknown critical
frames close the connection. Reserved bits and malformed/non-canonical values are
protocol errors. Fuzzing must cover every parser.

Datagram payload defaults to 1,200 bytes, which is safe for IPv6 minimum MTU after
headers. Do not rely on IP fragmentation. A later path-MTU probe may raise the
limit conservatively; black-hole detection must fall back to 1,200 bytes.

## 9. Reliable delivery over UDP

Each encrypted packet has a monotonically increasing packet number per key epoch.
The receiver records a bounded sliding window, drops duplicates, and emits ACK
ranges. ACK-only packets are not themselves retransmittable.

Reliable messages receive a connection-scoped message sequence per channel.
Fragments may arrive in any packet order, but complete reliable messages are
delivered to the application in sequence order within their channel. Different
channels do not head-of-line block one another.

Loss detection follows modern packet/time threshold principles:

- estimate smoothed RTT, RTT variance, minimum RTT, and latest RTT from ACKs;
- compensate only for bounded peer acknowledgement delay;
- mark a packet lost after sufficiently newer packets are acknowledged or a
  time threshold expires;
- queue lost retransmittable frames again; never retransmit the original packet
  byte-for-byte or reuse an AEAD nonce;
- use a probe timeout when acknowledgements stop;
- exponentially back off repeated probe timeouts and close after configured idle
  or no-progress limits.

Karn-style ambiguity must be avoided when sampling retransmitted data. Packet
number arithmetic must handle wrap/reconstruction safely. ACK range count and
history are bounded to prevent memory or CPU attacks.

Congestion control starts with a conservative Reno-style implementation:

- initial congestion window based on a small number of datagrams;
- slow start, congestion avoidance, multiplicative decrease on detected loss;
- bytes-in-flight accounting only for ack-eliciting packets;
- token/pacing schedule derived from congestion window and smoothed RTT;
- no bursts after the application or system stalls;
- separate receiver flow control so a fast sender cannot exhaust memory.

Congestion control is an internal strategy boundary so a later CUBIC/BBR-like
controller can be introduced without changing the public API or wire protocol.

## 10. Unreliable delivery

`UNRELIABLE` messages are sent at most once, may be lost, duplicated by the
network, or arrive out of order. Libft should suppress packet duplicates when
cheap, but the API does not promise delivery or ordering. They still use the
secure packet format and congestion/pacing limits.

`UNRELIABLE_SEQUENCED` assigns a sequence per channel. The receiver drops messages
older than the newest delivered sequence. This suits snapshots and player poses.

An unreliable message may be fragmented, but partial data is never exposed. Its
reassembly expires quickly and no fragment is retransmitted. For latency-sensitive
use, callers should keep unreliable messages below one datagram.

Message expiry is checked before packetisation and before retransmission. Expiry
of reliable messages is allowed only when the caller explicitly sets an expiry;
it emits a local delivery-failed event so the loss is visible.

## 11. Fragmentation and reassembly

Fragmentation is transparent to callers. Fragment size is derived from current
path MTU minus worst-case packet/frame/AEAD overhead. A message identifier is
never reused while old fragments could still be accepted.

Reassembly state records total length, received ranges, delivery metadata,
deadline, and allocated storage. It must reject:

- total lengths over the configured maximum;
- integer overflow in offset plus length;
- fragments outside total length;
- contradictory total lengths or metadata;
- pathological overlaps whose bytes disagree;
- excessive sparse ranges or fragments;
- too many concurrent messages or too many bytes per connection/transport.

Identical overlaps are tolerated to handle retransmission. Completion occurs only
when the full byte range is present. Reliable completion enters the per-channel
ordering queue; unreliable completion is delivered immediately if still current.

Defaults should cap a message at 1 MiB, reassembly memory per connection at 8 MiB,
and global reassembly memory at a configurable bounded value. Exact defaults must
be benchmarked and documented. Eviction may discard unreliable state first, but
must never silently discard reliable state: apply flow control or close a peer
that violates negotiated limits.

## 12. Priority lanes and channels

Channels define independent reliable ordering domains. Lanes define scheduling
priority and are orthogonal to channels. Recommended defaults:

- lane 0: control/handshake, strict highest priority, internally reserved;
- lane 1: critical gameplay inputs/events;
- lane 2: ordinary gameplay state;
- lane 3: bulk transfers/background data.

Use weighted deficit round robin among application lanes, with bounded strict
priority for control frames. This prevents a large low-priority message from
blocking input while also preventing starvation. Retransmitted fragments retain
their lane. Per-lane queued bytes/messages and optional reserved bandwidth are
bounded. Queue admission returns a backpressure error when limits are reached.

## 13. Encryption, identity, and authentication

Do not invent new cryptographic primitives. Implement the named, reviewed
standards directly in Libft's dedicated module named `crypto` (the source
directory is `Modules/Crypto`, and its archive is `crypto.a`). Every algorithm,
primitive state, key schedule, KDF, random-byte implementation, and secure-wipe
routine belongs to that module. None of those algorithms may be placed in
`Modules/Networking`.

Networking may expose only a small `networking_crypto_backend` adapter. That
adapter must expose key agreement, transcript hashing/KDF, authenticated
encryption/decryption, secure random bytes, and secret wiping without exposing
Crypto-specific types to packet or application code. It must fail compilation or
initialization clearly when the required secure backend is unavailable.

The networking transport must not depend on OpenSSL, CNG, CommonCrypto, or another
external crypto package. Libft owns the standalone `crypto` module, compiled on
every platform. Networking keeps only the transport-facing
`networking_crypto_backend` adapter; primitive implementations, key derivation,
secure randomness, and secret wiping belong to `crypto`. The adapter is the only
crypto boundary visible to packet code and must provide the same results and error
behavior everywhere.

The current backend is a Libft-owned ChaCha20-Poly1305 implementation using the
RFC 8439 construction. The primitive and key-schedule implementations belong in
the `crypto` module; the Networking backend only adapts them. It is not to be
described as production-secure until it has passed
independent cryptographic review, constant-time review, known-answer vectors,
misuse testing, fuzzing, and interoperability review. No custom key exchange or
identity scheme may be added without a separate security design review.

The networking build must link successfully when OpenSSL headers and libraries are
absent. A build with the Libft backend enabled provides encrypted connections; a
build where the backend is deliberately disabled returns `FT_ERR_UNSUPPORTED_TYPE`
when encryption is requested. It must never silently downgrade a secure connection
to plaintext. Plaintext is only permitted when the caller explicitly disables
encryption for a controlled/test configuration.

The current implementation has the standalone Libft-owned `crypto` module,
replay-window handling, authenticated packet headers carrying an explicit key
epoch, RFC-vector-tested
SHA-256/HMAC/HKDF, ChaCha20, Poly1305, AEAD, X25519, a direction-separated
session-key derivation API, and a reusable handshake component covering canonical
hellos, transcript construction, Finished MACs, and address-bound retry cookies.
The authenticated handshake is integrated into the message transport as an
opt-in path, including a stateless, address-bound retry-cookie challenge before
the responder allocates handshake state. The transport exposes an explicit safe
key-epoch update that requires reliable state to be drained and rejects packets
for the wrong epoch. Authenticated key-update control packets now coordinate
one-way send/receive rotation after reliable state is drained, with timeout
retransmission, authenticated acknowledgements, and a bounded previous-receive
epoch window for duplicate requests. Automatic multi-epoch policy, stable peer identity
signatures, and production NAT authentication remain release gates. The public
transport now has verifier-gated peer tickets, optional pinned peer-key
verification, and authenticated handshakes expose the peer's ephemeral X25519
key for diagnostics; pinning is a channel-establishment check, not a substitute
for the later long-term identity/signature layer. The reliable transport now uses bounded compact ACK ranges, Reno-style congestion
window growth/reduction, and RTT-derived pacing; this does not remove the
remaining protocol and security release gates.

### 13.1 Libft Crypto module cryptographic implementation stack

The target `Modules/Crypto` stack is, in dependency order:

1. SHA-256.
2. HMAC-SHA-256.
3. HKDF-SHA-256.
4. ChaCha20.
5. Poly1305.
6. ChaCha20-Poly1305 AEAD.
7. X25519.
8. Ed25519 later, after SHA-512 and the signature review are complete.

The implementation must follow the primary specifications exactly. These are
reference documents, not loose descriptions:

- [RFC 6234](https://www.rfc-editor.org/rfc/rfc6234) for SHA-256 and the
  SHA-based HMAC/HKDF reference algorithms.
- [RFC 2104](https://www.rfc-editor.org/rfc/rfc2104) for HMAC construction.
- [RFC 5869](https://www.rfc-editor.org/rfc/rfc5869) for HKDF extract and
  expand, including its salt and `info` rules.
- [RFC 8439](https://www.rfc-editor.org/rfc/rfc8439) for ChaCha20, Poly1305,
  and the ChaCha20-Poly1305 AEAD layout and vectors.
- [RFC 7748](https://www.rfc-editor.org/rfc/rfc7748) for X25519, scalar
  decoding/clamping, the Montgomery ladder, and all-zero output handling.
- [RFC 8032](https://www.rfc-editor.org/rfc/rfc8032) for the later Ed25519
  signing and verification implementation.

Deterministic random providers used by Crypto tests belong to the tester-only
`crypto_test_hooks` boundary. They must not be implemented in Networking or
linked into production archives; Networking tests may seed that Crypto hook
when they need reproducible handshake randomness.

The modules must have narrow, lifecycle-explicit APIs. They must not expose
OpenSSL, CNG, CommonCrypto, compiler SIMD types, or provider-specific state in
public headers. All buffers use Libft containers or caller-owned byte spans;
allocation failure returns an error and never throws. Secret buffers are wiped on
all destroy and failure paths.

Implementation rule: memory copying and clearing in the new crypto and message
transport code must use Libft's `ft_memcpy` and `ft_memset` primitives. Direct
calls to `std::memcpy` or `std::memset` are not permitted. This keeps the module
consistent with Libft's portability and test-instrumentation rules.

#### Primitive requirements

SHA-256:

- Use the RFC 6234 initial state, 64 round constants, right rotations, big-endian
  block parsing, `0x80` padding, and a 64-bit message length.
- Provide one-shot and incremental APIs so HMAC/HKDF do not concatenate large
  temporary buffers.
- Reject lengths that overflow the represented bit length and report allocation
  or invalid-state errors explicitly.
- Test the empty string, one-block, multi-block, boundary-length, and long-input
  vectors from RFC 6234 plus differential tests against an independently built
  reference in the tester.

HMAC-SHA-256:

- Use a 64-byte block size; hash keys longer than the block size first and pad
  shorter keys with zeros as specified by RFC 2104.
- Compute `H(K xor opad || H(K xor ipad || message))` without data-dependent
  branches in the compression path.
- Compare tags with a constant-time accumulator and expose only a generic
  authentication-failed result.
- Test RFC 6234 HMAC vectors, empty keys/messages, long keys, incremental versus
  one-shot equivalence, and truncated-tag rejection.

HKDF-SHA-256:

- `Extract` uses HMAC with the supplied salt; an absent salt is 32 zero bytes.
- `Expand` appends the previous block, `info`, and a one-byte counter beginning
  at one; reject output lengths greater than `255 * 32`.
- Keep transcript labels and direction labels length-delimited and distinct;
  never use ad-hoc string concatenation for protocol key separation.
- Test every RFC 5869 vector, zero-length `info`, maximum legal output, and
  rejection of oversized output.

ChaCha20:

- Implement the RFC 8439 16-word state, little-endian loading/storing, 20 rounds
  (ten column/diagonal double rounds), and 32-bit block counter semantics.
- The AEAD construction uses a 96-bit nonce and starts payload encryption at
  counter one; counter zero is reserved for the Poly1305 one-time key.
- Reject counter wrap and never reuse a `(key, nonce)` pair. Do not expose a
  random-access API that can accidentally reuse a counter range.
- Test quarter-round, block, and complete encryption vectors from RFC 8439.

Poly1305:

- Decode the one-time 32-byte key exactly as specified, clamp the `r` value, use
  the required 130-bit arithmetic modulo `2^130 - 5`, and encode the final tag
  little-endian.
- Use a fresh one-time key for every message. Poly1305 keys must never be cached
  across packets or reused with a different ciphertext/AAD.
- Test the RFC 8439 standalone and key-generation vectors, empty input, block
  boundaries, and long messages.

ChaCha20-Poly1305 AEAD:

- Build the one-time Poly1305 key with ChaCha20 counter zero, encrypt plaintext
  with counter one, and authenticate the exact RFC 8439 sequence: AAD, AAD
  padding, ciphertext, ciphertext padding, little-endian AAD length, and
  little-endian ciphertext length.
- Authenticate the transport header as AAD. The header fields used to route a
  packet must be authenticated before parsing the encrypted body.
- Verify the tag before releasing any plaintext; on failure clear the output and
  return a generic authentication error. Never partially deliver decrypted
  bytes.
- Test all RFC 8439 AEAD vectors plus header-bit flips, tag-bit flips, nonce
  reuse detection at the protocol layer, truncated inputs, oversized lengths,
  and failed-decryption plaintext clearing.

X25519:

- Implement RFC 7748's little-endian field encoding, high-bit masking, scalar
  clamping, constant-time Montgomery ladder, and final conditional reduction.
- Reject an all-zero shared secret after the ladder. Do not use the raw shared
  secret directly as an application key; pass it through HKDF with a transcript
  and explicit client/server direction labels.
- Test the RFC 7748 scalar, public-key, and Diffie-Hellman vectors, malformed
  encodings, low-order points, all-zero output, and repeated-use lifecycle paths.

Ed25519 (later):

- Add SHA-512 first; RFC 8032 Ed25519 depends on it for secret expansion and the
  deterministic per-message nonce.
- Follow RFC 8032 encoding, pruning, signing, verification, and canonical
  encoding rules exactly. Reject non-canonical points, invalid encodings, and
  small-order public keys according to the chosen verification policy.
- Test every RFC 8032 key, signature, and message vector, mutation rejection,
  context handling if exposed, and signature malleability cases.

#### Backend migration and release gates

The current ChaCha20-Poly1305 backend is an intermediate transport adapter over
the `crypto` module. The reusable handshake and cookie component is implemented
in Networking, but cryptographic algorithms remain exclusively in `crypto`.
The retry-cookie challenge is integrated into transport, while identity
verification, packet key epochs/rotation, and production NAT authentication
must be added before secure networking is called production-ready. The
transport-facing backend API should expose
`derive_session_keys`, `seal`, `open`, `random_bytes`, and `wipe`; the secure
channel remains responsible for packet-number replay windows and connection
state, while `Modules/Crypto` owns primitive correctness.

Before enabling encrypted Internet connections, all of the following are gates:

- no OpenSSL headers, libraries, or symbols are required by the message-transport
  networking target; the existing legacy HTTP/TLS/QUIC wrappers still contain
  optional OpenSSL paths and require a separate migration before the entire
  Networking archive can make this claim;
- RFC vector suites pass on Windows, Linux, and macOS;
- independent differential tests pass against at least two external references;
- allocation-failure, malformed-input, fuzz, replay, nonce-reuse, and tag-failure
  tests pass;
- constant-time review covers tag comparison, scalar multiplication, field
  arithmetic, and secret-dependent branches;
- secure random acquisition is backed by the platform CSPRNG and fails closed;
- an independent cryptographic review signs off the implementation and the
  handshake/key schedule before production use.

Recommended initial suite:

- X25519 ephemeral key agreement;
- HKDF-SHA-256 key schedule;
- AES-128-GCM and ChaCha20-Poly1305 AEAD negotiation;
- Ed25519 signatures for long-term peer/server identity where supported;
- cryptographically secure random nonces and connection identifiers.

If platform/OpenSSL support requires a different exact suite, record the decision
in an ADR before implementation. Never downgrade to plaintext automatically.

Supported identity modes:

- pinned server public key;
- application-provided certificate/signature verification callback;
- rendezvous-signed short-lived peer ticket binding peer identity, candidates,
  protocol version, expiry, and connection nonce;
- explicitly configured anonymous encrypted mode for local development only.

Security invariants:

- every application packet is AEAD protected;
- nonce uniqueness is guaranteed by direction, key epoch, and packet number;
- replay windows reject old and duplicate packets before application delivery;
- handshake transcript binds negotiation, identities, roles, and connection IDs;
- key updates occur before packet-number/AEAD limits and retain previous receive
  keys only for a bounded four-retransmission-timeout reordering window, after
  which the old key material is wiped and rejected;
- secret material is zeroed on destroy and excluded from logs/core diagnostics;
- authentication failure uses generic externally visible errors to avoid oracles;
- close frames received from the network are authenticated;
- parser and handshake work is bounded before source validation.

The existing experimental QUIC AEAD code may supply reviewed helper logic, but it
must not be copied as proof that the complete handshake is secure. A security
review and known-answer/interoperability tests are release gates.

## 14. NAT traversal and P2P

NAT traversal requires infrastructure in addition to the client library:

- a rendezvous/signalling service that lets authenticated peers exchange signed,
  short-lived connection tickets and candidates;
- at least two public STUN-compatible endpoints in different failure domains for
  server-reflexive candidate discovery and NAT mapping checks;
- a relay service for symmetric NAT, UDP-blocked networks, or failed punching.

The Libft client gathers host candidates from eligible local interfaces,
server-reflexive candidates from STUN, and relay candidates when configured.
Private/link-local addresses must not be advertised outside appropriate scope.
IPv6 candidates are preferred when validated; Happy-Eyeballs-style probing avoids
waiting serially for one address family.

Both peers receive a rendezvous-signed ticket containing peer identity,
candidates, expiry, unique attempt identifier, and shared role/tie-break data.
They send authenticated connectivity checks to candidate pairs concurrently at a
rate-limited pace. The Libft traversal API starts with `begin` and advances a
bounded batch through `probe_batch(now, minimum_interval, maximum_probes,
probe_io, sent_probes)`; `probe_next` remains the one-probe compatibility form.
It must return a pacing error instead of emitting a burst. Successful
request/response pairs must include the ticket attempt identifier and validate
the nominated path against that attempt. A
deterministic nomination step selects the best path by directness, address family,
measured RTT, and policy. All probes are bound to the ticket to prevent reflection
and off-path session injection.

If no direct path succeeds before the configured deadline, the connection uses a
relay without changing the message API. It may continue low-rate direct checks and
migrate only after validating and authenticating a better path. Migration cannot
reset packet numbers, reliability state, identity, or congestion state blindly.

NAT keepalive intervals adapt to observed mapping lifetime and application
traffic, with conservative bounded defaults. Connectivity checks and relays need
per-IP/per-ticket rate limits, amplification limits, expiry validation, and abuse
monitoring.

Testing must include full-cone/address-restricted/port-restricted/symmetric NAT,
double NAT, changing mappings, IPv4-only, IPv6-only, dual-stack, UDP blocked, and
relay loss. CI uses local namespace/container-based emulation where supported and
a deterministic logical NAT model elsewhere; it never depends on the public
Internet. The test-only model currently covers full-cone, address-restricted,
port-restricted, symmetric, double-NAT, and UDP-blocked mappings, while the
relay adapter and traversal tests cover deadline fallback and authenticated
attempt binding.

## 15. Connection-quality statistics

`connection_statistics` is a snapshot with at least:

- connection state and active path type (direct/relay, IPv4/IPv6);
- latest, minimum, smoothed RTT, RTT variance, and jitter;
- sent/received packets and bytes;
- acknowledged, lost, duplicate, reordered, retransmitted, and malformed packets;
- application messages/bytes by delivery mode and lane;
- send/receive rate, estimated available throughput, congestion window,
  bytes-in-flight, pacing rate, and queue depth;
- reassembly bytes/messages and timeout/drop counts;
- handshake duration, path migrations, NAT attempts, and relay fallback count;
- authentication/replay rejection counts without secret data;
- last receive, send, acknowledgement, and progress timestamps;
- a documented health classification derived from configurable thresholds.

Counters use saturating fixed-width arithmetic where overflow is possible.
Snapshots must be internally consistent and safe under the selected thread-safety
mode. `export_observability()` integrates an aggregate sample with
`Modules/Observability` after releasing the transport lock, while preserving a
cheap direct snapshot for game diagnostics. No peer-controlled string may become
an unbounded metric label.

The current message transport snapshot includes the RTT series, jitter, queue
depth, bytes in flight, pacing estimate, reassembly counts, authentication and
replay rejection counters, and progress timestamps. The current API also has a
bounded polled event queue, authenticated close frames with bounded debug text,
and a graceful-drain path that keeps reliable frames until acknowledgement before
emitting the terminal close. Aggregate Observability export is implemented by
`export_observability()`; worker-owned mutating commands use the synchronous
bounded command queue; statistics now include per-lane message/byte counters,
queued bytes, configured weights/reservations, and the current lane send rate.

## 16. Bad-network simulation

Simulation is inserted at the `datagram_io` boundary in both directions. It must
support independently configurable:

- fixed and random latency;
- jitter distributions with deterministic seeded output;
- packet loss, duplication, corruption, and reordering;
- bandwidth/rate limits and burst limits;
- queue capacity and overflow policy;
- temporary disconnects and one-way black holes;
- MTU limits and oversized-datagram drops;
- address/path changes;
- scripted outcomes keyed by packet ordinal or semantic test tag.

The simulator uses an injected monotonic clock and seeded PRNG. Tests advance a
manual clock; they do not sleep. Production protocol code cannot inspect simulator
state. Test builds may inspect pending datagrams and classify decoded pre-encryption
test frames through tester-only hooks.

The current simulator implements seeded fixed/jitter latency, loss, duplication,
corruption, deterministic reordering, MTU rejection, bounded pending queues,
fixed-window bandwidth budgets, manual time, disconnect/one-way black-hole
controls, and deterministic scripted outcomes keyed by outgoing packet ordinal.
The scripted actions cover drop, duplicate, corruption, delay, disconnect, and
MTU rejection. Scripted semantic tags and production/test hook separation remain
release-gated work.

All hooks, global overrides, and semantic packet tags are guarded by
`LIBFT_TEST_BUILD` and compiled out of normal builds. A development simulator may
be exposed only behind an explicit configuration macro and must default off.

## 17. Resource limits and denial-of-service resistance

Every remotely influenced collection has a limit: connections, half-open
handshakes, ACK ranges, queued messages/bytes, fragments, reassembly bytes,
channels, lanes, candidate pairs, packets processed per poll, and events.

Before address validation, responses must not amplify received bytes by more than
a small documented factor. Retry cookies and rendezvous tickets expire and rotate
keys. Invalid packet processing must be approximately constant bounded work;
repeated offenders may be rate-limited without an unbounded address table.

Allocation failures are normal error paths. A reliable enqueue either commits
fully or changes no state. Receive/reassembly allocation failure applies flow
control or closes with an explicit local reason. Secret-bearing buffers are wiped
before release. Packet payloads and debug text are never logged by default.

## 18. Error and event model

Events include connecting, connection requested, connected, path changed,
message available, local delivery failed, peer closing, closed, and failed.
Each includes a stable connection handle, machine-readable reason, and bounded
debug context. Event queues are bounded and coalesce nonessential statistics/path
notifications. Terminal events cannot be dropped.

Distinguish:

- caller/configuration errors;
- temporary backpressure/would-block;
- transport timeout or path failure;
- peer protocol violation;
- authentication/identity failure;
- local resource/allocation failure;
- graceful local or remote close.

Network input must never trigger `su_abort()`. Lifecycle misuse follows
`AGENTS.md`; malformed peer data returns protocol errors or closes that connection.

## 19. Test architecture

### 19.1 Deterministic protocol harness

Create two or more transports connected by in-memory `datagram_io`, a manual
monotonic clock, seeded randomness, and the simulator. A test advances until a
predicate or a strict step budget is reached. Failure output includes seed,
logical time, connection states, queue sizes, and a bounded packet event trace.

This harness is the primary source of correctness tests and must run identically
on Windows, Linux, and macOS.

### 19.2 Unit tests

Cover:

- every packet/frame codec boundary, truncation, overflow, and unknown type;
- packet-number encode/decode and wrap boundaries;
- ACK range insertion, merging, truncation, duplicates, and invalid ranges;
- RTT estimator, loss thresholds, probe timeout, and congestion transitions;
- lane fairness, control-frame bounds, starvation prevention, and expiry;
- fragment sizing at each MTU, overlap rules, completion, timeout, and quotas;
- reliable channel ordering and independence;
- unreliable and sequenced semantics;
- handshake transcript, retry cookie expiry/address binding, key derivation,
  AEAD known-answer tests, replay windows, key update, and tamper rejection;
- candidate-pair ordering, nomination, path validation, migration, and relay choice;
- every lifecycle state, double initialize, repeated destroy, explicit move,
  thread-safety enable/disable, and lock failure;
- every allocation site through tester-only deterministic CMA failure injection.

### 19.3 Property and model tests

For generated sequences of sends, loss, duplication, reordering, and time:

- a reliable message accepted without explicit expiry is delivered exactly once
  and in channel order, or the connection terminates with an observable reason;
- unreliable data is never fabricated or partially delivered;
- no application message is delivered before authentication;
- each AEAD nonce is unique per key;
- acknowledged bytes leave flight/retransmission state exactly once;
- memory and queue usage never exceed negotiated/configured bounds;
- lane service converges to configured weights when all lanes remain active;
- a closed connection never returns to an active state.

Keep failing seeds and add them as regression cases. Use a simple reference model
for reliable ordering, ACK state, and reassembly rather than comparing the
implementation to itself.

### 19.4 Fuzzing

Add fuzz targets for handshake envelope parsing, packet headers, each frame,
ACK ranges, fragment/reassembly input, rendezvous tickets, state-machine event
sequences, and decrypt-then-parse paths. Assertions: no crash, abort, leak,
out-of-bounds access, unbounded loop/allocation, or unauthenticated delivery.
Maintain a seed corpus with valid packets and every historical failure.

The current deterministic test harness includes a bounded malformed-datagram
corpus covering every input length from zero through the receive-buffer edge,
an additional 512-case byte-pattern corpus covering lengths through 1,536
bytes, and a seeded reliable-delivery run with 30% loss, duplication, and
reordering. It also includes a 16-message ordered-delivery model run under the
same impairment profile, asserting exact-once delivery, channel order, and
payload integrity. These are regression gates, not a substitute for
sanitizer-backed fuzzing or broader property/model generation. The
sanitizer-backed fuzz target is represented by
`Test/Fuzz/networking_fuzz_target.cpp` and the `networking-fuzz` Make target.
It drives both plaintext and encrypted
transport parsing and must be built with a compiler providing libFuzzer plus
AddressSanitizer/UndefinedBehaviorSanitizer runtimes. The repository CI now
builds and runs this target in the Ubuntu `networking-fuzz` job for 60 seconds;
the local Windows toolchain cannot provide the required POSIX/libFuzzer runtime.

### 19.5 Integration tests

- real loopback UDP with IPv4 and IPv6;
- two clients sharing one transport socket and many concurrent connections;
- loss/reordering during handshake and graceful close;
- large messages across many fragments with selected losses;
- high-priority input during saturated bulk transfer;
- idle timeout, keepalive, suspend/resume, and socket/path change;
- direct P2P, forced relay, relay-to-direct migration, and relay failure;
- worker-driven shutdown while sends, receives, and callbacks are active;
- builds with and without OpenSSL, where insecure production initialization must
  fail explicitly rather than silently downgrade.

### 19.6 Platform and network emulation tests

Linux CI should add namespace/firewall/traffic-control scenarios for NAT and
impairment. Windows and macOS run the deterministic NAT/simulator suite plus real
loopback tests. Scheduled privileged jobs may exercise platform-native firewall
and network shaping. Test both debug/release, sanitizers where available, 32/64-bit
relevant arithmetic, and forced byte-order codec vectors.

### 19.7 Stress, soak, and performance tests

- thousands of connection open/close cycles and many logical connections;
- sustained mixed reliable/unreliable traffic under loss for at least one hour in
  scheduled CI and longer pre-release soak runs;
- packet-number/key-update boundaries accelerated with test configuration;
- bounded-memory verification under fragment and handshake floods;
- race detection for send/close/poll/statistics/thread-safety transitions;
- throughput, message latency percentiles, CPU per packet/message, allocation
  count, and memory benchmarks at 0%, 1%, 5%, and 20% loss.

Performance tests have recorded baselines and regression thresholds, but cannot
replace correctness assertions.

### 19.8 Security tests

- forged identities/tickets/cookies and expired credentials;
- bit flips in every authenticated field and ciphertext/tag truncation;
- replay within/across connections and key epochs;
- amplification measurement before address validation;
- malformed varints/lengths, decompression-style bombs (if compression is ever
  added), fragment memory attacks, ACK floods, and CPU exhaustion attempts;
- downgrade, role-confusion, reflection, simultaneous-open, and path-hijack tests;
- third-party dependency scanning and an independent protocol/cryptography review.

## 20. Required test hooks

Under `LIBFT_TEST_BUILD` only:

- inject monotonic time and secure/random deterministic providers;
- replace `datagram_io` with in-memory or scripted implementations;
- fail the Nth semantic operation (allocate reassembly buffer, enqueue frame,
  create mutex, send datagram), using named failure points rather than byte counts;
- inspect bounded state snapshots without exposing mutable internals;
- reduce packet-number/key-update/resource limits to reach boundaries quickly;
- capture state transitions and frame lifecycle events;
- override NAT candidate sources and relay/rendezvous responses.

The current test-only hook implementation exposes named failure points for
connection records, outgoing frames, sent packets, reassembly records,
received-message copies, event enqueue, datagram send, and transport mutex
creation. It is independent of CMA and resets its counters when the controller
ends.

Hooks must not require changes to CMA itself and must compile out of production.
Tests reset all hooks after each case, including failure paths.

## 21. Implementation phases and gates

### Phase 0: contracts and harness

Implement clock/random/datagram abstractions, packet codec, manual-clock harness,
resource-limit configuration, handles/events, and lifecycle scaffolding.

Gate: codec fuzzing, lifecycle tests, allocation-failure tests, and identical
deterministic traces on all three operating systems.

### Phase 1: message transport and reliable UDP

Implement connection IDs/state machine, reliable ordered and unreliable messages,
ACK/loss/RTT logic, retransmission, pacing, congestion/flow control, channels,
lanes, fragmentation, reassembly, and statistics.

Gate: property tests pass across a large fixed seed set at up to 30% loss,
duplication/reordering; no leaks/races; bounded-memory flood tests pass.

### Phase 2: secure authenticated connections

Implement retry cookies, authenticated handshake, identities/tickets, AEAD,
replay protection, key updates, secure teardown, and path challenge/response.

Gate: known-answer and tamper/replay suites, fuzzing, amplification limit, and
independent security review findings resolved.

### Phase 3: NAT traversal and relay

Implement candidate gathering, rendezvous ticket parsing/verification, concurrent
punching, nomination, keepalive, relay protocol/client, and migration.

Gate: deterministic NAT matrix and Linux emulation matrix pass, including
symmetric NAT relay fallback and path changes. Infrastructure deployment/runbook
and abuse limits are documented.

### Phase 4: worker mode and production hardening

Implement worker wakeup/shutdown, observability integration, soak/performance
baselines, API examples, compatibility tests, and operational diagnostics.
The worker wakeup/shutdown and command-ownership implementation is present;
remaining work in this phase is verification and operational hardening.

Gate: Windows/Linux/macOS CI, sanitizer/race jobs, scheduled soak, API review,
and documentation complete. Remove any experimental label only after all gates.

Implementation checkpoint: the current branch has the standalone `crypto`
module, RFC-vector-tested primitives, basic message delivery/reassembly and
statistics, authenticated handshaking with stateless retry cookies, deterministic
network simulation with a 30%-loss reliable-delivery regression and malformed
datagram corpus, NAT candidate gathering, concurrent candidate-pair probing,
ticket-verification hooks, deterministic NAT model coverage for restrictive and
blocked mappings, relay fallback, lane configuration with
per-connection queue limits and flushing, bounded handshake retransmission,
compact bounded ACK ranges, congestion-window/pacing enforcement, transactional
reliable queue accounting, authenticated close/abort frames with graceful
reliable-data draining, a bounded event queue, opt-in transport locking,
advertised receive-flow credit with sender-side reliable backpressure,
wire-bound key-epoch updates, key-epoch component rotation, and an opt-in worker
lifecycle, explicit incoming-request accept/reject handling, deferred event
callbacks outside internal locks with owning-thread dispatch, and authenticated
path challenge/response
migration with source validation, rotating retransmission service for fair loss
recovery, duplicate-triggered ACK repair, and ordered-message ownership-safe
delivery. The remaining
phase work is still required before this document can be archived.

Phases may be developed in parallel at clear interfaces, but none may bypass its
gate in a production release.

## 22. Proposed file layout

```text
Modules/Networking/
  message_transport.hpp/.cpp
  message_connection.hpp/.cpp
  message_protocol.hpp
  networking_packet_codec.cpp
  networking_reliability.cpp
  networking_fragmentation.cpp
  networking_traffic_scheduler.cpp
  networking_secure_channel.cpp
  networking_handshake.cpp
  networking_crypto_backend.cpp       # Networking-to-Crypto adapter only
  networking_nat_traversal.cpp
  networking_relay.cpp
  networking_statistics.cpp
  internal/datagram_io.hpp
  internal/monotonic_clock.hpp
Modules/Crypto/                         # the deliberately named `crypto` module
  crypto_primitives.hpp/.cpp           # SHA-256, HMAC-SHA-256, HKDF-SHA-256
  crypto_chacha20.hpp/.cpp             # ChaCha20
  crypto_poly1305.hpp/.cpp              # Poly1305
  crypto_aead.hpp/.cpp                 # ChaCha20-Poly1305 AEAD
  crypto_x25519.hpp/.cpp               # X25519 key agreement
  crypto_random.hpp/.cpp                # secure random-byte provider boundary
  crypto_session.hpp/.cpp               # transcript/session key derivation
Test/Test/
  networking_message_test_support.hpp
  test_networking_message_codec.cpp
  test_networking_message_reliability.cpp
  test_networking_message_fragmentation.cpp
  test_networking_message_security.cpp
  test_networking_message_nat.cpp
  test_networking_message_simulation.cpp
  test_networking_message_thread_safety.cpp
  test_networking_message_failures.cpp
  test_networking_message_integration.cpp
```

Keep public headers narrow. Internal components include only their direct
dependencies, not the umbrella networking header. Register sources/tests through
the existing module Makefiles and test runner conventions.

## 23. Documentation deliverables

Before release, add:

- [`networking_api.md`](networking_api.md): public API reference and a minimal
  client/server/P2P example;
- delivery-mode and lane selection guidance;
- [`networking_operations.md`](networking_operations.md): queue, message-size,
  timeout, memory-limit, simulator, and NAT/relay operations guidance;
- identity/key management and threat model documentation;
- rendezvous/STUN/relay deployment and rotation runbook;
- [`networking_wire_protocol.md`](networking_wire_protocol.md): wire protocol
  specification with versioning rules and test vectors;
- [`networking_error_catalogue.md`](networking_error_catalogue.md): error and
  close reason catalogue;
- [`networking_statistics.md`](networking_statistics.md): statistics field
  definitions and health threshold guidance;
- network simulator usage and reproducible bug-report format;
- compatibility and migration policy.

## 24. Definition of done

The feature is complete only when applications can use one stable message API for
direct or relayed IPv4/IPv6 connections; choose reliable or unreliable delivery
per message; send messages larger than one packet; use independent channels and
priority lanes; authenticate/encrypt peers; inspect connection quality; and
reproduce adverse-network failures deterministically.

Completion additionally requires all phase gates, full lifecycle/allocation/error
coverage expected by `AGENTS.md`, no plaintext fallback, bounded resource use
under hostile input, a documented NAT infrastructure story, and passing Windows,
Linux, and macOS CI. A loopback demo alone is not evidence of completion.
