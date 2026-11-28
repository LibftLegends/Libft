#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t RECORD_COUNT = 8192U;
    constexpr uint32_t ITERATIONS = 16000U;
    card_game_engine engine;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    const card_game_phase_definition first_phase{
        1U, 2U, 0U, 0U, CARD_GAME_COMMAND_ADVANCE_PHASE};
    const card_game_phase_definition second_phase{
        2U, 1U, 0U, 0U, CARD_GAME_COMMAND_ADVANCE_PHASE};
    if (engine.initialize(rules) != FT_ERR_SUCCESS
        || engine.register_phase(first_phase) != FT_ERR_SUCCESS
        || engine.register_phase(second_phase) != FT_ERR_SUCCESS
        || engine.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    card_game_action action{};
    action.schema_version = FT_CARD_GAME_ACTION_SCHEMA_VERSION;
    action.session_id = 1U;
    action.command.type = CARD_GAME_INTENT_ADVANCE_PHASE;
    card_game_action_result result{};
    uint32_t index = 0U;
    while (index < RECORD_COUNT)
    {
        ++index;
        action.request_id = index;
        action.command.command_sequence = index;
        if (engine.submit_action(action, &result, ft_nullptr) != FT_ERR_SUCCESS
            || result.replayed != FT_FALSE)
            return (1);
    }
    const uint64_t expected_hash = result.state_hash;
    uint64_t checksum = 0U;
    index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        if (engine.submit_action(action, &result, ft_nullptr) != FT_ERR_SUCCESS
            || result.replayed != FT_TRUE || result.state_hash != expected_hash)
            return (1);
        checksum += result.state_hash;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("action retry checksum %" PRIu64 " %.6f us/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}
