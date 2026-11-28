#include "../../../Modules/Game/game_event_scheduler_telemetry.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t SAMPLES = 8000000U;
    game_event_scheduler_telemetry_state state;
    game_event_scheduler_telemetry_state_initialize(state, "benchmark");
    const t_event_scheduler_profile profile{13, 300, 7, 500U, 40U,
        810000, 90000, FT_ERR_SUCCESS};
    game_event_scheduler_telemetry_record(state, profile);
    const auto start = std::chrono::steady_clock::now();
    uint32_t index = 0U;
    while (index < SAMPLES)
    {
        game_event_scheduler_telemetry_record(state, profile);
        ++index;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("unchanged %.3f ns/call updates=%" PRId64 " total=%" PRId64 "\n",
        elapsed / SAMPLES, state.last_update_count, state.last_total_processing_ns);
    return (state.last_update_count != 13 || state.last_total_processing_ns != 810000);
}
