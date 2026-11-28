#include "../../../Modules/CardGame/card_game_replay.hpp"
#include "../../../Modules/File/file_utils.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

int main(int32_t argument_count, char **arguments)
{
    constexpr uint32_t EVENT_COUNT = 4096U;
    constexpr uint32_t ITERATIONS = 48U;
    static uint8_t serialized[1048576];
    const ft_bool save_file = argument_count == 2 && ft_strcmp(arguments[1], "--save") == 0;
    const char *path = "benchmark-card-game-replay.bin";
    card_game_replay_header header{};
    header.visibility_mode = CARD_GAME_REPLAY_FULL_INFORMATION;
    card_game_replay source;
    if (source.initialize(header) != FT_ERR_SUCCESS)
        return (1);
    uint32_t index = 0U;
    while (index < EVENT_COUNT)
    {
        card_game_replay_event event{};
        event.sequence = index + 1U;
        event.player_id = 1U;
        event.command_type = CARD_GAME_INTENT_PLAY_CARD;
        event.card_id = 1U;
        event.session_id = 23U;
        if (source.append(event) != FT_ERR_SUCCESS)
            return (1);
        ++index;
    }
    uint32_t size = 0U;
    if (source.serialize(serialized, sizeof(serialized), &size) != FT_ERR_SUCCESS)
        return (1);
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (save_file)
        {
            if (source.save_file(path) != FT_ERR_SUCCESS)
                return (1);
        }
        else
        {
            card_game_replay restored;
            card_game_replay_event last{};
            if (restored.deserialize(serialized, size) != FT_ERR_SUCCESS
                || restored.get_event(EVENT_COUNT - 1U, &last) != FT_ERR_SUCCESS)
                return (1);
            checksum += last.sequence + last.session_id;
        }
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::micro>(
        std::chrono::steady_clock::now() - start).count();
    if (save_file)
    {
        ft_string contents;
        if (contents.initialize() != FT_ERR_SUCCESS
            || file_read_all(path, contents) != FT_ERR_SUCCESS
            || file_delete(path) != FT_ERR_SUCCESS
            || contents.size() != size
            || ft_memcmp(contents.data(), serialized, size) != 0)
            return (1);
        const char *bytes = contents.data();
        uint32_t byte_index = 0U;
        while (byte_index < size)
        {
            checksum = checksum * 31U + static_cast<uint8_t>(bytes[byte_index]);
            ++byte_index;
        }
    }
    std::printf("replay checksum %" PRIu64 " %.3f us/op\n", checksum, elapsed / ITERATIONS);
    return (0);
}
