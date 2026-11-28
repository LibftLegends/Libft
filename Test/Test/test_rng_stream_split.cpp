#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_rng_stream_seed_rejects_null_output)
{
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, rng_stream_seed(42, 7, ft_nullptr));
    return (1);
}

FT_TEST(test_rng_stream_seed_is_deterministic)
{
    uint64_t first_value = 0;
    uint64_t second_value = 0;
    uint64_t different_stream_value = 0;
    if (rng_stream_seed(123456789ULL, 99, &first_value) != 0)
        return (0);
    if (rng_stream_seed(123456789ULL, 99, &second_value) != 0)
        return (0);
    if (rng_stream_seed(123456789ULL, 100, &different_stream_value) != 0)
        return (0);
    if (first_value != second_value)
        return (0);
    if (different_stream_value == first_value)
        return (0);
    return (1);
}

FT_TEST(test_rng_stream_seed_sequence_reproducible)
{
    uint32_t buffer_one[5];
    uint32_t buffer_two[5];
    size_t index = 0;
    if (rng_stream_seed_sequence(777, 21, buffer_one, 5) != 0)
        return (0);
    if (rng_stream_seed_sequence(777, 21, buffer_two, 5) != 0)
        return (0);
    while (index < 5)
    {
        if (buffer_one[index] != buffer_two[index])
            return (0);
        index++;
    }
    return (1);
}

FT_TEST(test_rng_stream_seed_sequence_preserves_word_order_and_buffer_bounds)
{
    constexpr uint32_t GUARD = 0xabcdef01U;
    constexpr uint32_t EXPECTED[] = {
        0xc5877b3bU, 0xb9543a3dU, 0x51b188b7U, 0xa69ff9e0U, 0x8175926cU, 0x8be5acc3U
    };
    uint32_t buffer[8] = {GUARD, GUARD, GUARD, GUARD, GUARD, GUARD, GUARD, GUARD};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_stream_seed_sequence(777, 21, ft_nullptr, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_stream_seed_sequence(777, 21, buffer + 1U, 5));
    FT_ASSERT_EQ(GUARD, buffer[0]);
    FT_ASSERT_EQ(GUARD, buffer[6]);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rng_stream_seed_sequence(777, 21, buffer + 1U, 6));
    ft_size_t index = 0U;
    while (index < 6U)
    {
        FT_ASSERT_EQ(EXPECTED[index], buffer[index + 1U]);
        index += 1U;
    }
    FT_ASSERT_EQ(GUARD, buffer[0]);
    FT_ASSERT_EQ(GUARD, buffer[7]);
    return (1);
}

FT_TEST(test_rng_stream_seed_from_string_matches_manual_seed)
{
    uint64_t from_string = 0;
    uint64_t manual_value = 0;
    if (rng_stream_seed_from_string("example-seed", 33, &from_string) != 0)
        return (0);
    uint32_t base_seed = ft_random_seed("example-seed");
    if (rng_stream_seed(static_cast<uint64_t>(base_seed), 33, &manual_value) != 0)
        return (0);
    if (from_string != manual_value)
        return (0);
    return (1);
}

FT_TEST(test_rng_stream_seed_sequence_from_string_rejects_null)
{
    uint32_t buffer[2];
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT,
        rng_stream_seed_sequence_from_string(ft_nullptr, 3, buffer, 2));
    return (1);
}
