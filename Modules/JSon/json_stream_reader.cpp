#include "json_stream_reader.hpp"

#include "json_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/pthread_internal.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local int32_t g_json_stream_last_error = FT_ERR_SUCCESS;

static constexpr ft_size_t JSON_STREAM_READ_ERROR = static_cast<ft_size_t>(-1);

#define JSON_STREAM_STATUS_OK 0
#define JSON_STREAM_STATUS_END 1
#define JSON_STREAM_STATUS_ERROR -1

static void json_stream_set_error(int32_t error_code)
{
    g_json_stream_last_error = error_code;
    return ;
}

static int32_t json_stream_get_error(void)
{
    return (g_json_stream_last_error);
}

#define JSON_STREAM_ERROR_RETURN(code, value) \
    do { json_stream_set_error(code); return (value); } while (0)

#define JSON_STREAM_SUCCESS_RETURN(value) \
    do { json_stream_set_error(FT_ERR_SUCCESS); return (value); } while (0)

struct json_stream_reader_error_guard
{
    int32_t *code;
    json_stream_reader_error_guard(int32_t *value)
        : code(value)
    {
        return ;
    }
    ~json_stream_reader_error_guard()
    {
        g_json_stream_last_error = *this->code;
        return ;
    }
};

static int32_t json_stream_last_error(void)
{
    return (g_json_stream_last_error);
}

static ft_size_t json_stream_reader_file_callback(void *user_data, char *buffer, ft_size_t max_size)
{
    FILE *file = static_cast<FILE *>(user_data);

    if (!file || !buffer || max_size == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    ft_size_t bytes_read = fread(buffer, 1, max_size, file);
    if (bytes_read == FT_ERR_SUCCESS)
    {
        if (ferror(file))
            return (JSON_STREAM_READ_ERROR);
    }
    return (bytes_read);
}

static int32_t json_stream_reader_fill(json_stream_reader *reader)
{
    if (!reader)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, JSON_STREAM_STATUS_ERROR);
    if (reader->read_callback == ft_nullptr || reader->buffer == ft_nullptr)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_STATE, JSON_STREAM_STATUS_ERROR);
    if (reader->buffer_index < reader->buffer_size)
        JSON_STREAM_SUCCESS_RETURN(JSON_STREAM_STATUS_OK);
    if (reader->end_of_stream)
        JSON_STREAM_SUCCESS_RETURN(JSON_STREAM_STATUS_END);
    ft_size_t bytes_read = reader->read_callback(reader->user_data, reader->buffer, reader->buffer_capacity);
    if (bytes_read == JSON_STREAM_READ_ERROR || bytes_read > reader->buffer_capacity)
    {
        reader->error_code = FT_ERR_IO;
        JSON_STREAM_ERROR_RETURN(FT_ERR_IO, JSON_STREAM_STATUS_ERROR);
    }
    if (bytes_read == FT_ERR_SUCCESS)
    {
        reader->end_of_stream = FT_TRUE;
        JSON_STREAM_SUCCESS_RETURN(JSON_STREAM_STATUS_END);
    }
    reader->buffer_index = 0;
    reader->buffer_size = bytes_read;
    JSON_STREAM_SUCCESS_RETURN(JSON_STREAM_STATUS_OK);
}

static int32_t json_stream_reader_peek(json_stream_reader *reader, char *out_char)
{
    if (out_char == ft_nullptr)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, JSON_STREAM_STATUS_ERROR);
    const int32_t status = json_stream_reader_fill(reader);
    if (status != JSON_STREAM_STATUS_OK)
        return (status);
    *out_char = reader->buffer[reader->buffer_index];
    return (JSON_STREAM_STATUS_OK);
}

static int32_t json_stream_reader_consume(json_stream_reader *reader, char *out_char)
{
    const int32_t status = json_stream_reader_peek(reader, out_char);
    if (status == JSON_STREAM_STATUS_OK)
        ++reader->buffer_index;
    return (status);
}

static int32_t json_stream_reader_expect(json_stream_reader *reader, char expected_char)
{
    char current_char;
    int32_t status = json_stream_reader_consume(reader, &current_char);
    int32_t consume_error = json_stream_get_error();
    if (consume_error != FT_ERR_SUCCESS)
    {
        json_stream_set_error(consume_error);
        return (FT_ERR_INVALID_STATE);
    }
    if (status != JSON_STREAM_STATUS_OK || current_char != expected_char)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    JSON_STREAM_SUCCESS_RETURN(0);
}

static int32_t json_stream_skip_whitespace(json_stream_reader *reader)
{
    while (json_stream_reader_fill(reader) == JSON_STREAM_STATUS_OK)
    {
        while (reader->buffer_index < reader->buffer_size)
        {
            const unsigned char character = static_cast<unsigned char>(reader->buffer[reader->buffer_index]);
            if (character != ' ' && (character < '\t' || character > '\r'))
                return (JSON_STREAM_STATUS_OK);
            ++reader->buffer_index;
        }
    }
    if (json_stream_get_error() != FT_ERR_SUCCESS)
        return (JSON_STREAM_STATUS_ERROR);
    return (JSON_STREAM_STATUS_END);
}

static int32_t json_stream_ensure_capacity(char **buffer, ft_size_t *capacity, ft_size_t minimum)
{
    if (!buffer || !capacity)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    if (*capacity > minimum)
        JSON_STREAM_SUCCESS_RETURN(0);
    ft_size_t new_capacity = *capacity;
    if (new_capacity == FT_ERR_SUCCESS)
        new_capacity = 1;
    if (minimum == FT_SYSTEM_SIZE_MAX)
        JSON_STREAM_ERROR_RETURN(FT_ERR_OUT_OF_RANGE, FT_ERR_INVALID_STATE);
    while (new_capacity <= minimum)
    {
        if (new_capacity > FT_SYSTEM_SIZE_MAX / 2U)
        {
            new_capacity = minimum + 1U;
            break ;
        }
        new_capacity *= 2U;
    }
    char *resized_buffer = static_cast<char *>(cma_realloc(*buffer, new_capacity));
    if (resized_buffer == ft_nullptr)
        JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, FT_ERR_INVALID_STATE);
    *buffer = resized_buffer;
    *capacity = new_capacity;
    JSON_STREAM_SUCCESS_RETURN(0);
}

static int32_t json_stream_hex_value(char character, uint32_t &value) noexcept
{
    if (character >= '0' && character <= '9')
    {
        value = static_cast<uint32_t>(character - '0');
        return (FT_ERR_SUCCESS);
    }
    if (character >= 'a' && character <= 'f')
    {
        value = static_cast<uint32_t>(character - 'a' + 10);
        return (FT_ERR_SUCCESS);
    }
    if (character >= 'A' && character <= 'F')
    {
        value = static_cast<uint32_t>(character - 'A' + 10);
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_STATE);
}

static int32_t json_stream_append_utf8(char **buffer,
    ft_size_t *capacity,
    ft_size_t *length,
    uint32_t code_point) noexcept
{
    if (!buffer || !capacity || !length)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    if (code_point <= 0x7F)
    {
        int32_t ensure_status = json_stream_ensure_capacity(buffer, capacity, *length + 1);
        int32_t ensure_error = json_stream_get_error();
        if (ensure_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(ensure_error);
            return (ensure_status);
        }
        (*buffer)[*length] = static_cast<char>(code_point);
        *length += 1;
        JSON_STREAM_SUCCESS_RETURN(0);
    }
    if (code_point <= 0x7FF)
    {
        int32_t ensure_status = json_stream_ensure_capacity(buffer, capacity, *length + 2);
        int32_t ensure_error = json_stream_get_error();
        if (ensure_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(ensure_error);
            return (ensure_status);
        }
        (*buffer)[*length] = static_cast<char>(0xC0 | (code_point >> 6));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | (code_point & 0x3F));
        *length += 1;
        JSON_STREAM_SUCCESS_RETURN(0);
    }
    if (code_point <= 0xFFFF)
    {
        int32_t ensure_status = json_stream_ensure_capacity(buffer, capacity, *length + 3);
        int32_t ensure_error = json_stream_get_error();
        if (ensure_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(ensure_error);
            return (ensure_status);
        }
        (*buffer)[*length] = static_cast<char>(0xE0 | (code_point >> 12));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | (code_point & 0x3F));
        *length += 1;
        JSON_STREAM_SUCCESS_RETURN(0);
    }
    if (code_point <= 0x10FFFF)
    {
        int32_t ensure_status = json_stream_ensure_capacity(buffer, capacity, *length + 4);
        int32_t ensure_error = json_stream_get_error();
        if (ensure_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(ensure_error);
            return (ensure_status);
        }
        (*buffer)[*length] = static_cast<char>(0xF0 | (code_point >> 18));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        *length += 1;
        (*buffer)[*length] = static_cast<char>(0x80 | (code_point & 0x3F));
        *length += 1;
        JSON_STREAM_SUCCESS_RETURN(0);
    }
    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
}

static int32_t json_stream_parse_code_unit(json_stream_reader *reader, uint32_t &code_unit)
{
    code_unit = 0;
    ft_size_t digit_index = 0;
    while (digit_index < 4)
    {
        char current_char;
        int32_t status = json_stream_reader_consume(reader, &current_char);
        int32_t consume_error = json_stream_get_error();
        if (consume_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(consume_error);
            return (FT_ERR_INVALID_STATE);
        }
        if (status != JSON_STREAM_STATUS_OK)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        uint32_t digit_value = 0;
        if (json_stream_hex_value(current_char, digit_value) != FT_ERR_SUCCESS)
        {
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
            return (FT_ERR_INVALID_STATE);
        }
        code_unit = (code_unit << 4) | digit_value;
        digit_index += 1;
    }
    JSON_STREAM_SUCCESS_RETURN(0);
}

static int32_t json_stream_append_escape(json_stream_reader *reader,
    char **buffer,
    ft_size_t *capacity,
    ft_size_t *length)
{
    char escape_char;
    int32_t status = json_stream_reader_consume(reader, &escape_char);
    int32_t consume_error = json_stream_get_error();
    if (consume_error != FT_ERR_SUCCESS)
    {
        json_stream_set_error(consume_error);
        return (status);
    }
    if (status != JSON_STREAM_STATUS_OK)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    if (escape_char == '"' || escape_char == '\\' || escape_char == '/')
    {
        if (json_stream_ensure_capacity(buffer, capacity, *length + 1) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        (*buffer)[*length] = escape_char;
        *length += 1;
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 'b')
        return (json_stream_append_utf8(buffer, capacity, length, '\b'));
    if (escape_char == 'f')
        return (json_stream_append_utf8(buffer, capacity, length, '\f'));
    if (escape_char == 'n')
        return (json_stream_append_utf8(buffer, capacity, length, '\n'));
    if (escape_char == 'r')
        return (json_stream_append_utf8(buffer, capacity, length, '\r'));
    if (escape_char == 't')
        return (json_stream_append_utf8(buffer, capacity, length, '\t'));
    if (escape_char != 'u')
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    uint32_t code_unit = 0;
    if (json_stream_parse_code_unit(reader, code_unit) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    uint32_t code_point = code_unit;
    if (code_unit >= 0xD800 && code_unit <= 0xDBFF)
    {
        char next_char;
        status = json_stream_reader_peek(reader, &next_char);
        if (status != JSON_STREAM_STATUS_OK || next_char != '\\')
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        status = json_stream_reader_consume(reader, &next_char);
        consume_error = json_stream_get_error();
        if (consume_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(consume_error);
            return (status);
        }
        if (status != JSON_STREAM_STATUS_OK)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        status = json_stream_reader_consume(reader, &next_char);
        consume_error = json_stream_get_error();
        if (consume_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(consume_error);
            return (status);
        }
        if (status != JSON_STREAM_STATUS_OK || next_char != 'u')
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        uint32_t low_unit = 0;
        if (json_stream_parse_code_unit(reader, low_unit) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        if (low_unit < 0xDC00 || low_unit > 0xDFFF)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        code_point = 0x10000;
        code_point = code_point + ((code_unit - 0xD800) << 10);
        code_point = code_point + (low_unit - 0xDC00);
    }
    else if (code_unit >= 0xDC00 && code_unit <= 0xDFFF)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    return (json_stream_append_utf8(buffer, capacity, length, code_point));
}

static int32_t json_stream_append_span(char **buffer, ft_size_t *capacity,
    ft_size_t &length, const char *span, ft_size_t span_length)
{
    if (span_length == 0U)
        return (FT_ERR_SUCCESS);
    if (span_length > FT_SYSTEM_SIZE_MAX - length - 1U)
        JSON_STREAM_ERROR_RETURN(FT_ERR_OUT_OF_RANGE, FT_ERR_INVALID_STATE);
    if (json_stream_ensure_capacity(buffer, capacity, length + span_length) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    ft_memcpy(*buffer + length, span, span_length);
    length += span_length;
    return (FT_ERR_SUCCESS);
}

static char *json_stream_parse_string(json_stream_reader *reader,
    ft_size_t *decoded_length = ft_nullptr)
{
    if (json_stream_reader_expect(reader, '"') != FT_ERR_SUCCESS)
        return (ft_nullptr);
    ft_size_t capacity = 32U;
    ft_size_t length = 0U;
    char *result = static_cast<char *>(cma_malloc(capacity));
    if (result == ft_nullptr)
        JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
    while (json_stream_reader_fill(reader) == JSON_STREAM_STATUS_OK)
    {
        const ft_size_t start = reader->buffer_index;
        while (reader->buffer_index < reader->buffer_size)
        {
            const unsigned char character = static_cast<unsigned char>(reader->buffer[reader->buffer_index]);
            if (character == '"' || character == '\\' || character < 0x20U)
                break ;
            ++reader->buffer_index;
        }
        const ft_size_t span = reader->buffer_index - start;
        if (json_stream_append_span(&result, &capacity, length, reader->buffer + start,
                span) != FT_ERR_SUCCESS)
        {
            cma_free(result);
            return (ft_nullptr);
        }
        if (reader->buffer_index == reader->buffer_size)
            continue ;
        const unsigned char character = static_cast<unsigned char>(reader->buffer[reader->buffer_index++]);
        if (character == '"')
        {
            result[length] = '\0';
            if (decoded_length != ft_nullptr)
                *decoded_length = length;
            JSON_STREAM_SUCCESS_RETURN(result);
        }
        if (character < 0x20U)
            break ;
        if (json_stream_append_escape(reader, &result, &capacity, &length) != FT_ERR_SUCCESS)
        {
            cma_free(result);
            return (ft_nullptr);
        }
    }
    cma_free(result);
    if (json_stream_get_error() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
}

static int32_t json_stream_match_literal(json_stream_reader *reader, const char *literal)
{
    ft_size_t index = 0;
    while (literal[index])
    {
        char current_char;
        int32_t status = json_stream_reader_consume(reader, &current_char);
        int32_t consume_error = json_stream_get_error();
        if (consume_error != FT_ERR_SUCCESS)
        {
            json_stream_set_error(consume_error);
            return (status);
        }
        if (status != JSON_STREAM_STATUS_OK)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        if (current_char != literal[index])
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
        index += 1;
    }
    JSON_STREAM_SUCCESS_RETURN(0);
}

static int32_t json_stream_append_digits(json_stream_reader *reader, char **number,
    ft_size_t *capacity, ft_size_t &length)
{
    int32_t status = json_stream_reader_fill(reader);
    while (status == JSON_STREAM_STATUS_OK)
    {
        const ft_size_t start = reader->buffer_index;
        while (reader->buffer_index < reader->buffer_size)
        {
            const char character = reader->buffer[reader->buffer_index];
            if (character < '0' || character > '9')
                break ;
            ++reader->buffer_index;
        }
        if (json_stream_append_span(number, capacity, length, reader->buffer + start,
                reader->buffer_index - start) != FT_ERR_SUCCESS)
            return (JSON_STREAM_STATUS_ERROR);
        if (reader->buffer_index < reader->buffer_size)
            return (JSON_STREAM_STATUS_OK);
        status = json_stream_reader_fill(reader);
    }
    return (status);
}

static int32_t json_stream_read_number_token(json_stream_reader *reader, char **number,
    ft_size_t *capacity, ft_size_t &length)
{
    const auto append_current = [&]() -> int32_t
    {
        const char *character = reader->buffer + reader->buffer_index;
        ++reader->buffer_index;
        return (json_stream_append_span(number, capacity, length, character, 1U));
    };
    int32_t status = json_stream_reader_fill(reader);
    if (status == JSON_STREAM_STATUS_ERROR)
        return (FT_ERR_INVALID_STATE);
    if (status == JSON_STREAM_STATUS_END)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    const char first = reader->buffer[reader->buffer_index];
    if ((first == '-' || first == '+') && append_current() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    const ft_size_t integer_start = length;
    status = json_stream_append_digits(reader, number, capacity, length);
    if (status == JSON_STREAM_STATUS_ERROR)
        return (FT_ERR_INVALID_STATE);
    if (length == integer_start)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    if (status == JSON_STREAM_STATUS_OK && reader->buffer[reader->buffer_index] == '.')
    {
        if (append_current() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        status = json_stream_append_digits(reader, number, capacity, length);
        if (status == JSON_STREAM_STATUS_ERROR)
            return (FT_ERR_INVALID_STATE);
    }
    if (status == JSON_STREAM_STATUS_OK
        && (reader->buffer[reader->buffer_index] == 'e' || reader->buffer[reader->buffer_index] == 'E'))
    {
        if (append_current() != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        status = json_stream_reader_fill(reader);
        if (status == JSON_STREAM_STATUS_ERROR)
            return (FT_ERR_INVALID_STATE);
        if (status == JSON_STREAM_STATUS_OK)
        {
            const char sign = reader->buffer[reader->buffer_index];
            if ((sign == '-' || sign == '+') && append_current() != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            if (json_stream_append_digits(reader, number, capacity, length) == JSON_STREAM_STATUS_ERROR)
                return (FT_ERR_INVALID_STATE);
        }
    }
    return (FT_ERR_SUCCESS);
}

static char *json_stream_parse_number(json_stream_reader *reader,
    ft_size_t *decoded_length = ft_nullptr)
{
    ft_size_t capacity = 32U;
    ft_size_t length = 0U;
    char *number = static_cast<char *>(cma_malloc(capacity));
    if (number == ft_nullptr)
        JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
    if (json_stream_read_number_token(reader, &number, &capacity, length) != FT_ERR_SUCCESS)
    {
        cma_free(number);
        return (ft_nullptr);
    }
    number[length] = '\0';
    if (decoded_length != ft_nullptr)
        *decoded_length = length;
    JSON_STREAM_SUCCESS_RETURN(number);
}

static char *json_stream_parse_value(json_stream_reader *reader)
{
    int32_t status = json_stream_skip_whitespace(reader);
    if (status != JSON_STREAM_STATUS_OK)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    char current_char;
    status = json_stream_reader_peek(reader, &current_char);
    if (status != JSON_STREAM_STATUS_OK)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    if (current_char == '"')
        return (json_stream_parse_string(reader));
    if (current_char == 't')
    {
        if (json_stream_match_literal(reader, "true") != FT_ERR_SUCCESS)
            return (ft_nullptr);
        char *value = adv_strdup("true");
        if (!value)
        {
            JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
        }
        JSON_STREAM_SUCCESS_RETURN(value);
    }
    if (current_char == 'f')
    {
        if (json_stream_match_literal(reader, "false") != FT_ERR_SUCCESS)
            return (ft_nullptr);
        char *value = adv_strdup("false");
        if (!value)
        {
            JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
        }
        JSON_STREAM_SUCCESS_RETURN(value);
    }
    if (ft_isdigit(static_cast<unsigned char>(current_char))
        || current_char == '-' || current_char == '+')
        return (json_stream_parse_number(reader));
    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
}

static json_item *json_stream_parse_items(json_stream_reader *reader)
{
    if (json_stream_skip_whitespace(reader) != JSON_STREAM_STATUS_OK)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    if (json_stream_reader_expect(reader, '{') != FT_ERR_SUCCESS)
        return (ft_nullptr);
    json_item *head = ft_nullptr;
    json_item *tail = ft_nullptr;
    ft_bool object_closed = FT_FALSE;
    while (!object_closed)
    {
        int32_t status = json_stream_skip_whitespace(reader);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        char current_char;
        status = json_stream_reader_peek(reader, &current_char);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        if (current_char == '}')
        {
            json_stream_reader_consume(reader, &current_char);
            object_closed = FT_TRUE;
            break ;
        }
        char *key = json_stream_parse_string(reader);
        if (!key)
        {
            json_free_items(head);
            return (ft_nullptr);
        }
        if (json_stream_skip_whitespace(reader) != JSON_STREAM_STATUS_OK)
        {
            cma_free(key);
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        if (json_stream_reader_expect(reader, ':') != FT_ERR_SUCCESS)
        {
            cma_free(key);
            json_free_items(head);
            return (ft_nullptr);
        }
        if (json_stream_skip_whitespace(reader) != JSON_STREAM_STATUS_OK)
        {
            cma_free(key);
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        char *value = json_stream_parse_value(reader);
        if (!value)
        {
            cma_free(key);
            json_free_items(head);
            return (ft_nullptr);
        }
        json_item *item = json_create_item_owned(key, value);
        if (!item)
        {
            cma_free(value);
            cma_free(key);
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_NO_MEMORY, ft_nullptr);
        }
        if (!head)
        {
            head = item;
            tail = item;
        }
        else
        {
            tail->next = item;
            tail = item;
        }
        status = json_stream_skip_whitespace(reader);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        status = json_stream_reader_peek(reader, &current_char);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_items(head);
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
        }
        if (current_char == ',')
        {
            json_stream_reader_consume(reader, &current_char);
            continue;
        }
        if (current_char == '}')
            continue;
        json_free_items(head);
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    }
    if (!object_closed)
    {
        json_free_items(head);
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    }
    JSON_STREAM_SUCCESS_RETURN(head);
}

static int32_t json_stream_dispatch_event(json_stream_event_callback callback,
    void *user_data,
    json_stream_event_type type,
    char *string_value,
    ft_bool bool_value,
    ft_size_t string_length = 0U)
{
    json_stream_event event;

    ft_bzero(&event, sizeof(event));
    event.type = type;
    event.bool_value = bool_value;
    if (string_value != ft_nullptr)
    {
        event.value.data = string_value;
        event.value.length = string_length;
    }
    else
    {
        event.value.data = ft_nullptr;
        event.value.length = 0;
    }
    if (callback != ft_nullptr)
    {
        int32_t callback_status;

        callback_status = callback(user_data, &event);
        if (string_value != ft_nullptr)
            cma_free(string_value);
        if (callback_status != FT_ERR_SUCCESS)
        {
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_OPERATION, FT_ERR_INVALID_STATE);
        }
    }
    else if (string_value != ft_nullptr)
        cma_free(string_value);
    JSON_STREAM_SUCCESS_RETURN(0);
}

static int32_t json_stream_read_value(json_stream_reader *reader,
    json_stream_event_callback callback,
    void *user_data);

static int32_t json_stream_read_array(json_stream_reader *reader,
    json_stream_event_callback callback,
    void *user_data)
{
    if (json_stream_reader_expect(reader, '[') != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_BEGIN_ARRAY,
            ft_nullptr,
            FT_FALSE) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    ft_bool finished = FT_FALSE;
    ft_bool first_entry = FT_TRUE;
    while (!finished)
    {
        int32_t status = json_stream_skip_whitespace(reader);
        if (status == JSON_STREAM_STATUS_ERROR)
            return (FT_ERR_INVALID_STATE);
        if (status == JSON_STREAM_STATUS_OK)
        {
            char current_char;

            status = json_stream_reader_peek(reader, &current_char);
            if (status != JSON_STREAM_STATUS_OK)
                return (FT_ERR_INVALID_STATE);
            if (current_char == ']')
            {
                json_stream_reader_consume(reader, &current_char);
                finished = FT_TRUE;
                break ;
            }
            if (!first_entry)
            {
                if (current_char != ',')
                {
                    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
                }
                json_stream_reader_consume(reader, &current_char);
                status = json_stream_skip_whitespace(reader);
                if (status != JSON_STREAM_STATUS_OK)
                    return (FT_ERR_INVALID_STATE);
                status = json_stream_reader_peek(reader, &current_char);
                if (status != JSON_STREAM_STATUS_OK)
                    return (FT_ERR_INVALID_STATE);
                if (current_char == ']')
                {
                    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
                }
            }
            if (json_stream_read_value(reader, callback, user_data) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            first_entry = FT_FALSE;
            continue ;
        }
        if (status == JSON_STREAM_STATUS_END)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    }
    if (!finished)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    return (json_stream_dispatch_event(callback,
        user_data,
        JSON_STREAM_EVENT_END_ARRAY,
        ft_nullptr,
        FT_FALSE));
}

static int32_t json_stream_read_object(json_stream_reader *reader,
    json_stream_event_callback callback,
    void *user_data)
{
    if (json_stream_reader_expect(reader, '{') != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_BEGIN_OBJECT,
            ft_nullptr,
            FT_FALSE) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    ft_bool finished = FT_FALSE;
    ft_bool first_entry = FT_TRUE;
    while (!finished)
    {
        int32_t status = json_stream_skip_whitespace(reader);
        if (status == JSON_STREAM_STATUS_ERROR)
            return (FT_ERR_INVALID_STATE);
        if (status == JSON_STREAM_STATUS_OK)
        {
            char current_char;

            status = json_stream_reader_peek(reader, &current_char);
            if (status != JSON_STREAM_STATUS_OK)
                return (FT_ERR_INVALID_STATE);
            if (current_char == '}')
            {
                json_stream_reader_consume(reader, &current_char);
                finished = FT_TRUE;
                break ;
            }
            if (!first_entry)
            {
                if (current_char != ',')
                {
                    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
                }
                json_stream_reader_consume(reader, &current_char);
                status = json_stream_skip_whitespace(reader);
                if (status != JSON_STREAM_STATUS_OK)
                    return (FT_ERR_INVALID_STATE);
                status = json_stream_reader_peek(reader, &current_char);
                if (status != JSON_STREAM_STATUS_OK)
                    return (FT_ERR_INVALID_STATE);
                if (current_char == '}' || current_char == ',')
                {
                    JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
                }
            }
            ft_size_t key_length;
            char *key = json_stream_parse_string(reader, &key_length);
            if (!key)
                return (FT_ERR_INVALID_STATE);
            if (json_stream_dispatch_event(callback,
                    user_data,
                    JSON_STREAM_EVENT_KEY,
                    key,
                    FT_FALSE, key_length) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            status = json_stream_skip_whitespace(reader);
            if (status != JSON_STREAM_STATUS_OK)
                return (FT_ERR_INVALID_STATE);
            if (json_stream_reader_expect(reader, ':') != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            if (json_stream_read_value(reader, callback, user_data) != FT_ERR_SUCCESS)
                return (FT_ERR_INVALID_STATE);
            first_entry = FT_FALSE;
            continue ;
        }
        if (status == JSON_STREAM_STATUS_END)
            JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    }
    if (!finished)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    return (json_stream_dispatch_event(callback,
        user_data,
        JSON_STREAM_EVENT_END_OBJECT,
        ft_nullptr,
        FT_FALSE));
}

static int32_t json_stream_read_value(json_stream_reader *reader,
    json_stream_event_callback callback,
    void *user_data)
{
    int32_t status = json_stream_skip_whitespace(reader);
    if (status != JSON_STREAM_STATUS_OK)
        return (FT_ERR_INVALID_STATE);
    char current_char;

    status = json_stream_reader_peek(reader, &current_char);
    if (status != JSON_STREAM_STATUS_OK)
        return (FT_ERR_INVALID_STATE);
    if (current_char == '"')
    {
        ft_size_t string_length;
        char *string_value = json_stream_parse_string(reader, &string_length);
        if (!string_value)
            return (FT_ERR_INVALID_STATE);
        return (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_STRING,
            string_value,
            FT_FALSE, string_length));
    }
    if (current_char == '{')
        return (json_stream_read_object(reader, callback, user_data));
    if (current_char == '[')
        return (json_stream_read_array(reader, callback, user_data));
    if (current_char == 't')
    {
        if (json_stream_match_literal(reader, "true") != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        return (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_BOOLEAN,
            ft_nullptr,
            FT_TRUE));
    }
    if (current_char == 'f')
    {
        if (json_stream_match_literal(reader, "false") != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        return (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_BOOLEAN,
            ft_nullptr,
            FT_FALSE));
    }
    if (current_char == 'n')
    {
        if (json_stream_match_literal(reader, "null") != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_STATE);
        return (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_NULL,
            ft_nullptr,
            FT_FALSE));
    }
    ft_size_t number_length;
    char *number_value = json_stream_parse_number(reader, &number_length);
    if (!number_value)
        return (FT_ERR_INVALID_STATE);
    return (json_stream_dispatch_event(callback,
        user_data,
        JSON_STREAM_EVENT_NUMBER,
        number_value,
        FT_FALSE, number_length));
}

int32_t json_stream_reader_traverse(json_stream_reader *reader,
    json_stream_event_callback callback,
    void *user_data)
{
    int32_t lock_error;
    int32_t status;
    int32_t error_code = FT_ERR_SUCCESS;
    json_stream_reader_error_guard stack_guard(&error_code);

    if (!reader)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_STATE);
    }
    if (json_stream_reader_enable_thread_safety(reader) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (reader->_mutex == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_STATE;
        return (FT_ERR_INVALID_STATE);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        error_code = lock_error;
        return (FT_ERR_INVALID_STATE);
    }
    reader->error_code = FT_ERR_SUCCESS;
    if (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_BEGIN_DOCUMENT,
            ft_nullptr,
            FT_FALSE) != FT_ERR_SUCCESS)
    {
        reader->error_code = error_code = json_stream_last_error();
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (json_stream_read_value(reader, callback, user_data) != FT_ERR_SUCCESS)
    {
        if (reader->error_code == FT_ERR_SUCCESS)
            reader->error_code = error_code = json_stream_last_error();
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    status = json_stream_skip_whitespace(reader);
    if (status == JSON_STREAM_STATUS_ERROR)
    {
        if (reader->error_code == FT_ERR_SUCCESS)
            reader->error_code = error_code = json_stream_last_error();
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (status == JSON_STREAM_STATUS_OK)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        reader->error_code = FT_ERR_INVALID_ARGUMENT;
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (json_stream_dispatch_event(callback,
            user_data,
            JSON_STREAM_EVENT_END_DOCUMENT,
            ft_nullptr,
            FT_FALSE) != FT_ERR_SUCCESS)
    {
        if (reader->error_code == FT_ERR_SUCCESS)
            reader->error_code = error_code = json_stream_last_error();
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    reader->error_code = FT_ERR_SUCCESS;
    error_code = FT_ERR_SUCCESS;
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t json_stream_read_from_file_events(FILE *file,
    ft_size_t buffer_capacity,
    json_stream_event_callback callback,
    void *user_data)
{
    int32_t error_code = FT_ERR_SUCCESS;
    json_stream_reader_error_guard stack_guard(&error_code);
    if (!file || buffer_capacity == FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_STATE);
    }
    json_stream_reader reader = json_stream_reader();
    if (json_stream_reader_init_file(&reader, file, buffer_capacity) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    int32_t status = json_stream_reader_traverse(&reader, callback, user_data);
    int32_t reader_error = reader.error_code;
    json_stream_reader_destroy(&reader);
    if (status != FT_ERR_SUCCESS)
    {
        if (reader_error != FT_ERR_SUCCESS)
            error_code = reader_error;
        else
            error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_STATE);
    }
    error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

int32_t json_stream_read_from_stream_events(json_stream_read_callback callback,
    void *user_data,
    ft_size_t buffer_capacity,
    json_stream_event_callback event_callback,
    void *event_user_data)
{
    int32_t error_code = FT_ERR_SUCCESS;
    json_stream_reader_error_guard stack_guard(&error_code);
    if (!callback || buffer_capacity == FT_ERR_SUCCESS)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_STATE);
    }
    json_stream_reader reader = json_stream_reader();
    if (json_stream_reader_init_callback(&reader, callback, user_data, buffer_capacity) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    int32_t status = json_stream_reader_traverse(&reader, event_callback, event_user_data);
    int32_t reader_error = reader.error_code;
    json_stream_reader_destroy(&reader);
    if (status != FT_ERR_SUCCESS)
    {
        if (reader_error != FT_ERR_SUCCESS)
            error_code = reader_error;
        else
            error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_STATE);
    }
    error_code = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

json_group *json_stream_reader_parse(json_stream_reader *reader)
{
    int32_t lock_error;
    int32_t status;
    json_group *head;
    json_group *tail;
    ft_bool object_closed;
    int32_t error_code = FT_ERR_SUCCESS;
    json_stream_reader_error_guard stack_guard(&error_code);

    if (!reader)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    if (json_stream_reader_enable_thread_safety(reader) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (reader->_mutex == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_STATE;
        return (ft_nullptr);
    }
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        error_code = lock_error;
        return (ft_nullptr);
    }
    reader->error_code = FT_ERR_SUCCESS;
    status = json_stream_skip_whitespace(reader);
    if (status != JSON_STREAM_STATUS_OK)
    {
        reader->error_code = FT_ERR_INVALID_ARGUMENT;
        error_code = FT_ERR_INVALID_ARGUMENT;
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (ft_nullptr);
    }
    if (json_stream_reader_expect(reader, '{') != FT_ERR_SUCCESS)
    {
        reader->error_code = FT_ERR_INVALID_ARGUMENT;
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (ft_nullptr);
    }
    head = ft_nullptr;
    tail = ft_nullptr;
    object_closed = FT_FALSE;
    while (!object_closed)
    {
        status = json_stream_skip_whitespace(reader);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        char current_char;
        status = json_stream_reader_peek(reader, &current_char);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        if (current_char == '}')
        {
            json_stream_reader_consume(reader, &current_char);
            object_closed = FT_TRUE;
            break ;
        }
        char *group_name = json_stream_parse_string(reader);
        if (!group_name)
        {
            json_free_groups(head);
            error_code = json_stream_last_error();
            reader->error_code = error_code;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        if (json_stream_skip_whitespace(reader) != JSON_STREAM_STATUS_OK)
        {
            cma_free(group_name);
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        if (json_stream_reader_expect(reader, ':') != FT_ERR_SUCCESS)
        {
            cma_free(group_name);
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        if (json_stream_skip_whitespace(reader) != JSON_STREAM_STATUS_OK)
        {
            cma_free(group_name);
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            json_stream_set_error(FT_ERR_INVALID_ARGUMENT);
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        json_item *items = json_stream_parse_items(reader);
        if (json_stream_last_error() != FT_ERR_SUCCESS)
        {
            cma_free(group_name);
            json_free_groups(head);
            error_code = json_stream_last_error();
            reader->error_code = error_code;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        json_group *group = json_create_group_owned(group_name);
        if (!group)
        {
            cma_free(group_name);
            json_free_items(items);
            json_free_groups(head);
            reader->error_code = FT_ERR_NO_MEMORY;
            json_stream_set_error(FT_ERR_NO_MEMORY);
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        group->items = items;
        if (!head)
        {
            head = group;
            tail = group;
        }
        else
        {
            tail->next = group;
            tail = group;
        }
        status = json_stream_skip_whitespace(reader);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        status = json_stream_reader_peek(reader, &current_char);
        if (status != JSON_STREAM_STATUS_OK)
        {
            json_free_groups(head);
            reader->error_code = FT_ERR_INVALID_ARGUMENT;
            error_code = FT_ERR_INVALID_ARGUMENT;
            (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
            return (ft_nullptr);
        }
        if (current_char == ',')
        {
            json_stream_reader_consume(reader, &current_char);
            continue;
        }
        if (current_char == '}')
            continue;
        json_free_groups(head);
        reader->error_code = FT_ERR_INVALID_ARGUMENT;
        error_code = FT_ERR_INVALID_ARGUMENT;
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (ft_nullptr);
    }
    if (!object_closed)
    {
        json_free_groups(head);
        reader->error_code = FT_ERR_INVALID_ARGUMENT;
        error_code = FT_ERR_INVALID_ARGUMENT;
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (ft_nullptr);
    }
    reader->error_code = FT_ERR_SUCCESS;
    json_stream_set_error(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    return (head);
}

int32_t json_stream_reader_init_callback(json_stream_reader *reader, json_stream_read_callback callback, void *user_data, ft_size_t buffer_capacity)
{
    int32_t lock_error;
    char *new_buffer;

    if (!reader || !callback || buffer_capacity == FT_ERR_SUCCESS)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    if (json_stream_reader_enable_thread_safety(reader) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    if (reader->_mutex == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_stream_set_error(lock_error);
        return (FT_ERR_INVALID_STATE);
    }
    new_buffer = static_cast<char *>(cma_malloc(buffer_capacity));
    if (!new_buffer)
    {
        reader->error_code = FT_ERR_NO_MEMORY;
        json_stream_reader_set_error_unlocked(reader, FT_ERR_NO_MEMORY);
        json_stream_set_error(FT_ERR_NO_MEMORY);
        (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    if (reader->buffer)
        cma_free(reader->buffer);
    reader->read_callback = callback;
    reader->user_data = user_data;
    reader->buffer = new_buffer;
    reader->buffer_capacity = buffer_capacity;
    reader->buffer_size = 0;
    reader->buffer_index = 0;
    reader->end_of_stream = FT_FALSE;
    reader->error_code = FT_ERR_SUCCESS;
    json_stream_reader_set_error_unlocked(reader, FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t json_stream_reader_init_file(json_stream_reader *reader, FILE *file, ft_size_t buffer_capacity)
{
    if (!file)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, FT_ERR_INVALID_STATE);
    return (json_stream_reader_init_callback(reader, json_stream_reader_file_callback, file, buffer_capacity));
}

void json_stream_reader_destroy(json_stream_reader *reader)
{
    int32_t lock_error;

    if (!reader)
        return ;
    if (json_stream_reader_enable_thread_safety(reader) != FT_ERR_SUCCESS)
        return ;
    if (reader->_mutex == ft_nullptr)
        return ;
    lock_error = pt_recursive_mutex_lock_if_not_null(reader->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        json_stream_set_error(lock_error);
        return ;
    }
    if (reader->buffer)
        cma_free(reader->buffer);
    reader->buffer = ft_nullptr;
    reader->buffer_capacity = 0;
    reader->buffer_size = 0;
    reader->buffer_index = 0;
    reader->end_of_stream = FT_FALSE;
    reader->error_code = FT_ERR_SUCCESS;
    reader->read_callback = ft_nullptr;
    reader->user_data = ft_nullptr;
    json_stream_reader_set_error_unlocked(reader, FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(reader->_mutex);
    (void)json_stream_reader_disable_thread_safety(reader);
    return ;
}

json_group *json_read_from_file_stream(FILE *file, ft_size_t buffer_capacity)
{
    if (!file)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    json_group *groups;
    int32_t result_errno;

    groups = ft_nullptr;
    result_errno = FT_ERR_SUCCESS;
    {
        json_stream_reader reader = json_stream_reader();

        if (json_stream_reader_init_file(&reader, file, buffer_capacity) != FT_ERR_SUCCESS)
            return (ft_nullptr);
        groups = json_stream_reader_parse(&reader);
        if (!groups)
        {
            int32_t parse_errno;
            int32_t error_code;

            parse_errno = json_stream_last_error();
            error_code = reader.error_code;
            result_errno = error_code;
            if (result_errno == FT_ERR_SUCCESS)
                result_errno = parse_errno;
            if (result_errno == FT_ERR_SUCCESS)
                result_errno = FT_ERR_INVALID_ARGUMENT;
        }
        json_stream_reader_destroy(&reader);
    }
        if (!groups)
            json_stream_set_error(result_errno);
    return (groups);
}

json_group *json_read_from_stream(json_stream_read_callback callback, void *user_data, ft_size_t buffer_capacity)
{
    if (!callback || buffer_capacity == FT_ERR_SUCCESS)
        JSON_STREAM_ERROR_RETURN(FT_ERR_INVALID_ARGUMENT, ft_nullptr);
    json_group *groups;
    int32_t result_errno;

    groups = ft_nullptr;
    result_errno = FT_ERR_SUCCESS;
    {
        json_stream_reader reader = json_stream_reader();

        if (json_stream_reader_init_callback(&reader, callback, user_data, buffer_capacity) != FT_ERR_SUCCESS)
            return (ft_nullptr);
        groups = json_stream_reader_parse(&reader);
        if (!groups)
        {
            int32_t parse_errno;
            int32_t error_code;

            parse_errno = json_stream_last_error();
            error_code = reader.error_code;
            result_errno = error_code;
            if (result_errno == FT_ERR_SUCCESS)
                result_errno = parse_errno;
            if (result_errno == FT_ERR_SUCCESS)
                result_errno = FT_ERR_INVALID_ARGUMENT;
        }
        json_stream_reader_destroy(&reader);
    }
        if (!groups)
            json_stream_set_error(result_errno);
    return (groups);
}
