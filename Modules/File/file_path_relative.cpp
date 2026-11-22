#include "file_utils.hpp"
#include <string_view>

enum class file_relative_root_kind : uint8_t
{
    relative,
    system_root,
    drive_absolute,
    drive_relative,
    unc
};

struct file_relative_root
{
    file_relative_root_kind kind = file_relative_root_kind::relative;
    std::string_view first;
    std::string_view second;
};

static constexpr ft_bool file_relative_is_separator(char character) noexcept
{
    return (character == '/' || character == '\\');
}

static constexpr char file_relative_ascii_lower(char character) noexcept
{
    if (character >= 'A' && character <= 'Z')
        return (static_cast<char>(character - 'A' + 'a'));
    return (character);
}

static std::string_view file_relative_take_component(std::string_view &path) noexcept
{
    while (!path.empty() && file_relative_is_separator(path.front()) == FT_TRUE)
        path.remove_prefix(1);
    ft_size_t length = 0;
    while (length < path.size() && file_relative_is_separator(path[length]) == FT_FALSE)
        ++length;
    const std::string_view component = path.substr(0, length);
    path.remove_prefix(length);
    return (component);
}

static file_relative_root file_relative_parse_root(std::string_view &path) noexcept
{
    file_relative_root root;
    if (path.size() >= 2 && path[1] == ':')
    {
        const char drive = file_relative_ascii_lower(path[0]);
        if (drive >= 'a' && drive <= 'z')
        {
            root.kind = file_relative_root_kind::drive_relative;
            if (path.size() >= 3 && file_relative_is_separator(path[2]) == FT_TRUE)
                root.kind = file_relative_root_kind::drive_absolute;
            root.first = path.substr(0, 1);
            (void)file_relative_take_component(path);
            return (root);
        }
    }
    if (!path.empty() && file_relative_is_separator(path.front()) == FT_TRUE)
        root.kind = file_relative_root_kind::system_root;
    if (path.size() >= 2 && file_relative_is_separator(path[0]) == FT_TRUE
        && file_relative_is_separator(path[1]) == FT_TRUE)
    {
        root.kind = file_relative_root_kind::unc;
        path.remove_prefix(2);
        if (!path.empty() && file_relative_is_separator(path.front()) == FT_FALSE)
        {
            root.first = file_relative_take_component(path);
            if (path.size() > 1 && file_relative_is_separator(path[1]) == FT_FALSE)
                root.second = file_relative_take_component(path);
        }
    }
    return (root);
}

static ft_bool file_relative_equal(std::string_view left, std::string_view right,
    ft_bool case_insensitive) noexcept
{
    if (case_insensitive == FT_FALSE)
        return (left == right);
    if (left.size() != right.size())
        return (FT_FALSE);
    ft_size_t index = 0;
    while (index < left.size())
    {
        if (file_relative_ascii_lower(left[index]) != file_relative_ascii_lower(right[index]))
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

static ft_bool file_relative_roots_match(const file_relative_root &from_root,
    const file_relative_root &to_root) noexcept
{
    if (from_root.kind != to_root.kind)
        return (FT_FALSE);
    if (from_root.kind == file_relative_root_kind::unc
        && (from_root.first.empty() || from_root.second.empty()
            || to_root.first.empty() || to_root.second.empty()))
        return (FT_FALSE);
    return (file_relative_equal(from_root.first, to_root.first, FT_TRUE)
        && file_relative_equal(from_root.second, to_root.second, FT_TRUE));
}

static int32_t file_relative_append_tail(ft_string &result, std::string_view tail,
    ft_size_t parent_count)
{
    int32_t error_code = result.clear();
    while (error_code == FT_ERR_SUCCESS && parent_count > 0)
    {
        error_code = result.append("../", 3);
        --parent_count;
    }
    std::string_view component = file_relative_take_component(tail);
    while (error_code == FT_ERR_SUCCESS && !component.empty())
    {
        if (!result.empty() && result.back() != '/')
            error_code = result.append('/');
        if (error_code == FT_ERR_SUCCESS)
            error_code = result.append(component.data(), component.size());
        component = file_relative_take_component(tail);
    }
    if (error_code == FT_ERR_SUCCESS && result.empty())
        error_code = result.append('.');
    return (error_code);
}

static int32_t file_relative_compute(ft_string &from, const ft_string &to)
{
    std::string_view from_tail(from.c_str(), from.size());
    std::string_view to_tail(to.c_str(), to.size());
    const file_relative_root from_root = file_relative_parse_root(from_tail);
    const file_relative_root to_root = file_relative_parse_root(to_tail);
    if (file_relative_roots_match(from_root, to_root) == FT_FALSE)
        return (FT_ERR_INVALID_PATH);
    const ft_bool case_insensitive = from_root.kind == file_relative_root_kind::drive_absolute
        || from_root.kind == file_relative_root_kind::drive_relative
        || from_root.kind == file_relative_root_kind::unc;
    ft_size_t parent_count = 0;
    while (FT_TRUE)
    {
        std::string_view next_to = to_tail;
        const std::string_view from_component = file_relative_take_component(from_tail);
        const std::string_view to_component = file_relative_take_component(next_to);
        if (from_component.empty())
            break ;
        if (file_relative_equal(from_component, to_component, case_insensitive) == FT_FALSE)
        {
            parent_count = 1;
            while (!file_relative_take_component(from_tail).empty())
                ++parent_count;
            break ;
        }
        to_tail = next_to;
    }
    return (file_relative_append_tail(from, to_tail, parent_count));
}

ft_string *file_path_relative(const char *from_path, const char *to_path)
{
    if (from_path == ft_nullptr || to_path == ft_nullptr)
        return (ft_nullptr);
    ft_string *from = file_path_normalize(from_path);
    if (from == ft_nullptr)
        return (ft_nullptr);
    ft_string *to = file_path_normalize(to_path);
    int32_t error_code = FT_ERR_NO_MEMORY;
    if (to != ft_nullptr)
    {
        error_code = file_relative_compute(*from, *to);
        delete to;
    }
    if (error_code != FT_ERR_SUCCESS)
    {
        delete from;
        return (ft_nullptr);
    }
    return (from);
}
