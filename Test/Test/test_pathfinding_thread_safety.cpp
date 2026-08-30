#include "../test_internal.hpp"
#include "../../Modules/Game/game_pathfinding.hpp"
#include "../../Modules/Game/game_map3d.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/PThread/pthread.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct path_step_update_args
{
    game_path_step *step_pointer;
    int           iterations;
    int           result_code;
};

struct path_step_read_args
{
    game_path_step *step_pointer;
    int           iterations;
    int           result_code;
};

struct pathfinding_recalc_args
{
    game_pathfinding   *finder_pointer;
    const game_map3d   *grid_pointer;
    int               iterations;
    int               result_code;
};

struct pathfinding_read_args
{
    game_pathfinding   *finder_pointer;
    const game_map3d   *grid_pointer;
    int               iterations;
    int               result_code;
};

static void *path_step_update_task(void *argument)
{
    path_step_update_args *arguments;
    int index;

    arguments = static_cast<path_step_update_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        size_t coordinate_value;

        coordinate_value = static_cast<size_t>(index % 32);
        arguments->result_code = arguments->step_pointer->set_coordinates(coordinate_value,
                coordinate_value + 1, coordinate_value + 2);
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static void *path_step_read_task(void *argument)
{
    path_step_read_args *arguments;
    int index;

    arguments = static_cast<path_step_read_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        size_t x_value;
        size_t y_value;
        size_t z_value;

        x_value = arguments->step_pointer->get_x();
        if (arguments->step_pointer->get_error() != FT_ERR_SUCCESS)
        {
            (void)x_value;
            arguments->result_code = arguments->step_pointer->get_error();
            return (ft_nullptr);
        }
        y_value = arguments->step_pointer->get_y();
        if (arguments->step_pointer->get_error() != FT_ERR_SUCCESS)
        {
            (void)y_value;
            arguments->result_code = arguments->step_pointer->get_error();
            return (ft_nullptr);
        }
        z_value = arguments->step_pointer->get_z();
        if (arguments->step_pointer->get_error() != FT_ERR_SUCCESS)
        {
            (void)z_value;
            arguments->result_code = arguments->step_pointer->get_error();
            return (ft_nullptr);
        }
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static void *pathfinding_recalc_task(void *argument)
{
    pathfinding_recalc_args *arguments;
    int index;

    arguments = static_cast<pathfinding_recalc_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        ft_vector<game_path_step> thread_path;
        int thread_path_initialize_result;

        thread_path_initialize_result = thread_path.initialize();
        if (thread_path_initialize_result != FT_ERR_SUCCESS)
        {
            arguments->result_code = thread_path_initialize_result;
            return (ft_nullptr);
        }
        arguments->result_code = arguments->finder_pointer->recalculate_path(
                *arguments->grid_pointer,
                0, 0, 0,
                4, 4, 0,
                thread_path);
        if (arguments->result_code != FT_ERR_SUCCESS)
            return (ft_nullptr);
        arguments->finder_pointer->update_obstacle(0, 0, 0, 1);
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

static void *pathfinding_read_task(void *argument)
{
    pathfinding_read_args *arguments;
    int index;

    arguments = static_cast<pathfinding_read_args *>(argument);
    if (arguments == ft_nullptr)
        return (ft_nullptr);
    index = 0;
    while (index < arguments->iterations)
    {
        ft_vector<game_path_step> thread_path;
        int thread_path_initialize_result;
        int result_code;
        size_t path_index;

        thread_path_initialize_result = thread_path.initialize();
        if (thread_path_initialize_result != FT_ERR_SUCCESS)
        {
            arguments->result_code = thread_path_initialize_result;
            return (ft_nullptr);
        }
        result_code = arguments->finder_pointer->astar_grid(*arguments->grid_pointer,
                0, 0, 0,
                4, 4, 0,
                thread_path);
        if (result_code != FT_ERR_SUCCESS)
        {
            arguments->result_code = result_code;
            return (ft_nullptr);
        }
        path_index = 0;
        while (path_index < thread_path.size())
        {
            game_path_step &step = thread_path[path_index];

            (void)step.get_x();
            if (step.get_error() != FT_ERR_SUCCESS)
            {
                arguments->result_code = step.get_error();
                return (ft_nullptr);
            }
            (void)step.get_y();
            if (step.get_error() != FT_ERR_SUCCESS)
            {
                arguments->result_code = step.get_error();
                return (ft_nullptr);
            }
            (void)step.get_z();
            if (step.get_error() != FT_ERR_SUCCESS)
            {
                arguments->result_code = step.get_error();
                return (ft_nullptr);
            }
            path_index += 1;
        }
        index += 1;
    }
    arguments->result_code = FT_ERR_SUCCESS;
    return (ft_nullptr);
}

FT_TEST(test_path_step_thread_safety)
{
    game_path_step *primary_step;
    game_path_step copy_target;
    game_path_step assign_target;
    game_path_step move_target;
    path_step_update_args *update_arguments;
    path_step_read_args *read_arguments;
    pthread_t update_thread;
    pthread_t read_thread;
    int create_update_result;
    int create_read_result;
    int join_result;
    int index;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    const long join_timeout_ms = 30000;

    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    create_update_result = 1;
    create_read_result = 1;
    primary_step = new game_path_step();
    update_arguments = new path_step_update_args();
    read_arguments = new path_step_read_args();
    if (primary_step->initialize() != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "primary_step->initialize() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed == 0
        && primary_step->enable_thread_safety() != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "primary_step->enable_thread_safety() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (copy_target.initialize() != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "copy_target.initialize() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (assign_target.initialize() != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "assign_target.initialize() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (move_target.initialize() != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "move_target.initialize() == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    update_arguments->step_pointer = primary_step;
    update_arguments->iterations = 2048;
    update_arguments->result_code = FT_ERR_SUCCESS;
    read_arguments->step_pointer = primary_step;
    read_arguments->iterations = 2048;
    read_arguments->result_code = FT_ERR_SUCCESS;
    create_update_result = pt_thread_create(&update_thread, ft_nullptr,
            path_step_update_task, update_arguments);
    if (create_update_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_update_result == 0";
        failure_line = __LINE__;
    }
    create_read_result = pt_thread_create(&read_thread, ft_nullptr,
            path_step_read_task, read_arguments);
    if (create_read_result != 0)
    {
        test_failed = 1;
        failure_expression = "create_read_result == 0";
        failure_line = __LINE__;
    }
    index = 0;
    while (index < 256 && test_failed == 0)
    {
        game_path_step constructed;
        game_path_step moved_constructed;

        if (constructed.initialize(*primary_step) != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "constructed.initialize(*primary_step) == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        if (test_failed == 0
            && moved_constructed.initialize(ft_move(constructed)) != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "moved_constructed.initialize(ft_move(constructed)) == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        if (test_failed == 0)
            FT_ASSERT_EQ(FT_ERR_SUCCESS, copy_target.destroy());
        if (test_failed == 0
            && copy_target.initialize(moved_constructed) != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "copy_target.initialize(moved_constructed) == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        if (copy_target.get_error() != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "copy_target.get_error() == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        if (test_failed == 0)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, assign_target.destroy());
            if (assign_target.initialize(copy_target) != FT_ERR_SUCCESS)
            {
                test_failed = 1;
                failure_expression = "assign_target.initialize(copy_target) == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
            if (test_failed == 0 && assign_target.get_error() != FT_ERR_SUCCESS)
            {
                test_failed = 1;
                failure_expression = "assign_target.get_error() == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
        }
        if (test_failed == 0)
        {
            FT_ASSERT_EQ(FT_ERR_SUCCESS, move_target.destroy());
            if (move_target.initialize(ft_move(assign_target)) != FT_ERR_SUCCESS)
            {
                test_failed = 1;
                failure_expression = "move_target.initialize(ft_move(assign_target)) == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
            if (test_failed == 0 && move_target.get_error() != FT_ERR_SUCCESS)
            {
                test_failed = 1;
                failure_expression = "move_target.get_error() == FT_ERR_SUCCESS";
                failure_line = __LINE__;
            }
        }
        index += 1;
    }
    if (create_update_result == 0)
    {
        join_result = pt_thread_timed_join(update_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (create_read_result == 0)
    {
        join_result = pt_thread_timed_join(read_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (update_arguments->result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "update_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (read_arguments->result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "read_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed != 0)
    {
        delete read_arguments;
        delete update_arguments;
        delete primary_step;
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    delete read_arguments;
    delete update_arguments;
    delete primary_step;
    return (1);
}

FT_TEST(test_pathfinding_thread_safety)
{
    game_map3d *grid_pointer;
    game_pathfinding *primary_finder;
    game_pathfinding copy_target;
    game_pathfinding assign_target;
    game_pathfinding move_target;
    pathfinding_recalc_args *recalc_arguments;
    pathfinding_read_args *read_arguments;
    pthread_t recalc_thread;
    pthread_t read_thread;
    int create_recalc_result;
    int create_read_result;
    int join_result;
    int index;
    int test_failed;
    const char *failure_expression;
    int failure_line;
    ft_vector<game_path_step> seed_path;
    const long join_timeout_ms = 30000;

    test_failed = 0;
    failure_expression = ft_nullptr;
    failure_line = 0;
    grid_pointer = new game_map3d();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, grid_pointer->initialize(5, 5, 1, 0));
    primary_finder = new game_pathfinding();
    FT_ASSERT(primary_finder != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_finder->initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_finder->enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, seed_path.initialize());
    recalc_arguments = new pathfinding_recalc_args();
    read_arguments = new pathfinding_read_args();
    if (primary_finder->recalculate_path(*grid_pointer, 0, 0, 0, 4, 4, 0, seed_path)
        != FT_ERR_SUCCESS)
    {
        test_failed = 1;
        failure_expression = "primary_finder.recalculate_path(...) == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed == 0)
    {
        primary_finder->update_obstacle(0, 0, 0, 1);
        recalc_arguments->finder_pointer = primary_finder;
        recalc_arguments->grid_pointer = grid_pointer;
        recalc_arguments->iterations = 512;
        recalc_arguments->result_code = FT_ERR_SUCCESS;
        read_arguments->finder_pointer = primary_finder;
        read_arguments->grid_pointer = grid_pointer;
        read_arguments->iterations = 512;
        read_arguments->result_code = FT_ERR_SUCCESS;
    }
    if (test_failed == 0)
    {
        create_recalc_result = pt_thread_create(&recalc_thread, ft_nullptr,
                pathfinding_recalc_task, recalc_arguments);
        if (create_recalc_result != 0)
        {
            test_failed = 1;
            failure_expression = "create_recalc_result == 0";
            failure_line = __LINE__;
        }
    }
    if (test_failed == 0)
    {
        create_read_result = pt_thread_create(&read_thread, ft_nullptr,
                pathfinding_read_task, read_arguments);
        if (create_read_result != 0)
        {
            test_failed = 1;
            failure_expression = "create_read_result == 0";
            failure_line = __LINE__;
        }
    }
    index = 0;
    while (index < 128 && test_failed == 0)
    {
        ft_vector<game_path_step> scratch_path;
        int scratch_path_initialize_result;

        scratch_path_initialize_result = scratch_path.initialize();
        if (scratch_path_initialize_result != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "scratch_path_initialize_result == FT_ERR_SUCCESS";
            failure_line = __LINE__;
            break;
        }
        int recalculation_result = primary_finder->recalculate_path(*grid_pointer,
                0, 0, 0, 0, 0, 0, scratch_path);
        if (recalculation_result != FT_ERR_SUCCESS)
        {
            test_failed = 1;
            failure_expression = "recalculation_result == FT_ERR_SUCCESS";
            failure_line = __LINE__;
        }
        index += 1;
    }
    if (create_recalc_result == 0)
    {
        join_result = pt_thread_timed_join(recalc_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (create_read_result == 0)
    {
        join_result = pt_thread_timed_join(read_thread, ft_nullptr, join_timeout_ms);
        if (join_result != 0 && test_failed == 0)
        {
            test_failed = 1;
            failure_expression = "join_result == 0";
            failure_line = __LINE__;
        }
    }
    if (recalc_arguments->result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "recalc_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (read_arguments->result_code != FT_ERR_SUCCESS && test_failed == 0)
    {
        test_failed = 1;
        failure_expression = "read_arguments.result_code == FT_ERR_SUCCESS";
        failure_line = __LINE__;
    }
    if (test_failed != 0)
    {
        delete read_arguments;
        delete recalc_arguments;
        delete primary_finder;
        delete grid_pointer;
        ft_test_fail(failure_expression, __FILE__, failure_line);
        return (0);
    }
    delete read_arguments;
    delete recalc_arguments;
    delete primary_finder;
    delete grid_pointer;
    return (1);
}
