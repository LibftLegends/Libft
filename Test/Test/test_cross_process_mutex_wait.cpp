#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_cross_process.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#if defined(_WIN32) || defined(_WIN64)
# include <cerrno>
# include <thread>

FT_TEST(test_cross_process_windows_mutex_timeout_preserves_unlocked_output)
{
    HANDLE mutex_handle = CreateMutexA(ft_nullptr, TRUE, ft_nullptr);
    FT_ASSERT(mutex_handle != ft_nullptr);
    cross_process_message message{};
    cmp_cross_process_mapping mapping{};
    mapping.mutex_address = &mutex_handle;
    cmp_cross_process_mutex_state state{};
    int32_t result = FT_ERR_SUCCESS;
    int32_t wait_error = 0;
    std::thread waiter([&]()
    {
        result = cmp_cross_process_lock_mutex(message, &mapping, &state);
        wait_error = errno;
        return ;
    });
    waiter.join();
    ReleaseMutex(mutex_handle);
    CloseHandle(mutex_handle);
    FT_ASSERT_EQ(cmp_map_system_error_to_ft(ETIMEDOUT), result);
    FT_ASSERT_EQ(ETIMEDOUT, wait_error);
    FT_ASSERT_EQ(FT_FALSE, state.owner_recovered);
    FT_ASSERT(state.platform_mutex == ft_nullptr);
    return (1);
}

FT_TEST(test_cross_process_windows_mutex_reports_abandonment)
{
    HANDLE mutex_handle = CreateMutexA(ft_nullptr, FALSE, ft_nullptr);
    FT_ASSERT(mutex_handle != ft_nullptr);
    DWORD owner_result = WAIT_FAILED;
    std::thread owner([&]()
    {
        owner_result = WaitForSingleObject(mutex_handle, 200U);
        return ;
    });
    owner.join();
    cross_process_message message{};
    cmp_cross_process_mapping mapping{};
    mapping.mutex_address = &mutex_handle;
    cmp_cross_process_mutex_state state{};
    const int32_t result = cmp_cross_process_lock_mutex(message, &mapping, &state);
    const ft_bool owner_recovered = state.owner_recovered;
    int32_t unlock_result = result;
    if (result == FT_ERR_SUCCESS)
        unlock_result = cmp_cross_process_unlock_mutex(message, &mapping, &state);
    CloseHandle(mutex_handle);
    FT_ASSERT_EQ(WAIT_OBJECT_0, owner_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(FT_TRUE, owner_recovered);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, unlock_result);
    return (1);
}
#endif
