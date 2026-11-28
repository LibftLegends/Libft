#include "encryption.hpp"
#include "encryption_digest_internal.hpp"
#include "../Basic/class_nullptr.hpp"

static constexpr uint32_t left_rotate(uint32_t value, uint32_t bits)
{
    return ((value << bits) | (value >> (32 - bits)));
}

static void sha1_process_block(uint32_t *hash_values, const uint8_t *block) noexcept
{
    uint32_t words[80];
    ft_size_t word_index;

    word_index = 0;
    while (word_index < 16)
    {
        ft_size_t byte_index;

        byte_index = word_index * 4;
        words[word_index] = static_cast<uint32_t>(block[byte_index]) << 24;
        words[word_index] |= static_cast<uint32_t>(block[byte_index + 1]) << 16;
        words[word_index] |= static_cast<uint32_t>(block[byte_index + 2]) << 8;
        words[word_index] |= static_cast<uint32_t>(block[byte_index + 3]);
        word_index++;
    }
    while (word_index < 80)
    {
        uint32_t value;

        value = words[word_index - 3] ^ words[word_index - 8] ^ words[word_index - 14] ^ words[word_index - 16];
        words[word_index] = left_rotate(value, 1);
        word_index++;
    }
    uint32_t hash_working_value_a;
    uint32_t hash_working_value_b;
    uint32_t hash_working_value_c;
    uint32_t hash_working_value_d;
    uint32_t hash_working_value_e;

    hash_working_value_a = hash_values[0];
    hash_working_value_b = hash_values[1];
    hash_working_value_c = hash_values[2];
    hash_working_value_d = hash_values[3];
    hash_working_value_e = hash_values[4];
    word_index = 0;
    while (word_index < 80)
    {
        uint32_t function_value;
        uint32_t constant_value;
        uint32_t temp_value;

        if (word_index < 20)
        {
            function_value = (hash_working_value_b & hash_working_value_c)
                | ((~hash_working_value_b) & hash_working_value_d);
            constant_value = 0x5A827999;
        }
        else if (word_index < 40)
        {
            function_value = hash_working_value_b
                ^ hash_working_value_c ^ hash_working_value_d;
            constant_value = 0x6ED9EBA1;
        }
        else if (word_index < 60)
        {
            function_value = (hash_working_value_b
                    & hash_working_value_c)
                | (hash_working_value_b & hash_working_value_d)
                | (hash_working_value_c & hash_working_value_d);
            constant_value = 0x8F1BBCDC;
        }
        else
        {
            function_value = hash_working_value_b
                ^ hash_working_value_c ^ hash_working_value_d;
            constant_value = 0xCA62C1D6;
        }
        temp_value = left_rotate(hash_working_value_a, 5)
            + function_value + hash_working_value_e + constant_value
            + words[word_index];
        hash_working_value_e = hash_working_value_d;
        hash_working_value_d = hash_working_value_c;
        hash_working_value_c = left_rotate(hash_working_value_b, 30);
        hash_working_value_b = hash_working_value_a;
        hash_working_value_a = temp_value;
        word_index++;
    }
    hash_values[0] += hash_working_value_a;
    hash_values[1] += hash_working_value_b;
    hash_values[2] += hash_working_value_c;
    hash_values[3] += hash_working_value_d;
    hash_values[4] += hash_working_value_e;
    return ;
}

void sha1_digest_initialize(sha1_digest_state &state) noexcept
{
    constexpr uint32_t INITIAL_HASH[] = {
        0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0
    };
    ft_memcpy(state.hash, INITIAL_HASH, sizeof(INITIAL_HASH));
    state.pending_length = 0;
    state.message_length = 0;
    return ;
}

void sha1_digest_update(sha1_digest_state &state, const uint8_t *data, ft_size_t length) noexcept
{
    encryption_digest_update<sizeof(state.hash) / sizeof(state.hash[0]), sha1_process_block>(state, data, length);
    return ;
}

void sha1_digest_final(sha1_digest_state &state, uint8_t *digest) noexcept
{
    encryption_digest_final<sizeof(state.hash) / sizeof(state.hash[0]), sha1_process_block>(state, digest);
    return ;
}

void sha1_hash(const void *data, ft_size_t length, uint8_t *digest)
{
    if (digest == ft_nullptr)
        return ;
    if (data == ft_nullptr && length != 0)
    {
        ft_memset(digest, 0, sizeof(sha1_digest_state::hash));
        return ;
    }
    sha1_digest_state state;
    sha1_digest_initialize(state);
    sha1_digest_update(state, static_cast<const uint8_t *>(data), length);
    sha1_digest_final(state, digest);
    return ;
}
