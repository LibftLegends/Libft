#include "../test_internal.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_ft_map_integral_lookup_and_removal_preserve_insertion_order)
{
    constexpr uint32_t COUNT = 19U;
    ft_map<uint32_t, uint32_t> values(0U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    uint32_t index = 0U;
    while (index < COUNT)
    {
        values.insert(COUNT - index, index);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
        ++index;
    }
    values.insert(11U, 123U);
    FT_ASSERT_EQ(COUNT, values.size());
    FT_ASSERT_EQ(123U, values.at(11U));
    FT_ASSERT(values.find(0U) == values.end());
    values.remove(11U);
    const Pair<uint32_t, uint32_t> *entry = values.find(COUNT);
    const Pair<uint32_t, uint32_t> *end = values.end();
    FT_ASSERT(entry != end);
    uint32_t expected_key = COUNT;
    while (entry != end)
    {
        if (expected_key == 11U)
            --expected_key;
        FT_ASSERT_EQ(expected_key, entry->key);
        ++entry;
        --expected_key;
    }
    FT_ASSERT_EQ(0U, expected_key);
    ft_map<uint32_t, uint32_t> oversized(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, oversized.initialize());
    return (1);
}

struct map_growth_test_value
{
    static inline uint32_t fail_on_value = 0U;
    static inline uint32_t live_copies = 0U;
    uint32_t value;
    ft_bool tracks_lifecycle;

    map_growth_test_value() : value(0U), tracks_lifecycle(FT_FALSE)
    {
        return ;
    }

    ~map_growth_test_value()
    {
        if (this->tracks_lifecycle)
            --live_copies;
        return ;
    }

    int32_t initialize(const map_growth_test_value &source)
    {
        if (source.value == fail_on_value)
            return (FT_ERR_NO_MEMORY);
        this->value = source.value;
        this->tracks_lifecycle = FT_TRUE;
        ++live_copies;
        return (FT_ERR_SUCCESS);
    }
};

FT_TEST(test_ft_map_growth_failure_keeps_existing_entries)
{
    ft_map<uint32_t, map_growth_test_value> values(2U);
    map_growth_test_value source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    source.value = 10U;
    values.insert(1U, source);
    source.value = 20U;
    values.insert(2U, source);
    const uint32_t live_copies_before_growth = map_growth_test_value::live_copies;
    FT_ASSERT_EQ(2U, live_copies_before_growth);
    source.value = 30U;
    map_growth_test_value::fail_on_value = 20U;
    values.insert(3U, source);
    const int32_t insert_error = values.get_error();
    map_growth_test_value::fail_on_value = 0U;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, insert_error);
    FT_ASSERT_EQ(live_copies_before_growth, map_growth_test_value::live_copies);
    FT_ASSERT_EQ(2U, values.size());
    FT_ASSERT_EQ(10U, values.at(1U).value);
    FT_ASSERT_EQ(20U, values.at(2U).value);
    values.insert(3U, source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
    FT_ASSERT_EQ(30U, values.at(3U).value);
    return (1);
}

FT_TEST(test_ft_map_copy_reports_entry_failure_and_keeps_destination)
{
    ft_map<uint32_t, map_growth_test_value> source(3U);
    ft_map<uint32_t, map_growth_test_value> destination(3U);
    map_growth_test_value value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    value.value = 10U;
    source.insert(1U, value);
    value.value = 20U;
    source.insert(2U, value);
    value.value = 30U;
    destination.insert(3U, value);
    const uint32_t original_live_copies = map_growth_test_value::live_copies;
    map_growth_test_value::fail_on_value = 20U;
    const int32_t error = destination.copy_from(source);
    map_growth_test_value::fail_on_value = 0U;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, error);
    FT_ASSERT_EQ(original_live_copies, map_growth_test_value::live_copies);
    FT_ASSERT_EQ(1U, destination.size());
    FT_ASSERT_EQ(30U, destination.at(3U).value);
    ft_map<uint32_t, map_growth_test_value> fresh;
    map_growth_test_value::fail_on_value = 20U;
    const int32_t fresh_error = fresh.copy_from(source);
    map_growth_test_value::fail_on_value = 0U;
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, fresh_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, fresh.copy_from(source));
    FT_ASSERT_EQ(2U, fresh.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.copy_from(source));
    FT_ASSERT_EQ(2U, destination.size());
    FT_ASSERT_EQ(20U, destination.at(2U).value);
    return (1);
}
