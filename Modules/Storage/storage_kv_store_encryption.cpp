#include "kv_store.hpp"

#include "../CMA/CMA.hpp"
#include "../Compression/compression.hpp"
#include "../Encryption/encryption.hpp"
#include "../Errno/errno_internal.hpp"

#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

static constexpr ft_size_t STORAGE_CIPHER_BLOCK_SIZE = 16U;
static constexpr char STORAGE_CIPHER_HEADER[] = "kvstore-aes-ctr!";

static void storage_kv_apply_block_cipher(uint8_t *payload, ft_size_t length,
    const uint8_t *key) noexcept
{
    uint64_t block_counter = 0;
    while (length != 0)
    {
        uint8_t counter_block[STORAGE_CIPHER_BLOCK_SIZE];
        ft_memcpy_fixed<sizeof(counter_block)>(counter_block, STORAGE_CIPHER_HEADER);
        ft_size_t counter_index = 0;
        while (counter_index < sizeof(block_counter))
        {
            counter_block[STORAGE_CIPHER_BLOCK_SIZE - 1U - counter_index] =
                static_cast<uint8_t>(block_counter >> (counter_index * 8U));
            ++counter_index;
        }
        aes_encrypt(counter_block, key);
        ft_size_t block_length = STORAGE_CIPHER_BLOCK_SIZE;
        if (length < block_length)
            block_length = length;
        ft_size_t index = 0;
        while (index < block_length)
        {
            payload[index] ^= counter_block[index];
            ++index;
        }
        payload += block_length;
        length -= block_length;
        ++block_counter;
    }
    return ;
}

int32_t kv_store::encrypt_value(const ft_string &plain_string, ft_string &encoded_string) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::encrypt_value");
    if (this->_encryption_enabled == FT_FALSE)
    {
        if (&plain_string == &encoded_string)
            return (FT_ERR_SUCCESS);
        return (encoded_string.assign(plain_string.c_str(), plain_string.size()));
    }
    if (this->_encryption_key.size() != STORAGE_CIPHER_BLOCK_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    const ft_size_t plain_size = plain_string.size();
    if (plain_size > FT_SYSTEM_SIZE_MAX - STORAGE_CIPHER_BLOCK_SIZE)
        return (FT_ERR_NO_MEMORY);
    const ft_size_t buffer_size = STORAGE_CIPHER_BLOCK_SIZE + plain_size;
    uint8_t *buffer = static_cast<uint8_t *>(cma_malloc(buffer_size));
    if (buffer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    const uint8_t *key = reinterpret_cast<const uint8_t *>(this->_encryption_key.c_str());
    ft_memcpy_fixed<STORAGE_CIPHER_BLOCK_SIZE>(buffer, STORAGE_CIPHER_HEADER);
    aes_encrypt(buffer, key);
    if (plain_size != 0)
    {
        ft_memcpy(buffer + STORAGE_CIPHER_BLOCK_SIZE, plain_string.c_str(), plain_size);
        storage_kv_apply_block_cipher(buffer + STORAGE_CIPHER_BLOCK_SIZE, plain_size, key);
    }
    std::size_t encoded_size = 0;
    uint8_t *encoded = ft_base64_encode(buffer, buffer_size, &encoded_size);
    cma_free(buffer);
    if (encoded == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    const int32_t result = encoded_string.assign(reinterpret_cast<const char *>(encoded), encoded_size);
    cma_free(encoded);
    return (result);
}

int32_t kv_store::decrypt_value(const ft_string &encoded_string, ft_string &plain_string) const
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::decrypt_value");
    if (this->_encryption_enabled == FT_FALSE)
    {
        if (&encoded_string == &plain_string)
            return (FT_ERR_SUCCESS);
        return (plain_string.assign(encoded_string.c_str(), encoded_string.size()));
    }
    if (this->_encryption_key.size() != STORAGE_CIPHER_BLOCK_SIZE)
        return (FT_ERR_INVALID_ARGUMENT);
    std::size_t decoded_size = 0;
    uint8_t *decoded = ft_base64_decode(reinterpret_cast<const uint8_t *>(encoded_string.c_str()),
        encoded_string.size(), &decoded_size);
    if (decoded == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if (decoded_size == 0)
        result = plain_string.clear();
    else if (decoded_size >= STORAGE_CIPHER_BLOCK_SIZE)
    {
        const uint8_t *key = reinterpret_cast<const uint8_t *>(this->_encryption_key.c_str());
        uint8_t expected_header[STORAGE_CIPHER_BLOCK_SIZE];
        ft_memcpy_fixed<sizeof(expected_header)>(expected_header, STORAGE_CIPHER_HEADER);
        aes_encrypt(expected_header, key);
        if (ft_memcmp(decoded, expected_header, sizeof(expected_header)) == 0)
        {
            uint8_t *payload = decoded + STORAGE_CIPHER_BLOCK_SIZE;
            const ft_size_t payload_size = decoded_size - STORAGE_CIPHER_BLOCK_SIZE;
            storage_kv_apply_block_cipher(payload, payload_size, key);
            result = plain_string.assign(reinterpret_cast<const char *>(payload), payload_size);
        }
    }
    cma_free(decoded);
    return (result);
}

int32_t kv_store::configure_encryption(const char *encryption_key, ft_bool enable_encryption,
    const char *encryption_algorithm_name)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "kv_store::configure_encryption");
    if (encryption_algorithm_name == ft_nullptr)
        encryption_algorithm_name = "aes-128-ecb-base64";
    if (encryption_key == ft_nullptr)
        encryption_key = "";
    if (enable_encryption && (ft_strlen_size_t(encryption_key) != STORAGE_CIPHER_BLOCK_SIZE
            || encryption_algorithm_name[0] == '\0'))
        return (FT_ERR_INVALID_ARGUMENT);
    ft_string staged_key;
    ft_string staged_algorithm;
    int32_t result = staged_key.initialize(encryption_key);
    if (result == FT_ERR_SUCCESS)
        result = staged_algorithm.initialize(encryption_algorithm_name);
    if (result != FT_ERR_SUCCESS)
        return (result);
    ft_bool lock_acquired = FT_FALSE;
    result = this->lock_store(&lock_acquired);
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_key.move(staged_key);
    if (result == FT_ERR_SUCCESS)
        result = this->_encryption_algorithm_name.move(staged_algorithm);
    if (result == FT_ERR_SUCCESS)
        this->_encryption_enabled = enable_encryption != FT_FALSE;
    this->unlock_store_guard(lock_acquired, result);
    return (result);
}
