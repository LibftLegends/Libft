#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_cpp_class_string_assigns_from_literal)
{
    ft_string target;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    ft_string &result = (target = "hello");
    FT_ASSERT(&result == &target);
    FT_ASSERT(target == "hello");
    return (1);
}

FT_TEST(test_cpp_class_string_assigns_from_pointer)
{
    ft_string target;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize("prefix"));
    const char *source = "world";

    target = source;
    FT_ASSERT(target == source);
    FT_ASSERT(target.c_str() != source);
    return (1);
}

FT_TEST(test_cpp_class_string_plus_equals_appends_string)
{
    ft_string base;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base.initialize("Hello"));
    ft_string suffix;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, suffix.initialize(" World"));

    ft_string &result = (base += suffix);
    FT_ASSERT(&result == &base);
    FT_ASSERT(base == "Hello World");
    FT_ASSERT(suffix == " World");
    return (1);
}

FT_TEST(test_cpp_class_string_char_prefix_concatenation)
{
    ft_string base;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, base.initialize("core"));
    ft_string prefixed;

    prefixed = '!' + base;
    FT_ASSERT(prefixed == "!core");
    FT_ASSERT(base == "core");
    return (1);
}

FT_TEST(test_cpp_class_string_self_append_preserves_source)
{
    ft_string target;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize("abcdefghijklmnop"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.append(target));
    FT_ASSERT(target == "abcdefghijklmnopabcdefghijklmnop");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.destroy());
    return (1);
}

FT_TEST(test_cpp_class_string_self_append_reuses_reserved_storage)
{
    ft_string value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize(64, 'x'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.resize_length(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    const char *storage = value.c_str();
    cma_set_alloc_limit(1);
    const int32_t result = value.append(value);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(storage, value.c_str());
    FT_ASSERT(value == "xxxxxxxx");
    return (1);
}

FT_TEST(test_cpp_class_string_raw_append_rebases_a_slice_when_growing)
{
    ft_string value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("abcdef"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.append("123"));
    const ft_size_t previous_capacity = value._capacity;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.append(value.c_str() + 2, 5));
    FT_ASSERT(value == "abcdef123cdef1");
    FT_ASSERT(value._capacity > previous_capacity);
    return (1);
}

FT_TEST(test_cpp_class_string_growth_failure_preserves_the_value)
{
    ft_string value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("abcd"));
    cma_set_alloc_limit(1);
    const int32_t append_error = value.append(value);
    const int32_t fill_error = value.assign(64, 'z');
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, append_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, fill_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, value.append("x", FT_SYSTEM_SIZE_MAX));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, value.resize_length(FT_SYSTEM_SIZE_MAX));
    FT_ASSERT(value == "abcd");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.erase(1, FT_SYSTEM_SIZE_MAX));
    FT_ASSERT(value == "a");
    return (1);
}

FT_TEST(test_ft_string_assign_handles_empty_and_self_with_locks)
{
    ft_string target;
    ft_string source;
    ft_string empty;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize("reserved text"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("short"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.assign(source));
    FT_ASSERT_EQ(5U, target.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.assign(target));
    FT_ASSERT_EQ(5U, target.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.assign(empty));
    FT_ASSERT_EQ(0U, target.size());
    FT_ASSERT_EQ('\0', target.c_str()[0]);
    FT_ASSERT_EQ(FT_TRUE, target.is_thread_safe());
    return (1);
}
