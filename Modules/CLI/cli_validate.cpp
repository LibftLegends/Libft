#include "cli_internal.hpp"
#include "../Basic/basic.hpp"
#include <memory>
#include <new>

struct cli_seen_name
{
    const char *name;
    uint64_t hash;
};

struct cli_seen_names
{
    cli_seen_name *entries;
    ft_size_t mask;
};

static ft_bool cli_insert_name(const cli_seen_names &seen, const char *name)
{
    constexpr uint64_t HASH_OFFSET = 14695981039346656037ULL;
    constexpr uint64_t HASH_PRIME = 1099511628211ULL;
    uint64_t hash = HASH_OFFSET;
    const char *character = name;
    while (*character != '\0')
    {
        hash = (hash ^ static_cast<uint8_t>(*character)) * HASH_PRIME;
        ++character;
    }
    ft_size_t index = hash & seen.mask;
    while (seen.entries[index].name != ft_nullptr)
    {
        if (seen.entries[index].hash == hash && ft_strcmp(seen.entries[index].name, name) == 0)
            return (FT_FALSE);
        index = (index + 1) & seen.mask;
    }
    seen.entries[index] = {name, hash};
    return (FT_TRUE);
}

template <typename Validate>
static int32_t cli_validate_with_seen_names(const cli_command *command,
    ft_size_t count, Validate validate)
{
    constexpr ft_size_t DIRECT_COMPARE_LIMIT = 8;
    constexpr ft_size_t STACK_CAPACITY = 64;
    constexpr ft_size_t MAX_CAPACITY = PTRDIFF_MAX / sizeof(cli_seen_name);
    if (count <= DIRECT_COMPARE_LIMIT)
        return (validate(command, {nullptr, 0}));
    if (count <= STACK_CAPACITY / 2)
    {
        cli_seen_name entries[STACK_CAPACITY]{};
        return (validate(command, {entries, STACK_CAPACITY - 1}));
    }
    ft_size_t capacity = STACK_CAPACITY;
    while (capacity / 2 < count)
    {
        if (capacity > MAX_CAPACITY / 2)
            return (validate(command, {nullptr, 0}));
        capacity *= 2;
    }

    std::unique_ptr<cli_seen_name[]> entries(
        new (std::nothrow) cli_seen_name[FT_UINT64_TO_NATIVE_SIZE_CAST(capacity)]());
    if (!entries)
        return (validate(command, {nullptr, 0}));
    return (validate(command, {entries.get(), capacity - 1}));
}

static ft_bool cli_is_empty_string(const char *string)
{
    return (string == ft_nullptr || string[0] == '\0');
}

static ft_bool cli_names_equal(const char *left, const char *right)
{
    while (*left == *right)
    {
        if (*left == '\0')
            return (FT_TRUE);
        ++left;
        ++right;
    }
    return (FT_FALSE);
}

static int32_t cli_validate_option_list(const cli_command *command, const cli_seen_names &seen)
{
    uint64_t short_names[4]{};
    ft_size_t index = 0;
    while (index < command->option_count)
    {
        const cli_option &option = command->options[index];
        if (cli_is_empty_string(option.long_name))
            return (FT_ERR_INVALID_ARGUMENT);
        if (option.type > CLI_OPTION_DOUBLE)
            return (FT_ERR_UNSUPPORTED_TYPE);
        const uint8_t short_name = static_cast<uint8_t>(option.short_name);
        if (short_name != 0)
        {
            const uint64_t bit = uint64_t{1} << (short_name % 64U);
            uint64_t &names = short_names[short_name / 64U];
            if ((names & bit) != 0)
                return (FT_ERR_ALREADY_EXISTS);
            names |= bit;
        }
        if (seen.entries != ft_nullptr)
        {
            if (!cli_insert_name(seen, option.long_name))
                return (FT_ERR_ALREADY_EXISTS);
        }
        else
        {
            ft_size_t previous = 0;
            while (previous < index)
            {
                if (cli_names_equal(command->options[previous].long_name, option.long_name))
                    return (FT_ERR_ALREADY_EXISTS);
                ++previous;
            }
        }
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t cli_validate_subcommand_list(const cli_command *command, const cli_seen_names &seen)
{
    ft_bool selected_found = command->selected_subcommand == ft_nullptr;
    ft_size_t index = 0;
    while (index < command->subcommand_count)
    {
        const cli_command *subcommand = command->subcommands + index;
        if (cli_is_empty_string(subcommand->name))
            return (FT_ERR_INVALID_ARGUMENT);
        if (seen.entries != ft_nullptr)
        {
            if (!cli_insert_name(seen, subcommand->name))
                return (FT_ERR_ALREADY_EXISTS);
        }
        else
        {
            ft_size_t previous = 0;
            while (previous < index)
            {
                if (cli_names_equal(command->subcommands[previous].name, subcommand->name))
                    return (FT_ERR_ALREADY_EXISTS);
                ++previous;
            }
        }
        const int32_t error = cli_validate(subcommand);
        if (error != FT_ERR_SUCCESS)
            return (error);
        if (subcommand == command->selected_subcommand)
            selected_found = FT_TRUE;
        ++index;
    }
    if (!selected_found)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

int32_t cli_validate(const cli_command *command)
{
    if (command == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (cli_is_empty_string(command->name))
        return (FT_ERR_INVALID_ARGUMENT);
    if ((command->positional_capacity != 0 && command->positionals == ft_nullptr)
        || (command->option_count != 0 && command->options == ft_nullptr))
        return (FT_ERR_INVALID_POINTER);
    const int32_t error = cli_validate_with_seen_names(command, command->option_count,
        cli_validate_option_list);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (command->subcommand_count != 0 && command->subcommands == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    return (cli_validate_with_seen_names(command, command->subcommand_count,
        cli_validate_subcommand_list));
}
