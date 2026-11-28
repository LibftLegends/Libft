#include "../../../Modules/CardGame/card_game.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

struct card_game_event_benchmark_handler
{
    uint64_t *checksum;
    uint32_t marker;
};

static int32_t record_event(const card_game_engine &engine,
    const card_game_effect_context &context, card_game_operation_buffer &operations,
    void *user_data) noexcept
{
    (void)engine;
    (void)operations;
    const card_game_event_benchmark_handler &handler =
        *static_cast<const card_game_event_benchmark_handler *>(user_data);
    *handler.checksum = *handler.checksum * 31U
        + handler.marker + context.source_instance;
    return (FT_ERR_SUCCESS);
}

int main(int32_t argument_count, char **arguments)
{
    if (argument_count != 2)
        return (1);
    const ft_bool unhandled = ft_strcmp(arguments[1], "--unhandled") == 0;
    if (!unhandled && ft_strcmp(arguments[1], "--handled") != 0)
        return (1);

    constexpr uint32_t EFFECTS = FT_CARD_GAME_MAX_EFFECTS;
    constexpr uint32_t EVENTS_PER_BATCH = 64U;
    constexpr uint32_t BATCHES = 16U;
    card_game_engine engine;
    card_game_event_benchmark_handler handlers[EFFECTS];
    uint64_t checksum = 0U;
    const card_game_rules rules{4U, 4U, 20U, 5U, 10U, 10U};
    if (engine.initialize(rules) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < EFFECTS)
    {
        handlers[index] = {&checksum, index};
        uint32_t effect_id;
        if (engine.register_effect_callback_with_priority(record_event,
                &handlers[index], 1U, index * 37U % 67U, &effect_id) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }

    uint32_t event_type = 1U;
    if (unhandled)
        event_type = 2U;
    double elapsed = 0.0;
    uint32_t batch = 0U;
    while (batch < BATCHES)
    {
        index = 0U;
        while (index < EVENTS_PER_BATCH)
        {
            if (engine.emit_event(event_type, index, 0U) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        const auto start = std::chrono::steady_clock::now();
        if (engine.resolve_events() != FT_ERR_SUCCESS)
            return (1);
        elapsed += std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - start).count();
        ++batch;
    }
    card_game_snapshot after;
    if (engine.get_snapshot(&after) != FT_ERR_SUCCESS || after.event_count != 0U)
        return (1);
    std::printf("event dispatch checksum %" PRIu64 " %.6f us/batch\n",
        checksum, elapsed / BATCHES);
    return (0);
}
