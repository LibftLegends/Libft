#include "crypto_primitives_internal.hpp"
#include "../Errno/errno.hpp"

int32_t crypto_secure_wipe(void *buffer, ft_size_t buffer_size) noexcept
{
    volatile uint8_t *bytes;
    ft_size_t index;

    if (buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    bytes = static_cast<volatile uint8_t *>(buffer);
    index = 0U;
    while (index < buffer_size)
    {
        bytes[index] = 0U;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

namespace
{
    constexpr uint32_t SHA256_ROUND_CONSTANTS[64] = {
        0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
        0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
        0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
        0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
        0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
        0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
        0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
        0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
        0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
        0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
        0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
        0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
        0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
        0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
        0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
        0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
    };

    constexpr uint32_t SHA256_INITIAL_HASH[8] = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };

    static uint32_t rotate_right(uint32_t value, uint32_t bits) noexcept
    {
        return ((value >> bits) | (value << (32U - bits)));
    }

    static uint32_t load_u32_be(const uint8_t *data) noexcept
    {
        return ((static_cast<uint32_t>(data[0]) << 24U)
            | (static_cast<uint32_t>(data[1]) << 16U)
            | (static_cast<uint32_t>(data[2]) << 8U)
            | static_cast<uint32_t>(data[3]));
    }

    static void store_u32_be(uint8_t *data, uint32_t value) noexcept
    {
        data[0] = static_cast<uint8_t>(value >> 24U);
        data[1] = static_cast<uint8_t>(value >> 16U);
        data[2] = static_cast<uint8_t>(value >> 8U);
        data[3] = static_cast<uint8_t>(value);
        return ;
    }

    static void store_u64_be(uint8_t *data, uint64_t value) noexcept
    {
        uint32_t index;

        index = 0U;
        while (index < 8U)
        {
            data[index] = static_cast<uint8_t>(value >> (56U - index * 8U));
            index += 1U;
        }
        return ;
    }

    static void clear_sensitive(void *data, ft_size_t length) noexcept
    {
        if (data != ft_nullptr && length != 0U)
            (void)crypto_secure_wipe(data, length);
        return ;
    }

    static int32_t prepare_hmac_sha256(const uint8_t *key, ft_size_t key_length,
        crypto_sha256 &inner, crypto_sha256 &outer) noexcept
    {
        uint8_t pad[64];
        ft_size_t index;
        int32_t result;

        ft_memset(pad, 0, sizeof(pad));
        if (key_length > sizeof(pad))
        {
            result = crypto_sha256_hash(key, key_length, pad);
            if (result != FT_ERR_SUCCESS)
            {
                clear_sensitive(pad, sizeof(pad));
                return (result);
            }
        }
        else if (key_length != 0U)
            ft_memcpy(pad, key, key_length);
        index = 0U;
        while (index < sizeof(pad))
        {
            pad[index] ^= 0x36U;
            index += 1U;
        }
        result = inner.initialize();
        if (result == FT_ERR_SUCCESS)
            result = inner.update(pad, sizeof(pad));
        if (result == FT_ERR_SUCCESS)
            result = outer.initialize();
        if (result == FT_ERR_SUCCESS)
        {
            index = 0U;
            while (index < sizeof(pad))
            {
                pad[index] ^= (0x36U ^ 0x5cU);
                index += 1U;
            }
            result = outer.update(pad, sizeof(pad));
        }
        clear_sensitive(pad, sizeof(pad));
        return (result);
    }

    static int32_t finish_hmac_sha256(crypto_sha256 &inner, crypto_sha256 &outer,
        const void *first, ft_size_t first_length,
        const uint8_t *second, ft_size_t second_length,
        const uint8_t *third, ft_size_t third_length,
        uint8_t digest[32]) noexcept
    {
        uint8_t inner_digest[32];
        int32_t result = FT_ERR_SUCCESS;

        if (first_length != 0U)
            result = inner.update(first, first_length);
        if (result == FT_ERR_SUCCESS && second_length != 0U)
            result = inner.update(second, second_length);
        if (result == FT_ERR_SUCCESS && third_length != 0U)
            result = inner.update(third, third_length);
        if (result == FT_ERR_SUCCESS)
            result = inner.final(inner_digest);
        if (result == FT_ERR_SUCCESS)
            result = outer.update(inner_digest, sizeof(inner_digest));
        if (result == FT_ERR_SUCCESS)
            result = outer.final(digest);
        clear_sensitive(inner_digest, sizeof(inner_digest));
        return (result);
    }
}

crypto_sha256::crypto_sha256() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _hash(), _buffer(),
      _buffer_size(0U), _message_length_bytes(0U)
{
    return ;
}

crypto_sha256::~crypto_sha256() noexcept
{
    (void)this->destroy();
    return ;
}

int32_t crypto_sha256::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_ALREADY_INITIALISED);
    ft_memcpy_fixed<sizeof(this->_hash)>(this->_hash, SHA256_INITIAL_HASH);
    ft_memset(this->_buffer, 0, sizeof(this->_buffer));
    this->_buffer_size = 0U;
    this->_message_length_bytes = 0U;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t crypto_sha256::destroy() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    clear_sensitive(this->_hash, sizeof(this->_hash));
    clear_sensitive(this->_buffer, sizeof(this->_buffer));
    this->_buffer_size = 0U;
    this->_message_length_bytes = 0U;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

#ifdef LIBFT_TEST_BUILD
ft_bool crypto_sha256::test_state_is_wiped() const noexcept
{
    uint32_t zero_hash[8];
    uint8_t zero_buffer[64];

    ft_memset(zero_hash, 0U, sizeof(zero_hash));
    ft_memset(zero_buffer, 0U, sizeof(zero_buffer));
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED
        || ft_memcmp(this->_hash, zero_hash, sizeof(zero_hash)) != 0
        || ft_memcmp(this->_buffer, zero_buffer, sizeof(zero_buffer)) != 0
        || this->_buffer_size != 0U || this->_message_length_bytes != 0U)
        return (FT_FALSE);
    return (FT_TRUE);
}
#endif

int32_t crypto_sha256::move(crypto_sha256 &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    (void)this->destroy();
    ft_memcpy(this->_hash, other._hash, sizeof(this->_hash));
    ft_memcpy(this->_buffer, other._buffer, sizeof(this->_buffer));
    this->_buffer_size = other._buffer_size;
    this->_message_length_bytes = other._message_length_bytes;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

void crypto_sha256::copy_hmac_prefix(const crypto_sha256 &prefix) noexcept
{
    // Prepared HMAC states end on a complete block, so there is no pending input.
    ft_memcpy_fixed<sizeof(this->_hash)>(this->_hash, prefix._hash);
    this->_buffer_size = 0U;
    this->_message_length_bytes = prefix._message_length_bytes;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return ;
}

void crypto_sha256::process_block(const uint8_t block[64]) noexcept
{
    uint32_t words[64];
    uint32_t working[8];
    uint32_t index;
    uint32_t round;
    uint32_t sigma_zero;
    uint32_t sigma_one;
    uint32_t big_sigma_zero;
    uint32_t big_sigma_one;
    uint32_t choose;
    uint32_t majority;
    uint32_t temporary_one;
    uint32_t temporary_two;

    index = 0U;
    while (index < 16U)
    {
        words[index] = load_u32_be(block + index * 4U);
        index += 1U;
    }
    while (index < 64U)
    {
        sigma_zero = rotate_right(words[index - 15U], 7U)
            ^ rotate_right(words[index - 15U], 18U)
            ^ (words[index - 15U] >> 3U);
        sigma_one = rotate_right(words[index - 2U], 17U)
            ^ rotate_right(words[index - 2U], 19U)
            ^ (words[index - 2U] >> 10U);
        words[index] = words[index - 16U] + sigma_zero
            + words[index - 7U] + sigma_one;
        index += 1U;
    }
    index = 0U;
    while (index < 8U)
    {
        working[index] = this->_hash[index];
        index += 1U;
    }
    round = 0U;
    while (round < 64U)
    {
        big_sigma_one = rotate_right(working[4], 6U)
            ^ rotate_right(working[4], 11U)
            ^ rotate_right(working[4], 25U);
        choose = (working[4] & working[5])
            ^ ((~working[4]) & working[6]);
        temporary_one = working[7] + big_sigma_one + choose
            + SHA256_ROUND_CONSTANTS[round] + words[round];
        big_sigma_zero = rotate_right(working[0], 2U)
            ^ rotate_right(working[0], 13U)
            ^ rotate_right(working[0], 22U);
        majority = (working[0] & working[1])
            ^ (working[0] & working[2])
            ^ (working[1] & working[2]);
        temporary_two = big_sigma_zero + majority;
        working[7] = working[6];
        working[6] = working[5];
        working[5] = working[4];
        working[4] = working[3] + temporary_one;
        working[3] = working[2];
        working[2] = working[1];
        working[1] = working[0];
        working[0] = temporary_one + temporary_two;
        round += 1U;
    }
    index = 0U;
    while (index < 8U)
    {
        this->_hash[index] += working[index];
        index += 1U;
    }
    clear_sensitive(words, sizeof(words));
    clear_sensitive(working, sizeof(working));
    return ;
}

int32_t crypto_sha256::update(const void *data, ft_size_t length) noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (data == ft_nullptr && length != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (length > 0x1fffffffffffffffULL - this->_message_length_bytes)
        return (FT_ERR_OUT_OF_RANGE);
    this->_message_length_bytes += length;
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    if (this->_buffer_size != 0U && length != 0U)
    {
        ft_size_t copied = sizeof(this->_buffer) - this->_buffer_size;
        if (copied > length)
            copied = length;
        ft_memcpy(this->_buffer + this->_buffer_size, bytes, copied);
        this->_buffer_size += copied;
        bytes += copied;
        length -= copied;
        if (this->_buffer_size == sizeof(this->_buffer))
        {
            this->process_block(this->_buffer);
            this->_buffer_size = 0U;
        }
    }
    while (length >= sizeof(this->_buffer))
    {
        this->process_block(bytes);
        bytes += sizeof(this->_buffer);
        length -= sizeof(this->_buffer);
    }
    if (length != 0U)
    {
        ft_memcpy(this->_buffer, bytes, length);
        this->_buffer_size = length;
    }
    return (FT_ERR_SUCCESS);
}

int32_t crypto_sha256::final(uint8_t digest[32]) noexcept
{
    uint8_t padding[128];
    ft_size_t padded_length;
    ft_size_t index;
    uint64_t bit_length;

    if (digest != ft_nullptr)
        clear_sensitive(digest, 32U);
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_NOT_INITIALISED);
    if (digest == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (this->_buffer_size < 56U)
        padded_length = 64U;
    else
        padded_length = 128U;
    bit_length = this->_message_length_bytes << 3U;
    if (this->_buffer_size != 0U)
        ft_memcpy(padding, this->_buffer, this->_buffer_size);
    padding[this->_buffer_size] = 0x80U;
    index = this->_buffer_size + 1U;
    ft_memset(padding + index, 0U, padded_length - 8U - index);
    store_u64_be(padding + padded_length - 8U, bit_length);
    this->process_block(padding);
    if (padded_length > 64U)
        this->process_block(padding + 64U);
    index = 0U;
    while (index < 8U)
    {
        store_u32_be(digest + index * 4U, this->_hash[index]);
        index += 1U;
    }
    clear_sensitive(padding, sizeof(padding));
    (void)this->destroy();
    return (FT_ERR_SUCCESS);
}

int32_t crypto_sha256_hash(const void *data, ft_size_t length,
    uint8_t digest[32]) noexcept
{
    crypto_sha256 context;
    uint8_t staged_digest[32];
    int32_t result;

    ft_memset(staged_digest, 0, sizeof(staged_digest));
    if (digest == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    result = context.initialize();
    if (result == FT_ERR_SUCCESS)
        result = context.update(data, length);
    if (result == FT_ERR_SUCCESS)
        result = context.final(staged_digest);
    if (result == FT_ERR_SUCCESS)
        ft_memcpy(digest, staged_digest, sizeof(staged_digest));
    else
        clear_sensitive(digest, 32U);
    clear_sensitive(staged_digest, sizeof(staged_digest));
    (void)context.destroy();
    return (result);
}

int32_t crypto_hmac_sha256(const uint8_t *key, ft_size_t key_length,
    const void *data, ft_size_t data_length, uint8_t digest[32]) noexcept
{
    if (digest == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t result = FT_ERR_INVALID_ARGUMENT;
    if ((key != ft_nullptr || key_length == 0U)
        && (data != ft_nullptr || data_length == 0U))
    {
        crypto_sha256 inner;
        crypto_sha256 outer;
        result = prepare_hmac_sha256(key, key_length, inner, outer);
        if (result == FT_ERR_SUCCESS)
            result = finish_hmac_sha256(inner, outer, data, data_length,
                ft_nullptr, 0U, ft_nullptr, 0U, digest);
    }
    if (result != FT_ERR_SUCCESS)
        clear_sensitive(digest, 32U);
    return (result);
}

int32_t crypto_hkdf_sha256_extract(const uint8_t *salt, ft_size_t salt_length,
    const uint8_t *input_key_material, ft_size_t input_key_material_length,
    uint8_t pseudorandom_key[32]) noexcept
{
    uint8_t zero_salt[32];

    if (pseudorandom_key == ft_nullptr
        || (salt == ft_nullptr && salt_length != 0U)
        || (input_key_material == ft_nullptr && input_key_material_length != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (salt_length == 0U)
    {
        ft_memset(zero_salt, 0, sizeof(zero_salt));
        salt = zero_salt;
        salt_length = sizeof(zero_salt);
    }
    {
        int32_t result;

        result = crypto_hmac_sha256(salt, salt_length, input_key_material,
            input_key_material_length, pseudorandom_key);
        clear_sensitive(zero_salt, sizeof(zero_salt));
        if (result != FT_ERR_SUCCESS)
            clear_sensitive(pseudorandom_key, 32U);
        return (result);
    }
}

int32_t crypto_hkdf_sha256_expand(const uint8_t pseudorandom_key[32],
    const uint8_t *info, ft_size_t info_length, uint8_t *output,
    ft_size_t output_length) noexcept
{
    uint8_t block[32];
    uint8_t counter;
    ft_size_t offset;
    ft_size_t copy_length;
    const uint8_t *previous_data;
    ft_size_t previous_length;
    int32_t result;

    if (pseudorandom_key == ft_nullptr
        || (info == ft_nullptr && info_length != 0U)
        || (output == ft_nullptr && output_length != 0U))
        return (FT_ERR_INVALID_ARGUMENT);
    if (output_length > 255U * 32U)
        return (FT_ERR_OUT_OF_RANGE);
    if (output_length == 0U)
        return (FT_ERR_SUCCESS);
    crypto_sha256 inner_prefix;
    crypto_sha256 outer_prefix;
    result = prepare_hmac_sha256(pseudorandom_key, 32U, inner_prefix, outer_prefix);
    if (result != FT_ERR_SUCCESS)
        return (result);
    crypto_sha256 inner;
    crypto_sha256 outer;
    offset = 0U;
    counter = 1U;
    while (offset < output_length)
    {
        previous_data = ft_nullptr;
        previous_length = 0U;
        if (counter != 1U)
        {
            // HMAC consumes the previous block before writing the next digest.
            previous_data = block;
            previous_length = sizeof(block);
        }
        crypto_sha256 *inner_context = &inner_prefix;
        crypto_sha256 *outer_context = &outer_prefix;
        if (output_length - offset > sizeof(block))
        {
            inner.copy_hmac_prefix(inner_prefix);
            outer.copy_hmac_prefix(outer_prefix);
            inner_context = &inner;
            outer_context = &outer;
        }
        result = finish_hmac_sha256(*inner_context, *outer_context,
            previous_data, previous_length, info, info_length, &counter, 1U, block);
        if (result != FT_ERR_SUCCESS)
        {
            clear_sensitive(block, sizeof(block));
            clear_sensitive(output, offset);
            return (result);
        }
        copy_length = output_length - offset;
        if (copy_length > sizeof(block))
            copy_length = sizeof(block);
        ft_memcpy(output + offset, block, copy_length);
        offset += copy_length;
        counter = static_cast<uint8_t>(counter + 1U);
    }
    clear_sensitive(block, sizeof(block));
    return (FT_ERR_SUCCESS);
}

int32_t crypto_hkdf_sha256_expand_labels(const uint8_t pseudorandom_key[32],
    const crypto_hkdf_sha256_label *labels, ft_size_t label_count) noexcept
{
    if (pseudorandom_key == ft_nullptr || labels == ft_nullptr || label_count == 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    uint8_t block[32];
    ft_size_t index = 0U;
    while (index < label_count)
    {
        const crypto_hkdf_sha256_label &label = labels[index];
        if ((label.info == ft_nullptr && label.info_length != 0U)
            || label.output == ft_nullptr || label.output_length == 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        if (label.output_length > sizeof(block))
            return (FT_ERR_OUT_OF_RANGE);
        index += 1U;
    }
    crypto_sha256 inner_prefix;
    crypto_sha256 outer_prefix;
    int32_t result = prepare_hmac_sha256(pseudorandom_key, 32U, inner_prefix, outer_prefix);
    if (result != FT_ERR_SUCCESS)
        return (result);
    constexpr uint8_t FIRST_BLOCK = 1U;
    crypto_sha256 inner;
    crypto_sha256 outer;
    index = 0U;
    while (index < label_count)
    {
        const crypto_hkdf_sha256_label &label = labels[index];
        inner.copy_hmac_prefix(inner_prefix);
        outer.copy_hmac_prefix(outer_prefix);
        result = finish_hmac_sha256(inner, outer, label.info, label.info_length,
            &FIRST_BLOCK, sizeof(FIRST_BLOCK), ft_nullptr, 0U, block);
        if (result != FT_ERR_SUCCESS)
            break ;
        ft_memcpy(label.output, block, label.output_length);
        index += 1U;
    }
    clear_sensitive(block, sizeof(block));
    if (result != FT_ERR_SUCCESS)
    {
        while (index != 0U)
        {
            index -= 1U;
            clear_sensitive(labels[index].output, labels[index].output_length);
        }
    }
    return (result);
}
