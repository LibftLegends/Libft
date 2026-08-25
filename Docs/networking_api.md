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
