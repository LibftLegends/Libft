# Networking

The `Networking` module provides portable socket wrappers, DNS resolution, event loops, UDP helpers, socket classes, HTTP/HTTPS clients and servers, WebSocket clients/servers, HTTP/2 support, QUIC experiments, and TLS/AEAD utilities.

## Low-Level Socket Wrappers

- `nw_socket`, `nw_bind`, `nw_connect`, `nw_accept`, `nw_listen`, `nw_close`, and `nw_shutdown` - Portable wrappers for core socket operations.
- `nw_send`, `nw_recv`, `nw_sendto`, and `nw_recvfrom` - Portable send/receive wrappers.
- `nw_inet_pton` - Parses textual IP addresses into network byte order.
- `nw_set_nonblocking` - Enables non-blocking mode.
- `nw_poll` - One-shot wait on read/write descriptor sets; readiness is
  reported by filtering the supplied arrays in place. Do not pass persistent
  event-loop registration storage to it. Its return value counts unique ready
  descriptors, even when a descriptor is registered for both read and write.
- `t_nw_socket_hook` and `nw_set_socket_hook` - Test hook for socket creation.
- `networking_check_socket_after_send(...)` and `networking_check_ssl_after_send(...)` - Convert send/SSL state into project error codes.

## DNS and Event Loops

- `networking_resolved_address` - Resolved socket address plus length.
- `networking_dns_resolve(...)` - Resolves all matching addresses into a vector.
- `networking_dns_resolve_first(...)` - Resolves the first matching address.
- `networking_resolved_address_to_string(...)` - Converts a resolved address into a printable IP string.
- `networking_dns_enable_thread_safety()`, `networking_dns_disable_thread_safety()`, and `networking_dns_is_thread_safe()` - Manage resolver synchronization.
- `networking_dns_clear_cache()` and `networking_dns_set_error(...)` - Clear cache or set resolver error state.
- `event_loop` - Persistent, thread-safe readiness loop with separate
  registration and readiness state.
- `event_loop_initialize`, `event_loop_destroy`, `event_loop_init`,
  `event_loop_clear`, `event_loop_add_interest`,
  `event_loop_remove_interest`, `event_loop_wait`, and `event_loop_interrupt`
  - Manage persistent registrations and explicit readiness events. A single
  descriptor may register read and write interest together.
- `event_loop_add_socket`, `event_loop_remove_socket`, and `event_loop_run`
  remain compatibility wrappers. `event_loop_run` no longer mutates its
  registrations.
- UDP event-loop helpers wait for read/write readiness or perform timed receive/send through an `udp_socket`.

## Socket Configuration and Classes

- `SocketType` - Server, client, or raw socket role.
- `SocketConfig` - Lifecycle configuration with IP, port, backlog, protocol, family, reuse-address flag, non-blocking flag, timeouts, and multicast fields.
- `socket_config_prepare_thread_safety`, `teardown`, `lock`, and `unlock` - Synchronize public socket configuration structs.
- `ft_socket_handle` - Lifecycle RAII-style socket descriptor holder with open/close/reset/release behavior.
- `ft_socket` - Lifecycle socket object for configure, bind/connect/listen/accept/send/receive/close workflows.
- `udp_socket` - Lifecycle UDP socket wrapper supporting bind, sendto/recvfrom, multicast, close, and optional thread safety.

## HTTP, HTTP/2, and TLS

- `http_client.hpp` - HTTP client helpers for request/response exchange.
- `http_server.hpp` and `ft_http_server` - HTTP server lifecycle class with route/listen/stop behavior.
- `http2_header_field` - HTTP/2 header key/value pair.
- `http2_frame` - HTTP/2 frame metadata and payload container.
- `http2_settings_state` - HTTP/2 settings values and acknowledgement state.
- `http2_stream_state` - Per-stream state data.
- `http2_stream_manager` - Tracks stream ids and stream lifecycle.
- `networking_tls_aead.hpp` - TLS-related AEAD helpers when OpenSSL support is enabled.
- `ssl_wrapper.hpp` - OpenSSL wrapper functions and lifecycle helpers.
- `openssl_support.hpp` - Compile-time OpenSSL availability and includes.

## WebSocket and QUIC

- `ft_websocket_client` - Lifecycle WebSocket client with connect, send, receive, close, and optional thread safety.
- `ft_websocket_server` - Lifecycle WebSocket server with listen/accept/broadcast/client management and optional thread safety.
- `s_connection_state` - WebSocket server client connection state.
- `quic_feature_configuration` - Feature flags/settings for experimental QUIC.
- `quic_datagram_plaintext` - Plaintext datagram payload holder.
- `quic_experimental_session` - Experimental lifecycle QUIC session abstraction.

## Message Transport

- `message_transport.hpp` - Message-oriented UDP transport with
  reliable ordered, unreliable, and unreliable-sequenced delivery modes.
  It also exposes listen/connect and explicit accept/reject for authenticated
  incoming requests, per-connection lane weights, queue limits, explicit
  flush, bulk receive, bounded polled events, close/abort reasons, wire-bound
  key epochs, and externally driven or socket-readiness-backed worker-driven
  operation.
  Thread-safe operation is opt-in through the transport configuration.
  Authenticated path challenge/response validates a new source before emitting
  `PATH_CHANGED` and migrating the active endpoint.
  `connect_peer` requires a caller-supplied ticket verifier; reliable sends are
  bounded by peer-advertised receive credit. `export_observability()` emits one
  aggregate bounded statistics sample through Libft's Networking Observability
  exporter without invoking user code while the transport lock is held. While
  the worker is running, mutating calls are copied into a bounded command queue
  and complete synchronously with the worker's result. Statistics include
  per-lane message/byte totals, queue pressure, weights/reservations, and the
  current lane send rate.
- `networking_udp_datagram_io` - Non-blocking production UDP adapter for the
  transport layer.
- `networking_crypto_backend.hpp` - Libft-owned cryptographic backend boundary;
  it has no OpenSSL dependency and adapts the standalone `Modules/Crypto`
  implementation to the transport. It does not contain the primitive algorithms;
  it exposes directional session-key derivation, secure randomness, and wiping
  for the handshake layer.
- `networking_secure_channel.hpp` - Encrypted channel wrapper with packet
  number-derived nonces, authenticated headers, and a replay window.
  Applications must provide key material; the message transport enables
  encryption by default and does not silently downgrade to plaintext. Key
  epoch rotation prepares replacement directional backends before committing,
  so preparation failures leave the prior channel usable.
- `networking_handshake.hpp` - Canonical client/server hello exchange,
  transcript-bound directional key derivation, Finished MAC verification,
  address-bound retry cookies, stateless retry challenges, and bounded
  retransmission. The connection API exposes the authenticated handshake's
  ephemeral peer key for diagnostics; long-term identity verification remains
  release-gated work.
- `networking_simulator.hpp` - Seeded latency, jitter, loss, duplication,
  corruption, reordering, MTU, and manual-clock simulation adapter.
- `networking_nat_traversal.hpp` - Candidate-provider gathering, authenticated
  ticket validation hooks, concurrent candidate-pair probing, direct-path
  nomination, and relay-fallback state machine. Rendezvous/STUN/relay services
  provide the network-side infrastructure and application ticket verifier.

The detailed implementation contract, wire-format rules, resource limits,
security requirements, and test gates are documented in
`Docs/steam_style_networking_design.md` until implementation completion.
