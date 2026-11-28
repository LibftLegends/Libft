#include "../../../Modules/Game/game_world_registry.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t populate(game_world_registry &source, int32_t count)
{
    game_region_definition region;
    game_world_region world;
    if (source.initialize() != FT_ERR_SUCCESS || region.initialize() != FT_ERR_SUCCESS
        || world.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t identifier = 1;
    while (identifier <= count)
    {
        region.set_region_id(identifier);
        world.set_world_id(identifier);
        if (source.register_region(region) != FT_ERR_SUCCESS
            || source.register_world(world) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    return (0);
}

static int32_t measure_copy(int32_t count)
{
    constexpr uint32_t ITERATIONS = 100U;
    game_world_registry source;
    if (populate(source, count) != 0)
        return (1);
    uint32_t index = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        game_world_registry copy;
        if (copy.initialize(source) != FT_ERR_SUCCESS)
            return (1);
        checksum += copy.get_regions().size() + copy.get_world_regions().size();
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    if (checksum != static_cast<uint64_t>(count) * ITERATIONS * 2U)
        return (1);
    std::printf("copy_%" PRId32 " %.3f %" PRIu64 "\n", count, elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_move(int32_t count)
{
    constexpr uint32_t ITERATIONS = 200U;
    game_world_registry first;
    game_world_registry second;
    if (populate(first, count) != 0)
        return (1);
    game_world_registry *source = &first;
    game_world_registry *destination = &second;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (destination->move(*source) != FT_ERR_SUCCESS)
            return (1);
        game_world_registry *previous_source = source;
        source = destination;
        destination = previous_source;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    const uint64_t checksum = source->get_regions().size() + source->get_world_regions().size();
    if (checksum != static_cast<uint64_t>(count) * 2U)
        return (1);
    std::printf("move_%" PRId32 " %.3f %" PRIu64 "\n", count, elapsed / ITERATIONS, checksum);
    return (0);
}

int main()
{
    if (measure_copy(8) != 0 || measure_copy(128) != 0 || measure_move(8) != 0)
        return (1);
    return (measure_move(128));
}
