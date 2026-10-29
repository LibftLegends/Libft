# LibFuzzer targets

Each target is built with ASan and UBSan by its matching Make target. The
checked-in corpus folders provide initial inputs; targets do not fetch data at
runtime. Crash inputs and logs should be retained as CI artifacts.

The scheduled `crypto-timing-review` target runs Welch t-tests over equal and
last-byte-different constant-time comparisons and two X25519 scalar classes.
It is review evidence only; statistical results cannot prove constant-time
behavior or replace independent cryptographic review.

The `crypto-review` target checks the AEAD tag-compare loop and X25519 ladder
source for early exits, secret-dependent branches, and secret-derived table
indexes. These source checks do not replace inspection of compiler output or an
independent cryptographic review.

Crypto targets cover primitive parsing and operations, including SHA-256,
HMAC/HKDF, AEAD, X25519, and session key updates. Networking targets cover
whole datagrams, packet codec mutations, ACK/state operation sequences,
connection listen/connect/accept/reject/close transitions, handshakes, NAT,
simulator scripts, and fragment reassembly. The networking targets initialize
transport state before parsing input so structured packets
can reach deeper paths.

The AEAD target derives key, nonce, AAD, and plaintext from each input, verifies
the valid seal/open round trip, then flips an authentication-tag bit and a
ciphertext bit. Both malformed opens must return the generic authentication
error and leave the plaintext output empty. The AEAD corpus includes a
non-empty ciphertext seed so both mutation paths are reached immediately.

The packet codec target uses `networking_codec.dict` for protocol magic,
control-frame types, and common length and packet-number encodings. The codec,
authenticated-packet, reassembly, handshake, ACK/state, NAT, simulator, and
connection-state targets provide bounded custom mutators. The packet and
fragment harnesses rebuild valid outer structures around mutated data; the
operation-driven harnesses preserve bounded input length while varying their
operation streams and protocol fields.

Run one target from the repository root, for example:

```sh
make crypto-session-fuzz FUZZ_CXX=clang++
./Test/crypto_session_fuzz -max_total_time=30 \
    Test/Fuzz/corpus/crypto_session
```
