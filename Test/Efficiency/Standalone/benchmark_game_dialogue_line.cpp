#include "../../../Modules/Game/game_dialogue_line.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static int32_t measure_move(const ft_string &speaker, const ft_string &text,
    const ft_vector<int32_t> &next_ids, ft_bool reuse_destination)
{
    constexpr uint32_t ITERATIONS = 2048U;
    using clock_type = std::chrono::steady_clock;
    game_dialogue_line reusable;
    clock_type::duration elapsed{};
    uint32_t iteration = 0U;
    uint64_t checksum = 0U;
    while (iteration < ITERATIONS)
    {
        game_dialogue_line source;
        game_dialogue_line fresh;
        game_dialogue_line *destination = &fresh;
        if (reuse_destination)
            destination = &reusable;
        if (source.initialize() != FT_ERR_SUCCESS
            || source.initialize(4, speaker, text, next_ids) != FT_ERR_SUCCESS)
            return (1);
        const auto start = clock_type::now();
        const int32_t result = destination->move(source);
        elapsed += clock_type::now() - start;
        if (result != FT_ERR_SUCCESS)
            return (1);
        checksum += destination->get_text().size() + destination->get_next_line_ids().size();
        ++iteration;
    }
    if (checksum != ITERATIONS * (text.size() + next_ids.size()))
        return (1);
    const double move_ns = std::chrono::duration<double, std::nano>(elapsed).count();
    std::printf("move_%u %.3f %" PRIu64 "\n", static_cast<uint32_t>(reuse_destination),
        move_ns / ITERATIONS, checksum);
    return (0);
}

int main(int argument_count, char **arguments)
{
    constexpr uint32_t ITERATIONS = 2048U;
    game_dialogue_line line;
    ft_string speaker;
    ft_string text;
    ft_vector<int32_t> next_ids;
    if (line.initialize() != FT_ERR_SUCCESS || speaker.initialize("npc") != FT_ERR_SUCCESS
        || text.initialize(4096U, 'x') != FT_ERR_SUCCESS || next_ids.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t next_id = 1;
    while (next_id <= 32)
    {
        if (next_ids.push_back(next_id) != FT_ERR_SUCCESS)
            return (1);
        ++next_id;
    }
    if (argument_count == 2 && ft_strcmp(arguments[1], "--move") == 0)
    {
        if (measure_move(speaker, text, next_ids, FT_FALSE) != 0)
            return (1);
        return (measure_move(speaker, text, next_ids, FT_TRUE));
    }
    if (line.initialize(4, speaker, text, next_ids) != FT_ERR_SUCCESS)
        return (1);
    using clock_type = std::chrono::steady_clock;
    uint32_t iteration = 0U;
    auto start = clock_type::now();
    while (iteration < ITERATIONS)
    {
        line.set_text(text);
        if (line.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double text_ns = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    iteration = 0U;
    start = clock_type::now();
    while (iteration < ITERATIONS)
    {
        if (line.initialize(4, speaker, text, next_ids) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double line_ns = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    const uint64_t checksum = line.get_text().size() + line.get_speaker().size()
        + line.get_next_line_ids().size() + line.get_line_id();
    if (checksum != 4135U)
        return (1);
    std::printf("set_text %.3f %" PRIu64 "\n", text_ns / ITERATIONS, checksum);
    std::printf("set_line %.3f %" PRIu64 "\n", line_ns / ITERATIONS, checksum);
    return (0);
}
