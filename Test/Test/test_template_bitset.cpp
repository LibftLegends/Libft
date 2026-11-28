#include "../test_internal.hpp"
#include "../../Modules/CPP_class/bitset.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int bitset_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_bitset_get_error_returned = FT_FALSE;
static int32_t g_bitset_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_bitset_get_error_str_returned = FT_FALSE;
static const char *g_bitset_get_error_str_result = ft_nullptr;

static void bitset_get_error_uninitialised_operation(void)
{
    ft_bitset bitset_value(8);

    g_bitset_get_error_result = bitset_value.get_error();
    g_bitset_get_error_returned = FT_TRUE;
    return ;
}

static void bitset_get_error_str_uninitialised_operation(void)
{
    ft_bitset bitset_value(8);

    g_bitset_get_error_str_result = bitset_value.get_error_str();
    g_bitset_get_error_str_returned = FT_TRUE;
    return ;
}

FT_TEST(test_ft_bitset_basic_operations)
{
    ft_bitset bitset_value(16);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.initialize());

    FT_ASSERT_EQ(16UL, bitset_value.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.set(3);
    FT_ASSERT(bitset_value.test(3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.set(7);
    FT_ASSERT(bitset_value.test(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.reset(3);
    FT_ASSERT_EQ(false, bitset_value.test(3));
    FT_ASSERT(bitset_value.test(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.flip(7);
    FT_ASSERT_EQ(false, bitset_value.test(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.set(0);
    bitset_value.clear();
    FT_ASSERT_EQ(false, bitset_value.test(0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());
    return (1);
}

FT_TEST(test_ft_bitset_out_of_range_errors)
{
    ft_bitset bitset_value(8);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.initialize());

    bitset_value.set(12);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, bitset_value.get_error());

    FT_ASSERT_EQ(false, bitset_value.test(9));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, bitset_value.get_error());

    bitset_value.reset(15);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, bitset_value.get_error());

    bitset_value.set(4);
    FT_ASSERT(bitset_value.test(4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());
    return (1);
}

FT_TEST(test_ft_bitset_clear_and_set)
{
    ft_bitset bitset_value(12);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.initialize());

    bitset_value.set(1);
    bitset_value.set(8);
    FT_ASSERT(bitset_value.test(1));
    FT_ASSERT(bitset_value.test(8));

    bitset_value.clear();
    FT_ASSERT_EQ(false, bitset_value.test(1));
    FT_ASSERT_EQ(false, bitset_value.test(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());

    bitset_value.set(5);
    FT_ASSERT(bitset_value.test(5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());
    return (1);
}

FT_TEST(test_ft_bitset_copy_across_blocks_preserves_bits)
{
    constexpr ft_size_t LAST_BIT = sizeof(ft_size_t) * CHAR_BIT * 2;
    ft_bitset source;
    ft_bitset destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(LAST_BIT + 1));
    source.set(0);
    source.set(LAST_BIT);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(LAST_BIT + 1, destination.size());
    FT_ASSERT_EQ(FT_TRUE, destination.test(0));
    FT_ASSERT_EQ(FT_FALSE, destination.test(LAST_BIT - 1));
    FT_ASSERT_EQ(FT_TRUE, destination.test(LAST_BIT));
    destination.reset(0);
    FT_ASSERT_EQ(FT_TRUE, source.test(0));
    return (1);
}

FT_TEST(test_ft_bitset_maximum_size_reports_allocation_failure)
{
    ft_bitset bitset_value;
    cma_set_alloc_limit(1);
    const int32_t result = bitset_value.initialize(FT_SYSTEM_SIZE_MAX);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_BITSET_NO_MEMORY, result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.initialize(1));
    FT_ASSERT_EQ(FT_FALSE, bitset_value.test(0));
    return (1);
}

FT_TEST(test_ft_bitset_error_queries_follow_lifecycle_contract)
{
    ft_bitset bitset_value(8);

    g_bitset_get_error_returned = FT_FALSE;
    g_bitset_get_error_result = FT_ERR_SUCCESS;
    g_bitset_get_error_str_returned = FT_FALSE;
    g_bitset_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, bitset_expect_sigabrt(
        bitset_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_bitset_get_error_returned);
    FT_ASSERT_EQ(1, bitset_expect_sigabrt(
        bitset_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_bitset_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, bitset_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(bitset_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}
