#include "../test_internal.hpp"
#include <cstring>
#include <cstdlib>
#include "test_scma_shared.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/SCMA/scma_internal.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_scma_allocation_roundtrip)
{
    scma_handle handle;
    int write_value;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(128));
    handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    write_value = 123456789;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, &write_value, sizeof(int)));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(write_value, read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_batch_write_prevalidation_is_atomic)
{
    scma_handle handles[2];
    scma_write_request write_requests[2];
    scma_read_request read_requests[2];
    int32_t initial_values[2];
    int32_t replacement_values[2];
    int32_t read_values[2];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_test_initialize(128));
    handles[0] = scma_allocate(sizeof(int32_t));
    handles[1] = scma_allocate(sizeof(int32_t));
    FT_ASSERT_EQ(FT_TRUE, scma_handle_is_valid(handles[0]));
    FT_ASSERT_EQ(FT_TRUE, scma_handle_is_valid(handles[1]));
    initial_values[0] = 11;
    initial_values[1] = 22;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handles[0], 0,
            &initial_values[0], sizeof(int32_t)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handles[1], 0,
            &initial_values[1], sizeof(int32_t)));
    replacement_values[0] = 33;
    replacement_values[1] = 44;
    write_requests[0] = {handles[0], 0, &replacement_values[0],
        sizeof(int32_t)};
    write_requests[1] = {handles[1], sizeof(int32_t),
        &replacement_values[1], sizeof(int32_t)};
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, scma_write_batch(write_requests, 2));
    read_values[0] = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handles[0], 0, &read_values[0],
            sizeof(int32_t)));
    FT_ASSERT_EQ(initial_values[0], read_values[0]);
    write_requests[1].offset = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write_batch(write_requests, 2));
    read_requests[0] = {handles[0], 0, &read_values[0], sizeof(int32_t)};
    read_requests[1] = {handles[1], 0, &read_values[1], sizeof(int32_t)};
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read_batch(read_requests, 2));
    FT_ASSERT_EQ(replacement_values[0], read_values[0]);
    FT_ASSERT_EQ(replacement_values[1], read_values[1]);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_lazy_compaction_wipes_freed_bytes)
{
    scma_handle handles[3];
    scma_handle reused_handle;
    ft_size_t freed_offset;
    ft_size_t third_offset;
    ft_size_t byte_index;
    unsigned char payload[8];

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_test_initialize(64));
    std::memset(payload, 0xA5, sizeof(payload));
    handles[0] = scma_allocate(sizeof(payload));
    handles[1] = scma_allocate(sizeof(payload));
    handles[2] = scma_allocate(sizeof(payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handles[1], 0, payload,
            sizeof(payload)));
    freed_offset = scma_blocks_data_ref()[handles[1].index].offset;
    third_offset = scma_blocks_data_ref()[handles[2].index].offset;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(handles[1]));
    FT_ASSERT_EQ(FT_TRUE, scma_compaction_needed_ref());
    FT_ASSERT_EQ(third_offset,
        scma_blocks_data_ref()[handles[2].index].offset);
    byte_index = 0;
    while (byte_index < sizeof(payload))
    {
        FT_ASSERT_EQ(0, scma_heap_data_ref()[freed_offset + byte_index]);
        byte_index++;
    }
    reused_handle = scma_allocate(sizeof(payload));
    FT_ASSERT_EQ(handles[1].index, reused_handle.index);
    FT_ASSERT(reused_handle.generation != handles[1].generation);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_allocation_unsigned_char_payload)
{
    scma_handle handle;
    unsigned char payload[6];
    unsigned char readback[6];
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(128));
    handle = scma_allocate(6);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    index = 0;
    while (index < 6)
    {
        payload[index] = static_cast<unsigned char>(index * 3 + 1);
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, payload, 6));
    std::memset(readback, 0, sizeof(readback));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, 0, readback, 6));
    index = 0;
    while (index < 6)
    {
        FT_ASSERT_EQ(payload[index], readback[index]);
        index = index + 1;
    }
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_compaction_preserves_live_blocks)
{
    scma_handle first_handle;
    scma_handle second_handle;
    scma_handle third_handle;
    int first_value;
    int second_value;
    int third_value;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(256));
    first_handle = scma_allocate(sizeof(int));
    second_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(first_handle));
    FT_ASSERT_EQ(1, scma_handle_is_valid(second_handle));
    first_value = 111;
    second_value = 222;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(first_handle, 0, &first_value, sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(second_handle, 0, &second_value, sizeof(int)));
    FT_ASSERT_EQ(0, scma_free(first_handle));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(second_handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(second_value, read_value);
    third_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(third_handle));
    third_value = 333;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(third_handle, 0, &third_value, sizeof(int)));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(second_handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(second_value, read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_compaction_rejects_corrupt_cycle_without_wiping_payloads)
{
    scma_handle first_handle;
    scma_handle second_handle;
    scma_handle third_handle;
    scma_block_span span;
    ft_size_t saved_next;
    int second_value;
    int third_value;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(256));
    first_handle = scma_allocate(sizeof(int));
    second_handle = scma_allocate(sizeof(int));
    third_handle = scma_allocate(sizeof(int));
    second_value = 222;
    third_value = 333;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(second_handle, 0,
            &second_value, sizeof(second_value)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(third_handle, 0,
            &third_value, sizeof(third_value)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(first_handle));
    span = scma_get_block_span();
    saved_next = span.data[second_handle.index].next_index;
    span.data[second_handle.index].next_index = second_handle.index;
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, scma_compact());
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(second_handle, 0,
            &read_value, sizeof(read_value)));
    FT_ASSERT_EQ(second_value, read_value);
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(third_handle, 0,
            &read_value, sizeof(read_value)));
    FT_ASSERT_EQ(third_value, read_value);
    span.data[second_handle.index].next_index = saved_next;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_compact());
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_allocation_double_precision_roundtrip)
{
    scma_handle handle;
    double write_values[3];
    double read_values[3];
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(double) * 3));
    handle = scma_allocate(sizeof(double) * 3);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    index = 0;
    while (index < 3)
    {
        write_values[index] = static_cast<double>(index) + 0.125;
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, write_values, sizeof(double) * 3));
    std::memset(read_values, 0, sizeof(read_values));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, 0, read_values, sizeof(double) * 3));
    index = 0;
    while (index < 3)
    {
        FT_ASSERT_EQ(write_values[index], read_values[index]);
        index = index + 1;
    }
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_compaction_reuses_space_for_new_blocks)
{
    scma_handle handles[3];
    int stored_values[3];
    ft_size_t index;
    int read_value;
    scma_handle expanded_handle;
    int expanded_values[4];
    ft_size_t expanded_index;

    FT_ASSERT_EQ(0, scma_test_initialize(256));
    index = 0;
    while (index < 3)
    {
        handles[index] = scma_allocate(sizeof(int));
        FT_ASSERT_EQ(1, scma_handle_is_valid(handles[index]));
        stored_values[index] = static_cast<int>(index * 100 + 1);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handles[index], 0, &stored_values[index], sizeof(int)));
        index = index + 1;
    }
    FT_ASSERT_EQ(0, scma_free(handles[1]));
    expanded_handle = scma_allocate(sizeof(int) * 4);
    FT_ASSERT_EQ(1, scma_handle_is_valid(expanded_handle));
    expanded_index = 0;
    while (expanded_index < 4)
    {
        expanded_values[expanded_index] = static_cast<int>(expanded_index + 10);
        FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(expanded_handle,
                sizeof(int) * expanded_index,
                &expanded_values[expanded_index],
                sizeof(int)));
        expanded_index = expanded_index + 1;
    }
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handles[0], 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(stored_values[0], read_value);
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handles[2], 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(stored_values[2], read_value);
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(expanded_handle, sizeof(int) * 3, &read_value, sizeof(int)));
    FT_ASSERT_EQ(expanded_values[3], read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_free_span_reuse_splits_and_wipes_payload)
{
    scma_handle first_handle;
    scma_handle middle_handle;
    scma_handle last_handle;
    scma_handle reused_handle;
    unsigned char payload[16];
    unsigned char cleared_payload[8];
    ft_size_t index;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_test_initialize(128));
    first_handle = scma_allocate(16);
    middle_handle = scma_allocate(16);
    last_handle = scma_allocate(16);
    FT_ASSERT_EQ(1, scma_handle_is_valid(first_handle));
    FT_ASSERT_EQ(1, scma_handle_is_valid(middle_handle));
    FT_ASSERT_EQ(1, scma_handle_is_valid(last_handle));
    index = 0;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<unsigned char>(0xA0U + index);
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(middle_handle, 0,
            payload, sizeof(payload)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(middle_handle));
    reused_handle = scma_allocate(8);
    FT_ASSERT_EQ(1, scma_handle_is_valid(reused_handle));
    FT_ASSERT_EQ(middle_handle.index, reused_handle.index);
    FT_ASSERT(middle_handle.generation != reused_handle.generation);
    std::memset(cleared_payload, 0xFF, sizeof(cleared_payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(reused_handle, 0,
            cleared_payload, sizeof(cleared_payload)));
    index = 0;
    while (index < sizeof(cleared_payload))
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0), cleared_payload[index]);
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(first_handle));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(last_handle));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_free(reused_handle));
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_resize_grow_and_shrink_preserves_data)
{
    scma_handle handle;
    unsigned char initial_payload[4];
    unsigned char buffer[8];
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(64));
    handle = scma_allocate(4);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    index = 0;
    while (index < 4)
    {
        initial_payload[index] = static_cast<unsigned char>(index + 1);
        index = index + 1;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, initial_payload, 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(handle, 8));
    FT_ASSERT_EQ(8, scma_get_size(handle));
    std::memset(buffer, 0, sizeof(buffer));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, 0, buffer, 4));
    index = 0;
    while (index < 4)
    {
        FT_ASSERT_EQ(initial_payload[index], buffer[index]);
        index = index + 1;
    }
    buffer[0] = 9;
    buffer[1] = 8;
    buffer[2] = 7;
    buffer[3] = 6;
    buffer[4] = 5;
    buffer[5] = 4;
    buffer[6] = 3;
    buffer[7] = 2;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, buffer, 8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_resize(handle, 2));
    FT_ASSERT_EQ(2, scma_get_size(handle));
    std::memset(initial_payload, 0, sizeof(initial_payload));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, 0, initial_payload, 2));
    FT_ASSERT_EQ(static_cast<unsigned char>(9), initial_payload[0]);
    FT_ASSERT_EQ(static_cast<unsigned char>(8), initial_payload[1]);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_resize_rejects_zero_size)
{
    scma_handle handle;

    FT_ASSERT_EQ(0, scma_test_initialize(32));
    handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, scma_resize(handle, 0));
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_write_and_read_reject_null_pointers)
{
    scma_handle handle;
    int value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int)));
    handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, scma_write(handle, 0, ft_nullptr, sizeof(int)));
    value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, &value, sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, scma_read(handle, 0, ft_nullptr, sizeof(int)));
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_reused_handle_invalidates_previous_generation)
{
    scma_handle first_handle;
    scma_handle cached_handle;
    scma_handle reused_handle;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 2));
    first_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(first_handle));
    cached_handle = first_handle;
    FT_ASSERT_EQ(0, scma_free(first_handle));
    reused_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(reused_handle));
    FT_ASSERT_EQ(cached_handle.index, reused_handle.index);
    FT_ASSERT_EQ(0, scma_handle_is_valid(cached_handle));
    FT_ASSERT(cached_handle.generation != reused_handle.generation);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_read_bounds_checks)
{
    scma_handle handle;
    int value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int)));
    handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    value = 11;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(handle, 0, &value, sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, scma_read(handle, sizeof(int), &value, sizeof(int)));
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_late_allocate_failure_preserves_existing_data)
{
    scma_handle stable_handle;
    scma_handle failed_handle;
    int stored_value;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(128));
    stable_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(stable_handle));
    stored_value = 31415;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(stable_handle, 0, &stored_value, sizeof(int)));
    failed_handle = scma_allocate(FT_SYSTEM_SIZE_MAX);
    FT_ASSERT_EQ(0, scma_handle_is_valid(failed_handle));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(stable_handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(stored_value, read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_late_resize_failure_preserves_existing_data)
{
    scma_handle first_handle;
    scma_handle second_handle;
    int first_value;
    int second_value;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(256));
    first_handle = scma_allocate(sizeof(int));
    second_handle = scma_allocate(sizeof(int));
    FT_ASSERT_EQ(1, scma_handle_is_valid(first_handle));
    FT_ASSERT_EQ(1, scma_handle_is_valid(second_handle));
    first_value = 101;
    second_value = 202;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(first_handle, 0, &first_value, sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_write(second_handle, 0, &second_value, sizeof(int)));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, scma_resize(second_handle, FT_SYSTEM_SIZE_MAX));
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(first_handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(first_value, read_value);
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(second_handle, 0, &read_value, sizeof(int)));
    FT_ASSERT_EQ(second_value, read_value);
    scma_shutdown();
    return (1);
}
