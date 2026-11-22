#include "../test_internal.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include <limits>

#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_strerror_covers_documented_codes)
{
    static const int error_code_table[] = {
        FT_ERR_SUCCESS,
        FT_ERR_NO_MEMORY,
        FT_ERR_INVALID_ARGUMENT,
        FT_ERR_INVALID_POINTER,
        FT_ERR_INVALID_HANDLE,
        FT_ERR_INVALID_OPERATION,
        FT_ERR_INVALID_STATE,
        FT_ERR_NOT_FOUND,
        FT_ERR_ALREADY_EXISTS,
        FT_ERR_OUT_OF_RANGE,
        FT_ERR_EMPTY,
        FT_ERR_FULL,
        FT_ERR_IO,
        FT_ERR_OVERLAP,
        FT_ERR_TERMINATED,
        FT_ERR_INTERNAL,
        FT_ERR_CONFIGURATION,
        FT_ERR_UNSUPPORTED_TYPE,
        FT_ERR_ALREADY_INITIALISED,
        FT_ERR_INITIALIZATION_FAILED,
        FT_ERR_END_OF_FILE,
        FT_ERR_DIVIDE_BY_ZERO,
        FT_ERR_BROKEN_PROMISE,
        FT_ERR_SOCKET_CREATION_FAILED,
        FT_ERR_SOCKET_BIND_FAILED,
        FT_ERR_SOCKET_LISTEN_FAILED,
        FT_ERR_SOCKET_CONNECT_FAILED,
        FT_ERR_INVALID_IP_FORMAT,
        FT_ERR_SOCKET_ACCEPT_FAILED,
        FT_ERR_SOCKET_SEND_FAILED,
        FT_ERR_SOCKET_RECEIVE_FAILED,
        FT_ERR_SOCKET_CLOSE_FAILED,
        FT_ERR_SOCKET_JOIN_GROUP_FAILED,
        FT_ERR_SOCKET_RESOLVE_FAILED,
        FT_ERR_SOCKET_RESOLVE_BAD_FLAGS,
        FT_ERR_SOCKET_RESOLVE_AGAIN,
        FT_ERR_SOCKET_RESOLVE_FAIL,
        FT_ERR_SOCKET_RESOLVE_FAMILY,
        FT_ERR_SOCKET_RESOLVE_SOCKTYPE,
        FT_ERR_SOCKET_RESOLVE_SERVICE,
        FT_ERR_SOCKET_RESOLVE_MEMORY,
        FT_ERR_SOCKET_RESOLVE_NO_NAME,
        FT_ERR_SOCKET_RESOLVE_OVERFLOW,
        FT_ERR_GAME_GENERAL_ERROR,
        FT_ERR_GAME_INVALID_MOVE,
        FT_ERR_MUTEX_NOT_OWNER,
        FT_ERR_MUTEX_ALREADY_LOCKED,
        FT_ERR_SSL_WANT_READ,
        FT_ERR_SSL_WANT_WRITE,
        FT_ERR_SSL_ZERO_RETURN,
        FT_ERR_BITSET_NO_MEMORY,
        FT_ERR_PRIORITY_QUEUE_EMPTY,
        FT_ERR_PRIORITY_QUEUE_NO_MEMORY,
        FT_ERR_SSL_SYSCALL_ERROR
    };
    size_t code_index;
    size_t code_count;

    code_index = 0;
    code_count = sizeof(error_code_table) / sizeof(error_code_table[0]);
    while (code_index < code_count)
    {
        const char *error_message;

        error_message = ft_strerror(error_code_table[code_index]);
        FT_ASSERT(error_message != ft_nullptr);
        code_index++;
    }
    return (1);
}

FT_TEST(test_errno_lookup_handles_dense_edges_gaps_and_sparse_codes)
{
    FT_ASSERT_EQ(0, std::strcmp("SSL system call error",
            ft_strerror(FT_ERR_SSL_SYSCALL_ERROR)));
    FT_ASSERT_EQ(0, std::strcmp("RW-lock reader ownership capacity reached",
            ft_strerror(FT_ERR_RWLOCK_READER_CAPACITY)));
    FT_ASSERT_EQ(0, std::strcmp("HTTP protocol mismatch",
            ft_strerror(FT_ERR_HTTP_PROTOCOL_MISMATCH)));
    FT_ASSERT_EQ(0, std::strcmp("API circuit breaker is open",
            ft_strerror(FT_ERR_API_CIRCUIT_OPEN)));
    FT_ASSERT_EQ(0, std::strcmp("Unrecognized error code", ft_strerror(1)));
    FT_ASSERT_EQ(0, std::strcmp("Unrecognized error code",
            ft_strerror(std::numeric_limits<int32_t>::min())));
    FT_ASSERT(ft_strerror(ERRNO_OFFSET + 2) != ft_nullptr);
    FT_ASSERT(std::strcmp("Unrecognized error code", ft_strerror(ERRNO_OFFSET + 2)) != 0);
    return (1);
}
