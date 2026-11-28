#include "../../../Modules/Game/game_region_definition.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

using region_clock = std::chrono::steady_clock;

static void report(const char *operation, region_clock::duration duration,
    uint32_t iterations, uint64_t checksum)
{
    const double elapsed = std::chrono::duration<double, std::nano>(duration).count();
    std::printf("%s %.3f %" PRIu64 "\n", operation, elapsed / iterations, checksum);
    return ;
}

int main()
{
    constexpr uint32_t ITERATIONS = 4096U;
    constexpr ft_size_t NAME_LENGTH = 256U;
    constexpr ft_size_t DESCRIPTION_LENGTH = 4096U;
    ft_string name;
    ft_string description;
    game_region_definition destination;
    if (name.initialize(NAME_LENGTH, 'n') != FT_ERR_SUCCESS
        || description.initialize(DESCRIPTION_LENGTH, 'd') != FT_ERR_SUCCESS
        || destination.initialize(3, name, description, 4) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    auto start = region_clock::now();
    while (iteration < ITERATIONS)
    {
        destination.set_name(name);
        destination.set_description(description);
        ++iteration;
    }
    const auto setter_elapsed = region_clock::now() - start;
    uint64_t checksum = destination.get_name().size() + destination.get_description().size();
    report("set_text", setter_elapsed, ITERATIONS, checksum);
    if (checksum != NAME_LENGTH + DESCRIPTION_LENGTH)
        return (1);
    checksum = 0U;
    iteration = 0U;
    region_clock::duration move_elapsed{};
    while (iteration < ITERATIONS)
    {
        game_region_definition source;
        if (source.initialize(3, name, description, 4) != FT_ERR_SUCCESS)
            return (1);
        start = region_clock::now();
        const int32_t result = destination.move(source);
        move_elapsed += region_clock::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum += destination.get_name().size() + destination.get_description().size();
        ++iteration;
    }
    report("move", move_elapsed, ITERATIONS, checksum);
    if (checksum != ITERATIONS * (NAME_LENGTH + DESCRIPTION_LENGTH))
        return (1);
    return (0);
}
