#include "../test_internal.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_su_locale_compare_default_ordering)
{
    int comparison_result;

    comparison_result = 0;
    FT_ASSERT_EQ(0, su_locale_compare("apple", "banana", ft_nullptr, &comparison_result));
    FT_ASSERT(comparison_result < 0);
    FT_ASSERT_EQ(0, su_locale_compare("banana", "banana", ft_nullptr, &comparison_result));
    FT_ASSERT_EQ(0, comparison_result);
    return (1);
}

FT_TEST(test_su_locale_compare_rejects_invalid_arguments)
{
    int comparison_result;

    comparison_result = 99;
    FT_ASSERT_EQ(-1, su_locale_compare(ft_nullptr, "banana", ft_nullptr, &comparison_result));
    FT_ASSERT_EQ(-1, su_locale_compare("apple", "banana", ft_nullptr, ft_nullptr));
    return (1);
}

FT_TEST(test_su_locale_compare_invalid_locale_sets_configuration_error)
{
    int comparison_result;

    comparison_result = 0;
    FT_ASSERT_EQ(-1, su_locale_compare("apple", "banana", "invalid_locale_name", &comparison_result));
    return (1);
}

FT_TEST(test_su_locale_casefold_lowercases_ascii_input)
{
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(0, su_locale_casefold("MiXeD CaSe", ft_nullptr, output));
    FT_ASSERT(output == "mixed case");
    return (1);
}

FT_TEST(test_su_locale_casefold_rejects_null_input)
{
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(-1, su_locale_casefold(ft_nullptr, ft_nullptr, output));
    return (1);
}

FT_TEST(test_su_locale_casefold_invalid_locale_sets_configuration_error)
{
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(-1, su_locale_casefold("Example", "invalid_locale_name", output));
    return (1);
}

FT_TEST(test_su_locale_casefold_supports_aliased_output_and_large_text)
{
    ft_string output;
    ft_string expected;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("MiXeD"));
    FT_ASSERT_EQ(0, su_locale_casefold(output.c_str(), "C", output));
    FT_ASSERT(output == "mixed");

    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.assign(257U, 'Z'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, expected.initialize(257U, 'z'));
    FT_ASSERT_EQ(0, su_locale_casefold(output.c_str(), "C", output));
    FT_ASSERT(output == expected);
    return (1);
}
