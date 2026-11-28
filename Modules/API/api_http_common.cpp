#include "api_http_common.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../CPP_class/class_big_number.hpp"
#include <charconv>
#include <cstddef>
#include <limits>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

int32_t api_http_build_authorization_headers(ft_string &output, const char *headers,
    api_http_authorization_scheme scheme, const char *credentials)
{
    static constexpr char BASIC_PREFIX[] = "Authorization: Basic ";
    static constexpr char BEARER_PREFIX[] = "Authorization: Bearer ";
    static constexpr char SEPARATOR[] = "\r\n";
    if (!credentials)
        return (FT_ERR_INVALID_ARGUMENT);
    const char *prefix = BASIC_PREFIX;
    ft_size_t prefix_size = sizeof(BASIC_PREFIX) - 1;
    if (scheme == api_http_authorization_scheme::BEARER)
    {
        prefix = BEARER_PREFIX;
        prefix_size = sizeof(BEARER_PREFIX) - 1;
    }
    ft_size_t headers_size = 0;
    ft_size_t separator_size = 0;
    if (headers && headers[0] != '\0')
    {
        headers_size = ft_strlen_size_t(headers);
        separator_size = sizeof(SEPARATOR) - 1;
    }
    const ft_size_t credentials_size = ft_strlen_size_t(credentials);
    if (headers_size > FT_SYSTEM_SIZE_MAX - prefix_size - separator_size - 1
        || credentials_size > FT_SYSTEM_SIZE_MAX - prefix_size - separator_size - headers_size - 1)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t prefix_offset = headers_size + separator_size;
    const ft_size_t credentials_offset = prefix_offset + prefix_size;
    const ft_size_t total_size = credentials_offset + credentials_size;
    char *buffer = static_cast<char *>(cma_malloc(total_size + 1));
    if (!buffer)
        return (FT_ERR_NO_MEMORY);
    if (headers_size != 0)
    {
        ft_memcpy(buffer, headers, headers_size);
        ft_memcpy(buffer + headers_size, SEPARATOR, separator_size);
    }
    ft_memcpy(buffer + prefix_offset, prefix, prefix_size);
    ft_memcpy(buffer + credentials_offset, credentials, credentials_size);
    buffer[total_size] = '\0';
    const int32_t error = ft_string_take_owned_buffer(output, buffer, total_size);
    if (error != FT_ERR_SUCCESS)
        cma_free(buffer);
    return (error);
}

char api_http_to_lower(char character)
{
    if (character >= 'A' && character <= 'Z')
        character = static_cast<char>(character - 'A' + 'a');
    return (character);
}

ft_bool api_http_is_whitespace(char character)
{
    if (character == ' ')
        return (FT_TRUE);
    if (character == '\t')
        return (FT_TRUE);
    if (character == '\r')
        return (FT_TRUE);
    if (character == '\n')
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool api_http_line_starts_with(const char *line_start, ft_size_t line_length,
    const char *name)
{
    ft_size_t name_length;
    ft_size_t index;

    if (!line_start || !name)
        return (FT_FALSE);
    name_length = ft_strlen(name);
    if (line_length < name_length)
        return (FT_FALSE);
    index = 0;
    while (index < name_length)
    {
        char line_character;
        char name_character;

        line_character = api_http_to_lower(line_start[index]);
        name_character = api_http_to_lower(name[index]);
        if (line_character != name_character)
            return (FT_FALSE);
        index++;
    }
    if (line_length > name_length)
    {
        if (line_start[name_length] != ':')
            return (FT_FALSE);
    }
    return (FT_TRUE);
}

const char *api_http_find_crlf(const char *start, const char *end)
{
    if (start == ft_nullptr || end == ft_nullptr || start >= end)
        return (ft_nullptr);
    const char *last = end - 1;
    const char *cursor = start;
    while (cursor < last)
    {
        if (cursor[0] == '\r' && cursor[1] == '\n')
            return (cursor);
        ++cursor;
    }
    return (ft_nullptr);
}

void api_http_trim_header_value(const char **value_start,
    const char **value_end)
{
    const char *begin;
    const char *finish;

    if (!value_start || !value_end)
        return ;
    begin = *value_start;
    finish = *value_end;
    while (begin < finish && api_http_is_whitespace(*begin))
        begin++;
    while (finish > begin && api_http_is_whitespace(*(finish - 1)))
        finish--;
    *value_start = begin;
    *value_end = finish;
    return ;
}

ft_bool api_http_parse_decimal(const char *start, const char *end,
    int64_t &value)
{
    if (start == ft_nullptr || end == ft_nullptr || start >= end)
        return (FT_FALSE);
    uint64_t parsed = 0;
    const auto result = std::from_chars(start, end, parsed, 10);
    if (result.ec != std::errc{} || result.ptr != end
        || parsed > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return (FT_FALSE);
    value = static_cast<int64_t>(parsed);
    return (FT_TRUE);
}

ft_bool api_http_parse_hex(const char *start, const char *end,
    int64_t &value)
{
    if (start == ft_nullptr || end == ft_nullptr || start >= end)
        return (FT_FALSE);
    if (*start == ';')
    {
        value = 0;
        return (FT_TRUE);
    }
    uint64_t parsed = 0;
    const auto result = std::from_chars(start, end, parsed, 16);
    if (result.ec != std::errc{} || (result.ptr != end && *result.ptr != ';')
        || parsed > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        return (FT_FALSE);
    value = static_cast<int64_t>(parsed);
    return (FT_TRUE);
}

void api_http_parse_headers(const char *headers_start,
    const char *headers_end, ft_bool &connection_close,
    ft_bool &chunked_encoding, ft_bool &has_length, int64_t &content_length)
{
    const char *walker;
    ft_bool first_line;

    connection_close = FT_FALSE;
    chunked_encoding = FT_FALSE;
    has_length = FT_FALSE;
    content_length = 0;
    walker = headers_start;
    first_line = FT_TRUE;
    while (walker < headers_end)
    {
        const char *line_end;
        ft_size_t line_length;

        line_end = api_http_find_crlf(walker, headers_end);
        if (!line_end)
            return ;
        line_length = static_cast<ft_size_t>(line_end - walker);
        if (first_line)
        {
            if (line_length >= 8)
            {
                const char *version_start;
                ft_bool http11;

                version_start = walker;
                http11 = FT_FALSE;
                if (api_http_to_lower(version_start[0]) == 'h' &&
                    api_http_to_lower(version_start[1]) == 't' &&
                    api_http_to_lower(version_start[2]) == 't' &&
                    api_http_to_lower(version_start[3]) == 'p' &&
                    version_start[4] == '/' &&
                    version_start[5] == '1' &&
                    version_start[6] == '.' &&
                    version_start[7] == '1')
                    http11 = FT_TRUE;
                if (!http11)
                    connection_close = FT_TRUE;
            }
            first_line = FT_FALSE;
        }
        else if (line_length > 0)
        {
            if (api_http_line_starts_with(walker, line_length, "content-length"))
            {
                const char *value_start;
                const char *value_end;
                int64_t parsed_length;

                value_start = walker;
                value_start += ft_strlen("Content-Length:");
                value_end = walker + line_length;
                api_http_trim_header_value(&value_start, &value_end);
                if (api_http_parse_decimal(value_start, value_end, parsed_length))
                {
                    has_length = FT_TRUE;
                    content_length = parsed_length;
                }
            }
            else if (api_http_line_starts_with(walker, line_length, "connection"))
            {
                const char *value_start;
                const char *value_end;
                const char *iterator;
                ft_bool saw_close;

                value_start = walker;
                value_start += ft_strlen("Connection:");
                value_end = walker + line_length;
                api_http_trim_header_value(&value_start, &value_end);
                iterator = value_start;
                saw_close = FT_FALSE;
                while (iterator < value_end)
                {
                    char lower_character;

                    lower_character = api_http_to_lower(*iterator);
                    if (lower_character == 'c')
                    {
                        const char *check_pointer;

                        check_pointer = iterator;
                        if ((value_end - check_pointer) >= 5 &&
                            api_http_to_lower(check_pointer[0]) == 'c' &&
                            api_http_to_lower(check_pointer[1]) == 'l' &&
                            api_http_to_lower(check_pointer[2]) == 'o' &&
                            api_http_to_lower(check_pointer[3]) == 's' &&
                            api_http_to_lower(check_pointer[4]) == 'e')
                        {
                            saw_close = FT_TRUE;
                            break ;
                        }
                    }
                    iterator++;
                }
                if (saw_close)
                    connection_close = FT_TRUE;
                else if (!has_length && !chunked_encoding)
                {
                    const char *keep_pointer;

                    keep_pointer = value_start;
                    while (keep_pointer < value_end)
                    {
                        if (api_http_to_lower(*keep_pointer) == 'k')
                        {
                            if ((value_end - keep_pointer) >= 10 &&
                                api_http_to_lower(keep_pointer[0]) == 'k' &&
                                api_http_to_lower(keep_pointer[1]) == 'e' &&
                                api_http_to_lower(keep_pointer[2]) == 'e' &&
                                api_http_to_lower(keep_pointer[3]) == 'p' &&
                                keep_pointer[4] == '-' &&
                                api_http_to_lower(keep_pointer[5]) == 'a' &&
                                api_http_to_lower(keep_pointer[6]) == 'l' &&
                                api_http_to_lower(keep_pointer[7]) == 'i' &&
                                api_http_to_lower(keep_pointer[8]) == 'v' &&
                                api_http_to_lower(keep_pointer[9]) == 'e')
                            {
                                connection_close = FT_FALSE;
                                break ;
                            }
                        }
                        keep_pointer++;
                    }
                }
            }
            else if (api_http_line_starts_with(walker, line_length,
                    "transfer-encoding"))
            {
                const char *value_start;
                const char *value_end;
                const char *iterator;

                value_start = walker;
                value_start += ft_strlen("Transfer-Encoding:");
                value_end = walker + line_length;
                api_http_trim_header_value(&value_start, &value_end);
                iterator = value_start;
                while (iterator < value_end)
                {
                    if (api_http_to_lower(*iterator) == 'c')
                    {
                        if ((value_end - iterator) >= 7 &&
                            api_http_to_lower(iterator[0]) == 'c' &&
                            api_http_to_lower(iterator[1]) == 'h' &&
                            api_http_to_lower(iterator[2]) == 'u' &&
                            api_http_to_lower(iterator[3]) == 'n' &&
                            api_http_to_lower(iterator[4]) == 'k' &&
                            api_http_to_lower(iterator[5]) == 'e' &&
                            api_http_to_lower(iterator[6]) == 'd')
                        {
                            chunked_encoding = FT_TRUE;
                            break ;
                        }
                    }
                    iterator++;
                }
            }
        }
        if (line_end == headers_end)
            break ;
        walker = line_end + 2;
    }
    return ;
}

static ft_bool api_http_walk_chunks(const char *body_start, ft_size_t body_size,
    api_http_chunk_scan &scan, ft_string *decoded_body)
{
    if (body_start == ft_nullptr || scan.invalid || scan.position > body_size
        || scan.line_search_position > body_size)
        return (FT_FALSE);
    while (!scan.complete && scan.position < body_size)
    {
        const char *cursor = body_start + scan.position;
        if (scan.chunk_size < 0 || scan.trailers)
        {
            const char *line_end = api_http_find_crlf(
                body_start + scan.line_search_position, body_start + body_size);
            if (line_end == ft_nullptr)
            {
                scan.line_search_position = body_size - 1U;
                return (FT_FALSE);
            }
            if (scan.trailers)
                scan.complete = cursor == line_end;
            else if (!api_http_parse_hex(cursor, line_end, scan.chunk_size))
            {
                scan.invalid = FT_TRUE;
                return (FT_FALSE);
            }
            else
                scan.trailers = scan.chunk_size == 0;
            scan.position = static_cast<ft_size_t>(line_end + 2 - body_start);
            scan.line_search_position = scan.position;
            continue ;
        }
        const ft_size_t available = body_size - scan.position;
        const ft_size_t chunk_size = static_cast<ft_size_t>(scan.chunk_size);
        if (available < 2U || chunk_size > available - 2U)
            return (FT_FALSE);
        if (cursor[chunk_size] != '\r' || cursor[chunk_size + 1U] != '\n')
        {
            scan.invalid = FT_TRUE;
            return (FT_FALSE);
        }
        if (decoded_body != ft_nullptr
            && decoded_body->append(cursor, chunk_size) != FT_ERR_SUCCESS)
            return (FT_FALSE);
        scan.position += chunk_size + 2U;
        scan.line_search_position = scan.position;
        scan.chunk_size = -1;
    }
    return (scan.complete);
}

ft_bool api_http_chunked_body_complete(const char *body_start,
    ft_size_t body_size, api_http_chunk_scan &scan)
{
    return (api_http_walk_chunks(body_start, body_size, scan, ft_nullptr));
}

ft_bool api_http_chunked_body_complete(const char *body_start,
    ft_size_t body_size, ft_size_t &consumed_length)
{
    consumed_length = 0;
    api_http_chunk_scan scan;
    if (!api_http_chunked_body_complete(body_start, body_size, scan))
        return (FT_FALSE);
    consumed_length = scan.position;
    return (FT_TRUE);
}

ft_bool api_http_decode_chunked(const char *body_start, ft_size_t body_size,
    ft_string &decoded_body, ft_size_t &consumed_length)
{
    consumed_length = 0;
    if (decoded_body.clear() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    api_http_chunk_scan scan;
    if (!api_http_walk_chunks(body_start, body_size, scan, &decoded_body))
        return (FT_FALSE);
    consumed_length = scan.position;
    return (FT_TRUE);
}

template <typename Emit>
static ft_bool api_http_emit_json_payload(const json_group *payload, Emit emit)
{
    const auto literal = [&emit](const auto &text) -> ft_bool
    {
        return (emit(text, sizeof(text) - 1U));
    };
    const auto text = [&emit](const char *value) -> ft_bool
    {
        if (value == ft_nullptr || *value == '\0')
            return (FT_TRUE);
        return (emit(value, ft_strlen_size_t(value)));
    };
    if (payload == ft_nullptr)
        return (FT_TRUE);
    if (!literal("{\n"))
        return (FT_FALSE);
    const json_group *group = payload;
    while (group != ft_nullptr)
    {
        if (!literal("  \"") || !text(group->name) || !literal("\": {\n"))
            return (FT_FALSE);
        const json_item *item = group->items;
        while (item != ft_nullptr)
        {
            if (!literal("    \"") || !text(item->key) || !literal("\": "))
                return (FT_FALSE);
            if (item->is_big_number == FT_TRUE && item->big_number != ft_nullptr)
            {
                if (!text(item->big_number->c_str()))
                    return (FT_FALSE);
            }
            else if (!literal("\"") || !text(item->value) || !literal("\""))
                return (FT_FALSE);
            if (item->next != ft_nullptr && !literal(","))
                return (FT_FALSE);
            if (!literal("\n"))
                return (FT_FALSE);
            item = item->next;
        }
        if (!literal("  }") || (group->next != ft_nullptr && !literal(",")) || !literal("\n"))
            return (FT_FALSE);
        group = group->next;
    }
    return (literal("}\n"));
}

ft_bool api_http_measure_json_payload(json_group *payload,
    ft_size_t &payload_length)
{
    payload_length = 0;
    ft_size_t length = 0;
    if (!api_http_emit_json_payload(payload, [&length](const char *, ft_size_t size) -> ft_bool
    {
        if (size > SIZE_MAX - length)
            return (FT_FALSE);
        length += size;
        return (FT_TRUE);
    }))
        return (FT_FALSE);
    payload_length = length;
    return (FT_TRUE);
}

ft_bool api_http_stream_json_payload(json_group *payload,
    api_http_send_callback send_callback, void *context, int32_t &error_code)
{
    error_code = FT_ERR_SUCCESS;
    if (payload == ft_nullptr)
        return (FT_TRUE);
    if (send_callback == ft_nullptr)
    {
        error_code = FT_ERR_INVALID_ARGUMENT;
        return (FT_FALSE);
    }
    char buffer[4096];
    ft_size_t used = 0;
    const auto flush = [&]() -> ft_bool
    {
        if (used == 0)
            return (FT_TRUE);
        if (!send_callback(buffer, used, context, error_code))
            return (FT_FALSE);
        used = 0;
        return (FT_TRUE);
    };
    if (!api_http_emit_json_payload(payload, [&](const char *bytes, ft_size_t size) -> ft_bool
    {
        if (size >= sizeof(buffer))
            return (flush() && send_callback(bytes, size, context, error_code));
        if (size > sizeof(buffer) - used && !flush())
            return (FT_FALSE);
        ft_memcpy(buffer + used, bytes, size);
        used += size;
        return (FT_TRUE);
    }))
        return (FT_FALSE);
    return (flush());
}
