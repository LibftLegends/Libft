#include "../test_internal.hpp"
#include "../../Modules/CLI/cli.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"

static void reset_option(cli_option *option, const char *long_name, char short_name,
    uint8_t type)
{
    option->long_name = long_name;
    option->short_name = short_name;
    option->type = type;
    option->required = FT_FALSE;
    option->env_name = ft_nullptr;
    option->help = "";
    option->default_value = ft_nullptr;
    option->present = FT_FALSE;
    option->value_string = ft_nullptr;
    option->value_string_storage = ft_nullptr;
    option->value_bool = FT_FALSE;
    option->value_int64 = 0;
    option->value_uint64 = 0U;
    option->value_double = 0.0;
    return ;
}

static void reset_command(cli_command *command, const char *name,
    cli_option *options, ft_size_t option_count)
{
    command->name = name;
    command->description = "";
    command->options = options;
    command->option_count = option_count;
    command->subcommands = ft_nullptr;
    command->subcommand_count = 0;
    command->positionals = ft_nullptr;
    command->positional_capacity = 0;
    command->positional_count = 0;
    command->selected_subcommand = ft_nullptr;
    command->help_requested = FT_FALSE;
    return ;
}

FT_TEST(test_cli_parse_typed_long_and_short_options)
{
    cli_option options[4];
    cli_command command;
    const char *arguments[7];

    reset_option(options + 0, "verbose", 'v', CLI_OPTION_BOOL);
    reset_option(options + 1, "name", 'n', CLI_OPTION_STRING);
    reset_option(options + 2, "count", 'c', CLI_OPTION_UINT64);
    reset_option(options + 3, "ratio", 'r', CLI_OPTION_DOUBLE);
    reset_command(&command, "tool", options, 4);
    arguments[0] = "tool";
    arguments[1] = "--verbose";
    arguments[2] = "--name=demo";
    arguments[3] = "-c";
    arguments[4] = "42";
    arguments[5] = "--ratio";
    arguments[6] = "2.5";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 7, arguments));
    FT_ASSERT_EQ(FT_TRUE, options[0].value_bool);
    FT_ASSERT_EQ(FT_TRUE, options[1].present);
    FT_ASSERT_EQ('d', options[1].value_string[0]);
    FT_ASSERT_EQ(static_cast<uint64_t>(42U), options[2].value_uint64);
    FT_ASSERT(options[3].value_double > 2.49);
    FT_ASSERT(options[3].value_double < 2.51);
    return (1);
}

FT_TEST(test_cli_parse_inline_long_option_requires_exact_match)
{
    cli_option options[4];
    cli_command command;
    reset_option(options + 0, "tar", '\0', CLI_OPTION_STRING);
    reset_option(options + 1, "target-more", '\0', CLI_OPTION_STRING);
    reset_option(options + 2, "targez", '\0', CLI_OPTION_STRING);
    reset_option(options + 3, "target", '\0', CLI_OPTION_STRING);
    reset_command(&command, "tool", options, 4);
    const char *arguments[] = {"tool", "--target=world=backup"};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 2, arguments));
    FT_ASSERT_EQ(FT_FALSE, options[0].present);
    FT_ASSERT_EQ(FT_FALSE, options[1].present);
    FT_ASSERT_EQ(FT_FALSE, options[2].present);
    FT_ASSERT_STR_EQ("world=backup", options[3].value_string);
    arguments[1] = "--targ=world";
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, cli_parse(&command, 2, arguments));
    return (1);
}

FT_TEST(test_cli_parse_integer_boundaries_rejects_overflow)
{
    cli_option options[2];
    cli_command command;
    reset_option(options + 0, "signed", '\0', CLI_OPTION_INT64);
    reset_option(options + 1, "unsigned", '\0', CLI_OPTION_UINT64);
    reset_command(&command, "tool", options, 2);
    const char *arguments[] = {"tool", "--signed=-9223372036854775808",
        "--unsigned=18446744073709551615"};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 3, arguments));
    FT_ASSERT_EQ(FT_LLONG_MIN, options[0].value_int64);
    FT_ASSERT_EQ(UINT64_MAX, options[1].value_uint64);
    arguments[1] = "--signed=+9223372036854775807";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 3, arguments));
    FT_ASSERT_EQ(FT_LLONG_MAX, options[0].value_int64);
    arguments[2] = "--unsigned=18446744073709551616";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 3, arguments));
    arguments[1] = "--signed=-9223372036854775809";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 2, arguments));
    arguments[1] = "--signed=9223372036854775808";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 2, arguments));
    arguments[1] = "--signed= 1";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 2, arguments));
    return (1);
}

FT_TEST(test_cli_parse_env_and_default_values)
{
    cli_option options[2];
    cli_command command;
    const char *arguments[1];

    reset_option(options + 0, "threads", 't', CLI_OPTION_UINT64);
    reset_option(options + 1, "mode", 'm', CLI_OPTION_STRING);
    options[0].default_value = "8";
    options[1].env_name = "PATH";
    reset_command(&command, "tool", options, 2);
    arguments[0] = "tool";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 1, arguments));
    FT_ASSERT_EQ(static_cast<uint64_t>(8U), options[0].value_uint64);
    FT_ASSERT_EQ(FT_TRUE, options[1].present);
    FT_ASSERT(options[1].value_string != ft_nullptr);
    return (1);
}

FT_TEST(test_cli_parse_subcommand_and_positionals)
{
    cli_option subcommand_options[1];
    cli_command command;
    cli_command subcommands[1];
    const char *positionals[2];
    const char *arguments[5];

    reset_option(subcommand_options + 0, "port", 'p', CLI_OPTION_UINT64);
    reset_command(&command, "tool", ft_nullptr, 0);
    reset_command(subcommands + 0, "serve", subcommand_options, 1);
    subcommands[0].description = "run server";
    subcommands[0].positionals = positionals;
    subcommands[0].positional_capacity = 2;
    command.subcommands = subcommands;
    command.subcommand_count = 1;
    arguments[0] = "tool";
    arguments[1] = "serve";
    arguments[2] = "--port";
    arguments[3] = "8080";
    arguments[4] = "public";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 5, arguments));
    FT_ASSERT_EQ(subcommands + 0, command.selected_subcommand);
    FT_ASSERT_EQ(static_cast<uint64_t>(8080U), subcommand_options[0].value_uint64);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), subcommands[0].positional_count);
    FT_ASSERT_EQ('p', subcommands[0].positionals[0][0]);
    return (1);
}

FT_TEST(test_cli_rejects_missing_required_option)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[1];

    reset_option(options + 0, "config", 'c', CLI_OPTION_STRING);
    options[0].required = FT_TRUE;
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 1, arguments));
    return (1);
}

FT_TEST(test_cli_format_help_lists_commands_and_options)
{
    cli_option options[1];
    cli_command command;
    cli_command subcommands[1];
    char buffer[256];

    reset_option(options + 0, "verbose", 'v', CLI_OPTION_BOOL);
    options[0].help = "enable verbose output";
    reset_command(&command, "tool", options, 1);
    command.description = "demo tool";
    reset_command(subcommands + 0, "serve", ft_nullptr, 0);
    subcommands[0].description = "run server";
    command.subcommands = subcommands;
    command.subcommand_count = 1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_format_help(&command, "tool", buffer, 256));
    FT_ASSERT(cli_find_option(&command, "verbose") == options + 0);
    FT_ASSERT(buffer[0] == 'U');
    return (1);
}

FT_TEST(test_cli_parse_boolean_negation)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[2];

    reset_option(options + 0, "color", 'c', CLI_OPTION_BOOL);
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    arguments[1] = "--no-color";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 2, arguments));
    FT_ASSERT_EQ(FT_TRUE, options[0].present);
    FT_ASSERT_EQ(FT_FALSE, options[0].value_bool);
    return (1);
}

FT_TEST(test_cli_parse_rejects_unknown_long_option)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[2];

    reset_option(options + 0, "known", 'k', CLI_OPTION_BOOL);
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    arguments[1] = "--unknown";
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, cli_parse(&command, 2, arguments));
    return (1);
}

FT_TEST(test_cli_parse_rejects_invalid_integer_value)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[3];

    reset_option(options + 0, "count", 'c', CLI_OPTION_INT64);
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    arguments[1] = "--count";
    arguments[2] = "12x";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 3, arguments));
    return (1);
}

FT_TEST(test_cli_parse_rejects_positional_overflow)
{
    cli_command command;
    const char *arguments[2];

    reset_command(&command, "tool", ft_nullptr, 0);
    arguments[0] = "tool";
    arguments[1] = "extra";
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, cli_parse(&command, 2, arguments));
    return (1);
}

FT_TEST(test_cli_parse_records_help_request)
{
    cli_command command;
    const char *arguments[2];

    reset_command(&command, "tool", ft_nullptr, 0);
    arguments[0] = "tool";
    arguments[1] = "--help";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_parse(&command, 2, arguments));
    FT_ASSERT_EQ(FT_TRUE, command.help_requested);
    return (1);
}

FT_TEST(test_cli_parse_rejects_missing_option_value)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[2];

    reset_option(options + 0, "output", 'o', CLI_OPTION_STRING);
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    arguments[1] = "--output";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 2, arguments));
    return (1);
}

FT_TEST(test_cli_parse_rejects_invalid_boolean_default)
{
    cli_option options[1];
    cli_command command;
    const char *arguments[1];

    reset_option(options + 0, "enabled", 'e', CLI_OPTION_BOOL);
    options[0].default_value = "maybe";
    reset_command(&command, "tool", options, 1);
    arguments[0] = "tool";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, cli_parse(&command, 1, arguments));
    return (1);
}

FT_TEST(test_cli_format_help_reports_small_buffer)
{
    cli_option options[1];
    cli_command command;
    char buffer[8];

    reset_option(options + 0, "verbose", 'v', CLI_OPTION_BOOL);
    options[0].help = "enable verbose output";
    reset_command(&command, "tool", options, 1);
    command.description = "demo tool";
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, cli_format_help(&command, "tool", buffer, 8));
    return (1);
}
