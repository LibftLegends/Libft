#include "../test_internal.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/Errno/errno.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct game_map3d_set_args
{
    game_map3d *map_pointer;
    size_t start_layer;
    size_t end_layer;
    int result_code;
};

static void *game_map3d_set_task(void *argument)
{
    game_map3d_set_args *arguments;
    size_t layer_index;

    arguments = static_cast<game_map3d_set_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    layer_index = arguments->start_layer;
    while (layer_index < arguments->end_layer)
    {
        size_t row_index;

        row_index = 0;
        while (row_index < 8)
        {
            size_t column_index;

            column_index = 0;
            while (column_index < 8)
            {
                arguments->map_pointer->set(column_index, row_index, layer_index,
                    static_cast<int>(arguments->start_layer + 1));
                column_index++;
            }
            row_index++;
        }
        layer_index++;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_game_map3d_concurrent_set_operations)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(8, 8, 8, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.get_error());
    pthread_t threads[4];
    game_map3d_set_args arguments[4];
    size_t thread_index;
    int create_result;
    int join_result;
    size_t created_thread_count;
    int test_failed;
    const char *failure_expression;
    int failure_line;

    created_thread_count = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    thread_index = 0;
    while (thread_index < 4)
    {
        arguments[thread_index].map_pointer = &map_instance;
        arguments[thread_index].start_layer = thread_index * 2;
        arguments[thread_index].end_layer = (thread_index + 1) * 2;
        arguments[thread_index].result_code = FT_ERR_SUCCESS;
        if (test_failed == 0)
        {
            create_result = pt_thread_create(&threads[thread_index], ft_nullptr,
                game_map3d_set_task, &arguments[thread_index]);
            if (create_result != 0 && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "create_result == 0";
                failure_line = __LINE__;
            }
            if (create_result == 0)
                created_thread_count++;
        }
        else
            threads[thread_index] = 0;
        thread_index++;
    }
    thread_index = 0;
    while (thread_index < created_thread_count)
    {
        join_result = pt_thread_join(threads[thread_index], ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
        if (join_result == 0)
        {
            if (arguments[thread_index].result_code != FT_ERR_SUCCESS && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "arguments[thread_index].result_code == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
        }
        thread_index++;
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    thread_index = 0;
    while (thread_index < 4)
    {
        size_t layer_index;

        layer_index = arguments[thread_index].start_layer;
        while (layer_index < arguments[thread_index].end_layer)
        {
            size_t row_index;

            row_index = 0;
            while (row_index < 8)
            {
                size_t column_index;

                column_index = 0;
                while (column_index < 8)
                {
                    int cell_value;

                    cell_value = map_instance.get(column_index, row_index, layer_index);
                    if (cell_value != static_cast<int>(arguments[thread_index].start_layer + 1))
                        return (0);
                    column_index++;
                }
                row_index++;
            }
            layer_index++;
        }
        thread_index++;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.get_error());
    return (1);
}

struct game_map3d_toggle_args
{
    game_map3d *map_pointer;
    size_t iterations;
    int result_code;
};

static void *game_map3d_toggle_task(void *argument)
{
    game_map3d_toggle_args *arguments;
    size_t iteration_index;

    arguments = static_cast<game_map3d_toggle_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    iteration_index = 0;
    while (iteration_index < arguments->iterations)
    {
        arguments->map_pointer->toggle_obstacle(3, 3, 3, ft_nullptr);
        iteration_index++;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_game_map3d_toggle_thread_safe)
{
    game_map3d map_instance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.initialize(6, 6, 6, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.get_error());
    pthread_t threads[3];
    game_map3d_toggle_args arguments[3];
    size_t thread_index;
    int create_result;
    int join_result;
    size_t created_thread_count;
    int test_failed;
    const char *failure_expression;
    int failure_line;

    created_thread_count = 0;
    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    thread_index = 0;
    while (thread_index < 3)
    {
        arguments[thread_index].map_pointer = &map_instance;
        arguments[thread_index].iterations = 4000;
        arguments[thread_index].result_code = FT_ERR_SUCCESS;
        if (test_failed == 0)
        {
            create_result = pt_thread_create(&threads[thread_index], ft_nullptr,
                game_map3d_toggle_task, &arguments[thread_index]);
            if (create_result != 0 && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "create_result == 0";
                failure_line = __LINE__;
            }
            if (create_result == 0)
                created_thread_count++;
        }
        else
            threads[thread_index] = 0;
        thread_index++;
    }
    thread_index = 0;
    while (thread_index < created_thread_count)
    {
        join_result = pt_thread_join(threads[thread_index], ft_nullptr);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
        if (join_result == 0)
        {
            if (arguments[thread_index].result_code != FT_ERR_SUCCESS && test_failed == 0)
            {
                test_failed = 1;
                failure_expression = "arguments[thread_index].result_code == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
        }
        thread_index++;
    }
    if (test_failed != 0)
    {
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    int final_value;

    final_value = map_instance.get(3, 3, 3);
    if (final_value != 0 && final_value != 1)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map_instance.get_error());
    return (1);
}
