#include "networking.hpp"
#include "../Errno/errno.hpp"
#include "../CMA/CMA.hpp"
#include "../Compatebility/compatebility_internal.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <algorithm>
#include <cerrno>
#ifdef _WIN32
# include <winsock2.h>
# include <io.h>
#else
# include <poll.h>
#endif

#ifdef _WIN32
static ft_bool nw_windows_descriptor_is_pipe(int32_t file_descriptor)
{
    HANDLE file_handle;
    DWORD file_type;
    int32_t socket_type;
    int32_t socket_type_length;

    socket_type = 0;
    socket_type_length = sizeof(socket_type);
    if (getsockopt(static_cast<SOCKET>(file_descriptor), SOL_SOCKET, SO_TYPE,
        reinterpret_cast<char *>(&socket_type), &socket_type_length) == 0)
        return (FT_FALSE);
    file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(file_descriptor));
    if (file_handle == INVALID_HANDLE_VALUE)
        return (FT_FALSE);
    file_type = GetFileType(file_handle);
    if (file_type == FILE_TYPE_PIPE)
        return (FT_TRUE);
    return (FT_FALSE);
}

static int32_t nw_windows_scan_pipe_ready(int32_t *read_file_descriptors,
    int32_t read_count, int32_t *write_file_descriptors, int32_t write_count,
    int32_t *read_ready_flags, int32_t *write_ready_flags)
{
    int32_t index;
    HANDLE file_handle;
    DWORD bytes_available;
    BOOL pipe_is_readable;
    DWORD pipe_error;
    ft_bool has_ready_descriptor;
    fd_set read_set;
    fd_set write_set;
    timeval zero_timeout;
    int32_t select_result;

    has_ready_descriptor = FT_FALSE;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    index = 0;
    while (index < read_count)
    {
        if (read_ready_flags)
            read_ready_flags[index] = 0;
        index++;
    }
    index = 0;
    while (index < write_count)
    {
        if (write_ready_flags)
            write_ready_flags[index] = 0;
        index++;
    }
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
        {
            if (nw_windows_descriptor_is_pipe(read_file_descriptors[index])
                == FT_TRUE)
            {
                file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(
                        read_file_descriptors[index]));
                if (file_handle != INVALID_HANDLE_VALUE)
                {
                    bytes_available = 0;
                    pipe_is_readable = PeekNamedPipe(file_handle, NULL, 0,
                            NULL, &bytes_available, NULL);
                    pipe_error = GetLastError();
                    if ((pipe_is_readable != 0 && bytes_available > 0)
                        || (pipe_is_readable == 0
                            && pipe_error == ERROR_BROKEN_PIPE))
                    {
                        if (read_ready_flags)
                            read_ready_flags[index] = 1;
                        has_ready_descriptor = FT_TRUE;
                    }
                }
            }
            else
            {
                FD_SET(static_cast<SOCKET>(read_file_descriptors[index]),
                    &read_set);
            }
        }
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
        {
            if (nw_windows_descriptor_is_pipe(write_file_descriptors[index])
                == FT_TRUE)
            {
                if (write_ready_flags)
                    write_ready_flags[index] = 1;
                has_ready_descriptor = FT_TRUE;
            }
            else
            {
                FD_SET(static_cast<SOCKET>(write_file_descriptors[index]),
                    &write_set);
            }
        }
        index++;
    }
    zero_timeout.tv_sec = 0;
    zero_timeout.tv_usec = 0;
    if (read_set.fd_count != 0 || write_set.fd_count != 0)
    {
        fd_set *read_interest = ft_nullptr;
        fd_set *write_interest = ft_nullptr;
        if (read_set.fd_count != 0)
            read_interest = &read_set;
        if (write_set.fd_count != 0)
            write_interest = &write_set;
        select_result = select(0, read_interest, write_interest, ft_nullptr, &zero_timeout);
        if (select_result < 0)
            return (-1);
        if (select_result > 0)
        {
            index = 0;
            while (read_file_descriptors && index < read_count)
            {
                if (read_file_descriptors[index] >= 0
                    && FD_ISSET(static_cast<SOCKET>(
                        read_file_descriptors[index]), &read_set))
                {
                    if (read_ready_flags)
                        read_ready_flags[index] = 1;
                    has_ready_descriptor = FT_TRUE;
                }
                index++;
            }
            index = 0;
            while (write_file_descriptors && index < write_count)
            {
                if (write_file_descriptors[index] >= 0
                    && FD_ISSET(static_cast<SOCKET>(
                        write_file_descriptors[index]), &write_set))
                {
                    if (write_ready_flags)
                        write_ready_flags[index] = 1;
                    has_ready_descriptor = FT_TRUE;
                }
                index++;
            }
        }
    }
    return (has_ready_descriptor);
}

static int32_t nw_windows_poll_pipes(int32_t *read_file_descriptors, int32_t read_count,
    int32_t *write_file_descriptors, int32_t write_count, int32_t timeout_milliseconds)
{
    int32_t pipe_ready_count;
    int32_t *read_ready_flags;
    int32_t *write_ready_flags;
    int64_t elapsed_milliseconds;
    int64_t start_milliseconds;
    int32_t readiness;

    read_ready_flags = ft_nullptr;
    write_ready_flags = ft_nullptr;
    if (read_count > 0 && read_file_descriptors != ft_nullptr)
    {
        read_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                    * static_cast<ft_size_t>(read_count)));
        if (read_ready_flags == ft_nullptr)
            return (-1);
    }
    if (write_count > 0 && write_file_descriptors != ft_nullptr)
    {
        write_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                    * static_cast<ft_size_t>(write_count)));
        if (write_ready_flags == ft_nullptr)
        {
            if (read_ready_flags)
                cma_free(read_ready_flags);
            return (-1);
        }
    }
    start_milliseconds = static_cast<int64_t>(GetTickCount64());
    while (1)
    {
        readiness = nw_windows_scan_pipe_ready(
            read_file_descriptors, read_count, write_file_descriptors,
            write_count, read_ready_flags, write_ready_flags);
        if (readiness != 0 || timeout_milliseconds == 0)
            break ;
        elapsed_milliseconds = static_cast<int64_t>(GetTickCount64())
            - start_milliseconds;
        if (timeout_milliseconds > 0
            && elapsed_milliseconds >= timeout_milliseconds)
            break ;
        cmp_thread_sleep(1);
    }
    if (readiness < 0)
    {
        if (read_ready_flags)
            cma_free(read_ready_flags);
        if (write_ready_flags)
            cma_free(write_ready_flags);
        return (-1);
    }
    pipe_ready_count = 0;
    int32_t index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0 && read_ready_flags
            && read_ready_flags[index] != 0)
            pipe_ready_count++;
        else
            read_file_descriptors[index] = -1;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0 && write_ready_flags
            && write_ready_flags[index] != 0)
        {
            if (read_file_descriptors == ft_nullptr)
                pipe_ready_count++;
            else
            {
                int32_t search_index;
                ft_bool already_counted;

                search_index = 0;
                already_counted = FT_FALSE;
                while (search_index < read_count)
                {
                    if (read_file_descriptors[search_index]
                        == write_file_descriptors[index])
                    {
                        already_counted = FT_TRUE;
                        break ;
                    }
                    search_index++;
                }
                if (already_counted == FT_FALSE)
                    pipe_ready_count++;
            }
        }
        else
            write_file_descriptors[index] = -1;
        index++;
    }
    if (read_ready_flags)
        cma_free(read_ready_flags);
    if (write_ready_flags)
        cma_free(write_ready_flags);
    return (pipe_ready_count);
}

#endif

static int32_t nw_poll_count_active(const int32_t *descriptors, int32_t count)
{
    int32_t active = 0;
    int32_t index = 0;
    while (descriptors && index < count)
    {
        if (descriptors[index] >= 0)
            ++active;
        ++index;
    }
    return (active);
}

static void nw_poll_collect(struct pollfd *requests, int32_t &count,
    const int32_t *descriptors, int32_t descriptor_count, int16_t interest)
{
    int32_t index = 0;
    while (descriptors && index < descriptor_count)
    {
        if (descriptors[index] >= 0)
            requests[count++] = {static_cast<decltype(pollfd::fd)>(descriptors[index]), interest, 0};
        ++index;
    }
    return ;
}

static ft_bool nw_poll_by_descriptor(const struct pollfd &left, const struct pollfd &right)
{
    return (left.fd < right.fd);
}

static int32_t nw_poll_merge(struct pollfd *requests, int32_t count)
{
    std::sort(requests, requests + count, nw_poll_by_descriptor);
    int32_t unique = 0;
    int32_t index = 0;
    while (index < count)
    {
        struct pollfd request = requests[index++];
        while (index < count && requests[index].fd == request.fd)
            request.events |= requests[index++].events;
        requests[unique++] = request;
    }
    return (unique);
}

static void nw_poll_filter(int32_t *descriptors, int32_t count, int16_t interest,
    const struct pollfd *requests, int32_t request_count)
{
    int32_t index = 0;
    while (descriptors && index < count)
    {
        int32_t &descriptor = descriptors[index++];
        if (request_count == 0 || descriptor < 0)
        {
            descriptor = -1;
            continue ;
        }
        const struct pollfd target{static_cast<decltype(pollfd::fd)>(descriptor), 0, 0};
        const struct pollfd *request = std::lower_bound(requests, requests + request_count,
                target, nw_poll_by_descriptor);
        if (request == requests + request_count || request->fd != target.fd
            || (request->revents & interest) == 0)
            descriptor = -1;
    }
    return ;
}

static int32_t nw_poll_descriptor_list(int32_t *read_file_descriptors, int32_t read_count,
    int32_t *write_file_descriptors, int32_t write_count, int32_t timeout_milliseconds)
{
#ifdef _WIN32
    // Two standard Winsock sets can reach this path due to one wake socket.
    constexpr int32_t STACK_REQUESTS = 2 * 64 + 1;
#else
    constexpr int32_t STACK_REQUESTS = 32;
#endif
    const int64_t active = static_cast<int64_t>(
        nw_poll_count_active(read_file_descriptors, read_count))
        + nw_poll_count_active(write_file_descriptors, write_count);
    if (active > FT_INT32_MAX)
    {
#ifdef _WIN32
        WSASetLastError(WSAEINVAL);
#else
        errno = EINVAL;
#endif
        return (-1);
    }
    if (active == 0)
    {
        nw_poll_filter(read_file_descriptors, read_count, POLLIN, ft_nullptr, 0);
        nw_poll_filter(write_file_descriptors, write_count, POLLOUT, ft_nullptr, 0);
        return (0);
    }

    struct pollfd stack_requests[STACK_REQUESTS];
    struct pollfd *requests = stack_requests;
    if (active > STACK_REQUESTS)
    {
        requests = static_cast<struct pollfd *>(cma_malloc(
                sizeof(struct pollfd) * static_cast<ft_size_t>(active)));
        if (!requests)
        {
#ifdef _WIN32
            WSASetLastError(WSAENOBUFS);
#else
            errno = ENOMEM;
#endif
            return (-1);
        }
    }
    int32_t count = 0;

#ifdef _WIN32
    constexpr int16_t READ_INTEREST = POLLRDNORM;
    constexpr int16_t READ_READY = POLLRDNORM | POLLERR | POLLHUP;
    constexpr int16_t WRITE_READY = POLLWRNORM | POLLERR | POLLHUP;
#else
    constexpr int16_t READ_INTEREST = POLLIN;
    constexpr int16_t READ_READY = POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL;
    constexpr int16_t WRITE_READY = POLLOUT | POLLERR | POLLHUP | POLLNVAL;
#endif
    nw_poll_collect(requests, count, read_file_descriptors, read_count, READ_INTEREST);
    nw_poll_collect(requests, count, write_file_descriptors, write_count, POLLOUT);
    count = nw_poll_merge(requests, count);
#ifdef _WIN32
    int32_t ready = WSAPoll(requests, static_cast<ULONG>(count), timeout_milliseconds);
    int32_t saved_error = 0;
    if (ready < 0)
        saved_error = WSAGetLastError();
    else if (ready > 0)
    {
        int32_t index = 0;
        while (index < count)
        {
            if ((requests[index].revents & POLLNVAL) != 0)
            {
                ready = -1;
                saved_error = WSAENOTSOCK;
                break ;
            }
            ++index;
        }
    }
#else
    const int32_t ready = poll(requests, static_cast<nfds_t>(count), timeout_milliseconds);
    const int32_t saved_error = errno;
#endif
    if (ready > 0)
    {
        nw_poll_filter(read_file_descriptors, read_count, READ_READY, requests, count);
        nw_poll_filter(write_file_descriptors, write_count, WRITE_READY, requests, count);
    }
    if (requests != stack_requests)
        cma_free(requests);
    if (ready < 0)
    {
#ifdef _WIN32
        WSASetLastError(saved_error);
#else
        errno = saved_error;
#endif
    }
    return (ready);
}

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
            int32_t *write_file_descriptors, int32_t write_count,
            int32_t timeout_milliseconds)
{
#ifdef _WIN32
    if ((read_file_descriptors && read_count > FD_SETSIZE)
        || (write_file_descriptors && write_count > FD_SETSIZE))
    {
        const int32_t ready = nw_poll_descriptor_list(read_file_descriptors, read_count,
                write_file_descriptors, write_count, timeout_milliseconds);
        if (ready >= 0)
            return (ready);
        const int32_t error = WSAGetLastError();
        if (error != WSAENOTSOCK && error != WSANOTINITIALISED && error != WSAEINVAL)
            return (ready);
        return (nw_windows_poll_pipes(read_file_descriptors, read_count,
                write_file_descriptors, write_count, timeout_milliseconds));
    }
    fd_set read_set;
    fd_set write_set;
    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    int32_t index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
            FD_SET(static_cast<SOCKET>(read_file_descriptors[index]), &read_set);
        ++index;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
            FD_SET(static_cast<SOCKET>(write_file_descriptors[index]), &write_set);
        ++index;
    }
    if (read_set.fd_count == 0 && write_set.fd_count == 0)
        return (nw_windows_poll_pipes(read_file_descriptors, read_count,
                write_file_descriptors, write_count, timeout_milliseconds));
    timeval timeout;
    timeval *timeout_pointer = ft_nullptr;
    if (timeout_milliseconds >= 0)
    {
        timeout.tv_sec = timeout_milliseconds / 1000;
        timeout.tv_usec = (timeout_milliseconds % 1000) * 1000;
        timeout_pointer = &timeout;
    }
    fd_set *read_interest = ft_nullptr;
    fd_set *write_interest = ft_nullptr;
    if (read_set.fd_count != 0)
        read_interest = &read_set;
    if (write_set.fd_count != 0)
        write_interest = &write_set;
    // Winsock rejects CRT pipes. Ordinary sockets need no descriptor type probe.
    const int32_t ready = select(0, read_interest, write_interest, ft_nullptr, timeout_pointer);
    if (ready < 0)
    {
        const int32_t error = WSAGetLastError();
        if (error == WSAENOTSOCK || error == WSANOTINITIALISED)
            return (nw_windows_poll_pipes(read_file_descriptors, read_count,
                    write_file_descriptors, write_count, timeout_milliseconds));
        return (ready);
    }
    if (ready == 0)
        return (0);
    int32_t unique_ready = 0;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        int32_t &descriptor = read_file_descriptors[index];
        if (descriptor < 0 || !FD_ISSET(static_cast<SOCKET>(descriptor), &read_set))
            descriptor = -1;
        else
            ++unique_ready;
        ++index;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        int32_t &descriptor = write_file_descriptors[index];
        if (descriptor < 0 || !FD_ISSET(static_cast<SOCKET>(descriptor), &write_set))
            descriptor = -1;
        else if (!FD_ISSET(static_cast<SOCKET>(descriptor), &read_set))
            ++unique_ready;
        ++index;
    }
    return (unique_ready);

#else
    return (nw_poll_descriptor_list(read_file_descriptors, read_count,
            write_file_descriptors, write_count, timeout_milliseconds));
#endif
}
