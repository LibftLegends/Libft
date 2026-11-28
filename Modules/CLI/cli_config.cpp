#include "cli_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CPP_class/class_string.hpp"
#include "../File/file_utils.hpp"
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool cli_is_space(char character)
{
    if (character == ' ' || character == '\t' || character == '\n'
        || character == '\r' || character == '\f' || character == '\v')
    {
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static const char *cli_skip_leading_space(const char *string)
{
    if (string == ft_nullptr)
    {
        return (ft_nullptr);
    }
    while (*string != '\0' && cli_is_space(*string) == FT_TRUE)
    {
        string++;
    }
    return (string);
}

static const char *cli_skip_trailing_space(const char *begin, const char *end)
{
    if (begin == ft_nullptr || end == ft_nullptr)
    {
        return (ft_nullptr);
    }
    while (end > begin && cli_is_space(*(end - 1)) == FT_TRUE)
    {
        end--;
    }
    return (end);
}

static cli_option *cli_find_option_with_length(cli_command *command,
    const char *name, ft_size_t length)
{
    ft_size_t index;

    if (command == ft_nullptr || name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->option_count)
    {
        const char *candidate = command->options[index].long_name;
        if (candidate != ft_nullptr && ft_strncmp(candidate, name, length) == 0
            && candidate[length] == '\0')
        {
            return (command->options + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static cli_command *cli_find_subcommand_with_length(cli_command *command,
    const char *name, ft_size_t length)
{
    ft_size_t index;

    if (command == ft_nullptr || name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        const char *candidate = command->subcommands[index].name;
        if (candidate != ft_nullptr && ft_strncmp(candidate, name, length) == 0
            && candidate[length] == '\0')
        {
            return (command->subcommands + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static cli_command *cli_find_command_path(cli_command *command, const char *path,
    ft_size_t length)
{
    ft_size_t index;
    ft_size_t segment_start;
    ft_size_t segment_length;
    cli_command *current_command;
    cli_command *subcommand;

    if (command == ft_nullptr || path == ft_nullptr)
    {
        return (ft_nullptr);
    }
    current_command = command;
    segment_start = 0;
    index = 0;
    while (index <= length)
    {
        if (index == length || path[index] == '.')
        {
            if (index == segment_start)
            {
                return (ft_nullptr);
            }
            segment_length = index - segment_start;
            subcommand = cli_find_subcommand_with_length(current_command,
                    path + segment_start, segment_length);
            if (subcommand == ft_nullptr)
            {
                return (ft_nullptr);
            }
            current_command = subcommand;
            segment_start = index + 1;
        }
        index++;
    }
    return (current_command);
}

static int32_t cli_store_string_option(cli_option *option, const char *value_start,
    ft_size_t value_length)
{
    ft_string *storage;
    ft_string *old_storage;
    int32_t error_code;

    if (option == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    old_storage = option->value_string_storage;
    storage = new (std::nothrow) ft_string();
    if (storage == ft_nullptr)
    {
        return (FT_ERR_NO_MEMORY);
    }
    error_code = storage->initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)storage->destroy();
        delete storage;
        return (error_code);
    }
    error_code = storage->assign(value_start, value_length);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)storage->destroy();
        delete storage;
        return (error_code);
    }
    if (old_storage != ft_nullptr)
    {
        (void)old_storage->destroy();
        delete old_storage;
    }
    option->value_string_storage = storage;
    option->value_string = storage->c_str();
    option->present = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static int32_t cli_apply_config_value(cli_option *option, const char *value_start,
    ft_size_t value_length, ft_bool has_explicit_value)
{
    ft_string temporary_value;
    int32_t error_code;

    if (option == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (option->type == CLI_OPTION_STRING)
    {
        if (has_explicit_value == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        return (cli_store_string_option(option, value_start, value_length));
    }
    if (has_explicit_value == FT_FALSE)
    {
        if (option->type != CLI_OPTION_BOOL)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        return (cli_apply_option_value(option, ft_nullptr));
    }
    error_code = temporary_value.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = temporary_value.assign(value_start, value_length);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)temporary_value.destroy();
        return (error_code);
    }
    error_code = cli_apply_option_value(option, temporary_value.c_str());
    if (error_code == FT_ERR_SUCCESS && option->type != CLI_OPTION_STRING)
    {
        option->value_string = ft_nullptr;
    }
    (void)temporary_value.destroy();
    return (error_code);
}

static int32_t cli_merge_config_line(cli_command *command, const char *line_start,
    const char *line_end)
{
    const char *trimmed_start;
    const char *trimmed_end;
    const char *equal_sign;
    const char *key_end;
    const char *value_start;
    const char *value_end;
    const char *last_dot;
    cli_command *target_command;
    cli_option *option;
    ft_size_t key_length;
    ft_size_t option_length;

    trimmed_start = cli_skip_leading_space(line_start);
    trimmed_end = cli_skip_trailing_space(trimmed_start, line_end);
    if (trimmed_start == ft_nullptr || trimmed_end == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (trimmed_start >= trimmed_end)
    {
        return (FT_ERR_SUCCESS);
    }
    if (*trimmed_start == '#' || *trimmed_start == ';')
    {
        return (FT_ERR_SUCCESS);
    }
    equal_sign = ft_nullptr;
    last_dot = ft_nullptr;
    line_start = trimmed_start;
    while (line_start < trimmed_end)
    {
        if (*line_start == '=')
        {
            equal_sign = line_start;
            break ;
        }
        if (*line_start == '.')
            last_dot = line_start;
        line_start++;
    }
    key_end = trimmed_end;
    value_start = ft_nullptr;
    value_end = ft_nullptr;
    if (equal_sign != ft_nullptr)
    {
        key_end = cli_skip_trailing_space(trimmed_start, equal_sign);
        value_start = cli_skip_leading_space(equal_sign + 1);
        value_end = cli_skip_trailing_space(value_start, trimmed_end);
        if (key_end == ft_nullptr || value_start == ft_nullptr || value_end == ft_nullptr)
        {
            return (FT_ERR_INVALID_POINTER);
        }
    }
    if (key_end <= trimmed_start)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (last_dot != ft_nullptr)
    {
        key_length = static_cast<ft_size_t>(last_dot - trimmed_start);
        target_command = cli_find_command_path(command, trimmed_start, key_length);
        if (target_command == ft_nullptr)
        {
            return (FT_ERR_NOT_FOUND);
        }
        option_length = static_cast<ft_size_t>(key_end - last_dot - 1);
        if (option_length == 0)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        option = cli_find_option_with_length(target_command, last_dot + 1, option_length);
    }
    else
    {
        target_command = command;
        option = cli_find_option_with_length(target_command, trimmed_start,
                static_cast<ft_size_t>(key_end - trimmed_start));
    }
    if (option == ft_nullptr)
    {
        return (FT_ERR_NOT_FOUND);
    }
    if (equal_sign == ft_nullptr)
    {
        return (cli_apply_config_value(option, ft_nullptr, 0, FT_FALSE));
    }
    if (value_start > value_end)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    return (cli_apply_config_value(option, value_start,
            static_cast<ft_size_t>(value_end - value_start), FT_TRUE));
}

int32_t cli_merge_config_text(cli_command *command, const char *config_text)
{
    const char *line_start;
    const char *line_end;
    int32_t error_code;

    if (command == ft_nullptr || config_text == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_validate(command);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    line_start = config_text;
    while (*line_start != '\0')
    {
        line_end = line_start;
        while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r')
        {
            line_end++;
        }
        error_code = cli_merge_config_line(command, line_start, line_end);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        while (*line_end == '\r' || *line_end == '\n')
        {
            line_end++;
        }
        line_start = line_end;
    }
    return (FT_ERR_SUCCESS);
}

int32_t cli_merge_config_file(cli_command *command, const char *path)
{
    ft_string file_contents;
    int32_t error_code;

    if (command == ft_nullptr || path == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = file_contents.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = file_read_all(path, file_contents);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)file_contents.destroy();
        return (error_code);
    }
    error_code = cli_merge_config_text(command, file_contents.c_str());
    (void)file_contents.destroy();
    return (error_code);
}
