#include "../../../Modules/CardGame/card_game_resolution_stack.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main()
{
    constexpr uint32_t ENTRY_COUNT = 4096U;
    constexpr uint32_t ITERATIONS = 32U;
    card_game_resolution_stack stack;
    if (stack.initialize(ENTRY_COUNT, CARD_GAME_RESOLUTION_FIFO,
            CARD_GAME_RESOLUTION_OPEN_CURRENT_BATCH) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        uint32_t index = 1U;
        while (index <= ENTRY_COUNT)
        {
            if (stack.push(index, 1U, (index * 19U) % 97U, index) != FT_ERR_SUCCESS)
                return (1);
            ++index;
        }
        while (stack.size() != 0U)
        {
            card_game_resolution_entry entry{};
            if (stack.pop_next(&entry) != FT_ERR_SUCCESS)
                return (1);
            checksum = checksum * 31U + entry.entry_id;
        }
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    std::printf("resolution stack checksum %" PRIu64 " %.3f us/op\n", checksum, elapsed / ITERATIONS);
    return (0);
}
