#include "../test_internal.hpp"
#include "test_scma_shared.hpp"
#include "../../Modules/SCMA/SCMA.hpp"
#include "../../Modules/SCMA/scma_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

namespace
{
    struct scma_stress_record
    {
        scma_handle handle;
        std::vector<uint8_t> bytes;
    };

    static void scma_fill_random_bytes(std::vector<uint8_t> &buffer,
        std::mt19937 &random_engine)
    {
        size_t index = 0;

        while (index < buffer.size())
        {
            buffer[index] = static_cast<uint8_t>(random_engine());
            index += 1;
        }
        return ;
    }

    static bool scma_handles_match(const scma_handle &first_handle,
        const scma_handle &second_handle)
    {
        if (first_handle.index != second_handle.index)
            return (false);
        if (first_handle.generation != second_handle.generation)
            return (false);
        return (true);
    }

    static bool scma_handle_is_invalid_value(const scma_handle &handle)
    {
        scma_handle invalid_handle;

        invalid_handle = scma_invalid_handle();
        return (scma_handles_match(handle, invalid_handle));
    }

    static bool scma_buffers_match(const std::vector<uint8_t> &first_buffer,
        const std::vector<uint8_t> &second_buffer)
    {
        size_t index = 0;

        if (first_buffer.size() != second_buffer.size())
            return (false);
        while (index < first_buffer.size())
        {
            if (first_buffer[index] != second_buffer[index])
                return (false);
            index += 1;
        }
        return (true);
    }

    static bool scma_buffer_prefix_matches(const std::vector<uint8_t> &expected_buffer,
        const std::vector<uint8_t> &actual_buffer, size_t prefix_size)
    {
        size_t index = 0;

        if (expected_buffer.size() < prefix_size)
            return (false);
        if (actual_buffer.size() < prefix_size)
            return (false);
        while (index < prefix_size)
        {
            if (expected_buffer[index] != actual_buffer[index])
                return (false);
            index += 1;
        }
        return (true);
    }

    static int scma_verify_record_contents(const scma_stress_record &record)
    {
        std::vector<uint8_t> read_buffer;
        ft_size_t reported_size;
        if (scma_handle_is_valid(record.handle) != 1)
            return (0);
        reported_size = scma_get_size(record.handle);
        if (record.bytes.size() != reported_size)
            return (0);
        read_buffer.resize(record.bytes.size());
        if (scma_read(record.handle, 0, read_buffer.data(),
                record.bytes.size()) != FT_ERR_SUCCESS)
            return (0);
        if (!scma_buffers_match(record.bytes, read_buffer))
            return (0);
        return (1);
    }

    static int scma_verify_all_records(const std::vector<scma_stress_record> &records)
    {
        size_t index = 0;

        while (index < records.size())
        {
            if (!scma_verify_record_contents(records[index]))
                return (0);
            index += 1;
        }
        return (1);
    }

    static ft_size_t scma_sum_record_sizes(const std::vector<scma_stress_record> &records)
    {
        size_t index;
        ft_size_t total_size;

        index = 0;
        total_size = 0;
        while (index < records.size())
        {
            total_size += records[index].bytes.size();
            index += 1;
        }
        return (total_size);
    }

    static int scma_verify_stats_consistency(const std::vector<scma_stress_record> &records)
    {
        scma_stats stats;
        ft_size_t expected_used_size;

        ft_bzero(&stats, sizeof(stats));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_get_stats(&stats));
        expected_used_size = scma_sum_record_sizes(records);
        FT_ASSERT_EQ(expected_used_size, stats.used_size);
        FT_ASSERT(stats.heap_capacity >= stats.used_size);
        FT_ASSERT(stats.block_count >= records.size());
        return (1);
    }
}

FT_TEST(test_scma_integrity_stress_random_operations)
{
    std::mt19937 random_engine;
    std::vector<scma_stress_record> records;
    size_t iteration_index;
    const size_t maximum_records = 48;
    const size_t iteration_count = 8000;

    scma_test_reset();
    FT_ASSERT_EQ(0, scma_initialize(1U << 16));
    random_engine.seed(0xC0DE1234U);
    iteration_index = 0;
    while (iteration_index < iteration_count)
    {
        int operation_selector;

        operation_selector = 0;
        if (!records.empty())
            operation_selector = static_cast<int>(random_engine() % 7U);
        if (operation_selector == 0 && records.size() < maximum_records)
        {
            scma_stress_record record;
            size_t allocation_size;

            allocation_size = (random_engine() % 1024U) + 1U;
            record.handle = scma_allocate(allocation_size);
            if (!scma_handle_is_invalid_value(record.handle))
            {
                record.bytes.resize(allocation_size);
                scma_fill_random_bytes(record.bytes, random_engine);
                FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(record.handle, 0, record.bytes.data(),
                    record.bytes.size()));
                records.push_back(record);
                FT_ASSERT_EQ(1, scma_handle_is_valid(record.handle));
                FT_ASSERT_EQ(record.bytes.size(), scma_get_size(record.handle));
            }
        }
        else if (!records.empty())
        {
            size_t record_index;
            scma_stress_record *record_pointer;

            record_index = random_engine() % records.size();
            record_pointer = &records[record_index];
            if (operation_selector == 1)
            {
                scma_fill_random_bytes(record_pointer->bytes, random_engine);
                FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(record_pointer->handle, 0,
                    record_pointer->bytes.data(), record_pointer->bytes.size()));
            }
            else if (operation_selector == 2)
            {
                FT_ASSERT_EQ(1, scma_verify_record_contents(*record_pointer));
            }
            else if (operation_selector == 3)
            {
                size_t record_size;
                size_t offset;
                size_t write_size;
                std::vector<uint8_t> patch_bytes;
                std::vector<uint8_t> read_back_bytes;
                size_t patch_index;

                record_size = record_pointer->bytes.size();
                offset = random_engine() % record_size;
                write_size = ((random_engine()
                    % (record_size - offset)) + 1U);
                patch_bytes.resize(write_size);
                scma_fill_random_bytes(patch_bytes, random_engine);
                FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(record_pointer->handle, offset,
                    patch_bytes.data(), patch_bytes.size()));
                patch_index = 0;
                while (patch_index < patch_bytes.size())
                {
                    record_pointer->bytes[offset + patch_index] = patch_bytes[patch_index];
                    patch_index += 1;
                }
                read_back_bytes.resize(write_size);
                FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(record_pointer->handle, offset,
                    read_back_bytes.data(), read_back_bytes.size()));
                FT_ASSERT(scma_buffers_match(patch_bytes, read_back_bytes));
            }
            else if (operation_selector == 4)
            {
                size_t new_size;
                std::vector<uint8_t> previous_bytes;
                std::vector<uint8_t> preserved_bytes;
                size_t preserved_size;
                std::vector<uint8_t> tail_bytes;
                size_t tail_offset;

                previous_bytes = record_pointer->bytes;
                new_size = (random_engine() % 1024U) + 1U;
                FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(record_pointer->handle, new_size));
                FT_ASSERT_EQ(new_size, scma_get_size(record_pointer->handle));
                preserved_size = previous_bytes.size();
                if (preserved_size > new_size)
                    preserved_size = new_size;
                preserved_bytes.resize(preserved_size);
                if (preserved_size > 0)
                {
                    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(record_pointer->handle, 0,
                        preserved_bytes.data(), preserved_bytes.size()));
                    FT_ASSERT(scma_buffer_prefix_matches(previous_bytes,
                        preserved_bytes, preserved_size));
                }
                record_pointer->bytes.resize(new_size);
                if (new_size > preserved_size)
                {
                    tail_offset = preserved_size;
                    tail_bytes.resize(new_size - preserved_size);
                    scma_fill_random_bytes(tail_bytes, random_engine);
                    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(record_pointer->handle, tail_offset,
                        tail_bytes.data(), tail_bytes.size()));
                    size_t tail_index = 0;

                    while (tail_index < tail_bytes.size())
                    {
                        record_pointer->bytes[tail_offset + tail_index] =
                            tail_bytes[tail_index];
                        tail_index += 1;
                    }
                }
            }
            else if (operation_selector == 5)
            {
                FT_ASSERT_EQ(0, scma_free(record_pointer->handle));
                FT_ASSERT_EQ(0, scma_handle_is_valid(record_pointer->handle));
                records.erase(records.begin() + record_index);
            }
            else
            {
                FT_ASSERT_EQ(1, scma_verify_record_contents(*record_pointer));
                FT_ASSERT_EQ(1, scma_verify_stats_consistency(records));
            }
        }
        if ((iteration_index % 64U) == 0U)
        {
            FT_ASSERT_EQ(1, scma_verify_all_records(records));
            FT_ASSERT_EQ(1, scma_verify_stats_consistency(records));
        }
        iteration_index += 1;
    }
    FT_ASSERT_EQ(1, scma_verify_all_records(records));
    FT_ASSERT_EQ(1, scma_verify_stats_consistency(records));
    while (!records.empty())
    {
        FT_ASSERT_EQ(0, scma_free(records.back().handle));
        records.pop_back();
    }
    scma_shutdown();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_disable_thread_safety());
    return (1);
}
