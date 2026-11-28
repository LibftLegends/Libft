#include "../test_internal.hpp"
#include "../../Modules/Template/queue.hpp"
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

FT_TEST(test_ft_queue_transfers_and_releases_one_reusable_node)
{
    ft_queue<uint32_t> source;
    ft_queue<uint32_t> destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    ft_size_t allocations_before = 0U;
    ft_size_t frees_before = 0U;
    cma_get_stats(&allocations_before, &frees_before);
    source.enqueue(11U);
    FT_ASSERT_EQ(11U, source.dequeue());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    const uint32_t value = 23U;
    destination.enqueue(value);
    FT_ASSERT_EQ(value, destination.dequeue());
    destination.clear();
    ft_size_t allocations_after = 0U;
    ft_size_t frees_after = 0U;
    cma_get_stats(&allocations_after, &frees_after);
    FT_ASSERT_EQ(1U, allocations_after - allocations_before);
    FT_ASSERT_EQ(1U, frees_after - frees_before);
    FT_ASSERT(destination.empty());
    return (1);
}

using queue_type = ft_queue<int>;

static void enqueue_values(queue_type &queue_instance, const std::vector<int> &values)
{
    for (int const value : values)
        queue_instance.enqueue(value);
}

FT_TEST(test_ft_queue_move_constructor_rebuilds_mutex)
{
    queue_type source_queue;
    std::vector<int> values = {5, 9};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    FT_ASSERT_EQ(0, source_queue.enable_thread_safety());
    enqueue_values(source_queue, values);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.get_error());

    queue_type moved_queue;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_queue.move(source_queue));

    FT_ASSERT_EQ(true, moved_queue.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    FT_ASSERT_EQ(false, source_queue.is_thread_safe());
    FT_ASSERT_EQ(values.size(), moved_queue.size());
    for (int expected : values)
        FT_ASSERT_EQ(expected, moved_queue.dequeue());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_queue.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.destroy());
    return (1);
}

FT_TEST(test_ft_queue_move_assignment_rebuilds_mutex)
{
    queue_type destination_queue;
    queue_type source_queue;
    std::vector<int> source_values = {13, 21};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.initialize());
    enqueue_values(destination_queue, {1, 2});
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.get_error());
    FT_ASSERT_EQ(0, destination_queue.enable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    enqueue_values(source_queue, source_values);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.get_error());
    FT_ASSERT_EQ(0, source_queue.enable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.move(source_queue));

    FT_ASSERT_EQ(true, destination_queue.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    FT_ASSERT_EQ(false, source_queue.is_thread_safe());
    FT_ASSERT_EQ(source_values.size(), destination_queue.size());
    for (int expected : source_values)
        FT_ASSERT_EQ(expected, destination_queue.dequeue());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.destroy());
    return (1);
}

FT_TEST(test_ft_queue_move_preserves_disabled_thread_safety)
{
    queue_type source_queue;
    queue_type destination_queue;
    std::vector<int> values = {4, 6};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    enqueue_values(source_queue, values);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.get_error());
    FT_ASSERT_EQ(false, source_queue.is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.move(source_queue));
    FT_ASSERT_EQ(false, destination_queue.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    FT_ASSERT_EQ(false, source_queue.is_thread_safe());

    FT_ASSERT_EQ(values.size(), destination_queue.size());
    for (int expected : values)
        FT_ASSERT_EQ(expected, destination_queue.dequeue());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.destroy());
    return (1);
}

FT_TEST(test_ft_queue_move_allows_reusing_source_after_transfer)
{
    queue_type source_queue;
    queue_type destination_queue;
    std::vector<int> values = {18, 22};

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    enqueue_values(source_queue, values);
    FT_ASSERT_EQ(0, source_queue.enable_thread_safety());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.move(source_queue));
    for (int expected : values)
        FT_ASSERT_EQ(expected, destination_queue.dequeue());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.initialize());
    FT_ASSERT_EQ(0, source_queue.enable_thread_safety());
    FT_ASSERT(source_queue.is_thread_safe());
    source_queue.enqueue(30);
    FT_ASSERT_EQ(30, source_queue.dequeue());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_queue.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_queue.destroy());
    return (1);
}
