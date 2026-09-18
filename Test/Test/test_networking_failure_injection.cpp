#include "../test_internal.hpp"
#include "../../Modules/Networking/networking_test_hooks.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_networking_failure_injection_rejects_invalid_schedule)
{
    networking_test_failure_point invalid_point;

    invalid_point = NETWORKING_TEST_FAILURE_POINT_COUNT;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        networking_test_failure_fail_next(
            NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE,
        networking_test_failure_fail_next(invalid_point));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE,
        networking_test_failure_fail_after(
            NETWORKING_TEST_CONNECTION_ALLOCATE, UINT64_MAX));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_failure_injection_schedules_relative_failure)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        networking_test_failure_fail_after(
            NETWORKING_TEST_DATAGRAM_SEND, 1U));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(static_cast<uint64_t>(3U),
        networking_test_failure_attempt_count(
            NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    return (1);
}

FT_TEST(test_networking_failure_injection_metadata_and_reset)
{
    networking_test_failure_point point;
    const char *point_name;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_begin());
    point = NETWORKING_TEST_CONNECTION_ALLOCATE;
    while (point < NETWORKING_TEST_FAILURE_POINT_COUNT)
    {
        point_name = networking_test_failure_point_name(point);
        FT_ASSERT(point_name != ft_nullptr);
        FT_ASSERT(point_name[0] != '\0');
        point = static_cast<networking_test_failure_point>(
            static_cast<uint8_t>(point) + 1U);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT(networking_test_failure_should_fail(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(1U, networking_test_failure_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(0U, networking_test_failure_attempt_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(0U, networking_test_failure_count(
        NETWORKING_TEST_CONNECTION_ALLOCATE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_fail_next(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_reset_all());
    FT_ASSERT_EQ(0U, networking_test_failure_attempt_count(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(0U, networking_test_failure_count(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT(!networking_test_failure_should_fail(
        NETWORKING_TEST_DATAGRAM_SEND));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        networking_test_failure_reset(NETWORKING_TEST_FAILURE_POINT_COUNT));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, networking_test_failure_end());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        networking_test_failure_reset_all());
    return (1);
}
