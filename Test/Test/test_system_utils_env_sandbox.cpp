#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "compatebility_system_test_hooks.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_su_environment_snapshot_capture_rejects_null)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, su_environment_snapshot_capture(ft_nullptr));
    return (1);
}

FT_TEST(test_su_environment_snapshot_failed_recapture_preserves_previous_values)
{
    const char *name = "LIBFT_SNAPSHOT_RECAPTURE_TEST";
    t_su_environment_snapshot snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv(name, "before", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_environment_snapshot_capture(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t unchanged_status = su_environment_snapshot_capture(&snapshot);
    (void)cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv(name, "after", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t changed_status = su_environment_snapshot_capture(&snapshot);
    (void)cma_set_alloc_limit(0U);
    ft_bool found_previous = FT_FALSE;
    ft_size_t index = 0U;
    while (index < snapshot.entries.size())
    {
        if (ft_strcmp(snapshot.entries[index].c_str(),
                "LIBFT_SNAPSHOT_RECAPTURE_TEST=before") == 0)
            found_previous = FT_TRUE;
        ++index;
    }
    (void)su_unsetenv(name);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unchanged_status);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, changed_status);
    FT_ASSERT_EQ(FT_TRUE, found_previous);
    return (1);
}

FT_TEST(test_su_environment_snapshot_restore_rejects_uninitialized_snapshot)
{
    t_su_environment_snapshot snapshot;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, su_environment_snapshot_restore(&snapshot));
    return (1);
}

FT_TEST(test_su_environment_snapshot_restore_does_not_unset_matching_variables)
{
    const char *name = "LIBFT_SNAPSHOT_UPDATE_TEST";
    t_su_environment_snapshot snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv(name, "saved", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_environment_snapshot_capture(&snapshot));
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv("libft_snapshot_update_test", "changed", 1));
#else
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv(name, "changed", 1));
#endif
    cmp_set_force_unsetenv_result(-1, 0);
    const int32_t status = su_environment_snapshot_restore(&snapshot);
    cmp_clear_force_unsetenv_result();
    const char *value = su_getenv(name);
    const ft_bool restored = value != ft_nullptr && ft_strcmp(value, "saved") == 0;
    (void)su_unsetenv(name);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT_EQ(FT_TRUE, restored);
    return (1);
}

FT_TEST(test_su_environment_sandbox_restore_can_retry_after_native_failure)
{
    const char *temporary = "LIBFT_SNAPSHOT_RESTORE_RETRY_TEST";
    (void)su_unsetenv(temporary);
    t_su_environment_snapshot snapshot;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_environment_sandbox_begin(&snapshot));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, su_setenv(temporary, "extra", 1));
    cmp_set_force_unsetenv_result(-1, 0);
    const int32_t failed = su_environment_sandbox_end(&snapshot);
    cmp_clear_force_unsetenv_result();
    const ft_bool still_present = su_getenv(temporary) != ft_nullptr;
    const int32_t retried = su_environment_sandbox_end(&snapshot);
    const ft_bool removed = su_getenv(temporary) == ft_nullptr;
    (void)su_unsetenv(temporary);
    FT_ASSERT_EQ(FT_ERR_IO, failed);
    FT_ASSERT_EQ(FT_TRUE, still_present);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, retried);
    FT_ASSERT_EQ(FT_TRUE, removed);
    return (1);
}

FT_TEST(test_su_environment_snapshot_restore_round_trip)
{
    const char  *variable_one = "FT_SU_SANDBOX_ONE";
    const char  *variable_two = "FT_SU_SANDBOX_TWO";
    const char  *variable_extra = "FT_SU_SANDBOX_EXTRA";
    t_su_environment_snapshot snapshot;
    char        *value;

    if (su_setenv(variable_one, "original_one", 1) != 0)
        return (0);
    if (su_setenv(variable_two, "original_two", 1) != 0)
    {
        su_unsetenv(variable_one);
        return (0);
    }
    if (su_environment_snapshot_capture(&snapshot) != 0)
    {
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        return (0);
    }
    if (su_setenv(variable_one, "mutated", 1) != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        return (0);
    }
    if (su_unsetenv(variable_two) != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        return (0);
    }
    if (su_setenv(variable_extra, "temporary", 1) != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        su_unsetenv(variable_extra);
        return (0);
    }
    if (su_environment_snapshot_restore(&snapshot) != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        su_unsetenv(variable_extra);
        return (0);
    }
    value = su_getenv(variable_one);
    if (value == ft_nullptr || ft_strcmp(value, "original_one") != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        su_unsetenv(variable_extra);
        return (0);
    }
    value = su_getenv(variable_two);
    if (value == ft_nullptr || ft_strcmp(value, "original_two") != 0)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        su_unsetenv(variable_extra);
        return (0);
    }
    value = su_getenv(variable_extra);
    if (value != ft_nullptr)
    {
        su_environment_snapshot_dispose(&snapshot);
        su_unsetenv(variable_one);
        su_unsetenv(variable_two);
        su_unsetenv(variable_extra);
        return (0);
    }
    su_environment_snapshot_dispose(&snapshot);
    su_unsetenv(variable_one);
    su_unsetenv(variable_two);
    su_unsetenv(variable_extra);
    return (1);
}

FT_TEST(test_su_environment_sandbox_end_restores_state)
{
    const char  *variable_name = "FT_SU_SANDBOX_AUTOCLEAR";
    t_su_environment_snapshot sandbox;

    su_unsetenv(variable_name);
    if (su_environment_sandbox_begin(&sandbox) != 0)
        return (0);
    if (su_setenv(variable_name, "sandboxed", 1) != 0)
    {
        su_environment_sandbox_end(&sandbox);
        su_unsetenv(variable_name);
        return (0);
    }
    if (su_environment_sandbox_end(&sandbox) != 0)
    {
        su_unsetenv(variable_name);
        return (0);
    }
    if (su_getenv(variable_name) != ft_nullptr)
    {
        su_unsetenv(variable_name);
        return (0);
    }
    return (1);
}
