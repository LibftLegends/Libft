#include "rng.hpp"
#include "../Errno/errno.hpp"

template<ft_size_t Count>
static void rng_write_uuid_group(char *output, const unsigned char *bytes)
{
    constexpr char HEX_DIGITS[] = "0123456789abcdef";
    ft_size_t index = 0U;
    while (index < Count)
    {
        const unsigned char byte_value = bytes[index];
        output[index * 2U] = HEX_DIGITS[byte_value >> 4U];
        output[index * 2U + 1U] = HEX_DIGITS[byte_value & 0x0FU];
        index += 1U;
    }
    return ;
}

void ft_generate_uuid(char output_buffer[37])
{
    if (output_buffer == ft_nullptr)
        return ;
    unsigned char uuid_bytes[16];
    if (rng_secure_bytes(uuid_bytes, sizeof(uuid_bytes)) != FT_ERR_SUCCESS)
    {
        output_buffer[0] = '\0';
        return ;
    }
    uuid_bytes[6] = static_cast<unsigned char>((uuid_bytes[6] & 0x0FU) | 0x40U);
    uuid_bytes[8] = static_cast<unsigned char>((uuid_bytes[8] & 0x3FU) | 0x80U);
    rng_write_uuid_group<4U>(output_buffer, uuid_bytes);
    rng_write_uuid_group<2U>(output_buffer + 9, uuid_bytes + 4);
    rng_write_uuid_group<2U>(output_buffer + 14, uuid_bytes + 6);
    rng_write_uuid_group<2U>(output_buffer + 19, uuid_bytes + 8);
    rng_write_uuid_group<6U>(output_buffer + 24, uuid_bytes + 10);
    output_buffer[8] = '-';
    output_buffer[13] = '-';
    output_buffer[18] = '-';
    output_buffer[23] = '-';
    output_buffer[36] = '\0';
    return ;
}
