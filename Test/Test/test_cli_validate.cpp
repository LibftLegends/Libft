#include "../test_internal.hpp"
#include "../../Modules/CLI/cli.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cinttypes>
#include <cstdio>

FT_TEST(test_cli_validate_large_option_list_preserves_error_order_and_allocation_fallback)
{
    constexpr uint32_t OPTION_COUNT = 40;
    char names[OPTION_COUNT][32];
    cli_option options[OPTION_COUNT]{};
    cli_command command{};
    command.name = "tool";
    command.options = options;
    command.option_count = OPTION_COUNT;
    uint32_t index = 0;
    while (index < OPTION_COUNT)
    {
        std::snprintf(names[index], sizeof(names[index]), "option-%" PRIu32, index);
        options[index].long_name = names[index];
        ++index;
    }

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_validate(&command));
    cma_set_alloc_limit(1);
    const int32_t fallback_status = cli_validate(&command);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fallback_status);

    options[0].short_name = static_cast<char>(0xE1);
    options[OPTION_COUNT - 2].short_name = options[0].short_name;
    options[OPTION_COUNT - 1].type = UINT8_MAX;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, cli_validate(&command));
    options[OPTION_COUNT - 2].short_name = '\0';
    options[OPTION_COUNT - 1].long_name = options[0].long_name;
    FT_ASSERT_EQ(FT_ERR_UNSUPPORTED_TYPE, cli_validate(&command));
    options[OPTION_COUNT - 1].type = CLI_OPTION_BOOL;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, cli_validate(&command));
    return (1);
}

FT_TEST(test_cli_validate_indexed_subcommands_preserve_tree_validation)
{
    constexpr uint32_t COMMAND_COUNT = 17;
    char names[COMMAND_COUNT][32];
    cli_command subcommands[COMMAND_COUNT]{};
    cli_command command{};
    command.name = "tool";
    command.subcommands = subcommands;
    command.subcommand_count = COMMAND_COUNT;
    command.selected_subcommand = &subcommands[1];
    uint32_t index = 0;
    while (index < COMMAND_COUNT)
    {
        std::snprintf(names[index], sizeof(names[index]), "action-%" PRIu32, index);
        subcommands[index].name = names[index];
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cli_validate(&command));
    cli_command unrelated{};
    command.selected_subcommand = &unrelated;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, cli_validate(&command));
    subcommands[COMMAND_COUNT - 1].name = subcommands[0].name;
    subcommands[3].positional_capacity = 1;
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, cli_validate(&command));
    subcommands[3].positional_capacity = 0;
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, cli_validate(&command));
    return (1);
}
