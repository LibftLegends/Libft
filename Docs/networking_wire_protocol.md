# Libft message transport wire protocol

Version 1 is intentionally Libft-owned and is not Steam-compatible. All
integers are unsigned network byte order. Every parser checks the remaining
buffer before reading a field and packet data is never cast to a C++ structure.

## Common envelope

Every packet begins with:

| Bytes | Field |
| ---: | --- |
| 1 | magic (`0x4c`) |
| 1 | version (`1`) |
| 1 | packet kind |
| 1 | flags/delivery value |

Authenticated application packets then carry a connection identifier, packet
number, key epoch, and an AEAD tag. The header through the key epoch is
associated data. ChaCha20-Poly1305 uses a directional IV combined with the
packet number and key epoch; a packet number is never reused within a key epoch.

## Packet kinds

The current implementation defines:

| Kind | Meaning |
| ---: | --- |
| 1 | message fragment |
| 2 | ACK with four bounded ranges |
| 3 | authenticated close |
| 4 | handshake hello |
| 5 | handshake Finished |
| 6 | stateless retry cookie |
| 7 | path challenge |
| 8 | path response |
| 9 | receive-flow credit |
| 10 | authenticated key-update request |
| 11 | authenticated key-update acknowledgement |

Message packets contain connection ID, message ID, packet number, key epoch,
largest received packet, four compact ACK range slots, channel, lane, fragment
count, total message length, fragment offset, message sequence, payload length,
and the fragment bytes. ACK ranges are inclusive and encoded as bounded deltas
from the largest received packet. ACK-only packets are not retransmitted.

Path challenge/response packets carry an eight-byte token. A packet received
from a new source is accepted for cryptographic validation but cannot change the
active endpoint until the matching response returns from that source. A valid
response emits `PATH_CHANGED` and preserves packet, reliability, identity, and
congestion state.

Flow-control packets carry an absolute cumulative receive-credit value. Credits
never decrease after the first advertisement. Reliable senders reserve payload
bytes before queueing and return that reservation as frames are acknowledged or
expired.

Key-update requests and acknowledgements are authenticated with the current
epoch and carry the requested epoch. The sender retains its old send key until
the acknowledgement arrives, retransmitting the request on timeout. The
receiver acknowledges before rotating its receive key, then retains the prior
receive key for four retransmission-timeout intervals as a bounded
duplicate/reordering window. The old key is then wiped and rejected. Application data is not
accepted under the new epoch until the corresponding update has been processed.

## Handshake and retry

Handshake envelopes contain a hello payload with protocol version, role,
connection ID, nonce, and X25519 public key. The responder may issue a
short-lived HMAC-SHA-256 retry cookie bound to the source endpoint and hello
digest before allocating handshake state. Finished values authenticate the
ordered hello transcript and prove possession of the derived directional keys.

Malformed, unauthenticated, replayed, wrong-epoch, and wrong-source packets are
not delivered to the application. Unknown protocol extensions must be assigned
a new version until a length-delimited optional-frame parser is added.

## Compatibility rules

The magic/version pair is a wire-version boundary. A future version must either
provide an explicit compatibility parser or fail closed. Key epochs are scoped
to the connection and cannot be reset during path migration. Applications must
not persist packet numbers, tickets, or ephemeral keys across connections.
