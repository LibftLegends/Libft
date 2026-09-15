#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifndef _WIN32

#include "compatebility_cross_process.hpp"
#include "compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <cstdio>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

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

static int32_t open_file_backing(const cross_process_message &message)
{
    int32_t file_descriptor;

    if (has_terminated_name(message.shared_memory_name) == FT_FALSE
        || message.shared_memory_name[0] == '\0')
    {
        errno = EINVAL;
        return (-1);
    }
    file_descriptor = open(message.shared_memory_name, O_RDWR);
    if (file_descriptor < 0)
        return (-1);
    return (file_descriptor);
}

int32_t cmp_cross_process_send_descriptor(int32_t socket_file_descriptor, const cross_process_message &message)
{
    uint8_t wire[CMP_CROSS_PROCESS_WIRE_SIZE];
    ft_size_t total_size;
    ft_size_t offset;

    if (cmp_cross_process_encode_wire(message, wire, sizeof(wire))
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    total_size = sizeof(wire);
    offset = 0;
    while (offset < total_size)
    {
        int64_t written;

        written = ::write(socket_file_descriptor, wire + offset,
                total_size - offset);
        if (written < 0)
        {
            if (errno == EINTR)
                continue;
            return (cmp_map_system_error_to_ft(errno));
        }
        offset += static_cast<ft_size_t>(written);
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
        int64_t received;

        received = ::read(socket_file_descriptor, wire + offset,
                total_size - offset);
        if (received < 0)
        {
            if (errno == EINTR)
                continue;
            return (cmp_map_system_error_to_ft(errno));
        }
        if (received == 0)
        {
            errno = ECONNRESET;
            return (cmp_map_system_error_to_ft(errno));
        }
        offset += static_cast<ft_size_t>(received);
    }
    return (cmp_cross_process_decode_wire(wire, total_size, message));
}

int32_t cmp_cross_process_open_mapping(const cross_process_message &message, cmp_cross_process_mapping *mapping)
{
    int32_t shared_memory_fd;
    void *mapping_pointer;
    struct stat file_status;
    ft_size_t mutex_offset;

    if (message.remote_memory_size == 0U
        || has_terminated_name(message.shared_memory_name) == FT_FALSE)
    {
        errno = EINVAL;
        return (cmp_map_system_error_to_ft(errno));
    }

    shared_memory_fd = shm_open(message.shared_memory_name, O_RDWR, 0600);
    if (shared_memory_fd < 0)
    {
        shared_memory_fd = open_file_backing(message);
        if (shared_memory_fd < 0)
        {
            return (cmp_file_error_to_errno(errno));
        }
    }
    if (fstat(shared_memory_fd, &file_status) != 0)
    {
        ::close(shared_memory_fd);
        return (cmp_map_system_error_to_ft(errno));
    }
    if (file_status.st_size < 0
        || static_cast<uint64_t>(file_status.st_size)
            < message.remote_memory_size)
    {
        errno = EINVAL;
        ::close(shared_memory_fd);
        return (cmp_map_system_error_to_ft(errno));
    }
    mapping_pointer = mmap(ft_nullptr, message.remote_memory_size, PROT_READ | PROT_WRITE, MAP_SHARED, shared_memory_fd, 0);
    ::close(shared_memory_fd);
    if (mapping_pointer == MAP_FAILED)
    {
        return (cmp_file_error_to_errno(errno));
    }
    mapping->mapping_address = reinterpret_cast<unsigned char *>(mapping_pointer);
    mapping->mapping_length = message.remote_memory_size;
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
        || sizeof(pthread_mutex_t) > mapping->mapping_length - mutex_offset)
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
    if (munmap(mapping->mapping_address, mapping->mapping_length) != 0)
        return (cmp_map_system_error_to_ft(errno));
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
    pthread_mutex_t *shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping->mutex_address);
    int32_t attempt_count;

    mutex_state->owner_recovered = FT_FALSE;
    attempt_count = 0;
    while (attempt_count < 5)
    {
        int32_t lock_error = pthread_mutex_trylock(shared_mutex);
        if (lock_error == 0)
        {
            mutex_state->platform_mutex = shared_mutex;
            return (FT_ERR_SUCCESS);
        }
#if defined(__linux__) && defined(EOWNERDEAD)
        if (lock_error == EOWNERDEAD)
        {
            int32_t consistent_error;

            consistent_error = pthread_mutex_consistent(shared_mutex);
            if (consistent_error != 0)
            {
                errno = consistent_error;
                return (cmp_map_system_error_to_ft(errno));
            }
            mutex_state->platform_mutex = shared_mutex;
            mutex_state->owner_recovered = FT_TRUE;
            errno = EOWNERDEAD;
            return (FT_ERR_SUCCESS);
        }
#endif
        if (lock_error != EBUSY)
        {
            errno = lock_error;
            return (cmp_map_system_error_to_ft(errno));
        }
        if (attempt_count >= 4)
            break ;
        usleep(50000);
        attempt_count += 1;
    }
    errno = ETIMEDOUT;
    return (cmp_map_system_error_to_ft(errno));
}

int32_t cmp_cross_process_unlock_mutex(const cross_process_message &message, cmp_cross_process_mapping *mapping, cmp_cross_process_mutex_state *mutex_state)
{
    pthread_mutex_t *shared_mutex;

    (void)message;
    (void)mapping;
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mutex_state->platform_mutex);
    if (shared_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    int32_t unlock_error = pthread_mutex_unlock(shared_mutex);
    if (unlock_error != 0)
    {
        errno = unlock_error;
        return (cmp_map_system_error_to_ft(errno));
    }
    mutex_state->platform_mutex = ft_nullptr;
    mutex_state->owner_recovered = FT_FALSE;
#if defined(LIBFT_TEST_BUILD)
    if (g_cross_process_fail_next_unlock == FT_TRUE)
    {
        g_cross_process_fail_next_unlock = FT_FALSE;
        errno = EIO;
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
