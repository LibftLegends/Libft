# Encryption

The `Encryption` module provides AES block operations, save-game encryption helpers, secure key/IV generation, secure wiping, hashes, HMAC, demonstration RSA helpers, runtime-swappable block-cipher hooks, and OpenSSL-backed AEAD helpers.

## AES and Save-Game Helpers

- `aes_encrypt(...)` / `aes_decrypt(...)` - Encrypt or decrypt one AES block, using the currently registered block-cipher hook when available and falling back to software AES otherwise.
- `aes_encrypt_software(...)` / `aes_decrypt_software(...)` - Force the software AES implementation for one block.
- `t_be_open_function` / `t_be_write_function` - Test-build file IO hooks used by save-game helpers.
- `be_save_game(...)` - Encrypts and writes save-game data.
- `be_decrypt_data(...)` - Decrypts a null-terminated data buffer array with a key string.
- `be_get_encryption_key()` - Returns a generated encryption key string.
- `be_set_save_game_hooks(...)` - Installs custom file open/write hooks in test builds.

## Key Material and Secure Wipe

- `encryption_generate_symmetric_key(ft_size_t key_length)` - Allocates and fills a symmetric key buffer with secure random bytes.
- `encryption_generate_initialization_vector(ft_size_t initialization_vector_length)` - Allocates and fills an IV buffer.
- `encryption_fill_secure_buffer(uint8_t *buffer, ft_size_t buffer_length)` - Fills caller storage with secure random bytes.
- `encryption_secure_wipe(void *buffer, ft_size_t buffer_size)` - Clears sensitive memory through a wipe path intended not to be optimized out.
- `encryption_secure_wipe_string(char *string_buffer)` - Securely clears a null-terminated string including its terminator.

## Hashing, HMAC, and RSA

- `sha1_hash(...)` - Writes a SHA-1 digest.
- `sha256_hash(...)` - Writes a SHA-256 digest.
- `sha3_256_hash(...)` / `sha3_512_hash(...)` - Write SHA-3 digests.
- `blake2b_hash(...)` / `blake2s_hash(...)` - Write BLAKE2 digests with caller-selected output length.
- `hmac_sha256(...)` - Writes an HMAC-SHA-256 digest when OpenSSL support is available.
- `hmac_sha256_stream` - OpenSSL streaming HMAC state.
- `hmac_sha256_stream_init(...)`, `update(...)`, `final(...)`, and `cleanup(...)` - Manage streaming HMAC-SHA-256.
- `rsa_generate_key_pair(...)` - Generates a small demonstration RSA key pair.
- `rsa_encrypt(...)` / `rsa_decrypt(...)` - Perform RSA modular exponentiation for small integer payloads.
- `rsa_set_force_mod_inverse_failure(ft_bool enable)` - Test-build hook for RSA inverse-failure paths.

## Block-Cipher Hooks

- `t_encryption_block_function` - Generic block-cipher encrypt/decrypt hook type.
- `s_encryption_block_hooks` - Generic block-cipher encrypt/decrypt hook table.
- `encryption_register_block_hooks(..., algorithm_name)` - Installs the current block-cipher implementation used by `aes_encrypt(...)` and `aes_decrypt(...)` and records the active algorithm name for persistence layers.
- `encryption_clear_block_hooks()` - Clears the registered block-cipher implementation.
- `encryption_get_block_hooks(...)` - Copies the current block-cipher hook table.
- `encryption_get_block_cipher_name(...)` - Writes the active algorithm name associated with the current block-cipher hooks into an initialized `ft_string`.
- `encryption_try_block_encrypt(...)` / `encryption_try_block_decrypt(...)` - Attempts one block operation through the registered implementation.
- `t_encryption_aes_block_function`, `s_encryption_hardware_hooks`, and the `encryption_register_hardware_hooks(...)` family remain as compatibility aliases for the same hook table.

## AEAD

When OpenSSL is available, `encryption_aead_context` provides lifecycle-managed authenticated encryption/decryption:

- Lifecycle and thread-safety methods - `initialize`, copy/move initialization, `destroy`, `move`, `enable_thread_safety`, `disable_thread_safety`, and `is_thread_safe`.
- `initialize_encrypt(...)` / `initialize_decrypt(...)` - Configures the context for AEAD encryption or decryption.
- `update_aad(...)` - Supplies additional authenticated data.
- `update(...)` - Encrypts or decrypts a data chunk.
- `finalize(...)` - Finalizes encryption and writes an authentication tag.
- `set_tag(...)` - Supplies the expected authentication tag before decrypt finalization.
- `reset()` - Clears the active OpenSSL operation.
- `encryption_aead_encrypt(...)` - One-shot AEAD encryption helper.
- `encryption_aead_decrypt(...)` - One-shot AEAD decryption helper.
