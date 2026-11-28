#include "cli_internal.hpp"
#include "../Basic/basic.hpp"

static int32_t cli_append_identifier_fragment(char *buffer, ft_size_t buffer_size,
    ft_size_t *used, const char *string)
{
    ft_size_t index;
    char character;
    ft_bool valid_character;

    if (string == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    index = 0;
    while (string[index] != '\0')
    {
        character = string[index];
        valid_character = FT_FALSE;
        if ((character >= 'a' && character <= 'z')
            || (character >= 'A' && character <= 'Z')
            || (character >= '0' && character <= '9')
            || character == '_')
        {
            valid_character = FT_TRUE;
        }
        if (valid_character == FT_FALSE)
        {
            character = '_';
        }
        if (*used + 1 >= buffer_size)
        {
            buffer[*used] = '\0';
            return (FT_ERR_OUT_OF_RANGE);
        }
        buffer[*used] = character;
        *used = *used + 1;
        index++;
    }
    buffer[*used] = '\0';
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

static int32_t cli_append_command_candidates(const cli_command *command,
    char *buffer, ft_size_t buffer_size, ft_size_t *used)
{
    ft_size_t index;
    int32_t error_code;
    if (command == ft_nullptr)
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, "--help -h");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    index = 0;
    while (index < command->option_count)
    {
        if (command->options[index].short_name != '\0')
        {
            error_code = cli_append_raw(buffer, buffer_size, used, " ");
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
            if (*used + 2 >= buffer_size)
            {
                buffer[*used] = '\0';
                return (FT_ERR_OUT_OF_RANGE);
            }
            buffer[*used] = '-';
            *used = *used + 1;
            buffer[*used] = command->options[index].short_name;
            *used = *used + 1;
            buffer[*used] = '\0';
        }
        error_code = cli_append_raw(buffer, buffer_size, used, " ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used, "--");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used,
                command->options[index].long_name);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        if (command->options[index].type == CLI_OPTION_BOOL)
        {
            error_code = cli_append_raw(buffer, buffer_size, used, " --no-");
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
            error_code = cli_append_raw(buffer, buffer_size, used,
                    command->options[index].long_name);
            if (error_code != FT_ERR_SUCCESS)
            {
                return (error_code);
            }
        }
        index++;
    }
    index = 0;
    while (index < command->subcommand_count)
    {
        error_code = cli_append_raw(buffer, buffer_size, used, " ");
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        error_code = cli_append_raw(buffer, buffer_size, used,
                command->subcommands[index].name);
        if (error_code != FT_ERR_SUCCESS)
        {
            return (error_code);
        }
        index++;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_case_entry(const char *path, const cli_command *command,
    char *buffer, ft_size_t buffer_size, ft_size_t *used)
{
    int32_t error_code;

    error_code = cli_append_raw(buffer, buffer_size, used, "        \"");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, path);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used, "\")\n            COMPREPLY=( $(compgen -W \"");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_command_candidates(command, buffer, buffer_size, used);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, used,
            "\" -- \"$cur\") )\n            ;;\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_append_case_entries_recursive(const cli_command *command,
    const char *path_prefix, ft_size_t prefix_length, char *buffer,
    ft_size_t buffer_size, ft_size_t *used)
{
    if (command == ft_nullptr || used == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    int32_t error_code = cli_append_case_entry(path_prefix, command, buffer,
        buffer_size, used);
    if (error_code != FT_ERR_SUCCESS || command->subcommand_count == 0)
        return (error_code);

    char child_path[256];
    ft_size_t name_offset = 0;
    if (prefix_length != 0)
    {
        if (prefix_length >= sizeof(child_path) - 1)
            return (FT_ERR_OUT_OF_RANGE);
        ft_memcpy(child_path, path_prefix, prefix_length);
        child_path[prefix_length] = '.';
        name_offset = prefix_length + 1;
    }
    const ft_size_t name_capacity = sizeof(child_path) - name_offset;
    ft_size_t index = 0;
    while (index < command->subcommand_count)
    {
        const cli_command *child = command->subcommands + index;
        const char *name = child->name;
        if (name == ft_nullptr)
            return (FT_ERR_INVALID_ARGUMENT);
        const ft_size_t name_length = ft_strnlen(name, name_capacity);
        if (name_length == name_capacity)
            return (FT_ERR_OUT_OF_RANGE);
        ft_memcpy(child_path + name_offset, name, name_length + 1);
        error_code = cli_append_case_entries_recursive(child, child_path,
            name_offset + name_length, buffer, buffer_size, used);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

int32_t cli_generate_completion_script(const cli_command *command,
    const char *program_name, char *buffer, ft_size_t buffer_size)
{
    ft_size_t used;
    int32_t error_code;
    const char *function_name;

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
    function_name = program_name;
    if (function_name == ft_nullptr || function_name[0] == '\0')
    {
        function_name = "libft";
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, "#!/usr/bin/env bash\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, "_");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_identifier_fragment(buffer, buffer_size, &used,
            function_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used,
            "_completion()\n{\n    local cur\n    local command_path\n    local index\n    local word\n    COMPREPLY=()\n    cur=\"${COMP_WORDS[COMP_CWORD]}\"\n    command_path=\"\"\n    index=1\n    while [ \"$index\" -lt \"$COMP_CWORD\" ]\n    do\n        word=\"${COMP_WORDS[$index]}\"\n        case \"$word\" in\n            -*)\n                ;;\n            *)\n                if [ -z \"$command_path\" ]\n                then\n                    command_path=\"$word\"\n                else\n                    command_path=\"$command_path.$word\"\n                fi\n                ;;\n        esac\n        index=$((index + 1))\n    done\n    case \"$command_path\" in\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_case_entries_recursive(command, "", 0, buffer, buffer_size,
            &used);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used,
            "    esac\n}\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, "complete -F _");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_identifier_fragment(buffer, buffer_size, &used,
            function_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, "_completion ");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, function_name);
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    error_code = cli_append_raw(buffer, buffer_size, &used, "\n");
    if (error_code != FT_ERR_SUCCESS)
    {
        return (error_code);
    }
    return (FT_ERR_SUCCESS);
}
