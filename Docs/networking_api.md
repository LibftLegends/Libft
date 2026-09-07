# Libft message networking API

This guide describes the stable surface currently provided by
`Modules/Networking/message_transport.hpp`. The transport is a message API over
UDP; callers do not construct packet headers or retransmission state.

## Minimal client/server flow

```cpp
networking_udp_datagram_io io;
networking_message_transport transport;
networking_message_transport_config config;
networking_message_endpoint endpoint;
networking_message_connection connection;

// Fill endpoint with an IPv4 or IPv6 sockaddr and its exact length.
transport.initialize(config, io);
transport.listen(endpoint);                 // server side
transport.connect(endpoint, connection);    // client side
transport.poll(25);                         // externally driven, bounded wait
```

For authenticated incoming connections, the server receives a
`CONNECTION_REQUESTED` event and must call `accept(connection_id)` or
`reject(connection_id, reason)`. A request is not promoted to `CONNECTED` by
merely receiving a hello.

Minecraft's replication server exposes two peer-admission paths around this
transport contract. Its transitional in-process adapter may register a peer
for deterministic vertical-slice tests, but production admission must use the
authenticated path: the connection must be `CONNECTED`,
`get_remote_identity()` must succeed, and the returned identity must report
`authenticated`. Replication state must not be created before those checks;
the session ID alone is not an authentication mechanism.

## Sending

`networking_message_connection::send_message` copies the payload before it
returns. `RELIABLE_ORDERED` data is retransmitted and delivered in sequence per
channel. `UNRELIABLE` data is sent at most once. `UNRELIABLE_SEQUENCED` data is
delivered only when its sequence is newer than the latest delivered sequence.

`lane` must be 0 through 3. Lane 0 is reserved for transport control and the
remaining lanes use weighted deficit scheduling. `channel` selects an
independent reliable ordering domain. A nonzero expiry is relative to the
transport clock and produces a `DELIVERY_FAILED` event when the message expires.

Queue admission is transactional. `FT_ERR_FULL` means the message was not
partially queued. Reliable admission also observes the peer's advertised flow
credit; callers should treat `FT_ERR_FULL` as backpressure and retry after
progress.

## Authoritative replication framing

`networking_replication_sender` provides the application-independent framing
used by an authoritative server. It sends reliable control, delta, and
snapshot messages on separate lanes, plus unreliable-sequenced updates. The
message type and payload schema remain owned by the application; Libft does
not interpret blocks, lights, meshes, or gameplay rules.

`networking_replication_decode_message` validates the protocol version and
length and nonzero server/session/sequence identity, then copies the payload
into caller-owned storage. The output envelope
and payload are committed only after the complete message is valid.

For each replicated stream, the consumer can use
`networking_replication_revision_tracker` before committing decoded data. It
requires a snapshot first, accepts only contiguous block and light revisions,
and rejects a light delta whose `source_block_revision` is no longer current.
This keeps stale asynchronous lighting work from reverting newer block state.
The tracker is intentionally caller-owned and non-thread-safe; the client
replica serializes access and applies its own per-frame byte, operation, mesh,
and GPU-upload budgets.

Use `networking_replication_apply_budget` for the message, payload-byte, and
decoded-operation portion of that per-frame limit. Call
`networking_replication_apply_budget_consume` immediately before applying one
decoded message. If it returns `FT_ERR_FULL`, do not apply the message; retain
the owned message for a later pump. Reset the token at the beginning of each
frame/tick. Mesh publication and GPU upload budgets remain application-owned
because Libft Networking never owns renderer objects.

On the authoritative side, use `networking_replication_retention_window` for
each retained stream or client acknowledgement state. Append revisions in
strict order, acknowledge the highest contiguous revision received by the
client, and call `can_replay_from` before replaying retained deltas. When it
returns false, send a snapshot instead of pretending that an unavailable
delta range can be reconstructed. The window tracks ranges, not payloads, so
the application can keep payloads in a separately bounded store.

`networking_replication_hash_payload` hashes only the caller's canonical
payload bytes with SHA-256. Applications must serialize chunk/content fields
in a deterministic order before calling it; revisions, pointer addresses,
mesh data, and container capacity must not be included in the canonical bytes.
Compare the resulting content hash separately from the revision tracker when
deciding whether a repair is needed.

`networking_replication_client` is the caller-owned replica gate. Configure it
with the server instance and session IDs, frame budgets, and three application
callbacks for snapshot, block-delta, and light-delta payloads. Each callback
must apply its payload transactionally to application-owned state and return an
error without committing on failure. The client invokes the callback before
advancing its revision tracker; therefore a failed application leaves the old
revision available for retry. A full budget also invokes no callback. Call
`reset_budget` once per frame/tick, and perform mesh publication and GPU work
after the callback through separate bounded queues.

Minecraft additionally has a `WorldReplicationBlockReplica` primitive for one
chunk of canonical block state. It stages snapshots in a temporary chunk and
requires contiguous block revisions for deltas. It rejects snapshots from an
older generation or an older revision, so a delayed snapshot cannot roll a
live client backward. It also accepts light-revision metadata only when the
light result is contiguous, was computed from the replica's current block
revision, and belongs to the current generation. The light payload remains
application-owned; this metadata gate prevents stale lighting from becoming
authoritative while allowing the lighting/mesh system to apply it later.

`WorldReplicationReplicaStore` owns a bounded collection of these chunk
replicas and routes block and light updates by chunk coordinate. A missing
replica is never implicitly created by a delta: the client must apply a valid
snapshot first. The store is single-owner and should be driven from the
client replica-applier thread, while rendering consumes separately published
immutable snapshots or meshes.

For canonical repair responses, the replica stages a complete temporary chunk,
copies untouched sections from the current replica, replaces only the sections
named by the response mask, verifies the SHA-256 payload hash, and commits the
temporary chunk only after every block has decoded successfully. A malformed,
truncated, hashed-differently, or stale repair therefore leaves the prior
replica unchanged. Application-defined repair formats still go through the
application callback because Libft cannot interpret their bytes.

The client gate is deliberately single-owner and non-thread-safe. The
application may run the transport receive loop elsewhere, but decoded payloads
must be handed to one replica-applier thread (normally the client world worker)
so callback state, revision state, and budget accounting cannot race. A failed
callback does not advance a revision or consume budget, allowing the caller to
retain and retry the owned payload.

### Staged block-edit and lighting publication

Voxel applications should replicate a player edit and its derived lighting as
separate stages:

```text
client -> EDIT_INTENT
server validates and commits the block change
server -> EDIT_RESULT + CHUNK_BLOCK_DELTA
client applies the block delta promptly
server schedules lighting asynchronously
server -> CHUNK_LIGHT_DELTA(source_block_revision)
client applies the light result only when its source revision is current
```

An accepted intent produces one contiguous block revision and an owned block
delta; lighting, meshing, and GPU work must not delay that response. A rejected
intent produces an authoritative rejection and no block delta. The light
delta's `source_block_revision` prevents delayed lighting work from replacing
newer block state. Clients must bound packet parsing and canonical state
application per frame, while lighting propagation, remeshing, mesh publication,
and GPU upload remain asynchronous application work outside the render-critical
path.

Minecraft's protocol layer supplies codecs for `EDIT_INTENT`, `EDIT_RESULT`,
`CHUNK_BLOCK_DELTA`, and `CHUNK_SNAPSHOT`, plus a `CHUNK_LIGHT_DELTA` envelope.
The light envelope carries the chunk identity, base and final light revisions,
the source block revision, a generation epoch, and an owned payload. Its
decoder is transactional and rejects invalid revision relationships,
oversized/truncated payloads, and trailing bytes. The payload format remains
application-owned so Libft Networking does not need to know voxel or light
representation details.

The Minecraft replication boundary also exposes `CHUNK_SYNC_REQUEST`. A client
places its last applied block and light revisions in this reliable control
message after subscribing or reconnecting. The server validates the session
and chunk interest, then independently replays contiguous entries from the
block and light journals or sends a snapshot when either cursor is outside the
retained range. The request decoder restores the input cursor and leaves its
destination unchanged on truncated input. Reconnects still require a fresh
snapshot until acknowledgement cursors are persisted across sessions.

`WorldReplicationClientRuntime` exposes the same synchronization request as a
runtime operation. Callers using the runtime facade should call
`request_chunk_sync()` rather than reaching through `client()`, keeping
initialization checks and the transport/runtime ownership boundary in one
place. The request remains a reliable control message and does not perform
world, lighting, or mesh work synchronously.

Hash reconciliation is currently represented by Minecraft-owned codecs for
`CHUNK_HASH_MANIFEST`, `CHUNK_REPAIR_REQUEST`, and
`CHUNK_REPAIR_RESPONSE`. These codecs validate identity, revisions, section
masks, fixed-size SHA-256 hashes, payload bounds, payload format, and trailing
input. The Minecraft client now exposes bounded repair-request sending and a
validated repair-response callback. For the canonical block-stream format it
verifies the wire payload hash before invoking the callback; application-owned
formats must perform canonical-content validation in the callback. The server session now validates
and dispatches repair requests through an application-owned provider before
sending the authoritative response. The world service also provides a default
response whose payload is the canonical little-endian block stream and whose
hash is computed over that stream. The default response emits only the
requested sections, in ascending section order, with blocks ordered `z`, `y`,
`x`; a full mask therefore remains a full chunk stream. The response's section
mask describes exactly those bytes. The service obtains one locked immutable
block snapshot before constructing the stream, rather than locking once per
block, so hash and repair work does not amplify authoritative-chunk
contention. The executable validator
`ft_vox --validate-network-repair` now covers snapshot reconstruction,
single-section repair, payload hashing, and preservation after a rejected
repair. Multi-client transport repair tests and application-specific repair
formats remain outstanding.
Libft Networking deliberately does not generate chunk hashes or interpret
repair payloads.

## Closing and identity

`close(reason, text)` drains reliable frames before sending the authenticated
close frame. `abort(reason)` discards queued and in-flight application data and
sends a best-effort close. Debug text is bounded to 95 bytes.

`get_remote_identity` returns the peer's ephemeral X25519 key after an
authenticated handshake reaches `CONNECTED`. This is useful for diagnostics
and channel binding. It is not a long-term Ed25519 identity; applications that
need a stable identity must verify a rendezvous ticket or an application-level
credential until the reviewed Ed25519 module is added.

Set `enable_peer_key_pinning` and provide `pinned_peer_public_key` when a
transport instance must accept only one expected ephemeral peer key. Pinning
requires the authenticated handshake and rejects a mismatched key before
traffic keys are derived. It is a channel-establishment check, not a
replacement for a stable signed identity.

`request_key_update(next_epoch)` sends an authenticated control packet and
rotates the caller's send key after an authenticated acknowledgement; requests
are retransmitted on timeout and the peer retains the prior receive epoch long
enough to answer duplicate requests. The prior receive key is wiped after four
retransmission-timeout intervals, so delayed old-epoch packets are rejected
after the recovery window. It requires no queued or in-flight
reliable data. The older `update_key_epoch` method remains available for
explicitly coordinated test or application transitions.

## Peer tickets

`connect_peer(ticket, verifier, connection)` rejects expired or malformed tickets
and calls the supplied verifier before selecting a candidate. The overload
without a verifier intentionally returns `FT_ERR_UNSUPPORTED_TYPE`; secure P2P
code must not silently trust an unverified candidate list. Ticket signatures are
opaque to Networking and are verified by the caller's reviewed rendezvous
implementation.

The lower-level `networking_nat_traversal` API gathers local candidates, accepts
verified peer tickets, and starts probing with `begin`. Use `probe_batch` with a
bounded `maximum_probes` value when several candidate pairs should be in flight;
the minimum interval applies between batches, not between individual probes.
`probe_next` remains the one-pair form. Both return `FT_ERR_TIMEOUT` until the
interval has elapsed and `FT_ERR_NOT_FOUND` after all pairs have been attempted.
`mark_probe_success` requires the ticket attempt identifier, so an old or
off-path response cannot nominate a path for a different attempt.
The `set_peer_ticket(ticket, now)` overload without a verifier is intentionally
unsupported; callers must use the verifier overload so a non-empty opaque
signature is never mistaken for an authenticated rendezvous ticket.

`networking_nat_relay_datagram_io` adapts an authenticated relay service to the
same `networking_datagram_io` contract used by the message transport. It forwards
complete datagrams, reports the relay's clock, and closes the relay allocation
when destroyed. The relay implementation must provide receive support; the
default base implementation returns `FT_ERR_UNSUPPORTED_TYPE`.

## Observability export

`export_observability()` aggregates the bounded snapshots of all current
connections and sends one sample to Libft's Networking Observability exporter.
The exporter is called after the transport lock is released, so it may inspect
or update application state without creating a transport-lock callback cycle.
Initialize the Observability exporter before calling this method.

## Worker mode

`start_worker` starts one long-lived worker. UDP-backed transports wait for
socket readability with a platform-neutral readiness boundary; custom
`networking_datagram_io` implementations that do not provide readiness use the
Libft wakeable timed fallback. `stop_worker` wakes and joins the worker. User
callbacks are never invoked by the worker; call `dispatch_callbacks()` from the
owning/application thread to drain deferred callbacks. While the worker is
running, mutating operations
(`open_connection`, `listen`, `accept`, `reject`, connection sends, close/key
updates, lane/queue changes, flush, and callback registration) are copied into
a bounded command queue and complete synchronously after worker execution.
`poll()` from a non-worker thread returns `FT_ERR_THREAD_BUSY` so two pollers
cannot advance the protocol concurrently.

Thread safety is opt-in. `is_thread_safe()` reports the current mode, while
`enable_thread_safety()` and `disable_thread_safety()` change it when no worker
is active. The transport returns a lifecycle/thread error instead of destroying
a mutex that could still be used by the worker.

`set_event_callback(callback, user_data)` is optional. Registered callbacks are
deferred until the transport has released its internal lock, and may therefore
query or update the transport. In externally driven mode `poll()` drains them;
in worker mode the owning thread must call `dispatch_callbacks()`. The callback
receives bounded event data; the normal event queue remains available for
polling as well. Passing a null callback disables delivery and clears deferred
callback notifications.

## Error handling

Every operation returns a Libft error code. `FT_ERR_INVALID_STATE` means the
connection lifecycle does not allow the operation, `FT_ERR_FULL` means bounded
backpressure, `FT_ERR_PERMISSION_DENIED` covers authentication/path failures,
and `FT_ERR_TIMEOUT` identifies a timer-driven failure. A reliable send never
returns success and silently discards its data.
