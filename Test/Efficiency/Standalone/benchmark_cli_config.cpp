#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CLI/cli.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t OPTION_COUNT = 32;
    constexpr uint32_t NAME_LENGTH = 128;
    constexpr uint32_t LINE_COUNT = 256;
    constexpr uint32_t ITERATIONS = 300;
    char names[OPTION_COUNT - 1][NAME_LENGTH + 1]{};
    char selected_name[NAME_LENGTH + 1];
    char config[LINE_COUNT * (NAME_LENGTH + 1) + 1];
    cli_option options[OPTION_COUNT]{};
    cli_command command{};

    uint32_t option_index = 0;
    while (option_index < OPTION_COUNT - 1)
    {
        std::snprintf(names[option_index], sizeof(names[option_index]),
            "option-%" PRIu32, option_index);
        options[option_index].long_name = names[option_index];
        options[option_index].type = CLI_OPTION_BOOL;
        ++option_index;
    }
    ft_memset(selected_name, 'a', NAME_LENGTH);
    selected_name[NAME_LENGTH] = '\0';
    options[OPTION_COUNT - 1].long_name = selected_name;
    options[OPTION_COUNT - 1].type = CLI_OPTION_BOOL;
    command.name = "tool";
    command.options = options;
    command.option_count = OPTION_COUNT;

    uint32_t line_index = 0;
    while (line_index < LINE_COUNT)
    {
        char *line = config + line_index * (NAME_LENGTH + 1);
        ft_memcpy(line, selected_name, NAME_LENGTH);
        line[NAME_LENGTH] = '\n';
        ++line_index;
    }
    config[sizeof(config) - 1] = '\0';

    uint32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (cli_merge_config_text(&command, config) != FT_ERR_SUCCESS
            || options[OPTION_COUNT - 1].value_bool != FT_TRUE)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("config %" PRIu32 " options, %" PRIu32 " lines of %" PRIu32
        " bytes %.3f us/op\n", OPTION_COUNT, LINE_COUNT, NAME_LENGTH, elapsed / ITERATIONS);
    return (0);
}
