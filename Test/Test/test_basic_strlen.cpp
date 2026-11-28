#include "../test_internal.hpp"
#include "../../Modules/Advanced/advanced.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static_assert(ft_strlen("compile\0time") == 7);
static_assert(ft_strlen_size_t("compile\0time") == 7U);
static_assert(ft_strlen_size_t(nullptr) == 0U);

FT_TEST(test_basic_strlen_nullptr)
{
    FT_ASSERT_EQ(0, ft_strlen(ft_nullptr));
    return (1);
}

FT_TEST(test_basic_strlen_simple)
{
    FT_ASSERT_EQ(4, ft_strlen("test"));
    return (1);
}

FT_TEST(test_basic_strlen_embedded_null)
{
    char string[6];

    string[0] = 'a';
    string[1] = 'b';
    string[2] = '\0';
    string[3] = 'c';
    string[4] = 'd';
    string[5] = '\0';
    FT_ASSERT_EQ(2, ft_strlen(string));
    return (1);
}

FT_TEST(test_basic_strlen_size_t_large_buffer)
{
    static char buffer[2049];
    ft_size_t index;

    index = 0;
    while (index < 2048)
    {
        buffer[index] = 'x';
        index++;
    }
    buffer[2048] = '\0';
    FT_ASSERT_EQ(2048, ft_strlen_size_t(buffer));
    return (1);
}

FT_TEST(test_basic_strlen_size_t_nullptr)
{
    FT_ASSERT_EQ(0, ft_strlen_size_t(ft_nullptr));
    return (1);
}

FT_TEST(test_basic_strnlen_truncates_to_bound)
{
    FT_ASSERT_EQ(6, ft_strnlen("bounded-string", 6));
    return (1);
}

FT_TEST(test_basic_strnlen_returns_full_length)
{
    FT_ASSERT_EQ(5, ft_strnlen("short", 32));
    return (1);
}

FT_TEST(test_basic_strnlen_stops_at_embedded_null_before_bound)
{
    char string[6];

    string[0] = 'a';
    string[1] = 'b';
    string[2] = '\0';
    string[3] = 'c';
    string[4] = 'd';
    string[5] = '\0';
    FT_ASSERT_EQ(2, ft_strnlen(string, 6));
    return (1);
}

FT_TEST(test_basic_span_dup_copies_without_embedded_null_requirement)
{
    char buffer[4];
    char *duplicate;

    buffer[0] = 'x';
    buffer[1] = 'y';
    buffer[2] = 'z';
    buffer[3] = 'w';
    duplicate = ft_span_dup(buffer, 4);
    FT_ASSERT(duplicate != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strncmp(duplicate, "xyzw", 5));
    FT_ASSERT_EQ('\0', duplicate[4]);
    cma_free(duplicate);
    return (1);
}

FT_TEST(test_basic_span_dup_zero_length_non_null_input_returns_empty_string)
{
    char source[3];
    char *duplicate;

    source[0] = 'a';
    source[1] = 'b';
    source[2] = '\0';
    duplicate = ft_span_dup(source, 0);
    FT_ASSERT(duplicate != ft_nullptr);
    FT_ASSERT_EQ('\0', duplicate[0]);
    cma_free(duplicate);
    return (1);
}
