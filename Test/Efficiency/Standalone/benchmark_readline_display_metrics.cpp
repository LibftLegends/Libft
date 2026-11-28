#include "../../../Modules/ReadLine/readline_internal.hpp"
#include "../../../Modules/ReadLine/readline.hpp"
#include "../../../Modules/Basic/basic.hpp"
#include "../../../Modules/CMA/CMA.hpp"
#include "../../../Modules/System_utils/system_utils.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>

static ft_size_t output_bytes;

static int64_t count_terminal_output(int32_t descriptor, const void *, ft_size_t length)
{
    if (descriptor != 1)
        return (-1);
    output_bytes += length;
    return (static_cast<int64_t>(length));
}

static int32_t measure_redraw(readline_state_t &state, int32_t line_length)
{
    constexpr int32_t ITERATIONS = 500;
    int32_t terminal_width = 0;
    if (rl_get_terminal_width(&terminal_width) != FT_ERR_SUCCESS || terminal_width <= 0)
        return (3);
    const ft_size_t line_count = (line_length + 1 + 2) / terminal_width + 1;
    const ft_size_t bytes_per_redraw = 2 + line_count * 4 + (line_count - 1) * 3 + line_length + 1 + 2;
    int32_t iteration = 0;
    output_bytes = 0;
    su_set_write_syscall_hook(count_terminal_output);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        state.position = line_length;
        state.buffer[line_length] = '\0';
        if (rl_handle_printable_char(&state, 'x', "> ") != FT_ERR_SUCCESS)
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    su_reset_write_syscall_hook();
    if (iteration != ITERATIONS || output_bytes != bytes_per_redraw * ITERATIONS
        || state.prev_display_columns != line_length + 1 || state.position != line_length + 1)
        return (4);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_terminal_width()
{
    constexpr int32_t ITERATIONS = 10000;
    int32_t expected_width = 0;
    if (rl_get_terminal_width(&expected_width) != FT_ERR_SUCCESS || expected_width <= 0)
        return (5);
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        int32_t width = 0;
        if (rl_get_terminal_width(&width) != FT_ERR_SUCCESS || width != expected_width)
            return (6);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_completion_ranking()
{
    constexpr int32_t ITERATIONS = 64;
    char candidates[MAX_SUGGESTIONS][12];
    int32_t index = 0;
    while (index < MAX_SUGGESTIONS)
    {
        const int32_t length = std::snprintf(candidates[index], sizeof(candidates[index]),
            "command-%03d", MAX_SUGGESTIONS - 1 - index);
        if (length != 11)
            return (7);
        suggestions[index] = candidates[index];
        ++index;
    }
    suggestion_count = MAX_SUGGESTIONS;
    char line[32] = "command-";
    readline_state_t state;
    state.buffer = line;
    state.buffer_size = sizeof(line);
    int32_t iteration = 0;
    output_bytes = 0;
    su_set_write_syscall_hook(count_terminal_output);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        state.in_completion_mode = 0;
        state.position = 8;
        state.buffer[8] = '\0';
        if (rl_handle_tab_completion(&state, "> ") != FT_ERR_SUCCESS)
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    su_reset_write_syscall_hook();
    suggestion_count = 0;
    if (iteration != ITERATIONS || state.current_match_count != MAX_SUGGESTIONS
        || ft_strcmp(line, "command-000") != 0 || output_bytes == 0)
        return (8);
    index = 0;
    while (index < MAX_SUGGESTIONS)
    {
        if (state.current_matches[index] != candidates[MAX_SUGGESTIONS - 1 - index])
            return (9);
        ++index;
    }
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_history_search()
{
    constexpr int32_t ITERATIONS = 64;
    char ordinary[] = "command with arguments and a mixed UTF-8 value caf\xC3\xA9";
    char oldest[] = "command with the needle-\xE7\xB5\x82";
    history[0] = oldest;
    int32_t index = 1;
    while (index < MAX_HISTORY)
    {
        history[index] = ordinary;
        ++index;
    }
    history_count = MAX_HISTORY;
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        int32_t match_index = -1;
        if (rl_history_search("needle-\xE7\xB5\x82", -1, FT_TRUE, &match_index) != FT_ERR_SUCCESS
            || match_index != 0)
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    history_count = 0;
    if (iteration != ITERATIONS)
        return (10);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_session_state(ft_bool initialize)
{
    constexpr int32_t ITERATIONS = 10000;
    readline_state_t state;
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (initialize == FT_TRUE)
        {
            if (rl_initialize_state(&state) != FT_ERR_SUCCESS || rl_disable_raw_mode() != FT_ERR_SUCCESS)
                break ;
        }
        else
        {
            state.in_completion_mode = 1;
            rl_reset_completion_mode(&state);
        }
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    cma_free(state.buffer);
    rl_state_teardown_thread_safety(&state);
    if (iteration != ITERATIONS || state.in_completion_mode != 0 || state.current_match_count != 0)
        return (11);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_suggestion_registration()
{
    constexpr int32_t ITERATIONS = 32;
    char candidates[MAX_SUGGESTIONS][12];
    int32_t index = 0;
    while (index < MAX_SUGGESTIONS)
    {
        const int32_t length = std::snprintf(candidates[index], sizeof(candidates[index]),
            "command-%03d", MAX_SUGGESTIONS - 1 - index);
        if (length != 11)
            return (12);
        ++index;
    }
    std::chrono::duration<double, std::micro> elapsed{};
    int32_t iteration = 0;
    while (iteration < ITERATIONS)
    {
        index = 0;
        const auto start = std::chrono::steady_clock::now();
        while (index < MAX_SUGGESTIONS)
        {
            rl_add_suggestion(candidates[index]);
            ++index;
        }
        elapsed += std::chrono::steady_clock::now() - start;
        rl_add_suggestion(candidates[MAX_SUGGESTIONS / 2]);
        const int32_t registered = suggestion_count;
        rl_clear_suggestions();
        if (registered != MAX_SUGGESTIONS || suggestion_count != 0)
            return (13);
        ++iteration;
    }
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static ft_size_t history_write_calls;

static int64_t count_history_write(int32_t, const void *, ft_size_t length)
{
    ++history_write_calls;
    output_bytes += length;
    return (static_cast<int64_t>(length));
}

static int32_t measure_history_save(ft_bool use_json)
{
    int32_t iterations = 32;
    const char *backend = "plain-text";
    if (use_json == FT_TRUE)
    {
        backend = "json";
        iterations = 8;
    }
    const char *path = "benchmark-readline-history-save.tmp";
    if (rl_history_set_backend(backend, path) != FT_ERR_SUCCESS)
        return (14);
    char entry[] = "command with arguments and a mixed UTF-8 value caf\xC3\xA9";
    int32_t index = 0;
    while (index < MAX_HISTORY)
    {
        history[index] = entry;
        ++index;
    }
    history_count = MAX_HISTORY;
    output_bytes = 0;
    history_write_calls = 0;
    int32_t iteration = 0;
    su_set_write_syscall_hook(count_history_write);
    const auto start = std::chrono::steady_clock::now();
    while (iteration < iterations)
    {
        if (rl_history_save() != FT_ERR_SUCCESS)
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    su_reset_write_syscall_hook();
    history_count = 0;
    if (std::remove(path) != 0 || iteration != iterations
        || output_bytes == 0
        || (use_json == FT_FALSE && output_bytes != sizeof(entry) * MAX_HISTORY * iterations))
        return (15);
    std::printf("%.3f us/operation writes=%" PRIu64 "\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / iterations,
        static_cast<uint64_t>(history_write_calls / iterations));
    return (0);
}

static int32_t measure_history_load()
{
    constexpr int32_t ITERATIONS = 16;
    const char *path = "benchmark-readline-history-load.tmp";
    FILE *file = std::fopen(path, "wb");
    if (file == ft_nullptr)
        return (16);
    const char line[] = "command with arguments and a mixed UTF-8 value caf\xC3\xA9\n";
    int32_t index = 0;
    while (index < MAX_HISTORY && std::fwrite(line, 1, sizeof(line) - 1, file) == sizeof(line) - 1)
        ++index;
    if (std::fclose(file) != 0 || index != MAX_HISTORY
        || rl_history_set_storage_path(path) != FT_ERR_SUCCESS
        || rl_history_enable_auto_save(FT_FALSE) != FT_ERR_SUCCESS)
        return (17);
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (rl_history_load() != FT_ERR_SUCCESS || history_count != MAX_HISTORY)
            break ;
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const ft_bool correct = (iteration == ITERATIONS
        && ft_strncmp(line, history[0], sizeof(line) - 2) == 0
        && ft_strncmp(line, history[MAX_HISTORY - 1], sizeof(line) - 2) == 0);
    rl_clear_history();
    if (std::remove(path) != 0 || correct == FT_FALSE)
        return (18);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_disabled_history_auto_save()
{
    constexpr int32_t ITERATIONS = 2000000;
    if (rl_history_set_storage_path("benchmark-readline-disabled-history.tmp") != FT_ERR_SUCCESS
        || rl_history_enable_auto_save(FT_FALSE) != FT_ERR_SUCCESS)
        return (31);
    rl_update_history("benchmark command");
    if (history_count != 1)
        return (31);
    history_write_calls = 0;
    su_set_write_syscall_hook(count_history_write);
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        rl_history_notify_updated();
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    su_reset_write_syscall_hook();
    rl_clear_history();
    if (history_write_calls != 0)
        return (32);
    std::printf("%.3f ns/operation\n",
        std::chrono::duration<double, std::nano>(elapsed).count() / ITERATIONS);
    return (0);
}

static int32_t measure_insert_growth()
{
    constexpr int32_t ITERATIONS = 1000;
    constexpr int32_t CAPACITY = 1024;
    std::chrono::duration<double, std::micro> elapsed{};
    int32_t iteration = 0;
    readline_state_t state;
    while (iteration < ITERATIONS)
    {
        state.buffer = static_cast<char *>(cma_malloc(CAPACITY));
        if (state.buffer == ft_nullptr)
            return (19);
        state.buffer_size = CAPACITY;
        state.position = CAPACITY / 2;
        (void)ft_memset(state.buffer, 'x', CAPACITY - 1);
        state.buffer[CAPACITY - 1] = '\0';
        const auto start = std::chrono::steady_clock::now();
        const int32_t result = rl_state_insert_text(&state, "new");
        elapsed += std::chrono::steady_clock::now() - start;
        const ft_bool correct = (result == FT_ERR_SUCCESS && state.position == CAPACITY / 2 + 3
            && state.buffer[CAPACITY + 2] == '\0'
            && ft_strncmp(state.buffer + CAPACITY / 2, "newx", 4) == 0);
        cma_free(state.buffer);
        if (correct == FT_FALSE)
            return (20);
        ++iteration;
    }
    std::printf("%.3f us/operation\n", elapsed.count() / ITERATIONS);
    return (0);
}

static int32_t measure_cursor_navigation()
{
    constexpr int32_t ITERATIONS = 5000;
    constexpr int32_t LINE_LENGTH = 2048;
    constexpr int32_t CURSOR = LINE_LENGTH * 3 / 4;
    char line[LINE_LENGTH + 1];
    (void)ft_memset(line, 'x', LINE_LENGTH);
    line[LINE_LENGTH] = '\0';
    int32_t iteration = 0;
    const auto start_time = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        int32_t start = 0;
        int32_t end = 0;
        int32_t width = 0;
        if (rl_utf8_find_previous_grapheme(line, CURSOR, &start, &end, &width) != FT_ERR_SUCCESS
            || start != CURSOR - 1 || end != CURSOR || width != 1)
            return (21);
        if (rl_utf8_find_next_grapheme(line, CURSOR, &start, &end, &width) != FT_ERR_SUCCESS
            || start != CURSOR || end != CURSOR + 1 || width != 1)
            return (22);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start_time;
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / (2 * ITERATIONS));
    return (0);
}

static int32_t count_benchmark_key(readline_state_t *, const char *, void *context)
{
    ++*static_cast<int32_t *>(context);
    return (FT_ERR_SUCCESS);
}

static int32_t measure_key_dispatch()
{
    constexpr int32_t ITERATIONS = 20000;
    constexpr int32_t BINDINGS = 64;
    int32_t callback_count = 0;
    int32_t index = 0;
    while (index < BINDINGS)
    {
        if (rl_bind_key(2 * index + 32, count_benchmark_key, &callback_count) != FT_ERR_SUCCESS)
            return (23);
        ++index;
    }
    readline_state_t state;
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        ft_bool handled = FT_FALSE;
        if (rl_dispatch_custom_key(&state, "", 2 * (BINDINGS - 1) + 32, &handled) != FT_ERR_SUCCESS
            || handled != FT_TRUE)
            return (24);
        if (rl_dispatch_custom_key(&state, "", 'x' + 1, &handled) != FT_ERR_SUCCESS
            || handled != FT_FALSE)
            return (25);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (callback_count != ITERATIONS)
        return (26);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / (2 * ITERATIONS));
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count == 2 && ft_strcmp(arguments[1], "--dispatch") == 0)
        return (measure_key_dispatch());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--cursor") == 0)
        return (measure_cursor_navigation());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--insert-growth") == 0)
        return (measure_insert_growth());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--history-load") == 0)
        return (measure_history_load());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--history-autosave-disabled") == 0)
        return (measure_disabled_history_auto_save());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--history-save") == 0)
        return (measure_history_save(FT_FALSE));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--history-json") == 0)
        return (measure_history_save(FT_TRUE));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--register") == 0)
        return (measure_suggestion_registration());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--initialize") == 0)
        return (measure_session_state(FT_TRUE));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--reset") == 0)
        return (measure_session_state(FT_FALSE));
    if (argument_count == 2 && ft_strcmp(arguments[1], "--history") == 0)
        return (measure_history_search());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--complete") == 0)
        return (measure_completion_ranking());
    if (argument_count == 2 && ft_strcmp(arguments[1], "--width") == 0)
        return (measure_terminal_width());
    constexpr int32_t ITERATIONS = 1000;
    constexpr int32_t REPEATS = 256;
    char line[REPEATS * 8 + 2];
    int32_t length = 0;
    int32_t width = 0;
    if (argument_count == 2 && ft_strcmp(arguments[1], "--mixed") == 0)
    {
        const char sequence[] = "Ae\xCC\x81\xE4\xB8\xAD";
        while (length < REPEATS * static_cast<int32_t>(sizeof(sequence) - 1))
        {
            (void)ft_memcpy(line + length, sequence, sizeof(sequence) - 1);
            length += sizeof(sequence) - 1;
        }
        width = REPEATS * 4;
    }
    else
    {
        length = REPEATS * 8;
        (void)ft_memset(line, 'x', length);
        width = length;
    }
    line[length] = '\0';

    readline_state_t state;
    state.buffer = line;
    state.buffer_size = sizeof(line);
    if (argument_count == 2 && ft_strcmp(arguments[1], "--redraw") == 0)
        return (measure_redraw(state, length));
    state.position = length * 3 / 4;
    int32_t iteration = 0;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS)
    {
        if (rl_update_display_metrics(&state) != FT_ERR_SUCCESS)
            return (1);
        ++iteration;
    }
    const auto elapsed = std::chrono::steady_clock::now() - start;
    if (state.prev_buffer_length != length || state.prev_display_columns != width
        || state.display_pos != width * 3 / 4)
        return (2);
    std::printf("%.3f us/operation\n",
        std::chrono::duration<double, std::micro>(elapsed).count() / ITERATIONS);
    return (0);
}
