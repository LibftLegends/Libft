#include "../test_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

static char *g_test_dumb_console_pending_line;

static char *test_dumb_console_get_next_line(int32_t, ft_size_t)
{
    char *line = g_test_dumb_console_pending_line;
    g_test_dumb_console_pending_line = ft_nullptr;
    return (line);
}

#define get_next_line test_dumb_console_get_next_line
#define ft_read_line test_dumb_console_read_line
#define ft_print_char test_dumb_console_print_char
#define ft_print_string test_dumb_console_print_string
#define ft_print_line test_dumb_console_print_line
#include "../../Modules/DUMB/dumb_console.cpp"
#undef get_next_line
#undef ft_read_line
#undef ft_print_char
#undef ft_print_string
#undef ft_print_line

static void test_dumb_console_stage_line(const char *line)
{
    const ft_size_t size = ft_strlen_size_t(line) + 1;
    g_test_dumb_console_pending_line = static_cast<char *>(cma_malloc(size));
    if (g_test_dumb_console_pending_line != ft_nullptr)
        (void)ft_memcpy(g_test_dumb_console_pending_line, line, size);
    return ;
}

FT_TEST(test_dumb_console_reads_owned_line_under_allocation_limit)
{
    char buffer[5];

    test_dumb_console_stage_line("abcd\n");
    FT_ASSERT(g_test_dumb_console_pending_line != ft_nullptr);
    cma_set_alloc_limit(1);
    const int32_t result = test_dumb_console_read_line(buffer, sizeof(buffer));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_STR_EQ("abcd", buffer);
    test_dumb_console_stage_line("\n");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_dumb_console_read_line(buffer, sizeof(buffer)));
    FT_ASSERT_STR_EQ("", buffer);
    return (1);
}

FT_TEST(test_dumb_console_oversized_line_preserves_output)
{
    char buffer[] = "old";

    test_dumb_console_stage_line("long\n");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, test_dumb_console_read_line(buffer, sizeof(buffer)));
    FT_ASSERT_STR_EQ("old", buffer);
    FT_ASSERT_EQ(FT_ERR_END_OF_FILE, test_dumb_console_read_line(buffer, sizeof(buffer)));
    return (1);
}
