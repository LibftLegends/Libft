#include "../test_internal.hpp"
#include "../../Modules/Template/set.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <random>
#include <set>
#include <string>

FT_TEST(test_ft_set_insert_remove_preserves_sorted_lookup_across_growth)
{
    ft_set<uint32_t> values(1U);
    std::set<uint32_t> expected;
    std::mt19937 random(381U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    uint32_t iteration = 0U;
    while (iteration < 1536U)
    {
        const uint32_t key = static_cast<uint32_t>(random() % 97U);
        if (random() % 2U == 0U)
        {
            const ft_bool existed = expected.erase(key) != 0U;
            values.remove(key);
            if (existed)
                FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
            else
                FT_ASSERT_EQ(FT_ERR_NOT_FOUND, values.get_error());
        }
        else
        {
            if (iteration % 2U == 0U)
                values.insert(key);
            else
                values.insert(uint32_t{key});
            FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
            expected.insert(key);
        }
        FT_ASSERT_EQ(expected.size(), values.size());
        uint32_t candidate = 0U;
        while (candidate < 97U)
        {
            const uint32_t *found = values.find(candidate);
            FT_ASSERT_EQ(expected.count(candidate) != 0U, found != ft_nullptr);
            if (found != ft_nullptr)
                FT_ASSERT_EQ(candidate, *found);
            ++candidate;
        }
        ++iteration;
    }
    return (1);
}

FT_TEST(test_ft_set_shifts_nontrivial_values_and_rejects_capacity_overflow)
{
    ft_set<uint64_t> oversized(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, oversized.initialize());
    const std::string first(80U, 'a');
    const std::string middle(80U, 'm');
    const std::string last(80U, 'z');
    ft_set<std::string> values;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    values.insert(last);
    values.insert(middle);
    values.insert(first);
    values.remove(middle);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
    FT_ASSERT(values.find(middle) == ft_nullptr);
    FT_ASSERT(values.find(first) != ft_nullptr);
    FT_ASSERT(values.find(last) != ft_nullptr);
    FT_ASSERT_EQ(2U, values.size());
    return (1);
}
