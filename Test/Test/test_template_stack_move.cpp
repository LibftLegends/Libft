#include "../test_internal.hpp"
#include "../../Modules/Template/stack.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <vector>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_stack_transfers_and_releases_one_reusable_node)
{
    ft_stack<uint32_t> source;
    ft_stack<uint32_t> destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    ft_size_t allocations_before = 0U;
    ft_size_t frees_before = 0U;
    cma_get_stats(&allocations_before, &frees_before);
    source.push(11U);
    FT_ASSERT_EQ(11U, source.pop());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    const uint32_t value = 23U;
    destination.push(value);
    FT_ASSERT_EQ(value, destination.pop());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    ft_size_t allocations_after = 0U;
    ft_size_t frees_after = 0U;
    cma_get_stats(&allocations_after, &frees_after);
    FT_ASSERT_EQ(1U, allocations_after - allocations_before);
    FT_ASSERT_EQ(1U, frees_after - frees_before);
    return (1);
}

using stack_type = ft_stack<int>;

static int stack_expect_sigabrt(void (*operation)(void))
{
    return (test_expect_sigabrt_signal(operation));
}

static ft_bool g_stack_get_error_returned = FT_FALSE;
static int32_t g_stack_get_error_result = FT_ERR_SUCCESS;
static ft_bool g_stack_get_error_str_returned = FT_FALSE;
static const char *g_stack_get_error_str_result = ft_nullptr;

static void stack_get_error_uninitialised_operation(void)
{
    stack_type stack_value;

    g_stack_get_error_result = stack_value.get_error();
    g_stack_get_error_returned = FT_TRUE;
    return ;
}

static void stack_get_error_str_uninitialised_operation(void)
{
    stack_type stack_value;

    g_stack_get_error_str_result = stack_value.get_error_str();
    g_stack_get_error_str_returned = FT_TRUE;
    return ;
}

static void push_values(stack_type &stack_instance, const std::vector<int> &values)
{
    ft_size_t index;

    index = 0;
    while (index < values.size())
    {
        stack_instance.push(values[index]);
        if (stack_instance.get_error() != FT_ERR_SUCCESS)
            return ;
        index += 1;
    }
    return ;
}

static std::vector<int> drain_stack(stack_type &stack_instance)
{
    std::vector<int> drained;
    while (!stack_instance.empty())
    {
        drained.push_back(stack_instance.pop());
        if (stack_instance.get_error() != FT_ERR_SUCCESS)
            break;
    }
    return (drained);
}

FT_TEST(test_ft_stack_move_constructor_rebuilds_mutex)
{
    stack_type source_stack;
    std::vector<int> values = {7, 11};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    push_values(source_stack, values);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.enable_thread_safety());
    stack_type destination_stack;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.move(source_stack));

    std::vector<int> drained = drain_stack(destination_stack);
    FT_ASSERT_EQ(values.size(), drained.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    FT_ASSERT_EQ(false, source_stack.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.destroy());
    return (1);
}

FT_TEST(test_ft_stack_move_assignment_rebuilds_mutex)
{
    stack_type destination_stack;
    stack_type source_stack;
    std::vector<int> source_values = {21, 34};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    push_values(destination_stack, {3, 4});
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.enable_thread_safety());

    push_values(source_stack, source_values);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.enable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.move(source_stack));

    std::vector<int> drained = drain_stack(destination_stack);
    FT_ASSERT_EQ(source_values.size(), drained.size());
    FT_ASSERT(destination_stack.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    FT_ASSERT_EQ(false, source_stack.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.destroy());
    return (1);
}

FT_TEST(test_ft_stack_move_preserves_disabled_thread_safety)
{
    stack_type source_stack;
    stack_type destination_stack;
    std::vector<int> values = {5, 9};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    push_values(source_stack, values);
    FT_ASSERT_EQ(false, source_stack.is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.move(source_stack));
    FT_ASSERT_EQ(false, destination_stack.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    FT_ASSERT_EQ(false, source_stack.is_thread_safe());
    std::vector<int> drained = drain_stack(destination_stack);
    FT_ASSERT_EQ(values.size(), drained.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.destroy());
    return (1);
}

FT_TEST(test_ft_stack_move_allows_reinitializing_source_mutex)
{
    stack_type source_stack;
    stack_type destination_stack;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    push_values(source_stack, {8, 12});
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.move(source_stack));
    destination_stack.clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.enable_thread_safety());
    source_stack.push(1);
    FT_ASSERT_EQ(1, source_stack.pop());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_stack.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_stack.destroy());
    return (1);
}

FT_TEST(test_ft_stack_error_queries_follow_lifecycle_contract)
{
    stack_type stack_value;

    g_stack_get_error_returned = FT_FALSE;
    g_stack_get_error_result = FT_ERR_SUCCESS;
    g_stack_get_error_str_returned = FT_FALSE;
    g_stack_get_error_str_result = ft_nullptr;
    FT_ASSERT_EQ(1, stack_expect_sigabrt(
        stack_get_error_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_stack_get_error_returned);
    FT_ASSERT_EQ(1, stack_expect_sigabrt(
        stack_get_error_str_uninitialised_operation));
    FT_ASSERT_EQ(FT_FALSE, g_stack_get_error_str_returned);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack_value.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack_value.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack_value.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, stack_value.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(stack_value.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}
