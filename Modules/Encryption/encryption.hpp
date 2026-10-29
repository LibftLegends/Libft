#ifndef ENCRYPTION_HPP
#define ENCRYPTION_HPP

#include "../Basic/basic.hpp"
#include <sys/types.h>
#include "encryption_aead_context.hpp"
#include "../Networking/openssl_support.hpp"

void    aes_encrypt(uint8_t *block_buffer, const uint8_t *key_buffer);
void    aes_decrypt(uint8_t *block_buffer, const uint8_t *key_buffer);
void    aes_encrypt_software(uint8_t *block_buffer, const uint8_t *key_buffer);
void    aes_decrypt_software(uint8_t *block_buffer, const uint8_t *key_buffer);

#ifdef LIBFT_TEST_BUILD
typedef int32_t (*t_be_open_function)(const char *path_name, int32_t flags,
    mode_t mode);
typedef int64_t (*t_be_write_function)(int32_t file_descriptor,
    const void *buffer, ft_size_t count);
#endif

int32_t         be_save_game(const char *filename, const char *data,
                    const char *key_string);
char            **be_decrypt_data(char **data_buffer, const char *key_string);
const char      *be_get_encryption_key();
#ifdef LIBFT_TEST_BUILD
void            be_set_save_game_hooks(t_be_open_function open_function,
                    t_be_write_function write_function);
#endif

uint8_t *encryption_generate_symmetric_key(ft_size_t key_length);
uint8_t *encryption_generate_initialization_vector(
    ft_size_t initialization_vector_length);
int32_t encryption_fill_secure_buffer(uint8_t *buffer, ft_size_t buffer_length);

int32_t encryption_secure_wipe(void *buffer, ft_size_t buffer_size);
int32_t encryption_secure_wipe_string(char *string_buffer);

void sha1_hash(const void *data, ft_size_t length, uint8_t *digest);
void sha256_hash(const void *data, ft_size_t length, uint8_t *digest);

int32_t sha3_256_hash(const void *data, ft_size_t length, uint8_t *digest);
int32_t sha3_512_hash(const void *data, ft_size_t length, uint8_t *digest);

int32_t blake2b_hash(const void *data, ft_size_t length, uint8_t *digest,
    ft_size_t digest_length);
int32_t blake2s_hash(const void *data, ft_size_t length, uint8_t *digest,
    ft_size_t digest_length);

int32_t rsa_generate_key_pair(uint64_t *public_key, uint64_t *private_key,
    uint64_t *modulus, int32_t bit_size);
uint64_t rsa_encrypt(uint64_t message, uint64_t public_key, uint64_t modulus);
uint64_t rsa_decrypt(uint64_t cipher, uint64_t private_key, uint64_t modulus);
#ifdef LIBFT_TEST_BUILD
void rsa_set_force_mod_inverse_failure(ft_bool enable);
#endif

typedef void (*t_encryption_block_function)(uint8_t *block_buffer,
    const uint8_t *key_buffer);
typedef t_encryption_block_function t_encryption_aes_block_function;

struct s_encryption_block_hooks
{
    union
    {
        t_encryption_block_function encrypt;
        t_encryption_block_function aes_encrypt;
    };
    union
    {
        t_encryption_block_function decrypt;
        t_encryption_block_function aes_decrypt;
    };
};

typedef s_encryption_block_hooks s_encryption_hardware_hooks;

int32_t encryption_register_block_hooks(
    const s_encryption_block_hooks &block_hooks,
    const char *algorithm_name = ft_nullptr);
void    encryption_clear_block_hooks(void);
void    encryption_get_block_hooks(
    s_encryption_block_hooks &block_hooks_output);
int32_t encryption_get_block_cipher_name(ft_string &algorithm_name_output);
ft_bool encryption_try_block_encrypt(uint8_t *block_buffer,
    const uint8_t *key_buffer);
ft_bool encryption_try_block_decrypt(uint8_t *block_buffer,
    const uint8_t *key_buffer);

int32_t encryption_register_hardware_hooks(
    const s_encryption_hardware_hooks &hardware_hooks,
    const char *algorithm_name = ft_nullptr);
void    encryption_clear_hardware_hooks(void);
void    encryption_get_hardware_hooks(
    s_encryption_hardware_hooks &hardware_hooks_output);
ft_bool encryption_try_hardware_aes_encrypt(uint8_t *block_buffer,
    const uint8_t *key_buffer);
ft_bool encryption_try_hardware_aes_decrypt(uint8_t *block_buffer,
    const uint8_t *key_buffer);

#if NETWORKING_HAS_OPENSSL
struct hmac_sha256_stream
{
    EVP_MAC     *mac_handle;
    EVP_MAC_CTX *context_handle;
    ft_bool     initialised;
    ft_bool     finished;
};

int32_t hmac_sha256_stream_init(hmac_sha256_stream &stream_state,
    const uint8_t *key_buffer, ft_size_t key_length);
int32_t hmac_sha256_stream_update(hmac_sha256_stream &stream_state,
    const void *data_buffer, ft_size_t data_length);
int32_t hmac_sha256_stream_final(hmac_sha256_stream &stream_state,
    uint8_t *digest_buffer, ft_size_t digest_length);
void hmac_sha256_stream_cleanup(hmac_sha256_stream &stream_state);

void hmac_sha256(const uint8_t *key_buffer, ft_size_t key_length,
    const void *data_buffer, ft_size_t data_length, uint8_t *digest_buffer);
#endif

#endif
