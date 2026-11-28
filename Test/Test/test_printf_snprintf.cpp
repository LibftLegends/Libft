#include "../test_internal.hpp"
#include "../../Modules/Printf/printf.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <cstdarg>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <errno.h>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

extern void pf_set_tmpfile_function(FILE *(*function)(void));
extern void pf_set_fflush_function(int (*function)(FILE *));
extern void pf_set_ftell_function(long (*function)(FILE *));
extern void pf_reset_tmpfile_function(void);
extern void pf_reset_fflush_function(void);
extern void pf_reset_ftell_function(void);

static int pf_vsnprintf_wrapper(char *string, size_t size, const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    int result = pf_vsnprintf(string, size, format, arguments);
    va_end(arguments);
    return (result);
}

static int std_vsnprintf_wrapper(char *string, size_t size, const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    int result = std::vsnprintf(string, size, format, arguments);
    va_end(arguments);
    return (result);
}

static FILE *pf_tmpfile_failure(void)
{
    return (static_cast<FILE *>(ft_nullptr));
}

static int pf_fflush_failure(FILE *stream)
{
    (void)stream;
    errno = EIO;
    return (-1);
}

static int64_t pf_ftell_failure(FILE *stream)
{
    (void)stream;
    errno = EIO;
    return (-1);
}

static int64_t pf_ftell_throw(FILE *stream)
{
    (void)stream;
    throw FT_ERR_IO;
}

static void build_repeated_string(char *buffer, size_t length, char character)
{
    size_t index;

    index = 0;
    while (index < length)
    {
        buffer[index] = character;
        index++;
    }
    buffer[length] = '\0';
    return ;
}

FT_TEST(test_pf_snprintf_null_string)
{
    int result = pf_snprintf(static_cast<char *>(ft_nullptr), 8, "%s", "noop");
    FT_ASSERT_EQ(-1, result);
    return (1);
}

FT_TEST(test_pf_snprintf_null_string_zero_size)
{
    int result = pf_snprintf(static_cast<char *>(ft_nullptr), 0, "%s", "noop");
    FT_ASSERT_EQ(4, result);
    return (1);
}

FT_TEST(test_pf_snprintf_null_format)
{
    char buffer[8];
    typedef int (*t_pf_snprintf_plain)(char *, ft_size_t, const char *, ...);
    t_pf_snprintf_plain call_pf_snprintf;

    buffer[0] = 'X';
    call_pf_snprintf = pf_snprintf;
    int result = call_pf_snprintf(buffer, sizeof(buffer), static_cast<const char *>(ft_nullptr));
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_vsnprintf_null_format)
{
    char buffer[8];

    buffer[0] = 'X';
    int result = pf_vsnprintf_wrapper(buffer, sizeof(buffer), static_cast<const char *>(ft_nullptr));
    FT_ASSERT_EQ(-1, result);
    return (1);
}

FT_TEST(test_pf_vsnprintf_null_string_zero_size)
{
    int result = pf_vsnprintf_wrapper(static_cast<char *>(ft_nullptr), 0, "%s", "noop");
    FT_ASSERT_EQ(4, result);
    return (1);
}

FT_TEST(test_pf_snprintf_tmpfile_failure)
{
    char buffer[8];

    buffer[0] = 'X';
    pf_set_tmpfile_function(pf_tmpfile_failure);
    int result = pf_snprintf(buffer, sizeof(buffer), "%s", "noop");
    pf_reset_tmpfile_function();
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_fflush_failure)
{
    char buffer[8];

    buffer[0] = 'X';
    errno = 0;
    pf_set_fflush_function(pf_fflush_failure);
    int result = pf_snprintf(buffer, sizeof(buffer), "%s", "noop");
    pf_reset_fflush_function();
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_ftell_failure)
{
    char buffer[8];

    buffer[0] = 'X';
    errno = 0;
    pf_set_ftell_function(pf_ftell_failure);
    int result = pf_snprintf(buffer, sizeof(buffer), "%s", "noop");
    pf_reset_ftell_function();
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_snprintf_clears_output_when_tell_hook_throws)
{
    char buffer[8] = "old";
    pf_set_ftell_function(pf_ftell_throw);
    const int32_t result = pf_snprintf(buffer, sizeof(buffer), "%s", "noop");
    pf_reset_ftell_function();
    FT_ASSERT_EQ(-1, result);
    FT_ASSERT_EQ('\0', buffer[0]);
    FT_ASSERT_EQ(4, pf_snprintf(buffer, sizeof(buffer), "%s", "noop"));
    return (1);
}

FT_TEST(test_pf_snprintf_matches_standard_exact_fit)
{
    char pf_buffer[32];
    char standard_buffer[32];

    std::memset(pf_buffer, 0, sizeof(pf_buffer));
    std::memset(standard_buffer, 0, sizeof(standard_buffer));

    int pf_result = pf_snprintf(pf_buffer, sizeof(pf_buffer), "%s %d", "value", 42);
    int standard_result = std::snprintf(standard_buffer, sizeof(standard_buffer), "%s %d", "value", 42);
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ(0, std::strcmp(standard_buffer, pf_buffer));
    return (1);
}

FT_TEST(test_pf_snprintf_matches_standard_truncation)
{
    char pf_buffer[6];
    char standard_buffer[6];
    char long_input[16];
    size_t long_length;

    std::memset(pf_buffer, 0, sizeof(pf_buffer));
    std::memset(standard_buffer, 0, sizeof(standard_buffer));
    long_length = 8;
    build_repeated_string(long_input, long_length, 't');

    int pf_result = pf_snprintf(pf_buffer, sizeof(pf_buffer), "%s", long_input);
    int standard_result = std::snprintf(standard_buffer, sizeof(standard_buffer), "%s", long_input);
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ(0, std::strcmp(standard_buffer, pf_buffer));
    return (1);
}

FT_TEST(test_pf_snprintf_matches_standard_zero_size)
{
    char pf_buffer[4];
    char standard_buffer[4];

    pf_buffer[0] = 'P';
    standard_buffer[0] = 'S';

    int pf_result = pf_snprintf(pf_buffer, 0, "%s", "noop");
    int standard_result = std::snprintf(standard_buffer, 0, "%s", "noop");
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ('P', pf_buffer[0]);
    FT_ASSERT_EQ('S', standard_buffer[0]);
    return (1);
}

FT_TEST(test_pf_vsnprintf_matches_standard_exact_fit)
{
    char pf_buffer[48];
    char standard_buffer[48];

    std::memset(pf_buffer, 0, sizeof(pf_buffer));
    std::memset(standard_buffer, 0, sizeof(standard_buffer));

    int pf_result = pf_vsnprintf_wrapper(pf_buffer, sizeof(pf_buffer), "%s %d %c", "value", 42, 'Z');
    int standard_result = std_vsnprintf_wrapper(standard_buffer, sizeof(standard_buffer), "%s %d %c", "value", 42, 'Z');
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ(0, std::strcmp(standard_buffer, pf_buffer));
    return (1);
}

FT_TEST(test_pf_vsnprintf_matches_standard_truncation)
{
    char pf_buffer[8];
    char standard_buffer[8];
    char long_input[32];
    size_t long_length;

    std::memset(pf_buffer, 0, sizeof(pf_buffer));
    std::memset(standard_buffer, 0, sizeof(standard_buffer));
    long_length = 16;
    build_repeated_string(long_input, long_length, 'v');

    int pf_result = pf_vsnprintf_wrapper(pf_buffer, sizeof(pf_buffer), "%s", long_input);
    int standard_result = std_vsnprintf_wrapper(standard_buffer, sizeof(standard_buffer), "%s", long_input);
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ(0, std::strcmp(standard_buffer, pf_buffer));
    return (1);
}

FT_TEST(test_pf_vsnprintf_matches_standard_float_general_format)
{
    char pf_buffer[64];
    char standard_buffer[64];

    std::memset(pf_buffer, 0, sizeof(pf_buffer));
    std::memset(standard_buffer, 0, sizeof(standard_buffer));

    int pf_result = pf_vsnprintf_wrapper(pf_buffer, sizeof(pf_buffer), "%.3g", 3.14159);
    int standard_result = std_vsnprintf_wrapper(standard_buffer, sizeof(standard_buffer), "%.3g", 3.14159);
    FT_ASSERT_EQ(standard_result, pf_result);
    FT_ASSERT_EQ(0, std::strcmp(standard_buffer, pf_buffer));
    return (1);
}

FT_TEST(test_pf_snprintf_counts_embedded_nul_and_truncated_bytes)
{
    char buffer[6] = {'x', 'x', 'x', 'x', 'x', 'x'};
    int32_t prefix_count = -1;
    FT_ASSERT_EQ(6, pf_snprintf(buffer, 5U, "A%cB%nCDE", 0, &prefix_count));
    FT_ASSERT_EQ(3, prefix_count);
    FT_ASSERT_EQ('A', buffer[0]);
    FT_ASSERT_EQ('\0', buffer[1]);
    FT_ASSERT_EQ('B', buffer[2]);
    FT_ASSERT_EQ('C', buffer[3]);
    FT_ASSERT_EQ('\0', buffer[4]);
    FT_ASSERT_EQ('x', buffer[5]);

    prefix_count = -1;
    FT_ASSERT_EQ(6, pf_snprintf(ft_nullptr, 0U, "A%cB%nCDE", 0, &prefix_count));
    FT_ASSERT_EQ(3, prefix_count);
    return (1);
}

FT_TEST(test_pf_snprintf_formats_conversions_across_stack_buffer_boundary)
{
    char buffer[257];
    FT_ASSERT_EQ(255, pf_snprintf(buffer, sizeof(buffer), "%0*d", 255, 7));
    FT_ASSERT_EQ('0', buffer[0]);
    FT_ASSERT_EQ('7', buffer[254]);
    FT_ASSERT_EQ('\0', buffer[255]);
    FT_ASSERT_EQ(256, pf_snprintf(buffer, sizeof(buffer), "%0*d", 256, 8));
    FT_ASSERT_EQ('8', buffer[255]);
    FT_ASSERT_EQ('\0', buffer[256]);
    FT_ASSERT_EQ(512, pf_snprintf(buffer, 8U, "%0*d", 512, 9));
    FT_ASSERT_EQ(0, ft_strcmp("0000000", buffer));
    return (1);
}

FT_TEST(test_pf_vsnprintf_preserves_literal_spans_and_positional_arguments)
{
    char buffer[32];
    FT_ASSERT_EQ(12, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "literal only"));
    FT_ASSERT_EQ(0, ft_strcmp("literal only", buffer));
    FT_ASSERT_EQ(13, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "[%2$*1$d] %% %3$s", 4, 7, "done"));
    FT_ASSERT_EQ(0, ft_strcmp("[   7] % done", buffer));

    int32_t count = -1;
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "before%n invalid%", &count));
    FT_ASSERT_EQ(-1, count);
    FT_ASSERT_EQ('\0', buffer[0]);
    return (1);
}

FT_TEST(test_pf_vsnprintf_uses_native_types_for_length_modifiers)
{
    char buffer[80];
    decltype(0L) counts[] = {-1L, 42L};
    FT_ASSERT_EQ(5, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "abc%ln%ls", &counts[0], L"ok"));
    FT_ASSERT_EQ(0, ft_strcmp("abcok", buffer));
    FT_ASSERT_EQ(3L, counts[0]);
    FT_ASSERT_EQ(42L, counts[1]);

    counts[0] = -1L;
    FT_ASSERT_EQ(3, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%2$ld%1$ln", &counts[0], -12L));
    FT_ASSERT_EQ(0, ft_strcmp("-12", buffer));
    FT_ASSERT_EQ(3L, counts[0]);
    FT_ASSERT_EQ(42L, counts[1]);

    std::intmax_t max_count = -1;
    std::ptrdiff_t difference_count = -1;
    FT_ASSERT_EQ(9, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%lc%jn:%zu:%td%tn",
        static_cast<std::wint_t>(L'X'), &max_count, static_cast<std::size_t>(99),
        static_cast<std::ptrdiff_t>(-123), &difference_count));
    FT_ASSERT_EQ(0, ft_strcmp("X:99:-123", buffer));
    FT_ASSERT_EQ(1, max_count);
    FT_ASSERT_EQ(9, difference_count);
    return (1);
}

FT_TEST(test_pf_vsnprintf_rejects_missing_positions_and_unrepresentable_width)
{
    char buffer[16];
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%2$d", 11, 22));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%1$*0$d", 11));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%1$.*0$d", 11));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%2147483647$d", 11));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%*d", FT_INT32_MIN, 1));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%2$*1$d", FT_INT32_MIN, 1));
    return (1);
}

FT_TEST(test_pf_vsnprintf_rejects_invalid_native_specifiers_before_output)
{
    char buffer[32];
    int32_t count = -1;
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "before%n %Ls", &count, "text"));
    FT_ASSERT_EQ(-1, count);
    FT_ASSERT_EQ('\0', buffer[0]);
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%Ld", 123));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%lls", "text"));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%2147483648d", 1));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%.2147483648d", 1));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%1$*2147483648$d", 1));
    FT_ASSERT_EQ(-1, pf_vsnprintf_wrapper(buffer, sizeof(buffer), "%1$.*2147483648$d", 1));
    return (1);
}
