#include "../test_internal.hpp"
#include "../../Modules/Template/map.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_ft_map_insert_unique_relocates_only_after_successful_growth_allocation)
{
    ft_map<uint32_t, uint32_t> values(1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.enable_thread_safety());
    values.insert(11U, 21U);
    uint32_t relocations = 0;
    const auto relocate = [&relocations](Pair<uint32_t, uint32_t> &destination,
        Pair<uint32_t, uint32_t> &source) noexcept
    {
        destination.key = source.key;
        destination.value = source.value;
        source.value = 0;
        ++relocations;
        return ;
    };
    Pair<uint32_t, uint32_t> duplicate(11U, 99U);
    FT_ASSERT_EQ(FT_ERR_ALREADY_EXISTS, values.insert_unique(duplicate, relocate));
    FT_ASSERT_EQ(99U, duplicate.value);
    Pair<uint32_t, uint32_t> next(12U, 22U);
    cma_set_alloc_limit(1);
    const int32_t failed = values.insert_unique(next, relocate);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed);
    FT_ASSERT_EQ(22U, next.value);
    FT_ASSERT_EQ(1U, values.capacity());
    FT_ASSERT_EQ(21U, values.at(11U));
    FT_ASSERT_EQ(0U, relocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.insert_unique(next, relocate));
    FT_ASSERT_EQ(0U, next.value);
    FT_ASSERT_EQ(2U, relocations);
    const auto *entries = values.end() - 2;
    FT_ASSERT_EQ(11U, entries[0].key);
    FT_ASSERT_EQ(21U, entries[0].value);
    FT_ASSERT_EQ(12U, entries[1].key);
    FT_ASSERT_EQ(22U, entries[1].value);
    return (1);
}

FT_TEST(test_ft_map_erase_at_shifts_only_the_suffix)
{
    ft_map<uint32_t, uint32_t> values;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.enable_thread_safety());
    values.insert(11U, 21U);
    values.insert(12U, 22U);
    values.insert(13U, 23U);
    values.insert(14U, 24U);
    uint32_t relocations = 0;
    const auto relocate = [&relocations](Pair<uint32_t, uint32_t> &destination,
        Pair<uint32_t, uint32_t> &source) noexcept
    {
        destination.key = source.key;
        destination.value = source.value;
        ++relocations;
        return ;
    };
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, values.erase_at(4U, relocate));
    FT_ASSERT_EQ(4U, values.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.erase_at(3U, relocate));
    FT_ASSERT_EQ(0U, relocations);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.erase_at(1U, relocate));
    FT_ASSERT_EQ(1U, relocations);
    FT_ASSERT_EQ(2U, values.size());
    const Pair<uint32_t, uint32_t> *begin = values.end() - 2;
    FT_ASSERT_EQ(11U, begin[0].key);
    FT_ASSERT_EQ(21U, begin[0].value);
    FT_ASSERT_EQ(13U, begin[1].key);
    FT_ASSERT_EQ(23U, begin[1].value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.erase_at(0U, relocate));
    FT_ASSERT_EQ(2U, relocations);
    FT_ASSERT_EQ(23U, values.at(13U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.erase_at(0U, relocate));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, values.erase_at(0U, relocate));
    FT_ASSERT_EQ(2U, relocations);
    FT_ASSERT_EQ(FT_TRUE, values.empty());
    return (1);
}

FT_TEST(test_ft_map_lookup_accepts_borrowed_string_key)
{
    ft_map<ft_string, uint32_t> values;
    ft_string key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("callback_long"));
    values.insert(key, 12U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
    FT_ASSERT(values.find_as("callback") == values.end());
    Pair<ft_string, uint32_t> *entry = values.find_as("callback_long");
    FT_ASSERT(entry != values.end());
    FT_ASSERT_EQ(12U, entry->value);
    const ft_map<ft_string, uint32_t> &read_only = values;
    FT_ASSERT(read_only.find_as("callback_longer") == read_only.end());
    const Pair<ft_string, uint32_t> *read_only_entry = read_only.find_as("callback_long");
    FT_ASSERT(read_only_entry != read_only.end());
    FT_ASSERT_EQ(12U, read_only_entry->value);
    return (1);
}

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
    static inline int32_t failure_error = FT_ERR_NO_MEMORY;
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
            return (failure_error);
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
    map_growth_test_value::failure_error = FT_ERR_SYS_MUTEX_LOCK_FAILED;
    values.insert(3U, source);
    const int32_t insert_error = values.get_error();
    values.insert(3U, ft_move(source));
    const int32_t move_insert_error = values.get_error();
    map_growth_test_value::fail_on_value = 0U;
    map_growth_test_value::failure_error = FT_ERR_NO_MEMORY;
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, insert_error);
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, move_insert_error);
    FT_ASSERT_EQ(live_copies_before_growth, map_growth_test_value::live_copies);
    FT_ASSERT_EQ(2U, values.size());
    FT_ASSERT_EQ(10U, values.at(1U).value);
    FT_ASSERT_EQ(20U, values.at(2U).value);
    values.insert(3U, source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
    FT_ASSERT_EQ(30U, values.at(3U).value);
    return (1);
}

FT_TEST(test_ft_map_transactional_remove_preserves_order_on_copy_failure)
{
    ft_map<uint32_t, map_growth_test_value> values(5U);
    map_growth_test_value source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.initialize());
    uint32_t index = 1U;
    while (index <= 4U)
    {
        source.value = index * 10U;
        values.insert(index, source);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, values.get_error());
        ++index;
    }
    const auto *original = values.find(1U);
    const uint32_t original_count = map_growth_test_value::live_copies;
    map_growth_test_value::fail_on_value = 40U;
    map_growth_test_value::failure_error = FT_ERR_SYS_MUTEX_LOCK_FAILED;
    const int32_t error = values.remove_transactional(2U);
    map_growth_test_value::fail_on_value = 0U;
    map_growth_test_value::failure_error = FT_ERR_NO_MEMORY;
    FT_ASSERT_EQ(FT_ERR_SYS_MUTEX_LOCK_FAILED, error);
    FT_ASSERT_EQ(original_count, map_growth_test_value::live_copies);
    FT_ASSERT_EQ(4U, values.size());
    FT_ASSERT(values.find(1U) == original);
    FT_ASSERT_EQ(20U, values.at(2U).value);
    FT_ASSERT_EQ(30U, values.at(3U).value);
    FT_ASSERT_EQ(40U, values.at(4U).value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.remove_transactional(2U));
    FT_ASSERT_EQ(original_count - 1U, map_growth_test_value::live_copies);
    FT_ASSERT_EQ(5U, values.capacity());
    FT_ASSERT_EQ(3U, values.size());
    const auto *begin = values.end() - values.size();
    FT_ASSERT_EQ(1U, begin[0].key);
    FT_ASSERT_EQ(3U, begin[1].key);
    FT_ASSERT_EQ(4U, begin[2].key);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.remove_transactional(99U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, values.remove_transactional(4U));
    FT_ASSERT(values.find(1U) == begin);
    FT_ASSERT_EQ(2U, values.size());
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
FT_TEST(test_template_map_visit_value_reports_absent_and_callback_errors)
{
    ft_map<int32_t, int32_t> map;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, map.initialize());
    map.insert(4, 6);
    uint32_t calls = 0U;
    const auto edit = [&calls](int32_t &value) noexcept -> int32_t
    {
        ++calls;
        value += 1;
        return (FT_ERR_OUT_OF_RANGE);
    };
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, map.visit_value(9, edit));
    FT_ASSERT_EQ(0U, calls);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map.visit_value(4, edit));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, map.get_error());
    const auto &read_only = map;
    int32_t observed = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_only.visit_value(4, [&observed](const int32_t &value) noexcept
    {
        observed = value;
        return (FT_ERR_SUCCESS);
    }));
    FT_ASSERT_EQ(1U, calls);
    FT_ASSERT_EQ(7, observed);
    return (1);
}
