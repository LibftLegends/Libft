#include "networking_test_hooks.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include <atomic>

namespace
{
    static std::atomic<ft_bool> g_active(FT_FALSE);
    static std::atomic<uint64_t> g_attempts[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {};
    static std::atomic<uint64_t> g_failure_calls[
        NETWORKING_TEST_FAILURE_POINT_COUNT] = {};

    static ft_bool networking_test_failure_valid_point(
        networking_test_failure_point point) noexcept
    {
        if (static_cast<uint8_t>(point)
            >= NETWORKING_TEST_FAILURE_POINT_COUNT)
            return (FT_FALSE);
        return (FT_TRUE);
    }
}

int32_t networking_test_failure_initialize() noexcept
{
    uint32_t index;

    g_active.store(FT_FALSE, std::memory_order_release);
    index = 0U;
    while (index < NETWORKING_TEST_FAILURE_POINT_COUNT)
    {
        g_attempts[index].store(0U, std::memory_order_release);
        g_failure_calls[index].store(0U, std::memory_order_release);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t networking_test_failure_begin() noexcept
{
    int32_t error_code;

    if (g_active.load(std::memory_order_acquire) != FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    error_code = networking_test_failure_initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    g_active.store(FT_TRUE, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t networking_test_failure_end() noexcept
{
    g_active.store(FT_FALSE, std::memory_order_release);
    return (networking_test_failure_initialize());
}

int32_t networking_test_failure_fail_next(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    return (networking_test_failure_fail_after(point, 0U));
}

int32_t networking_test_failure_fail_after(
    networking_test_failure_point point, uint64_t successful_calls) noexcept
{
    uint8_t point_index;

    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    point_index = static_cast<uint8_t>(point);
    g_failure_calls[point_index].store(
        g_attempts[point_index].load(std::memory_order_acquire)
            + successful_calls + 1U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

uint64_t networking_test_failure_attempt_count(
    networking_test_failure_point point) noexcept
{
    if (networking_test_failure_valid_point(point) == FT_FALSE)
        return (0U);
    return (g_attempts[static_cast<uint8_t>(point)].load(
        std::memory_order_acquire));
}

ft_bool networking_test_failure_should_fail(
    networking_test_failure_point point) noexcept
{
    uint8_t point_index;
    uint64_t call_number;
    uint64_t failure_call;

    if (networking_test_failure_valid_point(point) == FT_FALSE
        || g_active.load(std::memory_order_acquire) == FT_FALSE)
        return (FT_FALSE);
    point_index = static_cast<uint8_t>(point);
    call_number = g_attempts[point_index].fetch_add(
        1U, std::memory_order_acq_rel) + 1U;
    failure_call = g_failure_calls[point_index].load(
        std::memory_order_acquire);
    if (failure_call != 0U && failure_call == call_number)
        return (FT_TRUE);
    return (FT_FALSE);
}
