# Libft networking errors and close reasons

The message transport returns Libft `FT_ERR_*` values. Callers should treat
the result as the operation outcome; peer-controlled packet data never becomes
an exception or process abort.

## Common operation errors

| Result | Meaning | Caller action |
| --- | --- | --- |
| `FT_ERR_SUCCESS` | Operation completed. | Continue. |
| `FT_ERR_NOT_INITIALISED` | Lifecycle setup has not completed. | Call the required `initialize(...)` first. |
| `FT_ERR_ALREADY_INITIALISED` | Initialization was requested twice. | Destroy or reuse the initialized object according to its API. |
| `FT_ERR_INVALID_ARGUMENT` | A local argument is malformed. | Correct the caller input. |
| `FT_ERR_INVALID_HANDLE` | The connection handle is unknown or closed. | Discard the stale handle. |
| `FT_ERR_INVALID_STATE` | The operation is not legal in the current state. | Wait for the relevant event or change lifecycle state. |
| `FT_ERR_FULL` | A queue, flow-control window, or bounded resource is full. | Apply backpressure and retry after progress. |
| `FT_ERR_EMPTY` | No message or event is currently queued. | Poll again later. |
| `FT_ERR_NO_MEMORY` | A bounded allocation failed. | Report local resource pressure; no partial send is committed. |
| `FT_ERR_TIMEOUT` | A timed wait or protocol deadline expired. | Retry or close according to the connection policy. |
| `FT_ERR_SOCKET_SEND_FAILED` | The datagram provider could not send. | Treat the path as degraded and observe the connection event. |
| `FT_ERR_PERMISSION_DENIED` | Authentication, replay, cookie, or path validation failed. | Do not downgrade to plaintext; reject or close. |
| `FT_ERR_PROTOCOL_ERROR` | A packet violated the versioned wire contract. | Close the affected connection and record bounded diagnostics. |
| `FT_ERR_UNSUPPORTED_TYPE` | The requested optional feature is unavailable. | Use a deliberately configured compatible mode. |

Exact numeric values remain owned by `Modules/Errno`; applications should not
serialize numeric error values across the network.

## Close reasons

`networking_message_close_reason` is carried inside an authenticated close
frame and exposed in terminal events:

- `NONE`: no additional reason was supplied;
- `APPLICATION`: the local or remote application requested a graceful close;
- `PROTOCOL_ERROR`: malformed, non-canonical, or unsupported critical input;
- `AUTHENTICATION_FAILED`: handshake, identity, ticket, AEAD, or replay failure;
- `TIMEOUT`: idle, handshake, retransmission, or path deadline expired;
- `RESOURCE_LIMIT`: a configured connection, queue, fragment, or flow limit was
  violated;
- `INTERNAL_ERROR`: a local implementation or provider failure.

Graceful close drains acknowledged reliable data before sending the terminal
close. `abort(...)` discards pending data and releases its receive/reassembly
credit immediately. A terminal event is never silently converted into a
successful send result.

## Security and logging rules

Authentication and protocol failures exposed to a peer use generic outcomes;
the detailed local counter may distinguish replay, malformed, and tag failures.
Do not log packet payloads, keys, cookies, ticket signatures, or peer-supplied
debug text without an explicit secure diagnostic build.
