#include "../test_internal.hpp"
#include "../../Modules/Game/game_achievement.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct game_goal_increment_args
{
    game_goal *goal_pointer;
    int      iterations;
    int      result_code;
};

struct game_goal_read_args
{
    game_goal *goal_pointer;
    int      iterations;
    int      result_code;
};

static void *game_goal_increment_task(void *argument)
{
    game_goal_increment_args *arguments;
    int index;

    arguments = static_cast<game_goal_increment_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        arguments->goal_pointer->add_progress(1);
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static void *game_goal_read_task(void *argument)
{
    game_goal_read_args *arguments;
    int index;

    arguments = static_cast<game_goal_read_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        int progress_value;
        int target_value;

        progress_value = arguments->goal_pointer->get_progress();
        (void)progress_value;
        target_value = arguments->goal_pointer->get_target();
        (void)target_value;
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_game_goal_thread_safety)
{
    game_goal primary_goal;
    game_goal_increment_args increment_arguments;
    game_goal_read_args read_arguments;
    pthread_t increment_thread;
    pthread_t read_thread;
    int create_increment_result;
    int create_read_result;
    int join_result;
    int index;
    int test_failed;
    const char *failure_expression;
    int failure_line;

    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_goal.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_goal.enable_thread_safety());
    primary_goal.set_target(128);
    primary_goal.set_progress(0);
    increment_arguments.goal_pointer = &primary_goal;
    increment_arguments.iterations = 1024;
    increment_arguments.result_code = FT_ERR_SUCCESS;
    read_arguments.goal_pointer = &primary_goal;
    read_arguments.iterations = 1024;
    read_arguments.result_code = FT_ERR_SUCCESS;
    create_increment_result = pt_thread_create(&increment_thread, ft_nullptr,
            game_goal_increment_task, &increment_arguments);
    if (create_increment_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_increment_result == 0";
        failure_line = __LINE__;
    }
    create_read_result = pt_thread_create(&read_thread, ft_nullptr,
            game_goal_read_task, &read_arguments);
    if (create_read_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_read_result == 0";
        failure_line = __LINE__;
    }
    (void)index;
    if (create_increment_result == 0)
    {
        join_result = pt_thread_join(increment_thread, ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (create_read_result == 0)
    {
        join_result = pt_thread_join(read_thread, ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (increment_arguments.result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "increment_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (read_arguments.result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "read_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_goal.get_error());
    return (1);
}
