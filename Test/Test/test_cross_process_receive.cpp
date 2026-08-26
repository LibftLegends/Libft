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
#include <unistd.h>

namespace
{
    static int cleanup_shared_memory_file(const char *shared_memory_name)
    {
        char fallback_shared_memory_name[256];

        if (::shm_unlink(shared_memory_name) == 0)
            return (0);
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

    static int create_shared_memory(const char *name_prefix, const char *payload, size_t payload_length, cross_process_message &message,
        void *&mapping_ptr, unsigned char *&mapping, size_t &data_offset, size_t &error_offset)
    {
        char shared_memory_name[256];
        int shm_fd;
        size_t total_size;
        pthread_mutex_t *shared_mutex;
        pthread_mutexattr_t mutex_attributes;
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
        std::memset(mapping, 0, total_size);
        shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
        if (pthread_mutexattr_init(&mutex_attributes) != 0)
            return (-1);
        if (pthread_mutexattr_setpshared(&mutex_attributes,
                PTHREAD_PROCESS_SHARED) != 0)
        {
            (void)pthread_mutexattr_destroy(&mutex_attributes);
            return (-1);
        }
        if (pthread_mutex_init(shared_mutex, &mutex_attributes) != 0)
        {
            (void)pthread_mutexattr_destroy(&mutex_attributes);
            return (-1);
        }
        if (pthread_mutexattr_destroy(&mutex_attributes) != 0)
            return (-1);
        std::memcpy(mapping + data_offset, payload, payload_length);
        *reinterpret_cast<int *>(mapping + error_offset) = 123;
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

FT_TEST(test_cross_process_receive_memory_basic)
{
    cross_process_message message;
    cross_process_read_result result;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    size_t index;
    const char *payload;
    size_t payload_length;

    payload = "cross process payload";
    payload_length = std::strlen(payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_test", payload, payload_length, message, mapping_ptr, mapping, data_offset, error_offset));
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(0, receive_result);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    FT_ASSERT(result.shared_memory_name == message.shared_memory_name);
    FT_ASSERT_EQ(message.remote_memory_size - data_offset, result.payload.size());
    FT_ASSERT(std::memcmp(result.payload.data(), payload, payload_length) == 0);
    int captured_error_value;

    std::memcpy(&captured_error_value, result.payload.data() + payload_length, sizeof(int));
    FT_ASSERT_EQ(123, captured_error_value);
    index = 0;
    while (index < payload_length)
    {
        FT_ASSERT_EQ(0, mapping[data_offset + index]);
        index++;
    }
    int post_error_value;

    std::memcpy(&post_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(0, post_error_value);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_receive_memory_mutex_timeout)
{
    cross_process_message message;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    pthread_mutex_t *shared_mutex;

    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_mutex", "payload", std::strlen("payload"), message, mapping_ptr, mapping, data_offset, error_offset));
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
    FT_ASSERT_EQ(0, pthread_mutex_lock(shared_mutex));
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    cross_process_read_result timeout_result;

    errno = 0;
    receive_result = cp_receive_memory(sockets[1], timeout_result);
    FT_ASSERT_EQ(FT_ERR_IO, receive_result);
    FT_ASSERT_EQ(ETIMEDOUT, errno);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    FT_ASSERT_EQ(0, pthread_mutex_unlock(shared_mutex));
    FT_ASSERT_EQ(0, pthread_mutex_destroy(shared_mutex));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_receive_memory_invalid_payload_offset)
{
    cross_process_message message;
    cross_process_read_result result;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    const char *payload;
    size_t payload_length;
    size_t index;

    payload = "payload";
    payload_length = std::strlen(payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_invalid_payload", payload, payload_length, message, mapping_ptr, mapping,
        data_offset, error_offset));
    message.remote_memory_address = message.stack_base_address + message.remote_memory_size;
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    errno = 0;
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, receive_result);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    index = 0;
    while (index < payload_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(payload[index]), mapping[data_offset + index]);
        index++;
    }
    int stored_error_value;

    std::memcpy(&stored_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(123, stored_error_value);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_receive_memory_invalid_error_offset)
{
    cross_process_message message;
    cross_process_read_result result;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    const char *payload;
    size_t payload_length;
    size_t index;

    payload = "payload error";
    payload_length = std::strlen(payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_invalid_error", payload, payload_length, message, mapping_ptr, mapping,
        data_offset, error_offset));
    message.error_memory_address = message.stack_base_address + message.remote_memory_size;
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    errno = 0;
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, receive_result);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    index = 0;
    while (index < payload_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(payload[index]), mapping[data_offset + index]);
        index++;
    }
    int error_snapshot;

    std::memcpy(&error_snapshot, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(123, error_snapshot);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(reinterpret_cast<pthread_mutex_t *>(mapping)));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}

FT_TEST(test_cross_process_receive_memory_descriptor_disconnect)
{
    cross_process_read_result result;
    int sockets[2];
    int close_result;
    int receive_result;

    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    FT_ASSERT_EQ(0, ::close(sockets[0]));
    errno = 0;
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(FT_ERR_IO, receive_result);
    FT_ASSERT_EQ(ECONNRESET, errno);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    return (1);
}

FT_TEST(test_cross_process_receive_memory_missing_mapping)
{
    cross_process_message message;
    cross_process_read_result result;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    char shared_memory_name[64];

    std::memset(&message, 0, sizeof(message));
    std::snprintf(shared_memory_name, sizeof(shared_memory_name), "/cross_process_missing_%ld", static_cast<long>(getpid()));
    if (cleanup_shared_memory_file(shared_memory_name) != 0)
    {
        FT_ASSERT_EQ(ENOENT, errno);
    }
    std::snprintf(message.shared_memory_name, sizeof(message.shared_memory_name), "%s", shared_memory_name);
    message.remote_memory_size = 4096;
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    errno = 0;
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(FT_ERR_IO, receive_result);
    FT_ASSERT_EQ(ENOENT, errno);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    return (1);
}

FT_TEST(test_cross_process_receive_memory_invalid_mutex_offset)
{
    cross_process_message message;
    cross_process_read_result result;
    void *mapping_ptr;
    unsigned char *mapping;
    size_t data_offset;
    size_t error_offset;
    int sockets[2];
    int send_result;
    int receive_result;
    int close_result;
    const char *payload;
    size_t payload_length;
    size_t index;
    pthread_mutex_t *shared_mutex;

    payload = "mutex offset payload";
    payload_length = std::strlen(payload);
    mapping_ptr = ft_nullptr;
    mapping = ft_nullptr;
    data_offset = 0;
    error_offset = 0;
    FT_ASSERT_EQ(0, create_shared_memory("/cross_process_invalid_mutex", payload, payload_length, message, mapping_ptr, mapping,
        data_offset, error_offset));
    shared_mutex = reinterpret_cast<pthread_mutex_t *>(mapping);
    message.shared_mutex_address = message.stack_base_address + message.remote_memory_size;
    FT_ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    send_result = cp_send_descriptor(sockets[0], message);
    FT_ASSERT_EQ(0, send_result);
    errno = 0;
    receive_result = cp_receive_memory(sockets[1], result);
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, receive_result);
    close_result = ::close(sockets[0]);
    FT_ASSERT_EQ(0, close_result);
    close_result = ::close(sockets[1]);
    FT_ASSERT_EQ(0, close_result);
    index = 0;
    while (index < payload_length)
    {
        FT_ASSERT_EQ(static_cast<unsigned char>(payload[index]), mapping[data_offset + index]);
        index++;
    }
    int stored_error_value;

    std::memcpy(&stored_error_value, mapping + error_offset, sizeof(int));
    FT_ASSERT_EQ(123, stored_error_value);
    FT_ASSERT_EQ(0, pthread_mutex_destroy(shared_mutex));
    FT_ASSERT_EQ(0, munmap(mapping_ptr, message.remote_memory_size));
    FT_ASSERT_EQ(0, cleanup_shared_memory_file(message.shared_memory_name));
    return (1);
}
#endif
