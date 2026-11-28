#include "../test_internal.hpp"
#include "../../Modules/Game/game_resistance.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct game_resistance_update_args
{
    game_resistance *resistance_pointer;
    int            iterations;
    int            result_code;
};

struct game_resistance_read_args
{
    game_resistance *resistance_pointer;
    int            iterations;
    int            result_code;
};

static void *game_resistance_update_task(void *argument)
{
    game_resistance_update_args *arguments;
    int index;

    arguments = static_cast<game_resistance_update_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        int percent_value;
        int flat_value;

        percent_value = index % 100;
        flat_value = index;
        arguments->result_code = arguments->resistance_pointer->set_values(percent_value, flat_value);
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static void *game_resistance_read_task(void *argument)
{
    game_resistance_read_args *arguments;
    int index;
    game_resistance copy;

    arguments = static_cast<game_resistance_read_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        int percent_value;
        int flat_value;

        arguments->result_code = arguments->resistance_pointer->get_values(percent_value, flat_value);
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        if (percent_value != flat_value % 100)
        {
            arguments->result_code = FT_ERR_INVALID_STATE;
            return (ft_nullptr);
        }
        arguments->result_code = copy.initialize(*arguments->resistance_pointer);
        if (arguments->result_code == FT_ERR_SUCCESS)
            arguments->result_code = copy.get_values(percent_value, flat_value);
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        if (percent_value != flat_value % 100)
        {
            arguments->result_code = FT_ERR_INVALID_STATE;
            return (ft_nullptr);
        }
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_game_resistance_thread_safety)
{
    game_resistance primary_resistance;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_resistance.initialize());
    game_resistance_update_args update_arguments;
    game_resistance_read_args read_arguments;
    pthread_t update_thread;
    pthread_t read_thread;
    int create_update_result;
    int create_read_result;
    int join_result;
    int index;
    int test_failed;
    const char *failure_expression;
    int failure_line;

    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    update_arguments.resistance_pointer = &primary_resistance;
    update_arguments.iterations = 128;
    update_arguments.result_code = FT_ERR_SUCCESS;
    read_arguments.resistance_pointer = &primary_resistance;
    read_arguments.iterations = 128;
    read_arguments.result_code = FT_ERR_SUCCESS;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_resistance.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_resistance.get_error());
    create_update_result = pt_thread_create(&update_thread, ft_nullptr,
            game_resistance_update_task, &update_arguments);
    if (create_update_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_update_result == 0";
        failure_line = __LINE__;
    }
    create_read_result = pt_thread_create(&read_thread, ft_nullptr,
            game_resistance_read_task, &read_arguments);
    if (create_read_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_read_result == 0";
        failure_line = __LINE__;
    }
    index = 0;
    while (index < 64 && test_failed == 0)
    {
        if (primary_resistance.set_values(index % 100, index) != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "primary_resistance.set_values(index % 100, index) == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        if (test_failed == 0 && primary_resistance.get_error() != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "primary_resistance.get_error() == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        index += 1;
    }
    if (create_update_result == 0)
    {
        join_result = pt_thread_join(update_thread, ft_nullptr);
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
    if (update_arguments.result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "update_arguments.result_code == FT_ERR_SUCCESS";
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
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_resistance.disable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_resistance.get_error());
    return (1);
}

FT_TEST(test_game_resistance_move_releases_source_mutex)
{
    game_resistance source;
    game_resistance destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.set_values(-11, 42));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, destination.is_thread_safe());
    int32_t percent_value = 0;
    int32_t flat_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.get_values(percent_value, flat_value));
    FT_ASSERT_EQ(-11, percent_value);
    FT_ASSERT_EQ(42, flat_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.get_values(percent_value, flat_value));
    FT_ASSERT_EQ(0, percent_value);
    FT_ASSERT_EQ(0, flat_value);
    return (1);
}
