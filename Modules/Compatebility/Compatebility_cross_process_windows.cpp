#ifdef _WIN32

#include "compatebility_cross_process.hpp"
#include "compatebility_internal.hpp"
#include <cerrno>
#include <cstring>
#include <limits>
#include <winsock2.h>
#include <windows.h>

static constexpr ft_size_t CROSS_PROCESS_SHARED_MEMORY_NAME_CAPACITY = 256U;

#if defined(LIBFT_TEST_BUILD)
static ft_bool g_cross_process_fail_next_unlock = FT_FALSE;
#endif

static int32_t compute_offset(uint64_t pointer_value, uint64_t base_value,
    ft_size_t &offset)
{
    if (pointer_value < base_value)
    {
        errno = EINVAL;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    offset = pointer_value - base_value;
    return (FT_ERR_SUCCESS);
}

static ft_bool has_terminated_name(const char *name)
{
    return (std::memchr(name, '\0',
        CROSS_PROCESS_SHARED_MEMORY_NAME_CAPACITY)
        != ft_nullptr ? FT_TRUE : FT_FALSE);
}

int32_t cmp_cross_process_send_descriptor(int32_t socket_file_descriptor, const cross_process_message &message)
{
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    ft_size_t offset;

    if (cmp_cross_process_encode_wire(message, wire, sizeof(wire))
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    offset = 0U;
    while (offset < sizeof(wire))
    {
        int32_t sent;

        sent = ::send(static_cast<SOCKET>(socket_file_descriptor),
            reinterpret_cast<const char *>(wire + offset),
            static_cast<int32_t>(sizeof(wire) - offset), 0);
        if (sent == SOCKET_ERROR)
        {
            errno = WSAGetLastError();
            return (cmp_map_system_error_to_ft(errno));
        }
        if (sent == 0)
        {
            errno = ECONNRESET;
            return (cmp_map_system_error_to_ft(errno));
        }
        offset += static_cast<ft_size_t>(sent);
    }
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_receive_descriptor(int32_t socket_file_descriptor, cross_process_message &message)
{
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    ft_size_t total_size;
    ft_size_t offset;

    total_size = sizeof(wire);
    offset = 0;
    while (offset < total_size)
    {
        int32_t chunk_size;

        chunk_size = recv(static_cast<SOCKET>(socket_file_descriptor),
            reinterpret_cast<char *>(wire + offset),
            static_cast<int32_t>(total_size - offset), 0);
        if (chunk_size == SOCKET_ERROR)
        {
            int32_t windows_error;

            windows_error = WSAGetLastError();
            errno = static_cast<int32_t>(windows_error);
                return (cmp_map_system_error_to_ft(errno));
        }
        if (chunk_size == 0)
        {
            errno = ECONNRESET;
                return (cmp_map_system_error_to_ft(errno));
        }
        offset += static_cast<ft_size_t>(chunk_size);
    }
    return (cmp_cross_process_decode_wire(wire, total_size, message));
}

int32_t cmp_cross_process_open_mapping(const cross_process_message &message, cmp_cross_process_mapping *mapping)
{
    HANDLE mapping_handle;
    void *mapping_pointer;
    DWORD windows_error;
    ft_size_t mutex_offset;
    MEMORY_BASIC_INFORMATION memory_information;
    SIZE_T maximum_view_size;

    if (message.remote_memory_size == 0U
        || has_terminated_name(message.shared_memory_name) == FT_FALSE)
    {
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    maximum_view_size = std::numeric_limits<SIZE_T>::max();
    if (message.remote_memory_size > static_cast<uint64_t>(maximum_view_size))
    {
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }

    mapping_handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, message.shared_memory_name);
    if (mapping_handle == NULL)
    {
        windows_error = GetLastError();
        errno = static_cast<int32_t>(windows_error);
        return (cmp_map_system_error_to_ft(errno));
    }
    mapping_pointer = MapViewOfFile(mapping_handle, FILE_MAP_ALL_ACCESS, 0, 0, static_cast<SIZE_T>(message.remote_memory_size));
    CloseHandle(mapping_handle);
    if (mapping_pointer == ft_nullptr)
    {
        windows_error = GetLastError();
        errno = static_cast<int32_t>(windows_error);
        return (cmp_map_system_error_to_ft(errno));
    }
    if (VirtualQuery(mapping_pointer, &memory_information,
        sizeof(memory_information)) != sizeof(memory_information)
        || memory_information.RegionSize < static_cast<SIZE_T>(
            message.remote_memory_size))
    {
        UnmapViewOfFile(mapping_pointer);
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    mapping->mapping_address = reinterpret_cast<unsigned char *>(mapping_pointer);
    mapping->mapping_length = static_cast<ft_size_t>(message.remote_memory_size);
    mapping->platform_handle = ft_nullptr;
    mapping->mutex_address = ft_nullptr;
    if (message.shared_mutex_address == 0)
    {
        cmp_cross_process_close_mapping(mapping);
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    if (compute_offset(message.shared_mutex_address, message.stack_base_address,
        mutex_offset) != FT_ERR_SUCCESS
        || mutex_offset > mapping->mapping_length
        || sizeof(HANDLE) > mapping->mapping_length - mutex_offset)
    {
        cmp_cross_process_close_mapping(mapping);
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    mapping->mutex_address = mapping->mapping_address + mutex_offset;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_close_mapping(cmp_cross_process_mapping *mapping)
{
    if (mapping->mapping_address == ft_nullptr)
        return (FT_ERR_SUCCESS);
    if (UnmapViewOfFile(mapping->mapping_address) == 0)
    {
        DWORD windows_error;

        windows_error = GetLastError();
        errno = static_cast<int32_t>(windows_error);
        return (cmp_map_system_error_to_ft(errno));
    }
    mapping->mapping_address = ft_nullptr;
    mapping->mapping_length = 0;
    mapping->platform_handle = ft_nullptr;
    mapping->mutex_address = ft_nullptr;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_lock_mutex(const cross_process_message &message, cmp_cross_process_mapping *mapping, cmp_cross_process_mutex_state *mutex_state)
{
    (void)message;
    if (!mapping || mapping->mutex_address == ft_nullptr)
    {
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    HANDLE shared_mutex_handle = *reinterpret_cast<HANDLE *>(mapping->mutex_address);
    if (shared_mutex_handle == NULL)
    {
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }
    int32_t attempt_count = 0;
    ft_bool mutex_locked = FT_FALSE;
    mutex_state->owner_recovered = FT_FALSE;
    while (attempt_count < 5)
    {
        DWORD wait_result = WaitForSingleObject(shared_mutex_handle, 0);
        if (wait_result == WAIT_OBJECT_0 || wait_result == WAIT_ABANDONED)
        {
            mutex_locked = FT_TRUE;
            if (wait_result == WAIT_ABANDONED)
                mutex_state->owner_recovered = FT_TRUE;
            break ;
        }
        if (wait_result == WAIT_TIMEOUT)
        {
            attempt_count += 1;
            if (attempt_count >= 5)
                break ;
            Sleep(50);
            continue;
        }
        DWORD windows_error = GetLastError();
        errno = static_cast<int32_t>(windows_error);
        return (cmp_map_system_error_to_ft(errno));
    }
    if (mutex_locked == FT_FALSE)
    {
        errno = ETIMEDOUT;
        return (cmp_map_system_error_to_ft(errno));
    }
    mutex_state->platform_mutex = shared_mutex_handle;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_cross_process_unlock_mutex(const cross_process_message &message, cmp_cross_process_mapping *mapping, cmp_cross_process_mutex_state *mutex_state)
{
    HANDLE shared_mutex_handle;
    DWORD windows_error;

    (void)message;
    (void)mapping;
    shared_mutex_handle = reinterpret_cast<HANDLE>(mutex_state->platform_mutex);
    if (shared_mutex_handle == NULL)
        return (FT_ERR_SUCCESS);
    if (ReleaseMutex(shared_mutex_handle) == 0)
    {
        windows_error = GetLastError();
        errno = static_cast<int32_t>(windows_error);
        return (cmp_map_system_error_to_ft(errno));
    }
    mutex_state->platform_mutex = ft_nullptr;
    mutex_state->owner_recovered = FT_FALSE;
#if defined(LIBFT_TEST_BUILD)
    if (g_cross_process_fail_next_unlock == FT_TRUE)
    {
        g_cross_process_fail_next_unlock = FT_FALSE;
        errno = ERROR_GEN_FAILURE;
        return (cmp_map_system_error_to_ft(errno));
    }
#endif
    return (FT_ERR_SUCCESS);
}

#if defined(LIBFT_TEST_BUILD)
int32_t cmp_cross_process_test_fail_next_unlock(void)
{
    g_cross_process_fail_next_unlock = FT_TRUE;
    return (FT_ERR_SUCCESS);
}
#endif

#endif
