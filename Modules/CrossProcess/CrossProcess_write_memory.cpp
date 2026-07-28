#include "cross_process.hpp"

#include <cerrno>
#include <cstring>
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Compatebility/compatebility_cross_process.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

namespace
{
    static ft_size_t compute_offset(uint64_t pointer_value, uint64_t base_value)
    {
        if (pointer_value < base_value)
            return (0);
        return (pointer_value - base_value);
    }
}

int32_t cp_write_memory(const cross_process_message &message,
    const uint8_t *payload, ft_size_t payload_length, int32_t error_code)
{
    ft_size_t data_offset;
    ft_size_t payload_capacity;
    ft_size_t error_offset;
    ft_size_t zero_length;
    ft_size_t full_capacity;
    ft_bool has_error_slot;
    ft_bool has_failure;
    ft_bool zero_on_failure;
    int32_t failure_error;
    int32_t operation_error;
    int32_t cleanup_error;

    if (payload == ft_nullptr && payload_length != 0)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    data_offset = compute_offset(message.remote_memory_address, message.stack_base_address);
    if (data_offset >= message.remote_memory_size)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    full_capacity = message.remote_memory_size - data_offset;
    payload_capacity = full_capacity;
    has_error_slot = FT_FALSE;
    if (message.error_memory_address != 0)
        has_error_slot = FT_TRUE;
    has_failure = FT_FALSE;
    zero_on_failure = FT_TRUE;
    failure_error = FT_ERR_SUCCESS;
    error_offset = 0;
    if (has_error_slot == FT_TRUE)
    {
        error_offset = compute_offset(message.error_memory_address, message.stack_base_address);
        if (error_offset > message.remote_memory_size
            || sizeof(int32_t) > message.remote_memory_size - error_offset
            || error_offset < data_offset)
        {
            has_failure = FT_TRUE;
            failure_error = FT_ERR_INVALID_ARGUMENT;
            errno = EINVAL;
        }
        else
        {
            payload_capacity = error_offset - data_offset;
        }
    }
    if (has_failure == FT_FALSE && payload_length > payload_capacity)
    {
        has_failure = FT_TRUE;
        failure_error = FT_ERR_OUT_OF_RANGE;
        zero_on_failure = FT_FALSE;
    }
    cmp_cross_process_mapping mapping;
    cmp_cross_process_mutex_state mutex_state;

    mapping.mapping_address = ft_nullptr;
    mapping.mapping_length = 0;
    mapping.platform_handle = ft_nullptr;
    mapping.mutex_address = ft_nullptr;
    mutex_state.platform_mutex = ft_nullptr;
    operation_error = cmp_cross_process_open_mapping(message, &mapping);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = cmp_cross_process_lock_mutex(message, &mapping,
            &mutex_state);
    if (operation_error != FT_ERR_SUCCESS)
    {
        cleanup_error = cmp_cross_process_close_mapping(&mapping);
        if (cleanup_error != FT_ERR_SUCCESS)
            return (cleanup_error);
        return (operation_error);
    }
    zero_length = payload_capacity;
    if (zero_length > 0 && (has_failure == FT_FALSE
            || zero_on_failure == FT_TRUE))
        std::memset(mapping.mapping_address + data_offset, 0, zero_length);
    if (has_failure == FT_FALSE && payload_length > 0)
        std::memcpy(mapping.mapping_address + data_offset, payload, payload_length);
    if (has_failure == FT_FALSE && has_error_slot == FT_TRUE)
    {
        std::memcpy(mapping.mapping_address + error_offset, &error_code,
            sizeof(int32_t));
    }
    operation_error = cmp_cross_process_unlock_mutex(message, &mapping,
            &mutex_state);
    if (operation_error != FT_ERR_SUCCESS)
    {
        cleanup_error = cmp_cross_process_close_mapping(&mapping);
        if (cleanup_error != FT_ERR_SUCCESS)
            return (cleanup_error);
        return (operation_error);
    }
    operation_error = cmp_cross_process_close_mapping(&mapping);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    if (has_failure == FT_TRUE)
        return (failure_error);
    return (FT_ERR_SUCCESS);
}
