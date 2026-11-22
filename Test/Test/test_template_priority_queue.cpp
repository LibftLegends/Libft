#include "../test_internal.hpp"
#include "../../Modules/Template/priority_queue.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <functional>
#include <queue>
#include <utility>
#include <vector>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_priority_queue_push_pop_order)
{
    ft_priority_queue<int> queue_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.initialize());
    queue_instance.push(5);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    queue_instance.push(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    queue_instance.push(9);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());

    FT_ASSERT_EQ(false, queue_instance.empty());
    FT_ASSERT_EQ(3UL, queue_instance.size());
    FT_ASSERT_EQ(9, queue_instance.top());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());

    int first_value = queue_instance.pop();
    FT_ASSERT_EQ(9, first_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    FT_ASSERT_EQ(2UL, queue_instance.size());

    int second_value = queue_instance.pop();
    FT_ASSERT_EQ(5, second_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());

    int third_value = queue_instance.pop();
    FT_ASSERT_EQ(1, third_value);
    FT_ASSERT(queue_instance.empty());
    FT_ASSERT_EQ(0UL, queue_instance.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.destroy());
    return (1);
}

FT_TEST(test_ft_priority_queue_custom_comparator)
{
    ft_priority_queue<int, std::greater<int> > queue_instance(0, std::greater<int>());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.initialize());
    queue_instance.push(4);
    queue_instance.push(2);
    queue_instance.push(7);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    FT_ASSERT_EQ(3UL, queue_instance.size());
    FT_ASSERT_EQ(2, queue_instance.top());

    int first_value = queue_instance.pop();
    FT_ASSERT_EQ(2, first_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    FT_ASSERT_EQ(4, queue_instance.top());

    queue_instance.pop();
    queue_instance.pop();
    FT_ASSERT(queue_instance.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.destroy());
    return (1);
}

FT_TEST(test_ft_priority_queue_error_handling_and_moves)
{
    ft_priority_queue<int> queue_instance;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.initialize());
    int empty_value = queue_instance.pop();
    FT_ASSERT_EQ(0, empty_value);
    FT_ASSERT_EQ(FT_ERR_PRIORITY_QUEUE_EMPTY, queue_instance.get_error());

    int top_empty = queue_instance.top();
    FT_ASSERT_EQ(0, top_empty);
    FT_ASSERT_EQ(FT_ERR_PRIORITY_QUEUE_EMPTY, queue_instance.get_error());

    queue_instance.push(11);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());
    queue_instance.push(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.get_error());

    ft_priority_queue<int> recreated_queue;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.initialize());
    recreated_queue.push(11);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.get_error());
    recreated_queue.push(3);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.get_error());

    FT_ASSERT_EQ(false, recreated_queue.empty());
    FT_ASSERT_EQ(2UL, recreated_queue.size());
    FT_ASSERT_EQ(11, recreated_queue.top());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.get_error());

    int popped_value = recreated_queue.pop();
    FT_ASSERT_EQ(11, popped_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.get_error());

    recreated_queue.clear();
    FT_ASSERT(recreated_queue.empty());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue_instance.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, recreated_queue.destroy());
    return (1);
}

FT_TEST(test_ft_priority_queue_matches_reference_across_growth_and_duplicate_keys)
{
    ft_priority_queue<uint32_t, std::greater<uint32_t>> actual;
    std::priority_queue<uint32_t, std::vector<uint32_t>, std::greater<uint32_t>> expected;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, actual.initialize());
    uint32_t sample = 0U;
    while (sample < 4096U)
    {
        const uint32_t value = (sample * 131U) % 257U;
        actual.push(value);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, actual.get_error());
        expected.push(value);
        if (sample % 3U == 0U)
        {
            FT_ASSERT_EQ(expected.top(), actual.pop());
            expected.pop();
        }
        ++sample;
    }
    while (!expected.empty())
    {
        FT_ASSERT_EQ(expected.top(), actual.pop());
        expected.pop();
    }
    FT_ASSERT(actual.empty());
    return (1);
}

FT_TEST(test_ft_priority_queue_rejects_overflow_and_retries_failed_growth)
{
    ft_priority_queue<uint64_t> oversized(FT_SYSTEM_SIZE_MAX / sizeof(uint64_t) + 1U);
    FT_ASSERT_EQ(FT_ERR_PRIORITY_QUEUE_NO_MEMORY, oversized.initialize());
    ft_priority_queue<uint64_t> queue(1U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.initialize());
    queue.push(17U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.get_error());
    cma_set_alloc_limit(1U);
    queue.push(29U);
    const int32_t allocation_error = queue.get_error();
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, allocation_error);
    FT_ASSERT_EQ(1U, queue.size());
    FT_ASSERT_EQ(17U, queue.top());
    queue.push(29U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, queue.get_error());
    FT_ASSERT_EQ(29U, queue.pop());
    FT_ASSERT_EQ(17U, queue.pop());
    return (1);
}
