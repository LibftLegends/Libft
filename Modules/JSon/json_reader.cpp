#include "../Basic/basic.hpp"
#include "json_internal.hpp"
#include "document.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "../System_utils/system_utils.hpp"
#include <cstdio>
#include <limits>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static thread_local int32_t g_json_reader_last_error = FT_ERR_SUCCESS;

static void skip_whitespace(const char *json_string, ft_size_t &index)
{
    while (json_string[index]
        && ft_isspace(static_cast<unsigned char>(json_string[index])))
        index++;
    return ;
}

static void json_reader_set_error(int32_t error_code)
{
    g_json_reader_last_error = error_code;
    return ;
}

static int32_t json_reader_get_error(void)
{
    return (g_json_reader_last_error);
}

static void json_reader_set_io_error(int32_t last_error)
{
    int32_t normalized_error;

    normalized_error = cmp_decode_errno_offset_error(last_error);
    if (normalized_error == FT_ERR_SUCCESS)
        normalized_error = FT_ERR_IO;
    json_reader_set_error(normalized_error);
    return ;
}

static char *json_read_file_content(const char *filename)
{
    su_file *file_stream;
    int64_t file_size_long;
    ft_size_t file_size;
    char *content;
    ft_size_t read_count;
    int32_t io_error;

    file_stream = su_fopen(filename);
    io_error = json_reader_get_error();
    if (file_stream == ft_nullptr)
    {
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    if (su_fseek(file_stream, 0, SEEK_END) != FT_ERR_SUCCESS)
    {
        io_error = json_reader_get_error();
        su_fclose(file_stream);
        json_reader_get_error();
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    json_reader_get_error();
    file_size_long = su_ftell(file_stream);
    io_error = json_reader_get_error();
    if (file_size_long < 0)
    {
        su_fclose(file_stream);
        json_reader_get_error();
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    if (su_fseek(file_stream, 0, SEEK_SET) != FT_ERR_SUCCESS)
    {
        io_error = json_reader_get_error();
        su_fclose(file_stream);
        json_reader_get_error();
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    json_reader_get_error();
    if (static_cast<uint64_t>(file_size_long)
        > std::numeric_limits<ft_size_t>::max())
    {
        su_fclose(file_stream);
        json_reader_get_error();
        json_reader_set_error(FT_ERR_OUT_OF_RANGE);
        return (ft_nullptr);
    }
    file_size = static_cast<ft_size_t>(file_size_long);
    content = static_cast<char *>(cma_malloc(file_size + 1));
    if (content == ft_nullptr)
    {
        su_fclose(file_stream);
        json_reader_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    read_count = su_fread(content, 1, file_size, file_stream);
    io_error = json_reader_get_error();
    if (read_count != file_size)
    {
        cma_free(content);
        su_fclose(file_stream);
        json_reader_get_error();
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    content[file_size] = '\0';
    if (su_fclose(file_stream) != FT_ERR_SUCCESS)
    {
        io_error = json_reader_get_error();
        cma_free(content);
        json_reader_set_io_error(io_error);
        return (ft_nullptr);
    }
    json_reader_get_error();
    json_reader_set_error(FT_ERR_SUCCESS);
    return (content);
}

static int32_t json_reader_hex_value(char character, uint32_t &value) noexcept
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

static int32_t json_reader_ensure_capacity(char **buffer_ptr, ft_size_t &capacity, ft_size_t required) noexcept
{
    if (buffer_ptr == ft_nullptr)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_STATE);
    }
    if (capacity > required)
        return (FT_ERR_SUCCESS);
    ft_size_t new_capacity = capacity;
    if (new_capacity == FT_ERR_SUCCESS)
        new_capacity = 16;
    while (new_capacity <= required)
    {
        ft_size_t next_capacity = new_capacity * 2;
        if (next_capacity <= new_capacity)
        {
            json_reader_set_error(FT_ERR_OUT_OF_RANGE);
            return (FT_ERR_INVALID_STATE);
        }
        new_capacity = next_capacity;
    }
    char *resized_buffer;
    if (*buffer_ptr == ft_nullptr)
        resized_buffer = static_cast<char *>(cma_malloc(new_capacity));
    else
        resized_buffer = static_cast<char *>(cma_realloc(*buffer_ptr, new_capacity));
    if (!resized_buffer)
    {
        json_reader_set_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_INVALID_STATE);
    }
    *buffer_ptr = resized_buffer;
    capacity = new_capacity;
    json_reader_set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

static int32_t json_reader_append_utf8(char **buffer_ptr,
    ft_size_t &capacity,
    ft_size_t &length,
    uint32_t code_point) noexcept
{
    if (code_point <= 0x7F)
    {
        if (json_reader_ensure_capacity(buffer_ptr, capacity, length + 1) != FT_ERR_SUCCESS)
        {
            int32_t capacity_error = json_reader_get_error();
            json_reader_set_error(capacity_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        (*buffer_ptr)[length] = static_cast<char>(code_point);
        length++;
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (code_point <= 0x7FF)
    {
        if (json_reader_ensure_capacity(buffer_ptr, capacity, length + 2) != FT_ERR_SUCCESS)
        {
            int32_t capacity_error = json_reader_get_error();
            json_reader_set_error(capacity_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        (*buffer_ptr)[length] = static_cast<char>(0xC0 | (code_point >> 6));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | (code_point & 0x3F));
        length++;
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (code_point <= 0xFFFF)
    {
        if (json_reader_ensure_capacity(buffer_ptr, capacity, length + 3) != FT_ERR_SUCCESS)
        {
            int32_t capacity_error = json_reader_get_error();
            json_reader_set_error(capacity_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        (*buffer_ptr)[length] = static_cast<char>(0xE0 | (code_point >> 12));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | (code_point & 0x3F));
        length++;
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (code_point <= 0x10FFFF)
    {
        if (json_reader_ensure_capacity(buffer_ptr, capacity, length + 4) != FT_ERR_SUCCESS)
        {
            int32_t capacity_error = json_reader_get_error();
            json_reader_set_error(capacity_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        (*buffer_ptr)[length] = static_cast<char>(0xF0 | (code_point >> 18));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | ((code_point >> 12) & 0x3F));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | ((code_point >> 6) & 0x3F));
        length++;
        (*buffer_ptr)[length] = static_cast<char>(0x80 | (code_point & 0x3F));
        length++;
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
    return (FT_ERR_INVALID_STATE);
}

static int32_t json_reader_parse_code_unit(const char *json_string,
    ft_size_t length,
    ft_size_t &index,
    uint32_t &code_unit) noexcept
{
    code_unit = 0;
    ft_size_t digit_index = 0;
    while (digit_index < 4)
    {
        if (index >= length)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_STATE);
        }
        uint32_t digit_value = 0;
        if (json_reader_hex_value(json_string[index], digit_value) != FT_ERR_SUCCESS)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_STATE);
        }
        code_unit = (code_unit << 4) | digit_value;
        index++;
        digit_index++;
    }
    json_reader_set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

static int32_t json_reader_append_escape(const char *json_string,
    ft_size_t length,
    ft_size_t &index,
    char **buffer_ptr,
    ft_size_t &capacity,
    ft_size_t &out_length) noexcept
{
    if (index >= length)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_STATE);
    }
    char escape_char = json_string[index];
    index++;
    if (escape_char == '"' || escape_char == '\\' || escape_char == '/')
    {
        if (json_reader_ensure_capacity(buffer_ptr, capacity, out_length + 1) != FT_ERR_SUCCESS)
        {
            int32_t capacity_error = json_reader_get_error();
            json_reader_set_error(capacity_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        (*buffer_ptr)[out_length] = escape_char;
        out_length++;
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 'b')
    {
        int32_t utf8_error = json_reader_append_utf8(buffer_ptr, capacity, out_length, '\b');
        if (utf8_error != FT_ERR_SUCCESS)
        {
            int32_t last_error = json_reader_get_error();
            json_reader_set_error(last_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 'f')
    {
        int32_t utf8_error = json_reader_append_utf8(buffer_ptr, capacity, out_length, '\f');
        if (utf8_error != FT_ERR_SUCCESS)
        {
            int32_t last_error = json_reader_get_error();
            json_reader_set_error(last_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 'n')
    {
        int32_t utf8_error = json_reader_append_utf8(buffer_ptr, capacity, out_length, '\n');
        if (utf8_error != FT_ERR_SUCCESS)
        {
            int32_t last_error = json_reader_get_error();
            json_reader_set_error(last_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 'r')
    {
        int32_t utf8_error = json_reader_append_utf8(buffer_ptr, capacity, out_length, '\r');
        if (utf8_error != FT_ERR_SUCCESS)
        {
            int32_t last_error = json_reader_get_error();
            json_reader_set_error(last_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char == 't')
    {
        int32_t utf8_error = json_reader_append_utf8(buffer_ptr, capacity, out_length, '\t');
        if (utf8_error != FT_ERR_SUCCESS)
        {
            int32_t last_error = json_reader_get_error();
            json_reader_set_error(last_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        json_reader_set_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    if (escape_char != 'u')
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_STATE);
    }
    uint32_t code_unit = 0;
    if (json_reader_parse_code_unit(json_string, length, index, code_unit) != FT_ERR_SUCCESS)
    {
        int32_t parse_error = json_reader_get_error();
        json_reader_set_error(parse_error);
        return (FT_ERR_INVALID_STATE);
    }
    json_reader_get_error();
    uint32_t code_point = code_unit;
    if (code_unit >= 0xD800 && code_unit <= 0xDBFF)
    {
        if (index + 1 >= length)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_STATE);
        }
        if (json_string[index] != '\\' || json_string[index + 1] != 'u')
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_STATE);
        }
        index = index + 2;
        uint32_t low_unit = 0;
        if (json_reader_parse_code_unit(json_string, length, index, low_unit) != FT_ERR_SUCCESS)
        {
            int32_t parse_error = json_reader_get_error();
            json_reader_set_error(parse_error);
            return (FT_ERR_INVALID_STATE);
        }
        json_reader_get_error();
        if (low_unit < 0xDC00 || low_unit > 0xDFFF)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (FT_ERR_INVALID_STATE);
        }
        code_point = 0x10000;
        code_point = code_point + ((code_unit - 0xD800) << 10);
        code_point = code_point + (low_unit - 0xDC00);
    }
    else if (code_unit >= 0xDC00 && code_unit <= 0xDFFF)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_STATE);
    }
    int32_t utf8_result = json_reader_append_utf8(buffer_ptr, capacity, out_length, code_point);
    int32_t utf8_error = json_reader_get_error();
    if (utf8_result != FT_ERR_SUCCESS)
    {
        json_reader_set_error(utf8_error);
        return (FT_ERR_INVALID_STATE);
    }
    json_reader_set_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

static char *json_reader_copy_span(const char *start, ft_size_t length)
{
    char *result = static_cast<char *>(cma_malloc(length + 1U));
    if (result == ft_nullptr)
    {
        json_reader_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_memcpy(result, start, length);
    result[length] = '\0';
    json_reader_set_error(FT_ERR_SUCCESS);
    return (result);
}

static char *parse_string(const char *json_string, ft_size_t length, ft_size_t &index)
{
    if (index >= length || json_string[index] != '"')
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    ++index;
    ft_size_t capacity = 0U;
    ft_size_t out_length = 0U;
    char *result = ft_nullptr;
    while (index < length)
    {
        const ft_size_t start = index;
        while (index < length)
        {
            const unsigned char character = static_cast<unsigned char>(json_string[index]);
            if (character == '"' || character == '\\' || character < 0x20U)
                break ;
            ++index;
        }
        const ft_size_t span = index - start;
        if (index == length || static_cast<unsigned char>(json_string[index]) < 0x20U)
            break ;
        if (json_string[index] == '"' && result == ft_nullptr)
        {
            ++index;
            return (json_reader_copy_span(json_string + start, span));
        }
        if (json_reader_ensure_capacity(&result, capacity, out_length + span) != FT_ERR_SUCCESS)
        {
            cma_free(result);
            return (ft_nullptr);
        }
        ft_memcpy(result + out_length, json_string + start, span);
        out_length += span;
        if (json_string[index++] == '"')
        {
            result[out_length] = '\0';
            json_reader_set_error(FT_ERR_SUCCESS);
            return (result);
        }
        if (json_reader_append_escape(json_string, length, index, &result, capacity,
                out_length) != FT_ERR_SUCCESS)
        {
            cma_free(result);
            return (ft_nullptr);
        }
    }
    cma_free(result);
    json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
    return (ft_nullptr);
}

static char *parse_number(const char *json_string, ft_size_t length, ft_size_t &index)
{
    ft_size_t start_index = index;
    if (index < length && (json_string[index] == '-' || json_string[index] == '+'))
        index++;
    ft_bool has_digits = FT_FALSE;
    while (index < length
        && ft_isdigit(static_cast<unsigned char>(json_string[index])))
    {
        index++;
        has_digits = FT_TRUE;
    }
    if (index < length && json_string[index] == '.')
    {
        ft_size_t fractional_count;

        index++;
        fractional_count = 0;
        while (index < length
            && ft_isdigit(static_cast<unsigned char>(json_string[index])))
        {
            index++;
            fractional_count++;
        }
        if (fractional_count == FT_ERR_SUCCESS)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
    }
    if (index < length && (json_string[index] == 'e' || json_string[index] == 'E'))
    {
        ft_size_t exponent_count;

        index++;
        if (index < length && (json_string[index] == '-' || json_string[index] == '+'))
            index++;
        exponent_count = 0;
        while (index < length
            && ft_isdigit(static_cast<unsigned char>(json_string[index])))
        {
            index++;
            exponent_count++;
        }
        if (exponent_count == FT_ERR_SUCCESS)
        {
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
    }
    if (!has_digits)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    return (json_reader_copy_span(json_string + start_index, index - start_index));
}

static char *parse_value(const char *json_string, ft_size_t length, ft_size_t &index)
{
    skip_whitespace(json_string, index);
    if (index >= length)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    if (json_string[index] == '"')
        return (parse_string(json_string, length, index));
    if (length - index >= 4 && ft_strncmp(json_string + index, "true", 4) == FT_ERR_SUCCESS)
    {
        index += 4U;
        return (json_reader_copy_span("true", 4U));
    }
    if (length - index >= 5 && ft_strncmp(json_string + index, "false", 5) == FT_ERR_SUCCESS)
    {
        index += 5U;
        return (json_reader_copy_span("false", 5U));
    }
    if (ft_isdigit(static_cast<unsigned char>(json_string[index]))
        || json_string[index] == '-' || json_string[index] == '+')
        return (parse_number(json_string, length, index));
    json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
    return (ft_nullptr);
}

static json_item *parse_items(const char *json_string, ft_size_t length, ft_size_t &index)
{
    json_item *head = ft_nullptr;
    json_item *tail = ft_nullptr;
    skip_whitespace(json_string, index);
    if (index >= length || json_string[index] != '{')
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    index++;
    ft_bool object_closed = FT_FALSE;
    while (index < length)
    {
        skip_whitespace(json_string, index);
        if (index < length && json_string[index] == '}')
        {
            index++;
            object_closed = FT_TRUE;
            break ;
        }
        char *key = parse_string(json_string, length, index);
        int32_t key_error = json_reader_get_error();
        if (!key)
        {
            json_free_items(head);
            json_reader_set_error(key_error);
            return (ft_nullptr);
        }
        skip_whitespace(json_string, index);
        if (index >= length || json_string[index] != ':')
        {
            cma_free(key);
            json_free_items(head);
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        index++;
        char *value = parse_value(json_string, length, index);
        int32_t value_error = json_reader_get_error();
        if (!value)
        {
            cma_free(key);
            json_free_items(head);
            json_reader_set_error(value_error);
            return (ft_nullptr);
        }
        json_item *item = json_create_item_owned(key, value);
        if (!item)
        {
            cma_free(value);
            cma_free(key);
            json_free_items(head);
            json_reader_set_error(FT_ERR_NO_MEMORY);
            return (ft_nullptr);
        }
        if (!head)
            head = tail = item;
        else
        {
            tail->next = item;
            tail = item;
        }
        skip_whitespace(json_string, index);
        if (index < length && json_string[index] == ',')
        {
            index++;
            continue;
        }
    }
    if (!object_closed)
    {
        json_free_items(head);
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    json_reader_set_error(FT_ERR_SUCCESS);
    return (head);
}

json_group *json_read_from_file(const char *filename)
{
    char *content = json_read_file_content(filename);
    if (content == ft_nullptr)
        return (ft_nullptr);
    json_group *result = json_read_from_string(content);
    cma_free(content);
    return (result);
}

json_group *json_read_from_backend(ft_document_source &source)
{
    ft_string content_buffer;
    int32_t read_result;

    read_result = source.read_all(content_buffer);
    if (read_result != FT_ERR_SUCCESS)
    {
        json_reader_set_error(read_result);
        return (ft_nullptr);
    }
    json_group *result = json_read_from_string(content_buffer.c_str());
    int32_t parse_error = json_reader_get_error();
    if (!result)
    {
        json_reader_set_error(parse_error);
        return (ft_nullptr);
    }
    json_reader_set_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t json_document_read_from_backend(json_document &document, ft_document_source &source)
{
    return (document.read_from_backend(source));
}

json_group *json_read_from_string(const char *content)
{
    if (!content)
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    ft_size_t index = 0;
    skip_whitespace(content, index);
    ft_size_t length = ft_strlen_size_t(content);
    if (index >= length || content[index] != '{')
    {
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    index++;
    json_group *head = ft_nullptr;
    json_group *tail = ft_nullptr;
    ft_bool object_closed = FT_FALSE;
    while (index < length)
    {
        skip_whitespace(content, index);
        if (index < length && content[index] == '}')
        {
            index++;
            object_closed = FT_TRUE;
            break ;
        }
        char *group_name = parse_string(content, length, index);
        int32_t name_error = json_reader_get_error();
        if (!group_name)
        {
            if (name_error == FT_ERR_SUCCESS)
                name_error = FT_ERR_INVALID_ARGUMENT;
            json_free_groups(head);
            json_reader_set_error(name_error);
            return (ft_nullptr);
        }
        skip_whitespace(content, index);
        if (index >= length || content[index] != ':')
        {
            cma_free(group_name);
            json_free_groups(head);
            json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
            return (ft_nullptr);
        }
        index++;
        json_item *items = parse_items(content, length, index);
        int32_t items_error = json_reader_get_error();
        if (items_error != FT_ERR_SUCCESS)
        {
            cma_free(group_name);
            json_free_groups(head);
            json_reader_set_error(items_error);
            return (ft_nullptr);
        }
        json_group *group = json_create_group_owned(group_name);
        if (!group)
        {
            cma_free(group_name);
            json_free_items(items);
            json_free_groups(head);
            json_reader_set_error(FT_ERR_NO_MEMORY);
            return (ft_nullptr);
        }
        group->items = items;
        if (!head)
            head = tail = group;
        else
        {
            tail->next = group;
            tail = group;
        }
        skip_whitespace(content, index);
        if (index < length && content[index] == ',')
        {
            index++;
            continue;
        }
    }
    if (!object_closed)
    {
        json_free_groups(head);
        json_reader_set_error(FT_ERR_INVALID_ARGUMENT);
        return (ft_nullptr);
    }
    json_reader_set_error(FT_ERR_SUCCESS);
    return (head);
}
