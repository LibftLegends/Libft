#include "../../../Modules/Game/game_dialogue_table.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <new>

static int32_t measure_moves()
{
    constexpr int32_t LINE_COUNT = 128;
    constexpr uint32_t ITERATIONS = 128U;
    game_dialogue_table fixture;
    ft_string speaker;
    ft_string text;
    ft_vector<int32_t> next_ids;
    if (fixture.initialize() != FT_ERR_SUCCESS || speaker.initialize("npc") != FT_ERR_SUCCESS
        || text.initialize(128U, 'x') != FT_ERR_SUCCESS || next_ids.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t line_id = 1;
    while (line_id <= LINE_COUNT)
    {
        game_dialogue_line line;
        if (line.initialize() != FT_ERR_SUCCESS
            || line.initialize(line_id, speaker, text, next_ids) != FT_ERR_SUCCESS
            || fixture.register_line(line) != FT_ERR_SUCCESS)
            return (1);
        ++line_id;
    }
    using clock_type = std::chrono::steady_clock;
    clock_type::duration elapsed{};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_dialogue_table source;
        game_dialogue_table destination;
        if (source.initialize(fixture) != FT_ERR_SUCCESS)
            return (1);
        const auto start = clock_type::now();
        const int32_t result = destination.move(source);
        elapsed += clock_type::now() - start;
        if (result != FT_ERR_SUCCESS || destination.get_lines().size() != LINE_COUNT)
            return (1);
        game_dialogue_line fetched;
        if (fetched.initialize() != FT_ERR_SUCCESS
            || destination.fetch_line(LINE_COUNT, fetched) != FT_ERR_SUCCESS)
            return (1);
        checksum += fetched.get_line_id() + fetched.get_text().size();
        ++iteration;
    }
    const double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
    std::printf("move %.3f %" PRIu64 "\n", nanoseconds / ITERATIONS, checksum);
    return (checksum != ITERATIONS * (LINE_COUNT + 128U));
}

static int32_t measure_scripts()
{
    constexpr int32_t SCRIPT_COUNT = 512;
    constexpr uint32_t ITERATIONS = 64U;
    game_dialogue_table fixture;
    game_dialogue_table destination;
    game_dialogue_script script;
    if (fixture.initialize() != FT_ERR_SUCCESS || destination.initialize() != FT_ERR_SUCCESS
        || script.initialize() != FT_ERR_SUCCESS)
        return (1);
    int32_t script_id = 1;
    while (script_id <= SCRIPT_COUNT)
    {
        script.set_script_id(script_id);
        if (fixture.register_script(script) != FT_ERR_SUCCESS)
            return (1);
        ++script_id;
    }
    const auto &scripts = fixture.get_scripts();
    using clock_type = std::chrono::steady_clock;
    clock_type::duration copy_elapsed{};
    clock_type::duration set_elapsed{};
    uint64_t checksum = 0U;
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        game_dialogue_table clone;
        auto start = clock_type::now();
        if (clone.initialize(fixture) != FT_ERR_SUCCESS)
            return (1);
        copy_elapsed += clock_type::now() - start;
        start = clock_type::now();
        destination.set_scripts(scripts);
        set_elapsed += clock_type::now() - start;
        if (destination.get_error() != FT_ERR_SUCCESS)
            return (1);
        checksum += clone.get_scripts().size() + destination.get_scripts().size();
        ++iteration;
    }
    if (checksum != ITERATIONS * 2U * SCRIPT_COUNT
        || destination.fetch_script(SCRIPT_COUNT, script) != FT_ERR_SUCCESS
        || script.get_script_id() != SCRIPT_COUNT)
        return (1);
    const double copy_nanoseconds = std::chrono::duration<double, std::nano>(copy_elapsed).count();
    const double set_nanoseconds = std::chrono::duration<double, std::nano>(set_elapsed).count();
    std::printf("copy_scripts %.3f %" PRIu64 "\n", copy_nanoseconds / ITERATIONS, checksum);
    std::printf("set_scripts %.3f %" PRIu64 "\n", set_nanoseconds / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_registration()
{
    constexpr uint32_t ITERATIONS = 512U;
    constexpr ft_size_t TEXT_LENGTH = 1024U;
    game_dialogue_table table;
    game_dialogue_line line;
    game_dialogue_line fetched;
    ft_string speaker;
    ft_string text;
    ft_vector<int32_t> links;
    if (table.initialize() != FT_ERR_SUCCESS || line.initialize() != FT_ERR_SUCCESS
        || fetched.initialize() != FT_ERR_SUCCESS || speaker.initialize("npc") != FT_ERR_SUCCESS
        || text.initialize(TEXT_LENGTH, 'x') != FT_ERR_SUCCESS || links.initialize() != FT_ERR_SUCCESS
        || links.push_back(2) != FT_ERR_SUCCESS
        || line.initialize(1, speaker, text, links) != FT_ERR_SUCCESS)
        return (1);
    using clock_type = std::chrono::steady_clock;
    const auto start = clock_type::now();
    uint32_t iteration = 0U;
    while (iteration < ITERATIONS)
    {
        if (table.register_line(line) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double elapsed = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    if (table.fetch_line(1, fetched) != FT_ERR_SUCCESS || table.get_lines().size() != 1U)
        return (1);
    const uint64_t checksum = fetched.get_text().size() + fetched.get_next_line_ids().size();
    if (checksum != TEXT_LENGTH + 1U)
        return (1);
    std::printf("register %.3f %" PRIu64 "\n", elapsed / ITERATIONS, checksum);
    return (0);
}

static int32_t measure_fetches()
{
    constexpr int32_t CACHED_ID = 513;
    constexpr uint32_t ITERATIONS = 100000U;
    game_dialogue_table table;
    game_dialogue_line fetched;
    ft_string empty;
    ft_vector<int32_t> links;
    if (table.initialize() != FT_ERR_SUCCESS || fetched.initialize() != FT_ERR_SUCCESS
        || empty.initialize() != FT_ERR_SUCCESS || links.initialize() != FT_ERR_SUCCESS)
        return (1);
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
        || line->initialize(CACHED_ID, empty, empty, links) != FT_ERR_SUCCESS
        || table.register_line(*line) != FT_ERR_SUCCESS
        || line->enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    auto &lines = table.get_lines();
    int32_t identifier = 1;
    while (identifier < CACHED_ID)
    {
        lines.insert(identifier, owned);
        if (lines.get_error() != FT_ERR_SUCCESS)
            return (1);
        ++identifier;
    }
    if (lines.enable_thread_safety() != FT_ERR_SUCCESS)
        return (1);
    using clock_type = std::chrono::steady_clock;
    uint32_t iteration = 0U;
    auto start = clock_type::now();
    while (iteration < ITERATIONS)
    {
        if (table.fetch_line(CACHED_ID, fetched) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double cached_ns = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    iteration = 0U;
    start = clock_type::now();
    while (iteration < ITERATIONS)
    {
        if (table.fetch_line(CACHED_ID - 1, fetched) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const double fallback_ns = std::chrono::duration<double, std::nano>(clock_type::now() - start).count();
    const int32_t checksum = fetched.get_line_id();
    if (checksum != CACHED_ID)
        return (1);
    std::printf("cached %.3f %" PRId32 "\n", cached_ns / ITERATIONS, checksum);
    std::printf("fallback %.3f %" PRId32 "\n", fallback_ns / ITERATIONS, checksum);
    return (0);
}

int32_t main(int32_t argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--scripts") == 0)
        return (measure_scripts());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--register") == 0)
        return (measure_registration());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--fetch") == 0)
        return (measure_fetches());
    return (measure_moves());
}
