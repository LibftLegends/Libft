#include "../test_internal.hpp"
#include "../../Modules/Sink/sink.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include <atomic>
#include <thread>

FT_TEST(test_sink_record_message_and_retrieve)
{
    sink_record record;
    ft_size_t record_count;

    sink_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(12, "first message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(34, "second message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(0, &record));
    FT_ASSERT_EQ(12, record.level);
    FT_ASSERT_EQ(0, ft_strcmp(record.message, "first message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(1, &record));
    FT_ASSERT_EQ(34, record.level);
    FT_ASSERT_EQ(0, ft_strcmp(record.message, "second message"));
    sink_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), record_count);
    return (1);
}

FT_TEST(test_sink_rejects_null_message)
{
    sink_clear();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sink_record_message(1, ft_nullptr));
    return (1);
}

FT_TEST(test_sink_reports_invalid_arguments_and_out_of_range)
{
    sink_record record;
    ft_size_t record_count;

    sink_clear();
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sink_get_record_count(ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sink_get_record(0, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, sink_get_record(0, &record));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(7, "validation message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(0, &record));
    FT_ASSERT_EQ(7, record.level);
    FT_ASSERT_EQ(0, ft_strcmp(record.message, "validation message"));
    sink_clear();
    return (1);
}

FT_TEST(test_sink_clear_is_idempotent_and_reusable)
{
    sink_record record;
    ft_size_t record_count;

    sink_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), record_count);
    sink_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(22, "first reuse message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), record_count);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(0, &record));
    FT_ASSERT_EQ(22, record.level);
    FT_ASSERT_EQ(0, ft_strcmp(record.message, "first reuse message"));
    sink_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&record_count));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), record_count);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, sink_get_record(0, &record));
    return (1);
}
FT_TEST(test_sink_copies_counted_messages_and_keeps_existing_pointers_stable)
{
    sink_clear();
    char message[] = {'a', 'b', 'c'};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(3, message, 2U));
    message[0] = 'x';
    sink_record first;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(0U, &first));
    uint32_t index = 0U;
    while (index < 1024U)
    {
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_record_message(7, "record"));
        index += 1U;
    }
    sink_record first_again;
    sink_record last;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(0U, &first_again));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(index, &last));
    FT_ASSERT(first.message == first_again.message);
    FT_ASSERT_EQ(0, ft_strcmp("ab", first_again.message));
    FT_ASSERT_EQ(7, last.level);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, sink_record_message(0, "x", FT_SYSTEM_SIZE_MAX));
    sink_clear();
    return (1);
}

FT_TEST(test_sink_indexes_records_from_concurrent_writers)
{
    constexpr uint32_t RECORDS_PER_WRITER = 512U;
    sink_clear();
    std::atomic<ft_bool> successful{FT_TRUE};
    const auto record = [&successful](int32_t level)
    {
        uint32_t index = 0U;
        while (index < RECORDS_PER_WRITER)
        {
            if (sink_record_message(level, "concurrent record") != FT_ERR_SUCCESS)
                successful.store(FT_FALSE, std::memory_order_relaxed);
            index += 1U;
        }
        return ;
    };
    std::thread first(record, 1);
    std::thread second(record, 2);
    first.join();
    second.join();

    ft_size_t count;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record_count(&count));
    uint32_t first_count = 0U;
    ft_size_t index = 0U;
    while (index < count)
    {
        sink_record item;
        FT_ASSERT_EQ(FT_ERR_SUCCESS, sink_get_record(index, &item));
        FT_ASSERT(item.level == 1 || item.level == 2);
        FT_ASSERT_EQ(0, ft_strcmp("concurrent record", item.message));
        first_count += item.level == 1;
        index += 1U;
    }
    sink_clear();
    FT_ASSERT_EQ(FT_TRUE, successful.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(2U * RECORDS_PER_WRITER, count);
    FT_ASSERT_EQ(RECORDS_PER_WRITER, first_count);
    return (1);
}
