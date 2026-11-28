#include "../test_internal.hpp"
#include "../../Modules/Template/string_view.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include <atomic>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct string_view_thread_data
{
    ft_string_view<char> *shared;
    const ft_string_view<char> *source;
    std::atomic<bool> *running;
};

static void *string_view_assignment_worker(void *argument)
{
    string_view_thread_data *data;
    size_t index;

    data = static_cast<string_view_thread_data*>(argument);
    index = 0;
    while (index < 1000)
    {
        (void)data->shared->compare(*(data->source));
        index += 1;
    }
    data->running->store(false, std::memory_order_release);
    return (ft_nullptr);
}

static void *string_view_comparison_worker(void *argument)
{
    string_view_thread_data *data;

    data = static_cast<string_view_thread_data*>(argument);
    while (data->running->load(std::memory_order_acquire))
    {
        data->shared->compare(*(data->source));
        pt_thread_sleep(1);
    }
    return (ft_nullptr);
}

FT_TEST(test_string_view_basic)
{
    ft_string_view<char> view("hello");
    FT_ASSERT_EQ(5u, view.size());
    FT_ASSERT_EQ('h', view.data()[0]);
    return (1);
}

FT_TEST(test_string_view_default_constructed_empty)
{
    ft_string_view<char> view;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.initialize());
    FT_ASSERT_EQ(0u, view.size());
    FT_ASSERT(view.empty());
    return (1);
}

FT_TEST(test_string_view_compare_substr)
{
    ft_string_view<char> view("hello");
    ft_string_view<char> other_view("hello");
    FT_ASSERT_EQ(0, view.compare(other_view));
    ft_string_view<char> substring;
    ft_string_view<char> expected_view("ell");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, substring.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, view.substr(1, 3, substring));
    FT_ASSERT_EQ(0, substring.compare(expected_view));
    return (1);
}

FT_TEST(test_string_view_preserves_signed_order_after_equal_binary_prefix)
{
    signed char first[520] = {};
    signed char second[520] = {};
    first[517] = -1;
    second[517] = 1;
    ft_string_view<signed char> left(first, sizeof(first));
    ft_string_view<signed char> right(second, sizeof(second));
    ft_string_view<signed char> prefix(first, 517U);
    FT_ASSERT_EQ(-1, left.compare(right));
    FT_ASSERT_EQ(1, right.compare(left));
    FT_ASSERT_EQ(-1, prefix.compare(right));
    first[517] = 1;
    FT_ASSERT_EQ(0, left.compare(right));
    return (1);
}

FT_TEST(test_string_view_substr_oob)
{
    char    *buffer;
    buffer = static_cast<char*>(cma_malloc(6));
    buffer[0] = 'w';
    buffer[1] = 'o';
    buffer[2] = 'r';
    buffer[3] = 'l';
    buffer[4] = 'd';
    buffer[5] = '\0';
    ft_string_view<char> view(buffer);
    ft_string_view<char> substring;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, substring.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, view.substr(10, 2, substring));
    FT_ASSERT_EQ(0u, substring.size());
    FT_ASSERT_EQ(0u, substring.size());
    cma_free(buffer);
    return (1);
}

FT_TEST(test_string_view_thread_safety)
{
    ft_string_view<char> shared("initial");
    ft_string_view<char> source("updated");
    ft_string_view<char> expected("initial");
    std::atomic<bool> running(true);
    string_view_thread_data data;
    pthread_t assign_thread;
    pthread_t compare_thread;
    int32_t join_result;
    const long join_timeout_ms = 5000;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, shared.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    data.shared = &shared;
    data.source = &source;
    data.running = &running;
    if (pt_thread_create(&assign_thread, ft_nullptr,
            string_view_assignment_worker, &data) != 0)
        return (0);
    if (pt_thread_create(&compare_thread, ft_nullptr,
            string_view_comparison_worker, &data) != 0)
    {
        join_result = pt_thread_timed_join(assign_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0)
            (void)pt_thread_detach(assign_thread);
        return (0);
    }
    join_result = pt_thread_timed_join(assign_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(assign_thread);
    FT_ASSERT_EQ(0, join_result);
    join_result = pt_thread_timed_join(compare_thread, ft_nullptr, join_timeout_ms);
    if (join_result != 0)
        (void)pt_thread_detach(compare_thread);
    FT_ASSERT_EQ(0, join_result);
    FT_ASSERT_EQ(0, shared.compare(expected));
    shared.disable_thread_safety();
    source.disable_thread_safety();
    return (1);
}

FT_TEST(test_string_view_move_constructor_preserves_thread_safety_and_data)
{
    ft_string_view<char> source_view("moved");
    ft_string_view<char> *moved_view_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());

    moved_view_pointer = new ft_string_view<char>();
    FT_ASSERT(moved_view_pointer != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_view_pointer->move(source_view));
    FT_ASSERT_EQ(FT_TRUE, moved_view_pointer->is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_view_pointer->get_error());
    FT_ASSERT_EQ(5u, moved_view_pointer->size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_view_pointer->get_error());
    FT_ASSERT_EQ('m', moved_view_pointer->data()[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_view_pointer->get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.initialize("reset"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());
    FT_ASSERT_EQ(5u, source_view.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_view_pointer->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.destroy());
    delete moved_view_pointer;
    return (1);
}

FT_TEST(test_string_view_move_into_initialized_destination_preserves_source_thread_safety)
{
    ft_string_view<char> destination_view("old");
    ft_string_view<char> source_view("next");

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.get_error());
    FT_ASSERT_EQ(FT_FALSE, source_view.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.move(source_view));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.get_error());
    FT_ASSERT_EQ(FT_FALSE, destination_view.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.get_error());
    FT_ASSERT_EQ(4u, destination_view.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.get_error());
    FT_ASSERT_EQ('n', destination_view.data()[0]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.initialize("done"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());
    FT_ASSERT_EQ(4u, source_view.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.get_error());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_view.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_view.destroy());
    return (1);
}
