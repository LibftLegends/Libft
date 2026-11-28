#include "../test_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int su_test_health_success(void *context, ft_string &detail)
{
    int detail_error;

    (void)context;
    detail_error = detail.assign("ready", 5);
    if (detail_error != FT_ERR_SUCCESS)
        return (-1);
    return (0);
}

static int su_test_health_failure(void *context, ft_string &detail)
{
    int *failure_counter;
    int detail_error;

    failure_counter = static_cast<int *>(context);
    if (failure_counter != ft_nullptr)
        *failure_counter += 1;
    detail_error = detail.assign("database unavailable", 20);
    if (detail_error != FT_ERR_SUCCESS)
        return (-1);
    return (-1);
}

FT_TEST(test_su_health_register_and_run_success)
{
    t_su_health_check_result results[1];
    ft_size_t               count;

    su_health_clear_checks();
    if (su_health_register_check("ready", &su_test_health_success, ft_nullptr) != 0)
        return (0);
    count = 0;
    if (su_health_run_checks(results, 1, &count) != 0)
        return (0);
    if (count != 1)
        return (0);
    if (!(results[0].name == "ready"))
        return (0);
    if (!results[0].healthy)
        return (0);
    if (!(results[0].detail == "ready"))
        return (0);
    if (results[0].error_code != FT_ERR_SUCCESS)
        return (0);
    su_health_clear_checks();
    return (1);
}

FT_TEST(test_su_health_run_checks_reports_failure)
{
    t_su_health_check_result results[1];
    ft_size_t               count;
    int                      failure_counter;

    su_health_clear_checks();
    failure_counter = 0;
    if (su_health_register_check("database", &su_test_health_failure, &failure_counter) != 0)
        return (0);
    count = 0;
    if (su_health_run_checks(results, 1, &count) != -1)
        return (0);
    if (count != 1)
        return (0);
    if (!(results[0].name == "database"))
        return (0);
    if (results[0].healthy)
        return (0);
    if (!(results[0].detail == "database unavailable"))
        return (0);
    if (results[0].error_code != FT_ERR_INTERNAL)
        return (0);
    if (failure_counter != 1)
        return (0);
    su_health_clear_checks();
    return (1);
}

FT_TEST(test_su_health_run_check_targets_single_entry)
{
    t_su_health_check_result result;

    su_health_clear_checks();
    if (su_health_register_check("database", &su_test_health_success, ft_nullptr) != 0)
        return (0);
    if (su_health_register_check("database", &su_test_health_success, ft_nullptr) != -1)
        return (0);
    if (su_health_run_check("database", &result) != 0)
        return (0);
    if (!(result.name == "database"))
        return (0);
    if (!result.healthy)
        return (0);
    if (!(result.detail == "ready"))
        return (0);
    if (result.error_code != FT_ERR_SUCCESS)
        return (0);
    if (su_health_run_check("missing", &result) != -1)
        return (0);
    su_health_clear_checks();
    return (1);
}

FT_TEST(test_su_health_unregister_check_validates_name)
{
    su_health_clear_checks();
    if (su_health_unregister_check(ft_nullptr) != -1)
        return (0);
    return (1);
}

FT_TEST(test_su_health_unregister_check_removes_registered_entry)
{
    t_su_health_check_result result;

    su_health_clear_checks();
    if (su_health_register_check("cache", &su_test_health_success, ft_nullptr) != 0)
        return (0);
    if (su_health_run_check("cache", &result) != 0)
        return (0);
    if (!(result.name == "cache"))
        return (0);
    if (su_health_unregister_check("cache") != 0)
        return (0);
    if (su_health_run_check("cache", &result) != -1)
        return (0);
    su_health_clear_checks();
    return (1);
}

static int32_t su_test_health_count_only(void *context, ft_string &detail)
{
    (void)detail;
    ++*static_cast<int32_t *>(context);
    return (0);
}

static int32_t su_test_health_clear_during_check(void *context, ft_string &detail)
{
    su_health_clear_checks();
    return (su_test_health_count_only(context, detail));
}

FT_TEST(test_su_health_snapshot_keeps_order_when_callback_clears_registry)
{
    t_su_health_check_result results[2];
    int32_t callbacks = 0;
    ft_size_t count = 0;

    su_health_clear_checks();
    FT_ASSERT_EQ(0, su_health_register_check("first", su_test_health_clear_during_check, &callbacks));
    FT_ASSERT_EQ(0, su_health_register_check("second", su_test_health_count_only, &callbacks));
    FT_ASSERT_EQ(0, su_health_run_checks(results, 2, &count));
    FT_ASSERT_EQ(2u, count);
    FT_ASSERT_EQ(2, callbacks);
    FT_ASSERT(results[0].name == "first");
    FT_ASSERT(results[1].name == "second");
    FT_ASSERT_EQ(0, su_health_run_checks(ft_nullptr, 0, &count));
    FT_ASSERT_EQ(0u, count);
    return (1);
}

FT_TEST(test_su_health_snapshot_avoids_name_copies_and_rejects_small_buffer)
{
    t_su_health_check_result result;
    int32_t callbacks = 0;
    ft_size_t count = 0;

    su_health_clear_checks();
    FT_ASSERT_EQ(0, su_health_register_check("first-health-check", su_test_health_count_only, &callbacks));
    FT_ASSERT_EQ(0, su_health_register_check("second-health-check", su_test_health_count_only, &callbacks));
    cma_set_alloc_limit(1);
    const int32_t small_buffer_status = su_health_run_checks(&result, 1, &count);
    const ft_size_t required_count = count;
    const int32_t callbacks_after_failure = callbacks;
    const int32_t run_status = su_health_run_checks(ft_nullptr, 0, &count);
    cma_set_alloc_limit(0);
    su_health_clear_checks();
    FT_ASSERT_EQ(-1, small_buffer_status);
    FT_ASSERT_EQ(2u, required_count);
    FT_ASSERT_EQ(0, callbacks_after_failure);
    FT_ASSERT_EQ(0, run_status);
    FT_ASSERT_EQ(2u, count);
    FT_ASSERT_EQ(2, callbacks);
    return (1);
}
