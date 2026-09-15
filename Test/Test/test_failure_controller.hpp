#ifndef TEST_FAILURE_CONTROLLER_HPP
#define TEST_FAILURE_CONTROLLER_HPP

#include <atomic>
#include <cstdint>
#include "../../Modules/Errno/errno.hpp"

enum test_failure_point : uint16_t
{
    TEST_FAILURE_CARD_GAME_CALLBACK = 0U,
    TEST_FAILURE_CARD_GAME_OPERATION = 1U,
    TEST_FAILURE_SCENARIO_STEP = 2U,
    TEST_FAILURE_POINT_COUNT = 3U
};

int32_t test_failure_controller_begin() noexcept;
int32_t test_failure_controller_end() noexcept;
int32_t test_failure_controller_fail_next(test_failure_point point) noexcept;
int32_t test_failure_controller_fail_after(test_failure_point point,
    uint64_t successful_calls) noexcept;
uint64_t test_failure_controller_attempts(test_failure_point point) noexcept;
uint64_t test_failure_controller_failures(test_failure_point point) noexcept;
ft_bool test_failure_controller_should_fail(test_failure_point point) noexcept;

#endif
