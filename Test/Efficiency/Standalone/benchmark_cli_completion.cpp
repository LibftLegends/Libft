#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CLI/cli.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t CHILD_COUNT = 64;
    constexpr uint32_t ITERATIONS = 1000;
    char parent_name[193];
    char child_names[CHILD_COUNT][32];
    char output[32768];
    cli_command root{};
    cli_command parent{};
    cli_command children[CHILD_COUNT]{};

    ft_memset(parent_name, 'p', sizeof(parent_name) - 1);
    parent_name[sizeof(parent_name) - 1] = '\0';
    root.name = "tool";
    root.subcommands = &parent;
    root.subcommand_count = 1;
    parent.name = parent_name;
    parent.subcommands = children;
    parent.subcommand_count = CHILD_COUNT;
    uint32_t index = 0;
    while (index < CHILD_COUNT)
    {
        std::snprintf(child_names[index], sizeof(child_names[index]), "child-%" PRIu32, index);
        children[index].name = child_names[index];
        ++index;
    }
    index = 0;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (cli_generate_completion_script(&root, root.name, output, sizeof(output)) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();

    uint32_t checksum = 0;
    index = 0;
    while (output[index] != '\0')
    {
        checksum = checksum * 31 + static_cast<uint8_t>(output[index]);
        ++index;
    }
    std::printf("completion %" PRIu32 " children, checksum %" PRIu32 " %.3f us/op\n",
        CHILD_COUNT, checksum, elapsed / ITERATIONS);
    return (0);
}
