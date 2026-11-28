#include "../../../Modules/CLI/cli.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t MAX_OPTIONS = 512;
    const ft_bool small = argument_count == 2 && ft_strcmp(arguments[1], "--small") == 0;
    uint32_t count = MAX_OPTIONS;
    uint32_t iterations = 64;
    if (small)
    {
        count = 8;
        iterations = 100000;
    }

    char names[MAX_OPTIONS][48];
    cli_option options[MAX_OPTIONS]{};
    cli_command command{};
    command.name = "tool";
    command.options = options;
    command.option_count = count;
    uint32_t index = 0;
    while (index < count)
    {
        std::snprintf(names[index], sizeof(names[index]), "common-option-prefix-%" PRIu32, index);
        options[index].long_name = names[index];
        ++index;
    }
    index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < iterations)
    {
        if (cli_validate(&command) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("validation %" PRIu32 " options %.3f us/op\n", count, elapsed / iterations);
    return (0);
}
