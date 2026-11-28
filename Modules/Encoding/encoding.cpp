#include "encoding_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/utf8.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

static thread_local int32_t g_encoding_error = FT_ERR_SUCCESS;

static int32_t encoding_set_error(int32_t error_code) noexcept
{
    g_encoding_error = error_code;
    return (error_code);
}

int32_t encoding_get_error(void) noexcept
{
    return (g_encoding_error);
}

const char *encoding_get_error_str(void) noexcept
{
    return (ft_strerror(g_encoding_error));
}

static constexpr int32_t encoding_base64_value(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (character - 'A');
    if (character >= 'a' && character <= 'z')
        return (character - 'a' + 26);
    if (character >= '0' && character <= '9')
        return (character - '0' + 52);
    if (character == '+' || character == '-')
        return (62);
    if (character == '/' || character == '_')
        return (63);
    return (-1);
}

static constexpr int32_t encoding_is_unreserved_percent_byte(char value) noexcept
{
    if (value >= 'A' && value <= 'Z')
        return (FT_TRUE);
    if (value >= 'a' && value <= 'z')
        return (FT_TRUE);
    if (value >= '0' && value <= '9')
        return (FT_TRUE);
    if (value == '-' || value == '_' || value == '.' || value == '~')
        return (FT_TRUE);
    return (FT_FALSE);
}

static constexpr int32_t encoding_hex_value(char character) noexcept
{
    if (character >= '0' && character <= '9')
        return (character - '0');
    if (character >= 'a' && character <= 'f')
        return (character - 'a' + 10);
    if (character >= 'A' && character <= 'F')
        return (character - 'A' + 10);
    return (-1);
}

static constexpr int32_t encoding_base32_value(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (character - 'A');
    if (character >= 'a' && character <= 'z')
        return (character - 'a');
    if (character >= '2' && character <= '7')
        return (character - '2' + 26);
    return (-1);
}

struct encoding_byte_table
{
    int8_t values[256];
};

template <int32_t (*Classify)(char) noexcept>
static constexpr encoding_byte_table encoding_make_byte_table() noexcept
{
    encoding_byte_table table{};
    uint32_t index = 0U;
    while (index < sizeof(table.values))
    {
        if (index < 128U)
            table.values[index] = static_cast<int8_t>(Classify(
                static_cast<char>(index)));
        else
            table.values[index] = -1;
        ++index;
    }
    return (table);
}

static constexpr encoding_byte_table BASE64_VALUES =
    encoding_make_byte_table<encoding_base64_value>();
static constexpr encoding_byte_table BASE32_VALUES =
    encoding_make_byte_table<encoding_base32_value>();
static constexpr encoding_byte_table HEX_VALUES =
    encoding_make_byte_table<encoding_hex_value>();
static constexpr encoding_byte_table PERCENT_UNRESERVED =
    encoding_make_byte_table<encoding_is_unreserved_percent_byte>();
static constexpr char BASE64_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static constexpr char BASE64_URL_ALPHABET[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
static constexpr char BASE32_ALPHABET[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static int32_t encoding_base64_encoded_size(const uint8_t *input,
    ft_size_t input_size, ft_bool url_safe, ft_bool padded, ft_size_t *size) noexcept
{
    if (input == ft_nullptr && input_size != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (input_size > (FT_SYSTEM_SIZE_MAX / 4) * 3)
        return (FT_ERR_OUT_OF_RANGE);
    *size = ((input_size + 2) / 3) * 4;
    if (url_safe == FT_TRUE && padded == FT_FALSE)
    {
        if (input_size % 3 == 1)
            *size -= 2;
        else if (input_size % 3 == 2)
            *size -= 1;
    }
    return (FT_ERR_SUCCESS);
}

static void encoding_base64_write(const uint8_t *input, ft_size_t input_size,
    ft_bool url_safe, ft_bool padded, char *output) noexcept
{
    ft_size_t input_index = 0;
    ft_size_t output_index = 0;
    const char *alphabet = BASE64_ALPHABET;
    if (url_safe == FT_TRUE)
        alphabet = BASE64_URL_ALPHABET;
    while (input_size - input_index >= 3U)
    {
        const uint32_t byte_one = input[input_index];
        const uint32_t byte_two = input[input_index + 1U];
        const uint32_t byte_three = input[input_index + 2U];
        output[output_index] = alphabet[byte_one >> 2U];
        output[output_index + 1U] = alphabet[((byte_one & 3U) << 4U) | (byte_two >> 4U)];
        output[output_index + 2U] = alphabet[((byte_two & 15U) << 2U) | (byte_three >> 6U)];
        output[output_index + 3U] = alphabet[byte_three & 63U];
        input_index += 3U;
        output_index += 4U;
    }
    if (input_index < input_size)
    {
        const uint32_t byte_one = input[input_index++];
        uint32_t byte_two = 0U;
        if (input_index < input_size)
            byte_two = input[input_index];
        output[output_index++] = alphabet[byte_one >> 2U];
        output[output_index++] = alphabet[((byte_one & 3U) << 4U) | (byte_two >> 4U)];
        if (input_index < input_size)
            output[output_index++] = alphabet[(byte_two & 15U) << 2U];
        else if (padded == FT_TRUE)
            output[output_index++] = '=';
        if (padded == FT_TRUE)
            output[output_index++] = '=';
    }
    output[output_index] = '\0';
    return ;
}

static char *encoding_base64_encode_internal(const uint8_t *input,
    ft_size_t input_size, ft_bool url_safe, ft_bool padded)
{
    ft_size_t output_size;
    const int32_t status = encoding_base64_encoded_size(input, input_size,
        url_safe, padded, &output_size);
    if (status != FT_ERR_SUCCESS)
    {
        encoding_set_error(status);
        return (ft_nullptr);
    }
    char *output = static_cast<char *>(cma_malloc(output_size + 1U));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    encoding_base64_write(input, input_size, url_safe, padded, output);
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

char *encoding_base64_encode(const uint8_t *input, ft_size_t input_size)
{
    return (encoding_base64_encode_internal(input, input_size, FT_FALSE, FT_TRUE));
}

char *encoding_base64url_encode(const uint8_t *input, ft_size_t input_size,
    ft_bool padded)
{
    return (encoding_base64_encode_internal(input, input_size, FT_TRUE, padded));
}

int32_t encoding_base64url_encode_into(const uint8_t *input, ft_size_t input_size,
    ft_bool padded, char *output, ft_size_t output_capacity) noexcept
{
    if (output == ft_nullptr)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    ft_size_t output_size;
    const int32_t status = encoding_base64_encoded_size(input, input_size,
        FT_TRUE, padded, &output_size);
    if (status != FT_ERR_SUCCESS)
        return (encoding_set_error(status));
    if (output_size >= output_capacity)
        return (encoding_set_error(FT_ERR_OUT_OF_RANGE));
    encoding_base64_write(input, input_size, FT_TRUE, padded, output);
    return (encoding_set_error(FT_ERR_SUCCESS));
}

static int32_t encoding_base64_validate_input(const char *input,
    ft_size_t input_size, uint32_t *padding_count) noexcept
{
    if ((input == ft_nullptr && input_size != 0U) || input_size % 4U == 1U)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_size_t input_index = 0U;
    uint32_t padding = 0U;
    while (input_index < input_size)
    {
        const uint8_t character = static_cast<uint8_t>(input[input_index++]);
        if (character == '=')
            ++padding;
        if ((character != '=' && BASE64_VALUES.values[character] < 0)
            || padding > 2U)
            return (FT_ERR_INVALID_ARGUMENT);
    }
    if (padding_count != ft_nullptr)
        *padding_count = padding;
    return (FT_ERR_SUCCESS);
}

static int32_t encoding_base64_decode_bytes(const char *input, ft_size_t input_size,
    ft_bool url_safe, uint8_t *output, ft_size_t *output_size) noexcept
{
    ft_size_t input_index = 0U;
    ft_size_t output_index = 0U;
    while (input_index < input_size)
    {
        const char first = input[input_index];
        const char second = input[input_index + 1U];
        const ft_size_t remaining = input_size - input_index;
        char third = '=';
        char fourth = '=';
        if (remaining >= 3U)
            third = input[input_index + 2U];
        if (remaining >= 4U)
            fourth = input[input_index + 3U];
        const int32_t first_value = BASE64_VALUES.values[static_cast<uint8_t>(first)];
        const int32_t second_value = BASE64_VALUES.values[static_cast<uint8_t>(second)];
        int32_t third_value = 0;
        int32_t fourth_value = 0;
        if (third != '=')
            third_value = BASE64_VALUES.values[static_cast<uint8_t>(third)];
        if (fourth != '=')
            fourth_value = BASE64_VALUES.values[static_cast<uint8_t>(fourth)];
        if (first_value < 0 || second_value < 0
            || (url_safe == FT_TRUE && (first == '+' || first == '/'
                || second == '+' || second == '/' || third == '+' || third == '/'
                || fourth == '+' || fourth == '/')))
            return (FT_ERR_INVALID_ARGUMENT);
        output[output_index++] = static_cast<uint8_t>((first_value << 2) | (second_value >> 4));
        if (third != '=')
            output[output_index++] = static_cast<uint8_t>((second_value << 4) | (third_value >> 2));
        if (fourth != '=')
            output[output_index++] = static_cast<uint8_t>((third_value << 6) | fourth_value);
        if (remaining <= 4U)
            break ;
        input_index += 4U;
    }
    if (output_size != ft_nullptr)
        *output_size = output_index;
    return (FT_ERR_SUCCESS);
}

static uint8_t *encoding_base64_decode_internal(const char *input,
    ft_size_t input_size, ft_size_t *output_size, ft_bool url_safe)
{
    if (output_size != ft_nullptr)
        *output_size = 0U;
    int32_t status = encoding_base64_validate_input(input, input_size, ft_nullptr);
    if (status != FT_ERR_SUCCESS)
    {
        encoding_set_error(status);
        return (ft_nullptr);
    }
    ft_size_t output_capacity = (input_size / 4U) * 3U;
    if (input_size % 4U != 0U)
        output_capacity += 3U;
    uint8_t *output = static_cast<uint8_t *>(cma_malloc(output_capacity + 1U));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    status = encoding_base64_decode_bytes(input, input_size, url_safe, output, output_size);
    if (status != FT_ERR_SUCCESS)
    {
        cma_free(output);
        encoding_set_error(status);
        return (ft_nullptr);
    }
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

uint8_t *encoding_base64_decode(const char *input, ft_size_t input_size,
    ft_size_t *output_size)
{
    return (encoding_base64_decode_internal(input, input_size, output_size, FT_FALSE));
}

uint8_t *encoding_base64url_decode(const char *input, ft_size_t input_size,
    ft_size_t *output_size)
{
    return (encoding_base64_decode_internal(input, input_size, output_size, FT_TRUE));
}

int32_t encoding_base64url_decode_into(const char *input, ft_size_t input_size,
    uint8_t *output, ft_size_t output_capacity, ft_size_t *output_size) noexcept
{
    if (output_size == ft_nullptr)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    *output_size = 0U;
    if (output == ft_nullptr)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    uint32_t padding_count;
    const int32_t status = encoding_base64_validate_input(input, input_size, &padding_count);
    if (status != FT_ERR_SUCCESS)
        return (encoding_set_error(status));
    ft_size_t decoded_size = (input_size / 4U) * 3U;
    if (input_size % 4U != 0U)
        decoded_size += (input_size % 4U) - 1U;
    if (decoded_size < padding_count)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    if (decoded_size - padding_count > output_capacity)
        return (encoding_set_error(FT_ERR_OUT_OF_RANGE));
    return (encoding_set_error(encoding_base64_decode_bytes(input, input_size,
        FT_TRUE, output, output_size)));
}

static void encoding_base32_encode_block(const uint8_t *input, char *output)
{
    const uint32_t byte_one = input[0];
    const uint32_t byte_two = input[1];
    const uint32_t byte_three = input[2];
    const uint32_t byte_four = input[3];
    const uint32_t byte_five = input[4];
    output[0] = BASE32_ALPHABET[byte_one >> 3U];
    output[1] = BASE32_ALPHABET[((byte_one & 7U) << 2U) | (byte_two >> 6U)];
    output[2] = BASE32_ALPHABET[(byte_two >> 1U) & 31U];
    output[3] = BASE32_ALPHABET[((byte_two & 1U) << 4U) | (byte_three >> 4U)];
    output[4] = BASE32_ALPHABET[((byte_three & 15U) << 1U) | (byte_four >> 7U)];
    output[5] = BASE32_ALPHABET[(byte_four >> 2U) & 31U];
    output[6] = BASE32_ALPHABET[((byte_four & 3U) << 3U) | (byte_five >> 5U)];
    output[7] = BASE32_ALPHABET[byte_five & 31U];
    return ;
}

char *encoding_base32_encode(const uint8_t *input, ft_size_t input_size)
{
    if (input == ft_nullptr && input_size != 0U)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (input_size > (FT_SYSTEM_SIZE_MAX / 8U) * 5U)
    {
        encoding_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    const ft_size_t output_size = ((input_size + 4U) / 5U) * 8U;
    char *output = static_cast<char *>(cma_malloc(output_size + 1U));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_size_t input_index = 0U;
    ft_size_t output_index = 0U;
    while (input_size - input_index >= 5U)
    {
        encoding_base32_encode_block(input + input_index, output + output_index);
        input_index += 5U;
        output_index += 8U;
    }
    const ft_size_t remaining_size = input_size - input_index;
    if (remaining_size != 0U)
    {
        uint8_t last_block[5]{};
        ft_memcpy(last_block, input + input_index, remaining_size);
        encoding_base32_encode_block(last_block, output + output_index);
        output_index += (remaining_size * 8U + 4U) / 5U;
        while (output_index < output_size)
            output[output_index++] = '=';
    }
    output[output_size] = '\0';
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

static void encoding_base32_write_full_block(const uint8_t values[8],
    uint8_t *output)
{
    output[0] = static_cast<uint8_t>((values[0] << 3) | (values[1] >> 2));
    output[1] = static_cast<uint8_t>((values[1] << 6)
        | (values[2] << 1) | (values[3] >> 4));
    output[2] = static_cast<uint8_t>((values[3] << 4) | (values[4] >> 1));
    output[3] = static_cast<uint8_t>((values[4] << 7)
        | (values[5] << 2) | (values[6] >> 3));
    output[4] = static_cast<uint8_t>((values[6] << 5) | values[7]);
    return ;
}

static int32_t encoding_base32_decode_block(const uint8_t values[8],
    ft_size_t data_length, uint8_t *output, ft_size_t *output_size)
{
    if (data_length == 2)
    {
        if ((values[1] & 0x03U) != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        output[0] = static_cast<uint8_t>((values[0] << 3)
            | (values[1] >> 2));
        *output_size = 1;
        return (FT_ERR_SUCCESS);
    }
    if (data_length == 4)
    {
        if ((values[3] & 0x0FU) != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        output[0] = static_cast<uint8_t>((values[0] << 3)
            | (values[1] >> 2));
        output[1] = static_cast<uint8_t>(((values[1] & 0x03U) << 6)
            | (values[2] << 1) | (values[3] >> 4));
        *output_size = 2;
        return (FT_ERR_SUCCESS);
    }
    if (data_length == 5)
    {
        if ((values[4] & 0x01U) != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        output[0] = static_cast<uint8_t>((values[0] << 3)
            | (values[1] >> 2));
        output[1] = static_cast<uint8_t>(((values[1] & 0x03U) << 6)
            | (values[2] << 1) | (values[3] >> 4));
        output[2] = static_cast<uint8_t>(((values[3] & 0x0FU) << 4)
            | (values[4] >> 1));
        *output_size = 3;
        return (FT_ERR_SUCCESS);
    }
    if (data_length == 7)
    {
        if ((values[6] & 0x07U) != 0U)
            return (FT_ERR_INVALID_ARGUMENT);
        output[0] = static_cast<uint8_t>((values[0] << 3)
            | (values[1] >> 2));
        output[1] = static_cast<uint8_t>(((values[1] & 0x03U) << 6)
            | (values[2] << 1) | (values[3] >> 4));
        output[2] = static_cast<uint8_t>(((values[3] & 0x0FU) << 4)
            | (values[4] >> 1));
        output[3] = static_cast<uint8_t>(((values[4] & 0x01U) << 7)
            | (values[5] << 2) | (values[6] >> 3));
        *output_size = 4;
        return (FT_ERR_SUCCESS);
    }
    if (data_length == 8)
    {
        encoding_base32_write_full_block(values, output);
        *output_size = 5;
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_ARGUMENT);
}

static uint8_t *encoding_base32_decode_internal(const char *input,
    ft_size_t input_size, ft_size_t *output_size)
{
    uint8_t *output;
    ft_size_t output_capacity;
    ft_size_t input_index;
    ft_size_t output_index;
    ft_size_t block_index;
    ft_size_t output_block_size;
    uint8_t block_values[8];
    ft_bool padding_started;
    ft_size_t padding_count;
    int32_t value;
    int32_t block_error;

    if (output_size != ft_nullptr)
        *output_size = 0;
    if (input == ft_nullptr && input_size != 0)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    output_capacity = (input_size / 8U) * 5U;
    if (input_size % 8U != 0U)
        output_capacity += 5U;
    output = static_cast<uint8_t *>(cma_malloc(output_capacity + 1));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    input_index = 0;
    output_index = 0;
    block_index = 0;
    padding_started = FT_FALSE;
    padding_count = 0;
    while (input_size - input_index >= 8U)
    {
        const auto *block = reinterpret_cast<const uint8_t *>(input + input_index);
        const uint8_t values[8] = {
            static_cast<uint8_t>(BASE32_VALUES.values[block[0]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[1]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[2]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[3]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[4]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[5]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[6]]),
            static_cast<uint8_t>(BASE32_VALUES.values[block[7]])
        };
        const uint32_t combined = values[0] | values[1] | values[2] | values[3]
            | values[4] | values[5] | values[6] | values[7];
        if (combined >= 32U)
            break ;
        encoding_base32_write_full_block(values, output + output_index);
        input_index += 8U;
        output_index += 5U;
    }
    while (input_index < input_size)
    {
        if (input[input_index] == '=')
        {
            padding_started = FT_TRUE;
            padding_count++;
            input_index++;
            continue ;
        }
        if (padding_started == FT_TRUE)
        {
            cma_free(output);
            encoding_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        value = BASE32_VALUES.values[static_cast<uint8_t>(input[input_index])];
        if (value < 0)
        {
            cma_free(output);
            encoding_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        block_values[block_index] = static_cast<uint8_t>(value);
        block_index++;
        input_index++;
        if (block_index == 8)
        {
            block_error = encoding_base32_decode_block(block_values, 8,
                    output + output_index, &output_block_size);
            if (block_error != FT_ERR_SUCCESS)
            {
                cma_free(output);
                encoding_set_error(block_error);
                return (ft_nullptr);
            }
            output_index += output_block_size;
            block_index = 0;
        }
    }
    if (padding_started == FT_TRUE)
    {
        if (block_index != 2 && block_index != 4 && block_index != 5
            && block_index != 7)
        {
            cma_free(output);
            encoding_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        if (block_index + padding_count != 8)
        {
            cma_free(output);
            encoding_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
    }
    if (block_index != 0)
    {
        block_error = encoding_base32_decode_block(block_values, block_index,
                output + output_index, &output_block_size);
        if (block_error != FT_ERR_SUCCESS)
        {
            cma_free(output);
            encoding_set_error(block_error);
            return (ft_nullptr);
        }
        output_index += output_block_size;
    }
    if (output_size != ft_nullptr)
        *output_size = output_index;
    output[output_index] = 0;
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

uint8_t *encoding_base32_decode(const char *input, ft_size_t input_size,
    ft_size_t *output_size)
{
    return (encoding_base32_decode_internal(input, input_size, output_size));
}

static void encoding_hex_write(const uint8_t *input, ft_size_t input_size,
    ft_bool uppercase, char *output)
{
    const char *digits = "0123456789abcdef";
    if (uppercase == FT_TRUE)
        digits = "0123456789ABCDEF";
    ft_size_t input_index = 0;
    ft_size_t output_index = 0;
    while (input_index < input_size)
    {
        output[output_index++] = digits[(input[input_index] >> 4) & 0x0FU];
        output[output_index++] = digits[input[input_index] & 0x0FU];
        ++input_index;
    }
    output[output_index] = '\0';
    return ;
}

int32_t encoding_hex_encode_into(const uint8_t *input, ft_size_t input_size,
    ft_bool uppercase, char *output, ft_size_t output_capacity) noexcept
{
    if ((input == ft_nullptr && input_size != 0) || output == ft_nullptr)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    if (input_size > FT_SYSTEM_SIZE_MAX / 2 || output_capacity <= input_size * 2)
        return (encoding_set_error(FT_ERR_OUT_OF_RANGE));
    encoding_hex_write(input, input_size, uppercase, output);
    return (encoding_set_error(FT_ERR_SUCCESS));
}

char *encoding_hex_encode(const uint8_t *input, ft_size_t input_size,
    ft_bool uppercase)
{
    if (input == ft_nullptr && input_size != 0)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (input_size > FT_SYSTEM_SIZE_MAX / 2)
    {
        encoding_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    char *output = static_cast<char *>(cma_malloc((input_size * 2) + 1));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    encoding_hex_write(input, input_size, uppercase, output);
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

uint8_t *encoding_hex_decode(const char *input, ft_size_t input_size,
    ft_size_t *output_size)
{
    uint8_t *output;
    ft_size_t input_index;
    ft_size_t output_index;
    int32_t high_value;
    int32_t low_value;

    if (output_size != ft_nullptr)
        *output_size = 0;
    if ((input == ft_nullptr && input_size != 0) || input_size % 2 != 0)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    output = static_cast<uint8_t *>(cma_malloc((input_size / 2) + 1));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    input_index = 0;
    output_index = 0;
    while (input_index < input_size)
    {
        high_value = HEX_VALUES.values[static_cast<uint8_t>(input[input_index])];
        low_value = HEX_VALUES.values[static_cast<uint8_t>(input[input_index + 1])];
        if (high_value < 0 || low_value < 0)
        {
            cma_free(output);
            encoding_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        output[output_index++] = static_cast<uint8_t>((high_value << 4)
            | low_value);
        input_index += 2;
    }
    if (output_size != ft_nullptr)
        *output_size = output_index;
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

char *encoding_percent_encode(const uint8_t *input, ft_size_t input_size)
{
    const char *digits;
    char *output;
    ft_size_t input_index;
    ft_size_t output_size;
    ft_size_t output_index;

    if (input == ft_nullptr && input_size != 0)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (input_size == FT_SYSTEM_SIZE_MAX)
    {
        encoding_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    ft_size_t escaped_count = 0U;
    input_index = 0;
    while (input_index < input_size)
    {
        escaped_count += (PERCENT_UNRESERVED.values[input[input_index]] != FT_TRUE);
        input_index++;
    }
    if (escaped_count > (FT_SYSTEM_SIZE_MAX - 1U - input_size) / 2U)
    {
        encoding_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    output_size = input_size + escaped_count * 2U;
    output = static_cast<char *>(cma_malloc(output_size + 1));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    digits = "0123456789ABCDEF";
    input_index = 0;
    output_index = 0;
    if (escaped_count == 0U)
    {
        if (input_size != 0U)
            ft_memcpy(output, input, input_size);
        output_index = input_size;
    }
    else
    {
        while (input_index < input_size)
        {
            const uint8_t byte = input[input_index++];
            if (PERCENT_UNRESERVED.values[byte] == FT_TRUE)
                output[output_index++] = static_cast<char>(byte);
            else
            {
                output[output_index++] = '%';
                output[output_index++] = digits[byte >> 4U];
                output[output_index++] = digits[byte & 15U];
            }
        }
    }
    output[output_index] = '\0';
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

uint8_t *encoding_percent_decode(const char *input, ft_size_t input_size,
    ft_size_t *output_size)
{
    uint8_t *output;
    ft_size_t input_index;
    ft_size_t output_index;
    int32_t high_value;
    int32_t low_value;

    if (output_size != ft_nullptr)
        *output_size = 0;
    if (input == ft_nullptr && input_size != 0)
    {
        encoding_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (input_size == FT_SYSTEM_SIZE_MAX)
    {
        encoding_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    output = static_cast<uint8_t *>(cma_malloc(input_size + 1));
    if (output == ft_nullptr)
    {
        encoding_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    const char *first_escape = static_cast<const char *>(ft_memchr(input, '%',
            input_size));
    input_index = input_size;
    if (first_escape != ft_nullptr)
        input_index = static_cast<ft_size_t>(first_escape - input);
    output_index = input_index;
    if (input_index != 0U)
        ft_memcpy(output, input, input_index);
    while (input_index < input_size)
    {
        if (input[input_index] == '%')
        {
            if (input_size - input_index < 3U)
            {
                cma_free(output);
                encoding_set_error(FT_ERR_INVALID_ARGUMENT);
                return (ft_nullptr);
            }
            high_value = HEX_VALUES.values[static_cast<uint8_t>(input[input_index + 1])];
            low_value = HEX_VALUES.values[static_cast<uint8_t>(input[input_index + 2])];
            if (high_value < 0 || low_value < 0)
            {
                cma_free(output);
                encoding_set_error(FT_ERR_INVALID_ARGUMENT);
                return (ft_nullptr);
            }
            output[output_index++] = static_cast<uint8_t>((high_value << 4)
                | low_value);
            input_index += 3;
        }
        else
            output[output_index++] = static_cast<uint8_t>(input[input_index++]);
    }
    if (output_size != ft_nullptr)
        *output_size = output_index;
    encoding_set_error(FT_ERR_SUCCESS);
    return (output);
}

int32_t encoding_utf8_validate(const char *input, ft_size_t input_size)
{
    if (input == ft_nullptr && input_size != 0)
        return (encoding_set_error(FT_ERR_INVALID_ARGUMENT));
    return (encoding_set_error(ft_utf8_validate(input, input_size)));
}

int32_t encoding_utf8_next_codepoint(const char *input, ft_size_t input_size,
    ft_size_t *index_pointer, uint32_t *code_point_pointer)
{
    return (encoding_set_error(ft_utf8_next(input, input_size, index_pointer,
            code_point_pointer, ft_nullptr)));
}
