#include "test_failure_controller.hpp"

struct test_failure_controller_state
{
    std::atomic<uint8_t> active;
    std::atomic<uint64_t> attempts[TEST_FAILURE_POINT_COUNT];
    std::atomic<uint64_t> failure_targets[TEST_FAILURE_POINT_COUNT];
    std::atomic<uint64_t> failures[TEST_FAILURE_POINT_COUNT];
};

static test_failure_controller_state g_test_failure_controller;

static ft_bool test_failure_controller_valid_point(
    test_failure_point point) noexcept
{
    if (static_cast<uint16_t>(point) >= TEST_FAILURE_POINT_COUNT)
        return (FT_FALSE);
    return (FT_TRUE);
}

static void test_failure_controller_reset_points() noexcept
{
    uint16_t index;

    index = 0U;
    while (index < TEST_FAILURE_POINT_COUNT)
    {
        g_test_failure_controller.attempts[index].store(0U,
            std::memory_order_relaxed);
        g_test_failure_controller.failure_targets[index].store(0U,
            std::memory_order_relaxed);
        g_test_failure_controller.failures[index].store(0U,
            std::memory_order_relaxed);
        index += 1U;
    }
    return ;
}

int32_t test_failure_controller_begin() noexcept
{
    if (g_test_failure_controller.active.load(std::memory_order_acquire)
        == FT_TRUE)
        return (FT_ERR_ALREADY_INITIALISED);
    test_failure_controller_reset_points();
    if (g_test_failure_controller.active.exchange(FT_TRUE,
        std::memory_order_acq_rel) == FT_TRUE)
        return (FT_ERR_ALREADY_INITIALISED);
    return (FT_ERR_SUCCESS);
}

int32_t test_failure_controller_end() noexcept
{
    uint16_t index;

    if (g_test_failure_controller.active.exchange(FT_FALSE,
        std::memory_order_acq_rel) == FT_FALSE)
        return (FT_ERR_SUCCESS);
    index = 0U;
    while (index < TEST_FAILURE_POINT_COUNT)
    {
        g_test_failure_controller.failure_targets[index].store(0U,
            std::memory_order_release);
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
}

int32_t test_failure_controller_fail_after(test_failure_point point,
    uint64_t successful_calls) noexcept
{
    uint64_t attempt_count;

    if (test_failure_controller_valid_point(point) == FT_FALSE)
        return (FT_ERR_INVALID_ARGUMENT);
    if (g_test_failure_controller.active.load(std::memory_order_acquire)
        == FT_FALSE)
        return (FT_ERR_NOT_INITIALISED);
    attempt_count = g_test_failure_controller.attempts[point].load(
        std::memory_order_acquire);
    if (successful_calls == UINT64_MAX
        || attempt_count > UINT64_MAX - successful_calls - 1U)
        return (FT_ERR_OUT_OF_RANGE);
    g_test_failure_controller.failure_targets[point].store(
        attempt_count + successful_calls + 1U, std::memory_order_release);
    return (FT_ERR_SUCCESS);
}

int32_t test_failure_controller_fail_next(test_failure_point point) noexcept
{
    return (test_failure_controller_fail_after(point, 0U));
}

uint64_t test_failure_controller_attempts(test_failure_point point) noexcept
{
    if (test_failure_controller_valid_point(point) == FT_FALSE)
        return (0U);
    return (g_test_failure_controller.attempts[point].load(
        std::memory_order_acquire));
}

uint64_t test_failure_controller_failures(test_failure_point point) noexcept
{
    if (test_failure_controller_valid_point(point) == FT_FALSE)
        return (0U);
    return (g_test_failure_controller.failures[point].load(
        std::memory_order_acquire));
}

ft_bool test_failure_controller_should_fail(test_failure_point point) noexcept
{
    uint64_t attempt_number;
    uint64_t target;

    if (test_failure_controller_valid_point(point) == FT_FALSE
        || g_test_failure_controller.active.load(std::memory_order_acquire)
            == FT_FALSE)
        return (FT_FALSE);
    attempt_number = g_test_failure_controller.attempts[point].fetch_add(1U,
        std::memory_order_acq_rel) + 1U;
    target = g_test_failure_controller.failure_targets[point].load(
        std::memory_order_acquire);
    if (target == 0U || attempt_number != target)
        return (FT_FALSE);
    if (g_test_failure_controller.failure_targets[point].compare_exchange_strong(
        target, 0U, std::memory_order_acq_rel, std::memory_order_acquire)
        == FT_FALSE)
        return (FT_FALSE);
    g_test_failure_controller.failures[point].fetch_add(1U,
        std::memory_order_acq_rel);
    return (FT_TRUE);
}
