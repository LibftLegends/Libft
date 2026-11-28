#include "../../../Modules/Game/game_dialogue_script.hpp"
#include "../../../Modules/Game/game_dialogue_line.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static int32_t measure_metadata()
{
    constexpr uint32_t ITERATIONS = 2048U;
    constexpr ft_size_t TEXT_LENGTH = 4096U;
    game_dialogue_script script;
    ft_string text;
    if (script.initialize() != FT_ERR_SUCCESS
        || text.initialize(TEXT_LENGTH, 'x') != FT_ERR_SUCCESS)
        return (1);
    script.set_title(text);
    script.set_summary(text);
    using clock_type = std::chrono::steady_clock;
    const auto start = clock_type::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        script.set_title(text);
        script.set_summary(text);
        if (script.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    const uint64_t checksum = script.get_title().size() + script.get_summary().size();
    if (checksum != 2U * TEXT_LENGTH)
        return (1);
    std::printf("metadata %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--metadata") == 0)
        return (measure_metadata());
    constexpr int32_t LINE_COUNT = 32;
    constexpr int32_t LINK_COUNT = 16;
    constexpr uint32_t ITERATIONS = 64U;
    ft_vector<ft_sharedptr<game_dialogue_line>> lines;
    ft_vector<int32_t> next_ids;
    ft_string speaker;
    ft_string text;
    game_dialogue_script script;
    if (lines.initialize() != FT_ERR_SUCCESS || next_ids.initialize() != FT_ERR_SUCCESS
        || speaker.initialize("npc") != FT_ERR_SUCCESS || text.initialize(256U, 'x') != FT_ERR_SUCCESS
        || script.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t identifier = 1;
    while (identifier <= LINK_COUNT)
    {
        if (next_ids.push_back(identifier) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    identifier = 1;
    while (identifier <= LINE_COUNT)
    {
        auto *line = new (std::nothrow) game_dialogue_line();
        if (line == ft_nullptr)
            return (1);
        ft_sharedptr<game_dialogue_line> owned;
        if (owned.initialize(line, FT_FALSE, 0U) != FT_ERR_SUCCESS)
        {
            delete line;
            return (1);
        }
        if (line->initialize() != FT_ERR_SUCCESS
            || line->initialize(identifier, speaker, text, next_ids) != FT_ERR_SUCCESS
            || lines.push_back(ft_move(owned)) != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    script.set_lines(lines);
    if (script.get_error() != FT_ERR_SUCCESS)
        return (1);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--locked") == 0)
    {
        identifier = 0;
        while (identifier < LINE_COUNT)
        {
            if (script.get_lines()[identifier]->enable_thread_safety() != FT_ERR_SUCCESS)
                return (1);
            ++identifier;
        }
    }
    using clock_type = std::chrono::steady_clock;
    clock_type::duration elapsed{};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_dialogue_script clone;
        const auto start = clock_type::now();
        const int32_t result = clone.initialize(script);
        elapsed += clock_type::now() - start;
        if (result != FT_ERR_SUCCESS || clone.get_lines().size() != LINE_COUNT)
            return (1);
        const auto &last = clone.get_lines()[LINE_COUNT - 1];
        if (last.get() == ft_nullptr || last->get_line_id() != LINE_COUNT)
            return (1);
        checksum += last->get_next_line_ids().size() + last->get_text().size();
        ++iteration;
    }
    if (checksum != ITERATIONS * (LINK_COUNT + text.size()))
        return (1);
    const double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
    std::printf("copy %.3f %" PRIu64 "\n", nanoseconds / ITERATIONS, checksum);
    return (0);
}
