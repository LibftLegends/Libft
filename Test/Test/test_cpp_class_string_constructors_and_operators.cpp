#include "../test_internal.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Template/move.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_cpp_class_string_initialize_copy_move_paths)
{
    ft_string source_value;
    ft_string copy_value;
    ft_string moved_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("alpha"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_value.initialize(source_value));
    FT_ASSERT_EQ(0, ft_strcmp("alpha", copy_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_value.initialize(ft_move(source_value)));
    FT_ASSERT_EQ(0, ft_strcmp("alpha", moved_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_string_initialize_replaces_existing_destination)
{
    ft_string source_value;
    ft_string destination_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.initialize("source"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.initialize("old"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.initialize(source_value));
    FT_ASSERT_EQ(0, ft_strcmp("source", destination_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_string_copy_keeps_binary_value_and_future_growth)
{
    const char bytes[] = { 'x', '\0', 'y' };
    ft_string source;
    ft_string copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(256, 'z'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.assign(bytes, sizeof(bytes)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(source._capacity, copy._capacity);
    FT_ASSERT_EQ(sizeof(bytes), copy.size());
    FT_ASSERT_EQ(0, ft_memcmp(bytes, copy.c_str(), sizeof(bytes)));
    FT_ASSERT_EQ('\0', copy.c_str()[sizeof(bytes)]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.resize_length(8));
    ft_size_t index = sizeof(bytes);
    while (index <= copy.size())
    {
        FT_ASSERT_EQ('\0', copy.c_str()[index]);
        ++index;
    }
    FT_ASSERT_EQ(sizeof(bytes), source.size());
    return (1);
}

FT_TEST(test_cpp_class_string_raw_assign_supports_overlap_and_preserves_failed_destination)
{
    ft_string value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.initialize("abcdef"));
    const char *suffix = value.c_str() + 2;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.assign(suffix, 4));
    FT_ASSERT(value == "cdef");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.assign(value.c_str(), 4));
    FT_ASSERT(value == "cdef");
    const char replacement[] = "a longer value that requires a new allocation";
    cma_set_alloc_limit(1);
    const int32_t result = value.assign(replacement, sizeof(replacement) - 1);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT(value == "cdef");
    return (1);
}

FT_TEST(test_cpp_class_string_raw_assign_rebases_alias_after_growth)
{
    ft_string value;
    char *owned = static_cast<char *>(cma_malloc(7));
    FT_ASSERT(owned != ft_nullptr);
    (void)ft_memcpy(owned, "abcdef", 7);
    const int32_t take_error = ft_string_take_owned_buffer(value, owned, 6);
    if (take_error != FT_ERR_SUCCESS)
        cma_free(owned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, take_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.assign(value.c_str(), value.size()));
    FT_ASSERT(value == "abcdef");
    FT_ASSERT(value._capacity > 6);
    return (1);
}

FT_TEST(test_cpp_class_string_find_and_compare_include_embedded_nul_bytes)
{
    const char payload[] = { 'a', '\0', 'b', '\0', 'c' };
    ft_string source;
    ft_string copy;
    ft_string needle;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.assign(payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, needle.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, needle.assign(payload + 1, 2));
    FT_ASSERT_EQ(1U, source.find(needle));
    FT_ASSERT_EQ(4U, source.find("c"));
    FT_ASSERT_EQ(0U, source.find(""));
    FT_ASSERT_EQ(ft_string::npos, needle.find(source));
    FT_ASSERT_EQ(FT_TRUE, source == copy);
    FT_ASSERT_EQ(FT_FALSE, source == "a");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.resize_length(4));
    FT_ASSERT_EQ(FT_FALSE, source == copy);
    return (1);
}

FT_TEST(test_cpp_class_string_proxy_assignment_can_retry_after_copy_failure)
{
    ft_string source;
    ft_string destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("alpha"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize("saved"));
    const ft_string_proxy result = (source + "-").operator+("beta").operator+('!');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result.get_error());
    cma_set_alloc_limit(1);
    destination = result;
    const int32_t assignment_error = destination.get_error();
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, assignment_error);
    FT_ASSERT(destination == "saved");
    destination = result;
    FT_ASSERT(destination == "alpha-beta!");
    FT_ASSERT(source == "alpha");
    return (1);
}

FT_TEST(test_cpp_class_string_copy_and_move_assignment)
{
    ft_string left_value;
    ft_string right_value;
    ft_string temporary_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_value.initialize("left"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_value.initialize("right"));
    left_value = right_value;
    FT_ASSERT_EQ(0, ft_strcmp("right", left_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, temporary_value.initialize("moved"));
    right_value = ft_move(temporary_value);
    FT_ASSERT_EQ(0, ft_strcmp("moved", right_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, temporary_value.destroy());
    return (1);
}

FT_TEST(test_cpp_class_string_operator_plus_and_plus_equal)
{
    ft_string left_value;
    ft_string right_value;
    ft_string combined_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_value.initialize("Hello"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_value.initialize("World"));
    left_value += " ";
    left_value += right_value;
    FT_ASSERT_EQ(0, ft_strcmp("Hello World", left_value.c_str()));
    combined_value = left_value + "!";
    FT_ASSERT_EQ(0, ft_strcmp("Hello World!", combined_value.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, left_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, right_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, combined_value.destroy());
    return (1);
}
