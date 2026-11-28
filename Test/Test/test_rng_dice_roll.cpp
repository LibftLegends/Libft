#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "test_rng_test_utils.hpp"
#include <limits>
#include <random>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_dice_roll_deterministic_sum)
{
    std::uniform_int_distribution<int> distribution(0, std::numeric_limits<int>::max());
    int expected_result;
    int actual_result;
    int index;

    ft_rng_test_seed_engine(1337u, "rng_dice_roll_sequence");
    actual_result = ft_dice_roll(4, 6);

    ft_rng_test_seed_engine(1337u, "rng_dice_roll_sequence");
    expected_result = 0;
    index = 0;
    while (index < 4)
    {
        int roll_value;

        roll_value = distribution(g_random_engine);
        expected_result += (roll_value % 6) + 1;
        index++;
    }
    FT_ASSERT_EQ(expected_result, actual_result);
    return (1);
}

FT_TEST(test_ft_dice_roll_zero_arguments_success)
{
    int result;

    result = ft_dice_roll(0, 0);
    FT_ASSERT_EQ(0, result);
    return (1);
}

FT_TEST(test_ft_dice_roll_invalid_arguments)
{
    int result;

    result = ft_dice_roll(0, 6);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);

    result = ft_dice_roll(2, 0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    return (1);
}

FT_TEST(test_ft_dice_roll_single_face_returns_number)
{
    int result;

    result = ft_dice_roll(5, 1);
    FT_ASSERT_EQ(5, result);
    return (1);
}

FT_TEST(test_ft_dice_roll_overflow_stops_at_the_same_engine_draw)
{
    constexpr uint32_t SEED = 1337U;
    constexpr int32_t MAXIMUM_DICE = 20;
    std::mt19937 reference(SEED);
    std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);
    int32_t expected_sum = 0;
    int32_t index = 0;
    while (index < MAXIMUM_DICE)
    {
        const int32_t roll = (distribution(reference) % INT32_MAX) + 1;
        if (expected_sum > INT32_MAX - roll)
            break ;
        expected_sum += roll;
        index += 1;
    }
    FT_ASSERT(index < MAXIMUM_DICE);
    const int32_t expected_next = distribution(reference);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_enable_random_engine_thread_safety());
    ft_seed_random_engine(SEED);
    const int32_t result = ft_dice_roll(MAXIMUM_DICE, INT32_MAX);
    const int32_t next_value = ft_random_int();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_disable_random_engine_thread_safety());
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, result);
    FT_ASSERT_EQ(expected_next, next_value);
    return (1);
}
