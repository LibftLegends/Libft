#include "../test_internal.hpp"
#include "../../Modules/CLI/cli.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

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

FT_TEST(test_cli_merge_config_file_applies_nested_values)
{
    cli_option root_options[1];
    cli_option serve_options[1];
    cli_command command;
    cli_command subcommands[1];
    const char *name_value;
    const char config_text[] = "name = demo\nserve.port = 8080\n";
    uint64_t port_value;
    const char *config_path;

    config_path = "cli_config_test.conf";
    reset_option(root_options + 0, "name", 'n', CLI_OPTION_STRING);
    reset_option(serve_options + 0, "port", 'p', CLI_OPTION_UINT64);
    reset_command(&command, "tool", root_options, 1);
    reset_command(subcommands + 0, "serve", serve_options, 1);
    command.subcommands = subcommands;
    command.subcommand_count = 1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(config_path, config_text,
            sizeof(config_text) - 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_merge_config_file(&command, config_path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_get_string(&command, "name", &name_value));
    FT_ASSERT_EQ(0, std::strcmp(name_value, "demo"));
    port_value = subcommands[0].options[0].value_uint64;
    FT_ASSERT_EQ(static_cast<uint64_t>(8080U), port_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(config_path));
    if (root_options[0].value_string_storage != ft_nullptr)
    {
        (void)root_options[0].value_string_storage->destroy();
        delete root_options[0].value_string_storage;
        root_options[0].value_string_storage = ft_nullptr;
    }
    return (1);
}
