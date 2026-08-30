#include <stdint.h>
#include "encryption.hpp"
#include "encryption_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static const uint8_t s_box[256] = {
    0x63,0x7c,0x77,0x7b,0xf2,0x6b,0x6f,0xc5,0x30,0x01,0x67,0x2b,0xfe,0xd7,0xab,0x76,
    0xca,0x82,0xc9,0x7d,0xfa,0x59,0x47,0xf0,0xad,0xd4,0xa2,0xaf,0x9c,0xa4,0x72,0xc0,
    0xb7,0xfd,0x93,0x26,0x36,0x3f,0xf7,0xcc,0x34,0xa5,0xe5,0xf1,0x71,0xd8,0x31,0x15,
    0x04,0xc7,0x23,0xc3,0x18,0x96,0x05,0x9a,0x07,0x12,0x80,0xe2,0xeb,0x27,0xb2,0x75,
    0x09,0x83,0x2c,0x1a,0x1b,0x6e,0x5a,0xa0,0x52,0x3b,0xd6,0xb3,0x29,0xe3,0x2f,0x84,
    0x53,0xd1,0x00,0xed,0x20,0xfc,0xb1,0x5b,0x6a,0xcb,0xbe,0x39,0x4a,0x4c,0x58,0xcf,
    0xd0,0xef,0xaa,0xfb,0x43,0x4d,0x33,0x85,0x45,0xf9,0x02,0x7f,0x50,0x3c,0x9f,0xa8,
    0x51,0xa3,0x40,0x8f,0x92,0x9d,0x38,0xf5,0xbc,0xb6,0xda,0x21,0x10,0xff,0xf3,0xd2,
    0xcd,0x0c,0x13,0xec,0x5f,0x97,0x44,0x17,0xc4,0xa7,0x7e,0x3d,0x64,0x5d,0x19,0x73,
    0x60,0x81,0x4f,0xdc,0x22,0x2a,0x90,0x88,0x46,0xee,0xb8,0x14,0xde,0x5e,0x0b,0xdb,
    0xe0,0x32,0x3a,0x0a,0x49,0x06,0x24,0x5c,0xc2,0xd3,0xac,0x62,0x91,0x95,0xe4,0x79,
    0xe7,0xc8,0x37,0x6d,0x8d,0xd5,0x4e,0xa9,0x6c,0x56,0xf4,0xea,0x65,0x7a,0xae,0x08,
    0xba,0x78,0x25,0x2e,0x1c,0xa6,0xb4,0xc6,0xe8,0xdd,0x74,0x1f,0x4b,0xbd,0x8b,0x8a,
    0x70,0x3e,0xb5,0x66,0x48,0x03,0xf6,0x0e,0x61,0x35,0x57,0xb9,0x86,0xc1,0x1d,0x9e,
    0xe1,0xf8,0x98,0x11,0x69,0xd9,0x8e,0x94,0x9b,0x1e,0x87,0xe9,0xce,0x55,0x28,0xdf,
    0x8c,0xa1,0x89,0x0d,0xbf,0xe6,0x42,0x68,0x41,0x99,0x2d,0x0f,0xb0,0x54,0xbb,0x16
};

static const uint8_t inv_s_box[256] = {
    0x52,0x09,0x6a,0xd5,0x30,0x36,0xa5,0x38,0xbf,0x40,0xa3,0x9e,0x81,0xf3,0xd7,0xfb,
    0x7c,0xe3,0x39,0x82,0x9b,0x2f,0xff,0x87,0x34,0x8e,0x43,0x44,0xc4,0xde,0xe9,0xcb,
    0x54,0x7b,0x94,0x32,0xa6,0xc2,0x23,0x3d,0xee,0x4c,0x95,0x0b,0x42,0xfa,0xc3,0x4e,
    0x08,0x2e,0xa1,0x66,0x28,0xd9,0x24,0xb2,0x76,0x5b,0xa2,0x49,0x6d,0x8b,0xd1,0x25,
    0x72,0xf8,0xf6,0x64,0x86,0x68,0x98,0x16,0xd4,0xa4,0x5c,0xcc,0x5d,0x65,0xb6,0x92,
    0x6c,0x70,0x48,0x50,0xfd,0xed,0xb9,0xda,0x5e,0x15,0x46,0x57,0xa7,0x8d,0x9d,0x84,
    0x90,0xd8,0xab,0x00,0x8c,0xbc,0xd3,0x0a,0xf7,0xe4,0x58,0x05,0xb8,0xb3,0x45,0x06,
    0xd0,0x2c,0x1e,0x8f,0xca,0x3f,0x0f,0x02,0xc1,0xaf,0xbd,0x03,0x01,0x13,0x8a,0x6b,
    0x3a,0x91,0x11,0x41,0x4f,0x67,0xdc,0xea,0x97,0xf2,0xcf,0xce,0xf0,0xb4,0xe6,0x73,
    0x96,0xac,0x74,0x22,0xe7,0xad,0x35,0x85,0xe2,0xf9,0x37,0xe8,0x1c,0x75,0xdf,0x6e,
    0x47,0xf1,0x1a,0x71,0x1d,0x29,0xc5,0x89,0x6f,0xb7,0x62,0x0e,0xaa,0x18,0xbe,0x1b,
    0xfc,0x56,0x3e,0x4b,0xc6,0xd2,0x79,0x20,0x9a,0xdb,0xc0,0xfe,0x78,0xcd,0x5a,0xf4,
    0x1f,0xdd,0xa8,0x33,0x88,0x07,0xc7,0x31,0xb1,0x12,0x10,0x59,0x27,0x80,0xec,0x5f,
    0x60,0x51,0x7f,0xa9,0x19,0xb5,0x4a,0x0d,0x2d,0xe5,0x7a,0x9f,0x93,0xc9,0x9c,0xef,
    0xa0,0xe0,0x3b,0x4d,0xae,0x2a,0xf5,0xb0,0xc8,0xeb,0xbb,0x3c,0x83,0x53,0x99,0x61,
    0x17,0x2b,0x04,0x7e,0xba,0x77,0xd6,0x26,0xe1,0x69,0x14,0x63,0x55,0x21,0x0c,0x7d
};

static const uint8_t rcon[11] = {
    0x00,0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80,0x1b,0x36
};

static void sub_bytes(uint8_t *state)
{
    ft_size_t index = 0;
    while (index < 16)
    {
        state[index] = s_box[state[index]];
        ++index;
    }
    return ;
}

static void inv_sub_bytes(uint8_t *state)
{
    ft_size_t index = 0;
    while (index < 16)
    {
        state[index] = inv_s_box[state[index]];
        ++index;
    }
    return ;
}

static void shift_rows(uint8_t *state)
{
    uint8_t temporary_value;
    uint8_t secondary_temporary_value;

    temporary_value = state[1];
    state[1] = state[5];
    state[5] = state[9];
    state[9] = state[13];
    state[13] = temporary_value;

    temporary_value = state[2];
    secondary_temporary_value = state[6];
    state[2] = state[10];
    state[6] = state[14];
    state[10] = temporary_value;
    state[14] = secondary_temporary_value;

    temporary_value = state[3];
    state[3] = state[15];
    state[15] = state[11];
    state[11] = state[7];
    state[7] = temporary_value;
    return ;
}

static void inv_shift_rows(uint8_t *state)
{
    uint8_t temporary_value;
    uint8_t secondary_temporary_value;

    temporary_value = state[13];
    state[13] = state[9];
    state[9] = state[5];
    state[5] = state[1];
    state[1] = temporary_value;

    temporary_value = state[2];
    secondary_temporary_value = state[6];
    state[2] = state[10];
    state[6] = state[14];
    state[10] = temporary_value;
    state[14] = secondary_temporary_value;

    temporary_value = state[3];
    state[3] = state[7];
    state[7] = state[11];
    state[11] = state[15];
    state[15] = temporary_value;
    return ;
}

static uint8_t xtime(uint8_t value)
{
    uint8_t high_bit = value & 0x80;
    value = static_cast<uint8_t>(static_cast<uint32_t>(value) << 1U);
    if (high_bit)
        value ^= 0x1b;
    return (value);
}

static uint8_t multiply(uint8_t value, uint8_t multiplier)
{
    uint8_t result = 0;
    uint8_t bit_index = 0;
    while (bit_index < 8)
    {
        if (multiplier & 1)
            result ^= value;
        uint8_t high_bit = value & 0x80;
        value = static_cast<uint8_t>(static_cast<uint32_t>(value) << 1U);
        if (high_bit)
            value ^= 0x1b;
        multiplier >>= 1;
        ++bit_index;
    }
    return (result);
}

static void mix_columns(uint8_t *state)
{
    uint8_t column = 0;
    while (column < 4)
    {
        uint8_t index = column * 4;
        uint8_t state_value_zero = state[index];
        uint8_t state_value_one = state[index + 1];
        uint8_t state_value_two = state[index + 2];
        uint8_t state_value_three = state[index + 3];
        uint8_t result_value_zero;
        uint8_t result_value_one;
        uint8_t result_value_two;
        uint8_t result_value_three;

        result_value_zero = xtime(state_value_zero)
            ^ (xtime(state_value_one) ^ state_value_one)
            ^ state_value_two ^ state_value_three;
        result_value_one = state_value_zero ^ xtime(state_value_one)
            ^ (xtime(state_value_two) ^ state_value_two)
            ^ state_value_three;
        result_value_two = state_value_zero ^ state_value_one
            ^ xtime(state_value_two)
            ^ (xtime(state_value_three) ^ state_value_three);
        result_value_three = (xtime(state_value_zero) ^ state_value_zero)
            ^ state_value_one ^ state_value_two
            ^ xtime(state_value_three);

        state[index] = result_value_zero;
        state[index + 1] = result_value_one;
        state[index + 2] = result_value_two;
        state[index + 3] = result_value_three;

        ++column;
    }
    return ;
}

static void inv_mix_columns(uint8_t *state)
{
    uint8_t column = 0;
    while (column < 4)
    {
        uint8_t index = column * 4;
        uint8_t state_value_zero = state[index];
        uint8_t state_value_one = state[index + 1];
        uint8_t state_value_two = state[index + 2];
        uint8_t state_value_three = state[index + 3];
        uint8_t result_value_zero;
        uint8_t result_value_one;
        uint8_t result_value_two;
        uint8_t result_value_three;

        result_value_zero = multiply(state_value_zero, 0x0e)
            ^ multiply(state_value_one, 0x0b)
            ^ multiply(state_value_two, 0x0d)
            ^ multiply(state_value_three, 0x09);
        result_value_one = multiply(state_value_zero, 0x09)
            ^ multiply(state_value_one, 0x0e)
            ^ multiply(state_value_two, 0x0b)
            ^ multiply(state_value_three, 0x0d);
        result_value_two = multiply(state_value_zero, 0x0d)
            ^ multiply(state_value_one, 0x09)
            ^ multiply(state_value_two, 0x0e)
            ^ multiply(state_value_three, 0x0b);
        result_value_three = multiply(state_value_zero, 0x0b)
            ^ multiply(state_value_one, 0x0d)
            ^ multiply(state_value_two, 0x09)
            ^ multiply(state_value_three, 0x0e);

        state[index] = result_value_zero;
        state[index + 1] = result_value_one;
        state[index + 2] = result_value_two;
        state[index + 3] = result_value_three;

        ++column;
    }
    return ;
}

static void add_round_key(uint8_t *state, const uint8_t *round_key)
{
    ft_size_t index = 0;
    while (index < 16)
    {
        state[index] ^= round_key[index];
        ++index;
    }
    return ;
}

static void key_expansion(const uint8_t *key, uint8_t *round_keys)
{
    ft_size_t index = 0;
    while (index < 16)
    {
        round_keys[index] = key[index];
        ++index;
    }
    ft_size_t bytes_generated = 16;
    ft_size_t rcon_iteration = 1;
    uint8_t temporary_word[4];
    while (bytes_generated < 176)
    {
        ft_size_t temporary_word_index = 0;
        while (temporary_word_index < 4)
        {
            temporary_word[temporary_word_index] = round_keys[bytes_generated
                - 4 + temporary_word_index];
            ++temporary_word_index;
        }
        if (bytes_generated % 16 == 0)
        {
            uint8_t temporary_byte = temporary_word[0];
            temporary_word[0] = temporary_word[1];
            temporary_word[1] = temporary_word[2];
            temporary_word[2] = temporary_word[3];
            temporary_word[3] = temporary_byte;

            ft_size_t substitution_index = 0;
            while (substitution_index < 4)
            {
                temporary_word[substitution_index]
                    = s_box[temporary_word[substitution_index]];
                ++substitution_index;
            }
            temporary_word[0] ^= rcon[rcon_iteration];
            ++rcon_iteration;
        }
        temporary_word_index = 0;
        while (temporary_word_index < 4)
        {
            round_keys[bytes_generated] = round_keys[bytes_generated - 16]
                ^ temporary_word[temporary_word_index];
            ++bytes_generated;
            ++temporary_word_index;
        }
    }
    return ;
}

void aes_encrypt(uint8_t *block, const uint8_t *key)
{
    if (encryption_try_block_encrypt(block, key))
        return ;
    aes_encrypt_software(block, key);
    return ;
}

void aes_decrypt(uint8_t *block, const uint8_t *key)
{
    if (encryption_try_block_decrypt(block, key))
        return ;
    aes_decrypt_software(block, key);
    return ;
}

void aes_encrypt_software(uint8_t *block, const uint8_t *key)
{
    uint8_t round_keys[176];
    key_expansion(key, round_keys);
    add_round_key(block, round_keys);
    uint8_t round = 1;
    while (round < 10)
    {
        sub_bytes(block);
        shift_rows(block);
        mix_columns(block);
        add_round_key(block, round_keys + round * 16);
        ++round;
    }
    sub_bytes(block);
    shift_rows(block);
    add_round_key(block, round_keys + 160);
    return ;
}

void aes_decrypt_software(uint8_t *block, const uint8_t *key)
{
    uint8_t round_keys[176];
    int32_t round;

    key_expansion(key, round_keys);
    round = 10;
    add_round_key(block, round_keys + round * 16);
    while (round > 0)
    {
        inv_shift_rows(block);
        inv_sub_bytes(block);
        round -= 1;
        add_round_key(block, round_keys + round * 16);
        if (round > 0)
            inv_mix_columns(block);
    }
    return ;
}
