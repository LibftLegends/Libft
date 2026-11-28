#include "uri_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"

static ft_size_t uri_normalize_percent_path(char *path, ft_size_t size) noexcept
{
    ft_size_t read_index = 0U;
    ft_size_t write_index = 0U;
    while (read_index < size)
    {
        uint8_t value;
        if (path[read_index] == '%' && uri_read_percent_byte(path + read_index,
                size - read_index, value) == FT_TRUE)
        {
            if (uri_is_unreserved(value) == FT_TRUE || value == '/')
                path[write_index++] = static_cast<char>(value);
            else
            {
                path[write_index++] = '%';
                path[write_index++] = uri_hex_character(static_cast<uint8_t>(value >> 4U));
                path[write_index++] = uri_hex_character(static_cast<uint8_t>(value & 0x0FU));
            }
            read_index += 3U;
        }
        else
            path[write_index++] = path[read_index++];
    }
    path[write_index] = '\0';
    return (write_index);
}

static ft_size_t uri_remove_previous_segment(const char *output,
    ft_size_t output_index) noexcept
{
    if (output_index == 0)
        return (0);
    if (output_index > 1 && output[output_index - 1] == '/')
        output_index--;
    while (output_index > 0 && output[output_index - 1] != '/')
        output_index--;
    if (output_index > 1)
        output_index--;
    return (output_index);
}

static ft_size_t uri_remove_dot_segments(char *path, ft_size_t path_size) noexcept
{
    ft_size_t input_index = 0U;
    ft_size_t output_index = 0U;
    while (input_index < path_size)
    {
        const ft_bool leading_slash = path[input_index] == '/';
        if (leading_slash == FT_TRUE)
            input_index += 1U;
        ft_size_t segment_end = input_index;
        while (segment_end < path_size && path[segment_end] != '/')
            segment_end += 1U;
        const ft_size_t segment_size = segment_end - input_index;
        if (segment_size == 1U && path[input_index] == '.')
        {
            if (leading_slash == FT_TRUE && output_index == 0U)
                path[output_index++] = '/';
        }
        else if (segment_size == 2U && path[input_index] == '.'
            && path[input_index + 1U] == '.')
            output_index = uri_remove_previous_segment(path, output_index);
        else
        {
            if (leading_slash == FT_TRUE)
                path[output_index++] = '/';
            if (output_index != input_index && segment_size != 0U)
                ft_memmove(path + output_index, path + input_index, segment_size);
            output_index += segment_size;
        }
        input_index = segment_end;
    }
    path[output_index] = '\0';
    return (output_index);
}

struct uri_output_piece
{
    const char *text;
    ft_size_t length;
};

static char *uri_build_normalized(const uri_components &components, ft_size_t path_size)
{
    uri_output_piece pieces[] = {
        {components.scheme, ft_strlen_size_t(components.scheme)},
        {":", static_cast<ft_size_t>(components.scheme != ft_nullptr)},
        {"//", 2U * static_cast<ft_size_t>(components.host != ft_nullptr)},
        {components.userinfo, ft_strlen_size_t(components.userinfo)},
        {"@", static_cast<ft_size_t>(components.userinfo != ft_nullptr)},
        {components.host, ft_strlen_size_t(components.host)},
        {":", static_cast<ft_size_t>(components.port != ft_nullptr)},
        {components.port, ft_strlen_size_t(components.port)},
        {components.path, path_size},
        {"?", static_cast<ft_size_t>(components.query != ft_nullptr)},
        {components.query, ft_strlen_size_t(components.query)},
        {"#", static_cast<ft_size_t>(components.fragment != ft_nullptr)},
        {components.fragment, ft_strlen_size_t(components.fragment)}
    };
    constexpr ft_size_t PIECE_COUNT = sizeof(pieces) / sizeof(pieces[0]);
    ft_size_t length = 0U;
    ft_size_t index = 0U;
    while (index < PIECE_COUNT)
    {
        length += pieces[index].length;
        index += 1U;
    }
    char *output = static_cast<char *>(cma_malloc(length + 1U));
    if (output == ft_nullptr)
        return (ft_nullptr);
    char *destination = output;
    index = 0U;
    while (index < PIECE_COUNT)
    {
        const uri_output_piece &piece = pieces[index];
        if (piece.length == 1U)
            *destination = piece.text[0];
        else if (piece.length != 0U)
            ft_memcpy(destination, piece.text, piece.length);
        destination += piece.length;
        index += 1U;
    }
    *destination = '\0';
    return (output);
}

char *uri_normalize(const char *input, ft_size_t input_size)
{
    uri_components components;
    if (uri_parse(input, input_size, &components) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    uri_lower_string(components.scheme);
    uri_lower_string(components.host);
    ft_size_t path_size = uri_normalize_percent_path(components.path,
        ft_strlen_size_t(components.path));
    path_size = uri_remove_dot_segments(components.path, path_size);
    char *output = uri_build_normalized(components, path_size);
    uri_components_destroy(&components);
    if (output == ft_nullptr)
    {
        uri_set_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    uri_set_error(FT_ERR_SUCCESS);
    return (output);
}
