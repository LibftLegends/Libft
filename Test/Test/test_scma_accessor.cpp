#include "../test_internal.hpp"
#include <utility>
#include "test_scma_shared.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/SCMA/SCMA.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

struct scma_test_vector3
{
    float x;
    float y;
    float z;
};

struct scma_test_quad
{
    double a;
    double b;
    double c;
    double d;
};

FT_TEST(test_scma_accessor_struct_pointer_semantics)
{
    scma_handle handle;
    scma_handle_accessor<scma_test_pair> accessor;
    scma_test_pair result;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(scma_test_pair)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(scma_test_pair));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    {
        auto proxy = accessor.operator->();
        proxy->first = 7;
        proxy->second = 14;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_struct(result));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.get_error());
    FT_ASSERT_EQ(7, result.first);
    FT_ASSERT_EQ(14, result.second);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_float_array_semantics)
{
    scma_handle handle;
    scma_handle_accessor<float> accessor;
    float write_values[3];
    float read_value;
    ft_size_t index;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(float) * 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(float) * 3);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    index = 0;
    while (index < 3)
    {
        write_values[index] = static_cast<float>(index) + 0.5f;
        accessor[index] = write_values[index];
        index = index + 1;
    }
    read_value = accessor[2];
    FT_ASSERT_EQ(write_values[2], read_value);
    read_value = accessor[0];
    FT_ASSERT_EQ(write_values[0], read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_array_indexing)
{
    scma_handle handle;
    scma_handle_accessor<int> accessor;
    int read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(int) * 3);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    accessor[0] = 5;
    accessor[1] = 10;
    accessor[2] = 15;
    read_value = accessor[1];
    FT_ASSERT_EQ(10, read_value);
    read_value = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scma_read(handle, sizeof(int) * 2, &read_value, sizeof(int)));
    FT_ASSERT_EQ(15, read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_struct_vector_semantics)
{
    scma_handle handle;
    scma_handle_accessor<scma_test_vector3> accessor;
    scma_test_vector3 vector_value;
    scma_test_vector3 read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(scma_test_vector3) * 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(scma_test_vector3) * 2);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    {
        auto proxy = accessor.operator->();
        proxy->x = 1.0f;
        proxy->y = 2.0f;
        proxy->z = 3.0f;
    }
    vector_value.x = 4.0f;
    vector_value.y = 5.0f;
    vector_value.z = 6.0f;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.write_struct(vector_value));
    read_value.x = 0.0f;
    read_value.y = 0.0f;
    read_value.z = 0.0f;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_struct(read_value));
    FT_ASSERT_EQ(vector_value.x, read_value.x);
    FT_ASSERT_EQ(vector_value.y, read_value.y);
    FT_ASSERT_EQ(vector_value.z, read_value.z);
    {
        auto element_proxy = accessor[1];
        auto element_pointer = element_proxy.operator->();
        element_pointer->x = 7.0f;
        element_pointer->y = 8.0f;
        element_pointer->z = 9.0f;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_struct(read_value));
    FT_ASSERT_EQ(vector_value.x, read_value.x);
    FT_ASSERT_EQ(vector_value.y, read_value.y);
    FT_ASSERT_EQ(vector_value.z, read_value.z);
    read_value.x = 0.0f;
    read_value.y = 0.0f;
    read_value.z = 0.0f;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_at(read_value, 1));
    FT_ASSERT_EQ(7.0f, read_value.x);
    FT_ASSERT_EQ(8.0f, read_value.y);
    FT_ASSERT_EQ(9.0f, read_value.z);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_shared_handle_binding_semantics)
{
    scma_handle handle;
    scma_handle_accessor<scma_test_pair> primary_accessor;
    scma_handle_accessor<scma_test_pair> secondary_accessor;
    scma_test_pair pair_value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(scma_test_pair)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_accessor.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, secondary_accessor.initialize());
    handle = scma_allocate(sizeof(scma_test_pair));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, primary_accessor.bind(handle));
    FT_ASSERT_EQ(1, secondary_accessor.bind(handle));
    {
        auto proxy = primary_accessor.operator->();
        proxy->first = 55;
        proxy->second = 65;
    }
    FT_ASSERT_EQ(1, secondary_accessor.is_bound());
    {
        auto proxy = secondary_accessor.operator->();
        FT_ASSERT_EQ(55, proxy->first);
        FT_ASSERT_EQ(65, proxy->second);
    }
    pair_value.first = 77;
    pair_value.second = 88;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, secondary_accessor.write_struct(pair_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, primary_accessor.read_struct(pair_value));
    FT_ASSERT_EQ(77, pair_value.first);
    FT_ASSERT_EQ(88, pair_value.second);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_double_struct_semantics)
{
    scma_handle handle;
    scma_handle_accessor<scma_test_quad> accessor;
    scma_test_quad write_value;
    scma_test_quad read_value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(scma_test_quad)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(scma_test_quad));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    write_value.a = 1.125;
    write_value.b = 2.25;
    write_value.c = 3.5;
    write_value.d = 4.75;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.write_struct(write_value));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_struct(read_value));
    FT_ASSERT_EQ(write_value.a, read_value.a);
    FT_ASSERT_EQ(write_value.b, read_value.b);
    FT_ASSERT_EQ(write_value.c, read_value.c);
    FT_ASSERT_EQ(write_value.d, read_value.d);
    {
        auto proxy = accessor.operator->();
        proxy->a = proxy->a + 1.0;
        proxy->d = proxy->d + 2.0;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.read_struct(read_value));
    FT_ASSERT_EQ(2.125, read_value.a);
    FT_ASSERT_EQ(2.25, read_value.b);
    FT_ASSERT_EQ(3.5, read_value.c);
    FT_ASSERT_EQ(6.75, read_value.d);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_dereference_reads_value)
{
    scma_handle handle;
    scma_handle_accessor<int> accessor;
    int read_value;
    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(int) * 2);
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    accessor[0] = 12;
    accessor[1] = 34;
    read_value = *accessor;
    FT_ASSERT_EQ(12, read_value);
    const scma_handle_accessor<int> &const_accessor = accessor;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.get_error());
    read_value = const_accessor[1];
    FT_ASSERT_EQ(34, read_value);
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_bind_rejects_invalid_handle)
{
    scma_handle_accessor<int> accessor;
    scma_handle invalid_handle;

    FT_ASSERT_EQ(0, scma_test_initialize(32));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    invalid_handle.index = FT_SYSTEM_SIZE_MAX;
    invalid_handle.generation = FT_SYSTEM_SIZE_MAX;
    FT_ASSERT_EQ(0, accessor.bind(invalid_handle));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, accessor.get_error());
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_detects_stale_handle)
{
    scma_handle handle;
    scma_handle_accessor<scma_test_pair> accessor;
    scma_test_pair pair_value;

    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(scma_test_pair)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    handle = scma_allocate(sizeof(scma_test_pair));
    FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
    FT_ASSERT_EQ(1, accessor.bind(handle));
    FT_ASSERT_EQ(0, scma_free(handle));
    FT_ASSERT_EQ(0, accessor.read_struct(pair_value));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, accessor.get_error());
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_get_count_matches_capacity)
{
    FT_ASSERT_EQ(0, scma_test_initialize(sizeof(int) * 4));
    {
        scma_handle handle;
        scma_handle_accessor<int> accessor;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
        handle = scma_allocate(sizeof(int) * 4);
        FT_ASSERT_EQ(1, scma_handle_is_valid(handle));
        FT_ASSERT_EQ(1, accessor.bind(handle));
        accessor[0] = 3;
        accessor[1] = 6;
        accessor[2] = 9;
        accessor[3] = 12;
        FT_ASSERT_EQ(4, accessor.get_count());
    }
    scma_shutdown();
    return (1);
}

FT_TEST(test_scma_accessor_operations_require_binding)
{
    scma_handle_accessor<int> accessor;
    int value;

    scma_test_reset();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, accessor.initialize());
    FT_ASSERT_EQ(0, accessor.is_bound());
    value = 0;
    FT_ASSERT_EQ(0, accessor.read_struct(value));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, accessor.get_error());
    FT_ASSERT_EQ(0, accessor.write_struct(value));
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, accessor.get_error());
    FT_ASSERT_EQ(0, accessor.get_count());
    FT_ASSERT_EQ(FT_ERR_INVALID_HANDLE, accessor.get_error());
    return (1);
}
