#include "../test_internal.hpp"
#include "../../Modules/Advanced/advanced.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "test_cma_failure_injection.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_adv_calloc_zeroes_memory)
{
    unsigned char *memory_pointer;

    memory_pointer = static_cast<unsigned char *>(
            adv_calloc(4, sizeof(unsigned char)));
    FT_ASSERT(memory_pointer != ft_nullptr);
    FT_ASSERT_EQ(0, memory_pointer[0]);
    FT_ASSERT_EQ(0, memory_pointer[1]);
    FT_ASSERT_EQ(0, memory_pointer[2]);
    FT_ASSERT_EQ(0, memory_pointer[3]);
    cma_free(memory_pointer);
    return (1);
}

FT_TEST(test_adv_calloc_rejects_invalid_sizes)
{
    FT_ASSERT_EQ(ft_nullptr, adv_calloc(0, 8));
    FT_ASSERT_EQ(ft_nullptr, adv_calloc(2, 0));
    FT_ASSERT_EQ(ft_nullptr, adv_calloc(FT_SYSTEM_SIZE_MAX, 2));
    return (1);
}

FT_TEST(test_adv_calloc_allocation_failure)
{
    test_cma_failure_controller controller;
    void *memory_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_initialize(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_begin(controller));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_fail_next(controller,
            TEST_CMA_FAILURE_ALLOCATE));
    memory_pointer = adv_calloc(8, sizeof(unsigned char));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        test_cma_failure_controller_end(controller));
    FT_ASSERT_EQ(ft_nullptr, memory_pointer);
    return (1);
}

FT_TEST(test_adv_strdup_duplicates_text)
{
    char *duplicate;

    duplicate = adv_strdup("hello");
    FT_ASSERT(duplicate != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("hello", duplicate));
    cma_free(duplicate);
    return (1);
}

FT_TEST(test_adv_strdup_rejects_null_input)
{
    FT_ASSERT_EQ(ft_nullptr, adv_strdup(ft_nullptr));
    return (1);
}

FT_TEST(test_adv_strdup_allocation_failure)
{
    char *duplicate;

    cma_set_alloc_limit(1);
    duplicate = adv_strdup("hello");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, duplicate);
    return (1);
}

FT_TEST(test_adv_strndup_truncates_to_maximum_length)
{
    char *duplicate;

    duplicate = adv_strndup("abcdef", 3);
    FT_ASSERT(duplicate != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("abc", duplicate));
    cma_free(duplicate);
    return (1);
}

FT_TEST(test_adv_strndup_handles_zero_and_null_input)
{
    char *duplicate;

    duplicate = adv_strndup("abcdef", 0);
    FT_ASSERT(duplicate != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("", duplicate));
    cma_free(duplicate);
    FT_ASSERT_EQ(ft_nullptr, adv_strndup(ft_nullptr, 3));
    return (1);
}

FT_TEST(test_adv_strndup_allocation_failure)
{
    char *duplicate;

    cma_set_alloc_limit(1);
    duplicate = adv_strndup("abcdef", 3);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, duplicate);
    return (1);
}

FT_TEST(test_adv_memdup_copies_raw_bytes)
{
    unsigned char source_buffer[5];
    unsigned char *duplicate_buffer;

    source_buffer[0] = 'A';
    source_buffer[1] = 0;
    source_buffer[2] = 'B';
    source_buffer[3] = 'C';
    source_buffer[4] = 'D';
    duplicate_buffer = static_cast<unsigned char *>(
            adv_memdup(source_buffer, sizeof(source_buffer)));
    FT_ASSERT(duplicate_buffer != ft_nullptr);
    FT_ASSERT_EQ('A', duplicate_buffer[0]);
    FT_ASSERT_EQ(0, duplicate_buffer[1]);
    FT_ASSERT_EQ('B', duplicate_buffer[2]);
    FT_ASSERT_EQ('C', duplicate_buffer[3]);
    FT_ASSERT_EQ('D', duplicate_buffer[4]);
    cma_free(duplicate_buffer);
    return (1);
}

FT_TEST(test_adv_memdup_handles_zero_and_null)
{
    void *zero_length_duplicate;

    zero_length_duplicate = adv_memdup(ft_nullptr, 0);
    FT_ASSERT(zero_length_duplicate != ft_nullptr);
    cma_free(zero_length_duplicate);
    FT_ASSERT_EQ(ft_nullptr, adv_memdup(ft_nullptr, 5));
    return (1);
}

FT_TEST(test_adv_memdup_allocation_failure)
{
    unsigned char source_buffer[4];
    void *duplicate_buffer;

    source_buffer[0] = 'x';
    source_buffer[1] = 'y';
    source_buffer[2] = 'z';
    source_buffer[3] = '!';
    cma_set_alloc_limit(1);
    duplicate_buffer = adv_memdup(source_buffer, sizeof(source_buffer));
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, duplicate_buffer);
    return (1);
}
