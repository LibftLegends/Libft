#include "compression.hpp"
#include "../Basic/limits.hpp"

static constexpr char BASE64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr uint8_t BASE64_PADDING = 64U;
static constexpr uint8_t BASE64_WHITESPACE = 65U;
static constexpr uint8_t BASE64_INVALID = 255U;

struct compression_base64_decode_table
{
    uint8_t values[256];
};

static constexpr compression_base64_decode_table compression_make_base64_decode_table()
{
    compression_base64_decode_table table{};
    ft_size_t index = 0U;
    while (index < sizeof(table.values))
        table.values[index++] = BASE64_INVALID;
    index = 0U;
    while (index < 64U)
    {
        table.values[static_cast<uint8_t>(BASE64_ALPHABET[index])] =
            static_cast<uint8_t>(index);
        ++index;
    }
    table.values['='] = BASE64_PADDING;
    table.values[' '] = BASE64_WHITESPACE;
    table.values['\n'] = BASE64_WHITESPACE;
    table.values['\r'] = BASE64_WHITESPACE;
    table.values['\t'] = BASE64_WHITESPACE;
    table.values['\f'] = BASE64_WHITESPACE;
    table.values['\v'] = BASE64_WHITESPACE;
    return (table);
}

static constexpr compression_base64_decode_table BASE64_VALUES =
    compression_make_base64_decode_table();

uint8_t *ft_base64_encode(const uint8_t *input_buffer, std::size_t input_size,
    std::size_t *encoded_size)
{
    if (encoded_size == ft_nullptr)
        return (ft_nullptr);
    *encoded_size = 0U;
    if (input_buffer == ft_nullptr || input_size > (FT_SYSTEM_SIZE_MAX / 4U) * 3U)
        return (ft_nullptr);
    const ft_size_t output_length = ((input_size + 2U) / 3U) * 4U;
    char *output = static_cast<char *>(cma_malloc(output_length + 1U));
    if (output == ft_nullptr)
        return (ft_nullptr);
    ft_size_t input_index = 0U;
    ft_size_t output_index = 0U;
    while (input_size - input_index >= 3U)
    {
        const uint32_t first = input_buffer[input_index];
        const uint32_t second = input_buffer[input_index + 1U];
        const uint32_t third = input_buffer[input_index + 2U];
        output[output_index] = BASE64_ALPHABET[first >> 2U];
        output[output_index + 1U] = BASE64_ALPHABET[((first & 3U) << 4U) | (second >> 4U)];
        output[output_index + 2U] = BASE64_ALPHABET[((second & 15U) << 2U) | (third >> 6U)];
        output[output_index + 3U] = BASE64_ALPHABET[third & 63U];
        input_index += 3U;
        output_index += 4U;
    }
    if (input_index < input_size)
    {
        const uint32_t first = input_buffer[input_index++];
        uint32_t second = 0U;
        if (input_index < input_size)
            second = input_buffer[input_index];
        output[output_index++] = BASE64_ALPHABET[first >> 2U];
        output[output_index++] = BASE64_ALPHABET[((first & 3U) << 4U) | (second >> 4U)];
        output[output_index] = '=';
        if (input_index < input_size)
            output[output_index] = BASE64_ALPHABET[(second & 15U) << 2U];
        output[output_index + 1U] = '=';
    }
    output[output_length] = '\0';
    *encoded_size = output_length;
    return (reinterpret_cast<uint8_t *>(output));
}

uint8_t *ft_base64_decode(const uint8_t *input_buffer, std::size_t input_size,
    std::size_t *decoded_size)
{
    if (input_buffer == ft_nullptr || decoded_size == ft_nullptr)
        return (ft_nullptr);
    *decoded_size = 0U;
    ft_size_t remaining_symbols = 0U;
    ft_size_t input_index = 0U;
    while (input_index < input_size)
    {
        remaining_symbols += (BASE64_VALUES.values[input_buffer[input_index]]
            != BASE64_WHITESPACE);
        ++input_index;
    }
    if (remaining_symbols == 0U)
    {
        uint8_t *output = static_cast<uint8_t *>(cma_malloc(1U));
        if (output != ft_nullptr)
            output[0] = 0U;
        return (output);
    }
    if (remaining_symbols % 4U == 1U)
        return (ft_nullptr);
    ft_size_t output_capacity = (remaining_symbols / 4U) * 3U;
    if (remaining_symbols % 4U != 0U)
        output_capacity += 3U;
    uint8_t *output = static_cast<uint8_t *>(cma_malloc(output_capacity));
    if (output == ft_nullptr)
        return (ft_nullptr);
    input_index = 0U;
    ft_size_t output_index = 0U;
    while (remaining_symbols != 0U)
    {
        if (input_size - input_index >= 4U)
        {
            const uint32_t first = BASE64_VALUES.values[input_buffer[input_index]];
            const uint32_t second = BASE64_VALUES.values[input_buffer[input_index + 1U]];
            const uint32_t third = BASE64_VALUES.values[input_buffer[input_index + 2U]];
            const uint32_t fourth = BASE64_VALUES.values[input_buffer[input_index + 3U]];
            if ((first | second | third | fourth) < 64U)
            {
                output[output_index] = static_cast<uint8_t>((first << 2U) | (second >> 4U));
                output[output_index + 1U] = static_cast<uint8_t>((second << 4U) | (third >> 2U));
                output[output_index + 2U] = static_cast<uint8_t>((third << 6U) | fourth);
                input_index += 4U;
                output_index += 3U;
                remaining_symbols -= 4U;
                continue ;
            }
        }
        uint32_t values[4] = {BASE64_PADDING, BASE64_PADDING, BASE64_PADDING, BASE64_PADDING};
        ft_size_t count = 0U;
        while (count < 4U && input_index < input_size)
        {
            const uint8_t value = BASE64_VALUES.values[input_buffer[input_index++]];
            if (value != BASE64_WHITESPACE)
                values[count++] = value;
        }
        remaining_symbols -= count;
        if (values[0] >= BASE64_PADDING || values[1] >= BASE64_PADDING
            || values[2] > BASE64_PADDING || values[3] > BASE64_PADDING
            || (values[2] == BASE64_PADDING && values[3] != BASE64_PADDING)
            || ((values[2] == BASE64_PADDING || values[3] == BASE64_PADDING)
                && remaining_symbols != 0U))
        {
            cma_free(output);
            return (ft_nullptr);
        }
        output[output_index++] = static_cast<uint8_t>((values[0] << 2U) | (values[1] >> 4U));
        if (values[2] != BASE64_PADDING)
            output[output_index++] = static_cast<uint8_t>((values[1] << 4U) | (values[2] >> 2U));
        if (values[3] != BASE64_PADDING)
            output[output_index++] = static_cast<uint8_t>((values[2] << 6U) | values[3]);
    }
    *decoded_size = output_index;
    return (output);
}
