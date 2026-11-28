#include "../../../Modules/CardGame/card_game_script_bridge.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int argc, char **argv)
{
    const ft_bool manifest = argc == 2 && ft_strcmp(argv[1], "--manifest") == 0;
    if (argc != 1 && manifest == FT_FALSE)
        return (1);
    constexpr uint32_t SCRIPT_COUNT = 32U;
    constexpr uint32_t ITERATIONS = 1200U;
    card_game_engine engine;
    card_game_script_bridge bridge;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || bridge.initialize(engine) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < SCRIPT_COUNT)
    {
        uint32_t effect_id;
        if (bridge.register_script_effect(
                "emit_host_intent(910, 1, 1, 77);", index + 1U, &effect_id)
            != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    std::vector<uint8_t> bytes;
    uint64_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (manifest)
        {
            if (bridge.get_manifest_hash(&checksum) != FT_ERR_SUCCESS)
                return (1);
        }
        else if (bridge.serialize_script_catalog(bytes) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    index = 0U;
    while (index < bytes.size())
    {
        checksum = checksum * UINT64_C(31) + bytes[index];
        ++index;
    }
    std::printf("script catalog checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}
