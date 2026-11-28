#include "../test_internal.hpp"
#include "../../Modules/ReadLine/readline_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_readline_utf8_measures_cursor_and_full_line_together)
{
    char line[] = "Ae\xCC\x81\xE4\xB8\xAD\t\x7F\xFF" "B";
    constexpr int32_t expected_columns[] = {0, 1, 2, 3, 2, 3, 4, 4, 4, 4, 5, 6};
    constexpr int32_t LINE_LENGTH = sizeof(line) - 1;
    readline_state_t state;
    state.buffer = line;
    int32_t position = 0;
    while (position <= LINE_LENGTH)
    {
        state.position = position;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_update_display_metrics(&state));
        FT_ASSERT_EQ(expected_columns[position], state.display_pos);
        FT_ASSERT_EQ(6, state.prev_display_columns);
        FT_ASSERT_EQ(LINE_LENGTH, state.prev_buffer_length);
        ++position;
    }
    state.position = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_update_display_metrics(&state));
    FT_ASSERT_EQ(0, state.position);
    FT_ASSERT_EQ(0, state.display_pos);
    state.position = LINE_LENGTH + 10;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_update_display_metrics(&state));
    FT_ASSERT_EQ(LINE_LENGTH, state.position);
    FT_ASSERT_EQ(6, state.display_pos);
    int32_t columns = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_compute_columns(line, &columns));
    FT_ASSERT_EQ(6, columns);
    return (1);
}

FT_TEST(test_readline_utf8_navigation_preserves_combining_and_invalid_byte_boundaries)
{
    const char line[] = "Ae\xCC\x81\xFF" "B";
    int32_t start = 0;
    int32_t end = 0;
    int32_t columns = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_next_grapheme(line, 1, &start, &end, &columns));
    FT_ASSERT_EQ(1, start);
    FT_ASSERT_EQ(4, end);
    FT_ASSERT_EQ(1, columns);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_previous_grapheme(line, 4, &start, &end, &columns));
    FT_ASSERT_EQ(1, start);
    FT_ASSERT_EQ(4, end);
    FT_ASSERT_EQ(1, columns);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_next_grapheme(line, 2, &start, &end, &columns));
    FT_ASSERT_EQ(4, start);
    FT_ASSERT_EQ(5, end);
    FT_ASSERT_EQ(1, columns);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, rl_utf8_find_previous_grapheme("", 1, &start, &end, &columns));
    return (1);
}

FT_TEST(test_readline_utf8_cursor_skips_ascii_prefix_without_crossing_combining_marks)
{
    const char line[] = "\xFF" "A\tBe\xCC\x81" "Z";
    int32_t start = 0;
    int32_t end = 0;
    int32_t width = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_previous_grapheme(line, 3, &start, &end, &width));
    FT_ASSERT_EQ(2, start);
    FT_ASSERT_EQ(3, end);
    FT_ASSERT_EQ(0, width);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_next_grapheme(line, 4, &start, &end, &width));
    FT_ASSERT_EQ(4, start);
    FT_ASSERT_EQ(7, end);
    FT_ASSERT_EQ(1, width);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_previous_grapheme(line, 7, &start, &end, &width));
    FT_ASSERT_EQ(4, start);
    FT_ASSERT_EQ(7, end);
    FT_ASSERT_EQ(1, width);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_previous_grapheme(line, 6, &start, &end, &width));
    FT_ASSERT_EQ(5, start);
    FT_ASSERT_EQ(6, end);
    FT_ASSERT_EQ(1, width);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rl_utf8_find_previous_grapheme(line, 99, &start, &end, &width));
    FT_ASSERT_EQ(7, start);
    FT_ASSERT_EQ(8, end);
    FT_ASSERT_EQ(1, width);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, rl_utf8_find_next_grapheme(line, 8, &start, &end, &width));
    return (1);
}
