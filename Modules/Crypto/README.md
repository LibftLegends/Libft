# Crypto

`Crypto` is Libft's standalone cryptographic primitive module. It is intentionally
separate from Networking so the algorithms can be reviewed, tested, and reused
without importing socket or protocol code.

Current implementation:

- SHA-256 with incremental and one-shot APIs;
- HMAC-SHA-256;
- HKDF-SHA-256;
- ChaCha20;
- Poly1305;
- ChaCha20-Poly1305 AEAD;
- X25519 key agreement with RFC 7748 scalar handling and low-order rejection;
- HKDF-based client/server directional session-key derivation.

The implementation has no OpenSSL or third-party cryptographic-provider
dependency. Secure random bytes are obtained through Libft's OS CSPRNG boundary;
`crypto_network_backend` is the networking-facing adapter owned by this module.
It exposes authenticated packet protection, directional session-key derivation,
key updates, X25519, hashing, HMAC, secure randomness, and wiping without
making Networking own cryptographic implementation code. The old
`Networking/networking_crypto_backend.hpp` include remains a source-compatible
forwarding alias. The later Ed25519 implementation remains planned work and
must not be treated as available until its RFC vectors, malformed-input tests,
and independent review gates pass.
