#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <climits>
#include <chrono>
#include <type_traits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static std::chrono::system_clock::time_point    g_mock_time_now;

static std::chrono::system_clock::time_point    test_time_now_hook(void)
{
    return (g_mock_time_now);
}

FT_TEST(test_time_now_ms_returns_hooked_value)
{
    int64_t                     result;
    std::chrono::milliseconds   expected_duration(123456);

    g_mock_time_now = std::chrono::system_clock::time_point(expected_duration);
    time_set_clock_now_hook(test_time_now_hook);
    result = time_now_ms();
    time_reset_clock_now_hook();
    {
        const auto expected_count = expected_duration.count();
        int64_t expected_value;

        if constexpr (std::is_same_v<decltype(expected_count), int64_t>)
            expected_value = expected_count;
        else
            expected_value = static_cast<int64_t>(expected_count);
        FT_ASSERT_EQ(expected_value, result);
    }
    return (1);
}

FT_TEST(test_time_now_ms_preserves_full_width_epoch_values)
{
    constexpr int64_t EPOCH_MS = 1700000000123;
    g_mock_time_now = std::chrono::system_clock::time_point(std::chrono::milliseconds(EPOCH_MS));
    time_set_clock_now_hook(test_time_now_hook);
    const int64_t modern = time_now_ms();
    const int64_t legacy = ft_time_ms();
    g_mock_time_now = std::chrono::system_clock::time_point(std::chrono::milliseconds(-EPOCH_MS));
    const int64_t before_epoch = time_now_ms();
    time_reset_clock_now_hook();
    FT_ASSERT_EQ(EPOCH_MS, modern);
    FT_ASSERT_EQ(EPOCH_MS, legacy);
    FT_ASSERT_EQ(-EPOCH_MS, before_epoch);
    return (1);
}
