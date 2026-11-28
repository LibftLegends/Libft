#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ITERATIONS = 1000000U;
    constexpr uint32_t ALLOWED_COMMANDS = CARD_GAME_COMMAND_ADVANCE_PHASE
        | CARD_GAME_COMMAND_DRAW_CARD;
    card_game_engine engine;
    const card_game_rules rules{2U, 2U, 20U, 1U, 3U, 10U};
    if (engine.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    uint32_t phase_id = 1U;
    while (phase_id <= FT_CARD_GAME_MAX_PHASES)
    {
        const card_game_phase_definition phase{
            phase_id, phase_id % FT_CARD_GAME_MAX_PHASES + 1U,
            0U, 0U, ALLOWED_COMMANDS};
        if (engine.register_phase(phase) != FT_ERR_SUCCESS)
            return (1);
        ++phase_id;
    }
    if (engine.start_match(1U) != FT_ERR_SUCCESS)
        return (1);
    phase_id = 1U;
    while (phase_id < FT_CARD_GAME_MAX_PHASES)
    {
        if (engine.advance_phase() != FT_ERR_SUCCESS)
            return (1);
        ++phase_id;
    }
    uint64_t checksum = 0U;
    uint32_t index = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (index < ITERATIONS)
    {
        card_game_legal_actions actions;
        if (engine.query_legal_actions(0U, &actions) != FT_ERR_SUCCESS
            || actions.phase_id != FT_CARD_GAME_MAX_PHASES
            || actions.allowed_command_mask != ALLOWED_COMMANDS)
            return (1);
        checksum += actions.phase_id + actions.allowed_command_mask;
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("legal actions checksum %" PRIu64 " %.6f ns/operation\n",
        checksum, elapsed / ITERATIONS);
    return (0);
}
