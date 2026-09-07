#include "../test_internal.hpp"
#include "../../Modules/CrossProcess/cross_process.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <limits>
#include <chrono>

#ifndef _WIN32
#include "../../Modules/Basic/class_nullptr.hpp"
#include <fcntl.h>
#include <pthread.h>
#include <sys/mman.h>
#if defined(_WIN32) || defined(_WIN64)
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <sys/socket.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{
    static int cleanup_shared_memory_file(const char *shared_memory_name)
    {
        char fallback_shared_memory_name[256];

        if (::unlink(shared_memory_name) == 0)
            return (0);
        if (errno == ENOENT || errno == EROFS)
        {
            if (shared_memory_name[0] == '/' && std::strncmp(shared_memory_name, "/tmp/", 5) != 0)
            {
                std::snprintf(fallback_shared_memory_name, sizeof(fallback_shared_memory_name), "/tmp/%s",
                    shared_memory_name + 1);
                if (::unlink(fallback_shared_memory_name) == 0)
                    return (0);
                if (errno == ENOENT || errno == EROFS)
                    return (0);
            }
            else
                return (0);
        }
        return (-1);
    }

    static void build_shared_memory_path(const char *name_prefix, char *shared_memory_name, size_t shared_memory_name_size,
        long process_identifier, size_t shared_memory_suffix)
    {
        const char *path_prefix;

        path_prefix = name_prefix;
        if (path_prefix[0] == '/')
            path_prefix += 1;
        std::snprintf(shared_memory_name, shared_memory_name_size, "/tmp/%s_%ld_%zu",
            path_prefix, process_identifier, shared_memory_suffix);
    }

    static size_t cross_process_available_length(uint64_t total_size, size_t offset) noexcept
    {
        uint64_t remaining;

        if (total_size < offset)
            return (0);
        remaining = total_size - offset;
        if constexpr (sizeof(size_t) >= sizeof(uint64_t))
            return (remaining);
        if (remaining > std::numeric_limits<size_t>::max())
            return (std::numeric_limits<size_t>::max());
        return (remaining);
    }

    static int create_shared_memory(const char *name_prefix, size_t payload_length, cross_process_message &message,
        void *&mapping_ptr, unsigned char *&mapping, size_t &data_offset,
        size_t &error_offset, int robust_mutex = 0)
    {
        char shared_memory_name[256];
        int shm_fd;
        size_t total_size;
        pthread_mutex_t *shared_mutex;
        static size_t shared_memory_suffix_seed = 0;
        size_t shared_memory_suffix;

        data_offset = sizeof(pthread_mutex_t);
        error_offset = data_offset + payload_length;
        total_size = error_offset + sizeof(int);
        if (shared_memory_suffix_seed == 0)
            shared_memory_suffix_seed = static_cast<size_t>(
                    std::chrono::high_resolution_clock::now().time_since_epoch().count());
        shared_memory_suffix = shared_memory_suffix_seed++;
        build_shared_memory_path(name_prefix, shared_memory_name, sizeof(shared_memory_name),
            static_cast<long>(getpid()), shared_memory_suffix);
        (void)cleanup_shared_memory_file(shared_memory_name);
        shm_fd = ::open(shared_memory_name, O_CREAT | O_RDWR | O_TRUNC, 0600);
        if (shm_fd < 0)
            return (-1);
        if (ftruncate(shm_fd, static_cast<off_t>(total_size)) != 0)
        {
            ::close(shm_fd);
            return (-1);
        }
        mapping_ptr = mmap(ft_nullptr, total_size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (mapping_ptr == MAP_FAILED)
        {
            ::close(shm_fd);
            return (-1);
        }
        mapping = reinterpret_cast<unsigned char *>(mapping_ptr);
        if (::close(shm_fd) != 0)
            return (-1);
        std::memset(mapping, 0x7f, total_size);
        shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
        if (robust_mutex == 0)
        {
            if (pthread_mutex_init(shared_mutex, ft_nullptr) != 0)
                return (-1);
        }
        else
        {
#if defined(__linux__)
            pthread_mutexattr_t mutex_attributes;

            if (pthread_mutexattr_init(&mutex_attributes) != 0)
                return (-1);
            if (pthread_mutexattr_setpshared(&mutex_attributes,
                PTHREAD_PROCESS_SHARED) != 0
                || pthread_mutexattr_setrobust(&mutex_attributes,
                    PTHREAD_MUTEX_ROBUST) != 0
                || pthread_mutex_init(shared_mutex, &mutex_attributes) != 0)
            {
                (void)pthread_mutexattr_destroy(&mutex_attributes);
                return (-1);
            }
            if (pthread_mutexattr_destroy(&mutex_attributes) != 0)
                return (-1);
#else
            return (-1);
#endif
        }
        std::memset(&message, 0, sizeof(message));
        message.stack_base_address = reinterpret_cast<uint64_t>(mapping);
        message.remote_memory_address = message.stack_base_address + data_offset;
        message.remote_memory_size = total_size;
        message.shared_mutex_address = message.stack_base_address;
        message.error_memory_address = message.stack_base_address + error_offset;
        std::snprintf(message.shared_memory_name, sizeof(message.shared_memory_name), "%s", shared_memory_name);
        return (0);
    }
}

FT_TEST(test_cross_process_write_memory_basic)
{
    cross_process_message message;
    cross_process_message received_message;
    cross_process_read_result result;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int descriptor_result;
    int write_result;
    int receive_result;
    int close_result;
    size_t index;
    const char *written_payload;
    size_t written_payload_length;
    int error_value;

    written_payload = "cross process write payload";
    written_payload_length = std::strlen(written_payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_write_test", written_payload_length, message, mapping_ptr, mapping, data_offset, error_offset));
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    std::memset(&received_message, 0, sizeof(received_message));
    descriptor_result = cp_receive_descriptor(sockets[1], received_message);
    FT_ASSERT_EQ(0, descriptor_result);
    FT_ASSERT(std::memcmp(&message, &received_message, sizeof(cross_process_message)) == 0);
    error_value = -42;
    write_result = cp_write_memory(received_message, reinterpret_cast<const unsigned char *>(written_payload), written_payload_length, error_value);
    FT_ASSERT_EQ(0, write_result);
    index = 0;
    while (index < written_payload_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(written_payload[index]), mapping[data_offset + index]);
        index++;
    }
    int mapped_error_value;

    std::memcpy(&mapped_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(error_value, mapped_error_value);
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(0, receive_result);
    FT_ASSERT(result.shared_memory_name == message.shared_memory_name);
    FT_ASSERT_EQ(cross_process_available_length(message.remote_memory_size, data_offset),
        result.payload.size());
    FT_ASSERT(std::memcmp(result.payload.data(), written_payload, written_payload_length) == 0);
    int captured_error;

    std::memcpy(&captured_error, result.payload.data() + written_payload_length, sizeof(int));
    FT_ASSERT_EQ(error_value, captured_error);
    index = 0;
    while (index < written_payload_length)
    {
        FT_ASSERT_EQ(0, mapping[data_offset + index]);
        index++;
    }
    int cleared_error_value;

    std::memcpy(&cleared_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(0, cleared_error_value);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_length_error)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    const unsigned char payload[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    int write_result;
    size_t index;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_length", 4, message, mapping_ptr, mapping, data_offset, error_offset));
    errno = 0;
    write_result = cp_write_memory(message, payload, 8, 11);
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, write_result);
    index = 0;
    while (index < 4)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + index]);
        index++;
    }
    FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + 4]);
    FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + 5]);
    FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + 6]);
    FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + 7]);
    int error_region_snapshot;

    std::memcpy(&error_region_snapshot, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(0x7f7f7f7f, error_region_snapshot);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_null_payload)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int write_result;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_null_payload", 16, message, mapping_ptr, mapping, data_offset, error_offset));
    errno = 0;
    write_result = cp_write_memory(message, ft_nullptr, 4, -2);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, write_result);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_invalid_payload_offset)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    const unsigned char payload[4] = {9, 8, 7, 6};
    int write_result;
    size_t index;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_invalid_payload_offset", 4, message, mapping_ptr, mapping, data_offset,
        error_offset));
    message.remote_memory_address = message.stack_base_address + message.remote_memory_size;
    errno = 0;
    write_result = cp_write_memory(message, payload, 4, 17);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, write_result);
    index = 0;
    while (index < 4)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + index]);
        index++;
    }
    int error_snapshot;

    std::memcpy(&error_snapshot, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(0x7f7f7f7f, error_snapshot);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_invalid_error_offset)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    const unsigned char payload[6] = {1, 3, 5, 7, 9, 11};
    int write_result;
    size_t index;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_invalid_error_offset", 8, message, mapping_ptr, mapping, data_offset,
        error_offset));
    message.error_memory_address = message.stack_base_address + message.remote_memory_size;
    errno = 0;
    write_result = cp_write_memory(message, payload, 6, 4);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, write_result);
    FT_ASSERT_EQ(EINVAL, errno);
    index = 0;
    while (index < 8)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f),
            mapping[data_offset + index]);
        index++;
    }
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_zero_length_payload)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int write_result;
    size_t index;
    int stored_error_value;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_zero_length", 10, message, mapping_ptr, mapping, data_offset, error_offset));
    errno = 0;
    write_result = cp_write_memory(message, ft_nullptr, 0, 64);
    FT_ASSERT_EQ(0, write_result);
    index = 0;
    while (index < 10)
    {
        FT_ASSERT_EQ(0, mapping[data_offset + index]);
        index++;
    }
    std::memcpy(&stored_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(64, stored_error_value);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_mutex_timeout)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    pthread_mutex_t *shared_mutex;
    int write_result;
    size_t index;
    size_t available_length;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_mutex_timeout", 12, message, mapping_ptr, mapping, data_offset, error_offset));
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
    FT_ASSERT_EQ(0, pthread_mutex_lock(shared_mutex));
    errno = 0;
    write_result = cp_write_memory(message, reinterpret_cast<const unsigned char *>("timeout"), 7, 91);
    FT_ASSERT_EQ(FT_ERR_IO, write_result);
    FT_ASSERT_EQ(ETIMEDOUT, errno);
    available_length = cross_process_available_length(message.remote_memory_size, data_offset);
    index = 0;
    while (index < available_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + index]);
        index++;
    }
    FT_ASSERT_EQ(0, pthread_mutex_unlock(shared_mutex));
    FT_ASSERT_EQ(0, pthread_mutex_destroy(shared_mutex));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_open_failure)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int write_result;
    size_t index;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_open_failure", 6, message, mapping_ptr, mapping, data_offset, error_offset));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    errno = 0;
    write_result = cp_write_memory(message, reinterpret_cast<const unsigned char *>("broken"), 6, -11);
    FT_ASSERT_EQ(FT_ERR_IO, write_result);
    FT_ASSERT_EQ(ENOENT, errno);
    index = 0;
    while (index < cross_process_available_length(message.remote_memory_size, data_offset))
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + index]);
        index++;
    }
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    return (1);
}

FT_TEST(test_cross_process_write_memory_invalid_mutex_offset)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int write_result;
    size_t available_length;
    size_t index;
    pthread_mutex_t *shared_mutex;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_write_invalid_mutex", 8, message, mapping_ptr, mapping, data_offset,
        error_offset));
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
    message.shared_mutex_address = message.stack_base_address + message.remote_memory_size;
    errno = 0;
    write_result = cp_write_memory(message, reinterpret_cast<const unsigned char *>("payload"), 7, 31);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, write_result);
    available_length = cross_process_available_length(message.remote_memory_size, data_offset);
    index = 0;
    while (index < available_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(0x7f), mapping[data_offset + index]);
        index++;
    }
    FT_ASSERT_EQ(0, pthread_mutex_destroy(shared_mutex));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_without_error_slot)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    const char *payload;
    size_t payload_length;
    int write_result;
    size_t index;
    int stored_error_value;

    payload = "no error payload";
    payload_length = std::strlen(payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_write_no_error", payload_length, message, mapping_ptr, mapping,
        data_offset, error_offset));
    message.error_memory_address = 0;
    errno = 0;
    write_result = cp_write_memory(message, reinterpret_cast<const unsigned char *>(payload),
        payload_length, 55);
    FT_ASSERT_EQ(0, write_result);
    index = 0;
    while (index < payload_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(payload[index]), mapping[data_offset + index]);
        index++;
    }
    while (index < cross_process_available_length(message.remote_memory_size, data_offset))
    {
        FT_ASSERT_EQ(0, mapping[data_offset + index]);
        index++;
    }
    std::memcpy(&stored_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(0, stored_error_value);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, cross_process_available_length(message.remote_memory_size, 0)));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_write_memory_rejects_address_below_base)
{
    cross_process_message message;
    const unsigned char payload[1] = {1U};
    int32_t write_result;

    std::memset(&message, 0, sizeof(message));
    message.stack_base_address = 0x100000U;
    message.remote_memory_address = message.stack_base_address - 1U;
    message.remote_memory_size = 16U;
    errno = 0;
    write_result = cp_write_memory(message, payload, sizeof(payload), 0);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, write_result);
    FT_ASSERT_EQ(EINVAL, errno);
    return (1);
}

FT_TEST(test_cross_process_write_memory_rejects_unterminated_name)
{
    cross_process_message message;
    const unsigned char payload[1] = {1U};
    int32_t write_result;

    std::memset(&message, 0, sizeof(message));
    std::memset(message.shared_memory_name, 0xff,
        sizeof(message.shared_memory_name));
    message.stack_base_address = 0x100000U;
    message.remote_memory_address = message.stack_base_address + 1U;
    message.remote_memory_size = 16U;
    errno = 0;
    write_result = cp_write_memory(message, payload, sizeof(payload), 0);
    FT_ASSERT(write_result != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(EINVAL, errno);
    return (1);
}

FT_TEST(test_cross_process_write_memory_rejects_oversized_mapping)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int32_t result;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0U;
    error_offset = 0U;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_short_backing", 8U,
        message, mapping_ptr, mapping, data_offset, error_offset));
    message.remote_memory_size += 4096U;
    errno = 0;
    result = cp_write_memory(message,
        reinterpret_cast<const unsigned char *>("payload"), 7U, 0);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, result);
    FT_ASSERT_EQ(EINVAL, errno);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(
        reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr,
        message.remote_memory_size - 4096U));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

#if defined(__linux__)
FT_TEST(test_cross_process_write_memory_recovers_owner_death)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    pthread_mutex_t *shared_mutex;
    pid_t child_process;
    int child_status;
    int32_t result;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0U;
    error_offset = 0U;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_owner_death", 8U,
        message, mapping_ptr, mapping, data_offset, error_offset, 1));
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
    child_process = fork();
    FT_ASSERT(child_process >= 0);
    if (child_process == 0)
    {
        if (pthread_mutex_lock(shared_mutex) != 0)
            _exit(2);
        _exit(0);
    }
    FT_ASSERT_EQ(child_process, waitpid(child_process, &child_status, 0));
    FT_ASSERT(WIFEXITED(child_status));
    FT_ASSERT_EQ(0, WEXITSTATUS(child_status));
    errno = 0;
    result = cp_write_memory(message,
        reinterpret_cast<const unsigned char *>("x"), 1U, 7);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, result);
    FT_ASSERT_EQ(EOWNERDEAD, errno);
    result = cp_write_memory(message,
        reinterpret_cast<const unsigned char *>("x"), 1U, 7);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(shared_mutex));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}
#endif
#endif
