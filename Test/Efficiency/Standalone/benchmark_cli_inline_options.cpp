#include "../../../Modules/CLI/cli.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t OPTION_COUNT = 48;
    constexpr uint32_t VALUE_COUNT = 256;
    constexpr uint32_t ITERATIONS = 400;
    char names[OPTION_COUNT][64];
    cli_option options[OPTION_COUNT]{};
    cli_command command{};
    const char *arguments[VALUE_COUNT + 1];
    uint32_t index = 0;
    while (index + 1 < OPTION_COUNT)
    {
        std::snprintf(names[index], sizeof(names[index]),
            "different-option-with-a-long-name-%" PRIu32, index);
        options[index].long_name = names[index];
        options[index].type = CLI_OPTION_BOOL;
        ++index;
    }
    options[index].long_name = "target";
    options[index].type = CLI_OPTION_STRING;
    command.name = "tool";
    command.options = options;
    command.option_count = OPTION_COUNT;
    arguments[0] = "tool";
    index = 1;
    while (index <= VALUE_COUNT)
    {
        arguments[index] = "--target=world=backup";
        ++index;
    }
    uint64_t checksum = 0;
    index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (cli_parse(&command, VALUE_COUNT + 1, arguments) != FT_ERR_SUCCESS)
            return (1);
        const char *value = options[OPTION_COUNT - 1].value_string;
        if (ft_strcmp(value, "world=backup") != 0)
            return (1);
        checksum = checksum * 31U + static_cast<uint8_t>(value[0]);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("cli %.3f us/parse checksum=%" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}
