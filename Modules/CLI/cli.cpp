#include "cli_internal.hpp"
#include "../CPP_class/class_string.hpp"
#include <cstdlib>
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool cli_streq(const char *left, const char *right)
{
    ft_size_t index;

    if (left == ft_nullptr || right == ft_nullptr)
    {
        return (FT_FALSE);
    }
    index = 0;
    while (left[index] != '\0' && right[index] != '\0')
    {
        if (left[index] != right[index])
        {
            return (FT_FALSE);
        }
        index++;
    }
    if (left[index] == right[index])
    {
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static ft_bool cli_is_digit(char character)
{
    if (character >= '0' && character <= '9')
    {
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static ft_bool cli_has_prefix(const char *string, const char *prefix)
{
    ft_size_t index;

    if (string == ft_nullptr || prefix == ft_nullptr)
    {
        return (FT_FALSE);
    }
    index = 0;
    while (prefix[index] != '\0')
    {
        if (string[index] != prefix[index])
        {
            return (FT_FALSE);
        }
        index++;
    }
    return (FT_TRUE);
}

static const char *cli_find_equal(const char *string)
{
    ft_size_t index;

    if (string == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (string[index] != '\0')
    {
        if (string[index] == '=')
        {
            return (string + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static ft_bool cli_bool_from_string(const char *value, ft_bool *result)
{
    if (value == ft_nullptr || result == ft_nullptr)
    {
        return (FT_FALSE);
    }
    if (cli_streq(value, "1") == FT_TRUE || cli_streq(value, "true") == FT_TRUE
        || cli_streq(value, "yes") == FT_TRUE || cli_streq(value, "on") == FT_TRUE)
    {
        *result = FT_TRUE;
        return (FT_TRUE);
    }
    if (cli_streq(value, "0") == FT_TRUE || cli_streq(value, "false") == FT_TRUE
        || cli_streq(value, "no") == FT_TRUE || cli_streq(value, "off") == FT_TRUE)
    {
        *result = FT_FALSE;
        return (FT_TRUE);
    }
    return (FT_FALSE);
}

static ft_bool cli_int64_from_string(const char *value, int64_t *result)
{
    if (value == ft_nullptr || result == ft_nullptr
        || (cli_is_digit(value[0]) == FT_FALSE && value[0] != '+' && value[0] != '-'))
        return (FT_FALSE);
    char *end = ft_nullptr;
    int64_t parsed_value;
    if (ft_parse_int64(value, &end, &parsed_value) != FT_ERR_SUCCESS || *end != '\0')
        return (FT_FALSE);
    *result = parsed_value;
    return (FT_TRUE);
}

static ft_bool cli_uint64_from_string(const char *value, uint64_t *result)
{
    constexpr uint64_t CUTOFF = UINT64_MAX / 10U;
    constexpr uint64_t LAST_DIGIT = UINT64_MAX % 10U;
    if (value == ft_nullptr || result == ft_nullptr || value[0] == '\0')
        return (FT_FALSE);
    uint64_t parsed_value = 0;
    ft_size_t index = 0;
    while (value[index] != '\0')
    {
        if (cli_is_digit(value[index]) == FT_FALSE)
            return (FT_FALSE);
        const auto digit = static_cast<uint64_t>(value[index] - '0');
        if (parsed_value > CUTOFF || (parsed_value == CUTOFF && digit > LAST_DIGIT))
            return (FT_FALSE);
        parsed_value = parsed_value * 10U + digit;
        ++index;
    }
    *result = parsed_value;
    return (FT_TRUE);
}

static ft_bool cli_double_from_string(const char *value, double *result)
{
    char *end_pointer;

    if (value == ft_nullptr || result == ft_nullptr || value[0] == '\0')
    {
        return (FT_FALSE);
    }
    end_pointer = ft_nullptr;
    *result = std::strtod(value, &end_pointer);
    if (end_pointer == value || *end_pointer != '\0')
    {
        return (FT_FALSE);
    }
    return (FT_TRUE);
}

int32_t cli_apply_option_value(cli_option *option, const char *value) noexcept
{
    ft_string *old_storage;

    if (option == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    old_storage = option->value_string_storage;
    if (option->type == CLI_OPTION_BOOL)
    {
        if (value == ft_nullptr)
        {
            option->value_bool = FT_TRUE;
            option->value_string = "true";
            option->present = FT_TRUE;
            if (old_storage != ft_nullptr)
            {
                (void)old_storage->destroy();
                delete old_storage;
            }
            option->value_string_storage = ft_nullptr;
            return (FT_ERR_SUCCESS);
        }
        if (cli_bool_from_string(value, &option->value_bool) == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
    }
    else if (option->type == CLI_OPTION_STRING)
    {
        if (value == ft_nullptr)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        option->value_string = value;
        option->present = FT_TRUE;
        if (old_storage != ft_nullptr)
        {
            (void)old_storage->destroy();
            delete old_storage;
        }
        option->value_string_storage = ft_nullptr;
        return (FT_ERR_SUCCESS);
    }
    else if (option->type == CLI_OPTION_INT64)
    {
        if (cli_int64_from_string(value, &option->value_int64) == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
    }
    else if (option->type == CLI_OPTION_UINT64)
    {
        if (cli_uint64_from_string(value, &option->value_uint64) == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
    }
    else if (option->type == CLI_OPTION_DOUBLE)
    {
        if (cli_double_from_string(value, &option->value_double) == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
    }
    else
    {
        return (FT_ERR_UNSUPPORTED_TYPE);
    }
    option->value_string = value;
    option->present = FT_TRUE;
    if (old_storage != ft_nullptr)
    {
        (void)old_storage->destroy();
        delete old_storage;
    }
    option->value_string_storage = ft_nullptr;
    return (FT_ERR_SUCCESS);
}

static void cli_reset_command(cli_command *command)
{
    ft_size_t index;

    if (command == ft_nullptr)
    {
        return ;
    }
    command->selected_subcommand = ft_nullptr;
    command->help_requested = FT_FALSE;
    command->positional_count = 0;
    index = 0;
    while (index < command->option_count)
    {
        if (command->options[index].value_string_storage != ft_nullptr)
        {
            (void)command->options[index].value_string_storage->destroy();
            delete command->options[index].value_string_storage;
            command->options[index].value_string_storage = ft_nullptr;
        }
        command->options[index].present = FT_FALSE;
        command->options[index].value_string = ft_nullptr;
        command->options[index].value_bool = FT_FALSE;
        command->options[index].value_int64 = 0;
        command->options[index].value_uint64 = 0U;
        command->options[index].value_double = 0.0;
        index++;
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        cli_reset_command(command->subcommands + index);
        index++;
    }
    return ;
}

cli_option *cli_find_option(cli_command *command, const char *long_name)
{
    return (const_cast<cli_option *>(cli_find_option_const(command, long_name)));
}

const cli_option *cli_find_option_const(const cli_command *command,
    const char *long_name)
{
    ft_size_t index;

    if (command == ft_nullptr || long_name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->option_count)
    {
        if (cli_streq(command->options[index].long_name, long_name) == FT_TRUE)
        {
            return (command->options + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static cli_option *cli_find_short_option(cli_command *command, char short_name)
{
    ft_size_t index;

    if (command == ft_nullptr || short_name == '\0')
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->option_count)
    {
        if (command->options[index].short_name == short_name)
        {
            return (command->options + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static cli_option *cli_find_long_option_with_length(cli_command *command,
    const char *long_name, ft_size_t name_length)
{
    if (command == ft_nullptr || long_name == ft_nullptr)
        return (ft_nullptr);
    ft_size_t index = 0;
    while (index < command->option_count)
    {
        const char *candidate = command->options[index].long_name;
        if (candidate != ft_nullptr && ft_strncmp(candidate, long_name, name_length) == 0
            && candidate[name_length] == '\0')
            return (command->options + index);
        ++index;
    }
    return (ft_nullptr);
}

cli_command *cli_find_subcommand(cli_command *command, const char *name) noexcept
{
    ft_size_t index;

    if (command == ft_nullptr || name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        if (cli_streq(command->subcommands[index].name, name) == FT_TRUE)
        {
            return (command->subcommands + index);
        }
        index++;
    }
    return (ft_nullptr);
}

const cli_command *cli_find_subcommand(const cli_command *command,
    const char *name) noexcept
{
    ft_size_t index;

    if (command == ft_nullptr || name == ft_nullptr)
    {
        return (ft_nullptr);
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        if (cli_streq(command->subcommands[index].name, name) == FT_TRUE)
        {
            return (command->subcommands + index);
        }
        index++;
    }
    return (ft_nullptr);
}

static int32_t cli_apply_defaults(cli_command *command)
{
    ft_size_t index;
    const char *value;
    int32_t error_code;

    if (command == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    index = 0;
    while (index < command->option_count)
    {
        if (command->options[index].present == FT_FALSE)
        {
            value = ft_nullptr;
            if (command->options[index].env_name != ft_nullptr)
            {
                value = std::getenv(command->options[index].env_name);
            }
            if (value == ft_nullptr)
            {
                value = command->options[index].default_value;
            }
            if (value != ft_nullptr)
            {
                error_code = cli_apply_option_value(command->options + index, value);
                if (error_code != FT_ERR_SUCCESS)
                {
                    return (error_code);
                }
            }
        }
        if (command->options[index].required == FT_TRUE
            && command->options[index].present == FT_FALSE)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        index++;
    }
    return (FT_ERR_SUCCESS);
}

int32_t cli_apply_defaults_recursive(cli_command *command) noexcept
{
    int32_t error_code;

    if (command == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_apply_defaults(command);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    if (command->selected_subcommand != ft_nullptr)
    {
        error_code = cli_apply_defaults_recursive(command->selected_subcommand);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_store_positional(cli_command *command, const char *value)
{
    if (command == ft_nullptr || value == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    if (command->positionals == ft_nullptr)
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    if (command->positional_count >= command->positional_capacity)
    {
        return (FT_ERR_OUT_OF_RANGE);
    }
    command->positionals[command->positional_count] = value;
    command->positional_count++;
    return (FT_ERR_SUCCESS);
}

static int32_t cli_parse_long_option(cli_command *command, ft_size_t *argument_index,
    ft_size_t argument_count, const char *const *argument_values)
{
    const char *token;
    const char *option_name;
    const char *value;
    const char *equal_sign;
    cli_option *option;
    ft_size_t option_name_length;

    token = argument_values[*argument_index];
    option_name = token + 2;
    value = ft_nullptr;
    if (cli_has_prefix(option_name, "no-") == FT_TRUE)
    {
        option = cli_find_option(command, option_name + 3);
        if (option == ft_nullptr || option->type != CLI_OPTION_BOOL)
        {
            return (FT_ERR_NOT_FOUND);
        }
        return (cli_apply_option_value(option, "false"));
    }
    equal_sign = cli_find_equal(option_name);
    if (equal_sign != ft_nullptr)
    {
        value = equal_sign + 1;
        option_name_length = static_cast<ft_size_t>(equal_sign - option_name);
        option = cli_find_long_option_with_length(command, option_name,
                option_name_length);
    }
    else
    {
        option = cli_find_option(command, option_name);
    }
    if (option == ft_nullptr)
    {
        return (FT_ERR_NOT_FOUND);
    }
    if (value == ft_nullptr && option->type != CLI_OPTION_BOOL)
    {
        if (*argument_index + 1 >= argument_count)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        *argument_index = *argument_index + 1;
        value = argument_values[*argument_index];
    }
    return (cli_apply_option_value(option, value));
}

static int32_t cli_parse_short_option(cli_command *command, ft_size_t *argument_index,
    ft_size_t argument_count, const char *const *argument_values)
{
    const char *token;
    const char *value;
    cli_option *option;

    token = argument_values[*argument_index];
    if (token[1] == '\0' || token[2] != '\0')
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    option = cli_find_short_option(command, token[1]);
    if (option == ft_nullptr)
    {
        return (FT_ERR_NOT_FOUND);
    }
    value = ft_nullptr;
    if (option->type != CLI_OPTION_BOOL)
    {
        if (*argument_index + 1 >= argument_count)
        {
            return (FT_ERR_INVALID_ARGUMENT);
        }
        *argument_index = *argument_index + 1;
        value = argument_values[*argument_index];
    }
    return (cli_apply_option_value(option, value));
}

int32_t cli_parse(cli_command *command, ft_size_t argument_count,
    const char *const *argument_values)
{
    ft_size_t argument_index;
    cli_command *current_command;
    cli_command *subcommand;
    int32_t error_code;

    if (command == ft_nullptr || (argument_count > 0 && argument_values == ft_nullptr))
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_validate(command);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    cli_reset_command(command);
    current_command = command;
    argument_index = 1;
    while (argument_index < argument_count)
    {
        if (cli_streq(argument_values[argument_index], "--help") == FT_TRUE
            || cli_streq(argument_values[argument_index], "-h") == FT_TRUE)
        {
            current_command->help_requested = FT_TRUE;
        }
        else if (cli_has_prefix(argument_values[argument_index], "--") == FT_TRUE)
        {
            error_code = cli_parse_long_option(current_command, &argument_index,
                    argument_count, argument_values);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
        }
        else if (cli_has_prefix(argument_values[argument_index], "-") == FT_TRUE)
        {
            error_code = cli_parse_short_option(current_command, &argument_index,
                    argument_count, argument_values);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
        }
        else
        {
            subcommand = cli_find_subcommand(current_command,
                    argument_values[argument_index]);
            if (subcommand != ft_nullptr)
            {
                current_command->selected_subcommand = subcommand;
                current_command = subcommand;
            }
            else
            {
                error_code = cli_store_positional(current_command,
                        argument_values[argument_index]);
                if (error_code != FT_ERR_SUCCESS)
                {
                    return (error_code);
                }
            }
        }
        argument_index++;
    }
    error_code = cli_apply_defaults_recursive(command);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_raw(char *buffer, ft_size_t buffer_size, ft_size_t *used,
    const char *string)
{
    ft_size_t index;

    if (buffer == ft_nullptr || used == ft_nullptr || string == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    index = 0;
    while (string[index] != '\0')
    {
        if (*used + 1 >= buffer_size)
        {
            buffer[*used] = '\0';
            return (FT_ERR_OUT_OF_RANGE);
        }
        buffer[*used] = string[index];
        *used = *used + 1;
        index++;
    }
    buffer[*used] = '\0';
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_optional_raw(char *buffer, ft_size_t buffer_size,
    ft_size_t *used, const char *string)
{
    if (string == ft_nullptr)
    {
        return (cli_append_raw(buffer, buffer_size, used, ""));
    }
    return (cli_append_raw(buffer, buffer_size, used, string));
}

static int32_t cli_append_indent(char *buffer, ft_size_t buffer_size,
    ft_size_t *used, ft_size_t indent_level)
{
    ft_size_t index;
    int32_t error_code;

    index = 0;
    while (index < indent_level)
    {
        error_code = cli_append_raw(buffer, buffer_size, used, "  ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        index++;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_option_type_label(char *buffer, ft_size_t buffer_size,
    ft_size_t *used, uint8_t type)
{
    if (type == CLI_OPTION_BOOL)
    {
        return (cli_append_raw(buffer, buffer_size, used, "bool"));
    }
    if (type == CLI_OPTION_STRING)
    {
        return (cli_append_raw(buffer, buffer_size, used, "string"));
    }
    if (type == CLI_OPTION_INT64)
    {
        return (cli_append_raw(buffer, buffer_size, used, "int64"));
    }
    if (type == CLI_OPTION_UINT64)
    {
        return (cli_append_raw(buffer, buffer_size, used, "uint64"));
    }
    if (type == CLI_OPTION_DOUBLE)
    {
        return (cli_append_raw(buffer, buffer_size, used, "double"));
    }
    return (cli_append_raw(buffer, buffer_size, used, "unknown"));
}

static int32_t cli_append_option_detail_line(char *buffer, ft_size_t buffer_size,
    ft_size_t *used, const cli_option *option)
{
    int32_t error_code;

    if (option == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_append_indent(buffer, buffer_size, used, 1);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    if (option->short_name != '\0')
    {
        error_code = cli_append_raw(buffer, buffer_size, used, "-");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        if (*used + 1 >= buffer_size)
        {
            buffer[*used] = '\0';
            return (FT_ERR_OUT_OF_RANGE);
        }
        buffer[*used] = option->short_name;
        *used = *used + 1;
        buffer[*used] = '\0';
        error_code = cli_append_raw(buffer, buffer_size, used, ", ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    error_code = cli_append_raw(buffer, buffer_size, used, "--");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, option->long_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, " [");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_option_type_label(buffer, buffer_size, used, option->type);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, "]");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    if (option->required == FT_TRUE)
    {
        error_code = cli_append_raw(buffer, buffer_size, used, " [required]");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (option->env_name != ft_nullptr && option->env_name[0] != '\0')
    {
        error_code = cli_append_raw(buffer, buffer_size, used, " [env: ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, option->env_name);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, "]");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (option->default_value != ft_nullptr && option->default_value[0] != '\0')
    {
        error_code = cli_append_raw(buffer, buffer_size, used, " [default: ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, option->default_value);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, "]");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (option->help != ft_nullptr && option->help[0] != '\0')
    {
        error_code = cli_append_raw(buffer, buffer_size, used, " - ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, option->help);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    error_code = cli_append_raw(buffer, buffer_size, used, "\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_command_tree_recursive(const cli_command *command,
    char *buffer, ft_size_t buffer_size, ft_size_t *used, ft_size_t depth)
{
    ft_size_t index;
    int32_t error_code;

    if (command == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        error_code = cli_append_indent(buffer, buffer_size, used, depth);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, "- ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_optional_raw(buffer, buffer_size, used,
                command->subcommands[index].name);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        if (command->subcommands[index].description != ft_nullptr
            && command->subcommands[index].description[0] != '\0')
        {
            error_code = cli_append_raw(buffer, buffer_size, used, " - ");
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
            error_code = cli_append_raw(buffer, buffer_size, used,
                    command->subcommands[index].description);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
        }
        error_code = cli_append_raw(buffer, buffer_size, used, "\n");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        if (command->subcommands[index].subcommand_count > 0)
        {
            error_code = cli_append_command_tree_recursive(command->subcommands + index,
                    buffer, buffer_size, used, depth + 1);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
        }
        index++;
    }
    return (FT_ERR_SUCCESS);
}

int32_t cli_format_help(const cli_command *command, const char *program_name,
    char *buffer, ft_size_t buffer_size)
{
    ft_size_t used;
    ft_size_t index;
    int32_t error_code;

    if (command == ft_nullptr || buffer == ft_nullptr || buffer_size == 0)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_validate(command);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    used = 0;
    buffer[0] = '\0';
    error_code = cli_append_raw(buffer, buffer_size, &used, "Usage: ");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_optional_raw(buffer, buffer_size, &used, program_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    if (command->name != ft_nullptr && command->name[0] != '\0'
        && (program_name == ft_nullptr || cli_streq(program_name, command->name) == FT_FALSE))
    {
        error_code = cli_append_raw(buffer, buffer_size, &used, " ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, &used, command->name);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (command->subcommand_count > 0)
    {
        error_code = cli_append_raw(buffer, buffer_size, &used, " <command>");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, " [options]\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    if (command->description != ft_nullptr && command->description[0] != '\0')
    {
        error_code = cli_append_raw(buffer, buffer_size, &used, "\n");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, &used, command->description);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, &used, "\n");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (command->subcommand_count > 0)
    {
        error_code = cli_append_raw(buffer, buffer_size, &used, "\nCommands:\n");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_command_tree_recursive(command, buffer, buffer_size,
                &used, 1);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
    }
    if (command->option_count > 0)
    {
        error_code = cli_append_raw(buffer, buffer_size, &used, "\nOptions:\n");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        index = 0;
        while (index < command->option_count)
        {
            error_code = cli_append_option_detail_line(buffer, buffer_size, &used,
                    command->options + index);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
            index++;
        }
    }
    return (FT_ERR_SUCCESS);
}
