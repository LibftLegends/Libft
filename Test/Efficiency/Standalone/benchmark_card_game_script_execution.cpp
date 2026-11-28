#include "../../../Modules/CardGame/card_game_script_bridge.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <string>

int main()
{
    constexpr uint32_t CARD_COUNT = 1024U;
    constexpr uint32_t INVOCATIONS = 600U;
    constexpr uint32_t CONTEXT_READS = 32U;
    card_game_engine engine;
    card_game_script_bridge bridge;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    if (engine.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < CARD_COUNT)
    {
        const card_game_card_definition definition{
            index + 1U, CARD_GAME_CREATURE, 0U, 1, 1, CARD_GAME_NO_EFFECT};
        if (engine.register_card(definition) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    if (engine.start_match(1U) != FT_ERR_SUCCESS
        || bridge.initialize(engine) != FT_ERR_SUCCESS
        || bridge.register_context_key(41U) != FT_ERR_SUCCESS
        || bridge.set_context_value(41U, 7) != FT_ERR_SUCCESS)
        return (1);
    std::string source;
    index = 0U;
    while (index < CONTEXT_READS)
    {
        source += "context_value(41);";
        ++index;
    }
    uint32_t effect_id;
    if (bridge.register_script_effect(source.c_str(), 1U, &effect_id) != FT_ERR_SUCCESS)
        return (1);
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < INVOCATIONS)
    {
        if (engine.emit_event(1U, 0U, 0U) != FT_ERR_SUCCESS
            || engine.resolve_events() != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    uint64_t checksum;
    if (engine.get_state_hash(&checksum) != FT_ERR_SUCCESS)
        return (1);
    std::printf("script execution checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / INVOCATIONS);
    return (0);
}
