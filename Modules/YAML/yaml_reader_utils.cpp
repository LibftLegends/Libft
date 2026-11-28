#include "yaml.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Template/move.hpp"
#include "../Template/vector.hpp"

ft_size_t yaml_find_char(const ft_string &string, char character) noexcept
{
    const char *data = string.c_str();
    const char *match = static_cast<const char *>(ft_memchr(data, character, string.size()));

    if (match == ft_nullptr)
        return (static_cast<ft_size_t>(-1));
    return (static_cast<ft_size_t>(match - data));
}

static int32_t yaml_initialize_slice(ft_string &output, const char *data, ft_size_t length) noexcept
{
    const int32_t initialize_error = output.initialize();

    if (initialize_error != FT_ERR_SUCCESS || length == 0)
        return (initialize_error);
    return (output.assign(data, length));
}

ft_string *yaml_substr(const ft_string &string, ft_size_t start, ft_size_t length) noexcept
{
    const ft_size_t string_length = string.size();
    if (start > string_length)
        start = string_length;
    if (length > string_length - start)
        length = string_length - start;

    ft_string *output = new (std::nothrow) ft_string();
    if (output == ft_nullptr)
        return (ft_nullptr);
    if (yaml_initialize_slice(*output, string.c_str() + start, length) != FT_ERR_SUCCESS)
    {
        delete output;
        return (ft_nullptr);
    }
    return (output);
}

ft_string *yaml_substr_from(const ft_string &string, ft_size_t start) noexcept
{
    const ft_size_t string_length = string.size();

    if (start > string_length)
        return (ft_nullptr);
    return (yaml_substr(string, start, string_length - start));
}

ft_size_t yaml_count_indent(const ft_string &line) noexcept
{
    const char *data = line.c_str();
    ft_size_t index = 0;
    ft_size_t length = line.size();
    while (index < length && data[index] == ' ')
        index++;
    return (index);
}

void yaml_trim(ft_string &string) noexcept
{
    const char *data = string.c_str();
    ft_size_t start_index = 0;
    ft_size_t string_length = string.size();
    while (start_index < string_length && ft_isspace(static_cast<unsigned char>(data[start_index])))
        start_index++;
    ft_size_t end_index = string_length;
    while (end_index > start_index && ft_isspace(static_cast<unsigned char>(data[end_index - 1])))
        end_index--;
    if (start_index == 0 && end_index == string_length)
        return ;
    (void)string.assign(data + start_index, end_index - start_index);
    return ;
}

int32_t yaml_split_lines(const ft_string &content, ft_vector<ft_string> &lines) noexcept
{
    if (lines.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        const int32_t initialize_error = lines.initialize();
        if (initialize_error != FT_ERR_SUCCESS)
            return (initialize_error);
    }
    const char *data = content.c_str();
    ft_size_t start_index = 0;
    const ft_size_t content_length = content.size();
    while (start_index < content_length)
    {
        const char *line_start = data + start_index;
        ft_size_t line_length = content_length - start_index;
        const char *line_end = static_cast<const char *>(ft_memchr(line_start, '\n', line_length));
        if (line_end != ft_nullptr)
            line_length = static_cast<ft_size_t>(line_end - line_start);

        ft_string part;
        const int32_t slice_error = yaml_initialize_slice(part, line_start, line_length);
        if (slice_error != FT_ERR_SUCCESS)
            return (slice_error);
        const int32_t append_error = lines.push_back(ft_move(part));
        if (append_error != FT_ERR_SUCCESS)
            return (append_error);
        start_index += line_length;
        if (start_index < content_length)
            ++start_index;
    }
    return (FT_ERR_SUCCESS);
}
