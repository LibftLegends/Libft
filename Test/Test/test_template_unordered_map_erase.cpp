#include "../test_internal.hpp"
#include "../../Modules/Template/unordered_map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <array>
#include <random>
#include <string>

struct s_clustered_map_key
{
    uint32_t value;

    ft_bool operator==(const s_clustered_map_key &other) const noexcept;
};

ft_bool s_clustered_map_key::operator==(const s_clustered_map_key &other) const noexcept
{
    return (this->value == other.value);
}

template <>
struct std::hash<s_clustered_map_key>
{
    std::size_t operator()(const s_clustered_map_key &key) const noexcept;
};

std::size_t std::hash<s_clustered_map_key>::operator()(const s_clustered_map_key &key) const noexcept
{
    return (14U + key.value % 7U);
}

FT_TEST(test_unordered_map_erase_preserves_lookup_in_wrapped_and_mixed_clusters)
{
    constexpr uint32_t KEY_COUNT = 97U;
    ft_unordered_map<s_clustered_map_key, uint32_t> values(17U);
    std::array<uint32_t, KEY_COUNT> expected{};
    std::mt19937 random(592U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    uint32_t step = 1U;
    while (step <= 3072U)
    {
        const uint32_t key = static_cast<uint32_t>(random() % KEY_COUNT);
        if (random() % 3U == 0U)
        {
            values.erase({key});
            expected[key] = 0U;
        }
        else
        {
            if (step % 2U == 0U)
                values[{key}] = step;
            else
                values.insert({key}, step);
            expected[key] = step;
        }
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
        if (step % 32U == 0U)
        {
            uint32_t candidate = 0U;
            while (candidate < KEY_COUNT)
            {
                auto found = values.find({candidate});
                if (expected[candidate] == 0U)
                    FT_ASSERT(found == values.end());
                else
                {
                    FT_ASSERT(found != values.end());
                    FT_ASSERT_EQ(expected[candidate], found->second);
                }
                ++candidate;
            }
        }
        ++step;
    }
    return (1);
}

FT_TEST(test_unordered_map_growth_preserves_a_value_referenced_from_another_entry)
{
    ft_unordered_map<uint32_t, std::string> values(2U);
    const std::string expected(120U, 'a');
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    values.insert(1U, expected);
    auto existing = values.find(1U);
    FT_ASSERT(existing != values.end());
    values.insert(3U, existing->second);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
    auto added = values.find(3U);
    FT_ASSERT(added != values.end());
    FT_ASSERT(added->second == expected);
    return (1);
}

FT_TEST(test_unordered_map_rejects_unrepresentable_storage_size)
{
    ft_unordered_map<uint64_t, uint64_t> values(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, values.initialize());
    return (1);
}
