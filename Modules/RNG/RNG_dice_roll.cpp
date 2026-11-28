#include "rng.hpp"
#include "rng_internal.hpp"
#include "../Printf/printf.hpp"

template <typename Draw>
static int32_t rng_sum_dice(int32_t number, int32_t faces, Draw draw, int32_t &value)
{
    int32_t sum = 0;
    int32_t index = 0;
    while (index < number)
    {
        const int32_t roll = (draw() % faces) + 1;
        if (sum > INT32_MAX - roll)
            return (FT_ERR_OUT_OF_RANGE);
        sum += roll;
        index += 1;
    }
    value = sum;
    return (FT_ERR_SUCCESS);
}

int32_t rng_dice_roll_checked(int32_t number, int32_t faces, int32_t &value)
{
    if (faces == 0 && number == 0)
    {
        value = 0;
        return (FT_ERR_SUCCESS);
    }
    if (faces < 1 || number < 1)
        return (FT_ERR_INVALID_ARGUMENT);
    if (faces == 1)
    {
        value = number;
        return (FT_ERR_SUCCESS);
    }
    ft_init_random_engine();
    int32_t error;
    if (rng_lock_random_engine_mutex() == FT_ERR_SUCCESS)
    {
        std::uniform_int_distribution<int32_t> distribution(0, INT32_MAX);
        error = rng_sum_dice(number, faces, [&distribution]()
        {
            return (distribution(g_random_engine));
        }, value);
        (void)rng_unlock_random_engine_mutex();
    }
    else
        error = rng_sum_dice(number, faces, ft_random_int, value);
    if (DEBUG == 1 && error == FT_ERR_SUCCESS)
        pf_printf_fd(2, "The dice rolled %d on %d faces with %d amount of dice\n",
                value, faces, number);
    return (error);
}

int32_t ft_dice_roll(int32_t number, int32_t faces)
{
    int32_t value;
    const int32_t error = rng_dice_roll_checked(number, faces, value);
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (value);
}
