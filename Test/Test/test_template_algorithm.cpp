#include "../test_internal.hpp"
#include "../../Modules/Template/algorithm.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/RNG/rng_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "test_rng_test_utils.hpp"
#include <vector>
#include <array>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_sort_orders_values)
{
    std::vector<int> values;
    values.push_back(5);
    values.push_back(1);
    values.push_back(4);
    values.push_back(2);
    values.push_back(3);
    ft_sort(values.begin(), values.end());
    std::array<int, 5> expected = {1, 2, 3, 4, 5};
    size_t index;

    index = 0;
    while (index < expected.size())
    {
        FT_ASSERT_EQ(expected[index], values[index]);
        index++;
    }
    return (1);
}

FT_TEST(test_ft_sort_custom_comparator)
{
    std::vector<int> values;
    values.push_back(2);
    values.push_back(5);
    values.push_back(1);
    values.push_back(4);
    values.push_back(3);
    ft_sort(values.begin(), values.end(), [](int left, int right){ return (left > right); });
    std::array<int, 5> expected = {5, 4, 3, 2, 1};
    size_t index;

    index = 0;
    while (index < expected.size())
    {
        FT_ASSERT_EQ(expected[index], values[index]);
        index++;
    }
    return (1);
}

FT_TEST(test_ft_sort_avoids_quadratic_comparisons_on_large_random_access_ranges)
{
    constexpr uint32_t LENGTH = 4096U;
    std::array<uint32_t, LENGTH> values;
    uint32_t index = 0U;
    while (index < LENGTH)
    {
        values[index] = (index * 17U) % 257U;
        ++index;
    }
    uint32_t comparisons = 0U;
    ft_sort(values.begin(), values.end(), [&comparisons](uint32_t left, uint32_t right)
    {
        ++comparisons;
        return (left < right);
    });
    FT_ASSERT(std::is_sorted(values.begin(), values.end()));
    FT_ASSERT(comparisons < LENGTH * 32U);
    return (1);
}

FT_TEST(test_ft_binary_search_default)
{
    std::vector<int> values;
    values.push_back(1);
    values.push_back(3);
    values.push_back(5);
    values.push_back(7);
    values.push_back(9);

    FT_ASSERT(ft_binary_search(values.begin(), values.end(), 7));
    FT_ASSERT_EQ(false, ft_binary_search(values.begin(), values.end(), 4));
    return (1);
}

FT_TEST(test_ft_binary_search_custom_comparator)
{
    std::vector<int> values;
    values.push_back(9);
    values.push_back(7);
    values.push_back(5);
    values.push_back(3);
    values.push_back(1);

    FT_ASSERT(ft_binary_search(values.begin(), values.end(), 5,
            [](int left, int right){ return (left > right); }));
    FT_ASSERT_EQ(false, ft_binary_search(values.begin(), values.end(), 6,
            [](int left, int right){ return (left > right); }));
    return (1);
}

FT_TEST(test_ft_binary_search_empty_range)
{
    std::vector<int> values;

    FT_ASSERT_EQ(false, ft_binary_search(values.begin(), values.end(), 1));
    return (1);
}

FT_TEST(test_ft_shuffle_matches_manual_simulation)
{
    std::vector<int> expected;
    expected.push_back(1);
    expected.push_back(2);
    expected.push_back(3);
    expected.push_back(4);
    expected.push_back(5);
    if (!expected.empty())
    {
        size_t iterator_index;

        ft_rng_test_seed_engine(123u, "template_algorithm_shuffle");
        iterator_index = expected.size() - 1;
        while (iterator_index > 0)
        {
            size_t random_index;

            random_index = static_cast<size_t>(ft_random_int());
            random_index %= iterator_index + 1;
            int temporary_value = expected[iterator_index];
            expected[iterator_index] = expected[random_index];
            expected[random_index] = temporary_value;
            iterator_index--;
        }
    }
    std::vector<int> shuffled;
    shuffled.push_back(1);
    shuffled.push_back(2);
    shuffled.push_back(3);
    shuffled.push_back(4);
    shuffled.push_back(5);
    ft_rng_test_seed_engine(123u, "template_algorithm_shuffle");
    ft_shuffle(shuffled.begin(), shuffled.end());
    size_t index;

    index = 0;
    while (index < shuffled.size())
    {
        FT_ASSERT_EQ(expected[index], shuffled[index]);
        index++;
    }
    return (1);
}

FT_TEST(test_ft_reverse_basic)
{
    std::vector<int> values;
    values.push_back(1);
    values.push_back(2);
    values.push_back(3);
    values.push_back(4);
    values.push_back(5);
    ft_reverse(values.begin(), values.end());
    std::array<int, 5> expected = {5, 4, 3, 2, 1};
    size_t index;

    index = 0;
    while (index < expected.size())
    {
        FT_ASSERT_EQ(expected[index], values[index]);
        index++;
    }
    return (1);
}

FT_TEST(test_ft_reverse_single_element)
{
    std::vector<int> values;
    values.push_back(42);
    ft_reverse(values.begin(), values.end());
    FT_ASSERT_EQ(1UL, values.size());
    FT_ASSERT_EQ(42, values[0]);
    return (1);
}
