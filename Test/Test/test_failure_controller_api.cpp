#include "../test_internal.hpp"
#include "test_failure_controller.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_failure_controller_metadata_and_reset)
{
    test_failure_point point;
    const char *point_name;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_begin());
    point = TEST_FAILURE_CARD_GAME_CALLBACK;
    while (point < TEST_FAILURE_POINT_COUNT)
    {
        point_name = test_failure_controller_point_name(point);
        FT_ASSERT(point_name != ft_nullptr);
        FT_ASSERT(point_name[0] != '\0');
        point = static_cast<test_failure_point>(
            static_cast<uint16_t>(point) + 1U);
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_fail_next(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT(test_failure_controller_should_fail(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(1U, test_failure_controller_failures(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(FT_TRUE, test_failure_controller_last_failure_point_name()
        != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, ft_strcmp(
        "card_game_callback",
        test_failure_controller_last_failure_point_name()) == 0);
    FT_ASSERT_EQ(1U, test_failure_controller_last_failure_occurrence());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_reset(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT(test_failure_controller_last_failure_point_name()
        == ft_nullptr);
    FT_ASSERT_EQ(0U, test_failure_controller_last_failure_occurrence());
    FT_ASSERT_EQ(0U, test_failure_controller_attempts(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(0U, test_failure_controller_failures(
        TEST_FAILURE_CARD_GAME_CALLBACK));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_fail_after(
        TEST_FAILURE_CARD_GAME_OPERATION, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_reset_all());
    FT_ASSERT_EQ(0U, test_failure_controller_attempts(
        TEST_FAILURE_CARD_GAME_OPERATION));
    FT_ASSERT_EQ(0U, test_failure_controller_failures(
        TEST_FAILURE_CARD_GAME_OPERATION));
    FT_ASSERT(!test_failure_controller_should_fail(
        TEST_FAILURE_CARD_GAME_OPERATION));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        test_failure_controller_reset(TEST_FAILURE_POINT_COUNT));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, test_failure_controller_end());
    FT_ASSERT_EQ(FT_ERR_NOT_INITIALISED,
        test_failure_controller_reset_all());
    FT_ASSERT(test_failure_controller_point_name(TEST_FAILURE_POINT_COUNT)
        == ft_nullptr);
    return (1);
}
