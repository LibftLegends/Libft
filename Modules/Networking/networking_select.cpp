#include "networking.hpp"
#include "../Errno/errno.hpp"
#include "../Compatebility/compatebility_internal.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <io.h>
#else
# include <sys/select.h>
# include <poll.h>
# include <unistd.h>
# include <cerrno>
#endif

#ifdef _WIN32
static ft_bool nw_windows_descriptor_is_pipe(int32_t file_descriptor)
{
    HANDLE file_handle;
    DWORD file_type;

    file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(file_descriptor));
    if (file_handle == INVALID_HANDLE_VALUE)
        return (FT_FALSE);
    file_type = GetFileType(file_handle);
    if (file_type == FILE_TYPE_PIPE)
        return (FT_TRUE);
    return (FT_FALSE);
}
#endif

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
            int32_t *write_file_descriptors, int32_t write_count,
            int32_t timeout_milliseconds)
{
#ifdef _WIN32
    fd_set read_set;
    fd_set write_set;
    int32_t index;
    int32_t max_descriptor;
    int32_t ready_descriptors;
    int32_t total_ready;
    int32_t unique_ready;
    timeval timeout;
    timeval *timeout_pointer;
    ft_bool use_pipe_fallback;
    HANDLE file_handle;
    DWORD wait_result;

    use_pipe_fallback = FT_TRUE;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
        {
            if (nw_windows_descriptor_is_pipe(read_file_descriptors[index]) == FT_FALSE)
            {
                use_pipe_fallback = FT_FALSE;
                break ;
            }
        }
        index++;
    }
    index = 0;
    while (use_pipe_fallback == FT_TRUE && write_file_descriptors
        && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
        {
            if (nw_windows_descriptor_is_pipe(write_file_descriptors[index]) == FT_FALSE)
            {
                use_pipe_fallback = FT_FALSE;
                break ;
            }
        }
        index++;
    }
    if (use_pipe_fallback == FT_TRUE)
    {
        int32_t pipe_ready_count;
        int32_t *read_ready_flags;
        int32_t *write_ready_flags;
        DWORD bytes_available;

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
        pipe_ready_count = 0;
        index = 0;
        while (read_file_descriptors && index < read_count)
        {
            if (read_file_descriptors[index] >= 0)
            {
                file_handle = reinterpret_cast<HANDLE>(_get_osfhandle(read_file_descriptors[index]));
                if (file_handle != INVALID_HANDLE_VALUE)
                {
                    wait_result = WaitForSingleObject(file_handle, 0);
                    bytes_available = 0;
                    if (PeekNamedPipe(file_handle, NULL, 0, NULL, &bytes_available, NULL) != 0
                        && bytes_available > 0)
                    {
                        if (read_ready_flags)
                            read_ready_flags[index] = 1;
                        pipe_ready_count++;
                    }
                    else if (wait_result == WAIT_OBJECT_0)
                    {
                        if (read_ready_flags)
                            read_ready_flags[index] = 1;
                        pipe_ready_count++;
                    }
                    else if (PeekNamedPipe(file_handle, NULL, 0, NULL, &bytes_available, NULL) == 0
                        && GetLastError() == ERROR_BROKEN_PIPE)
                    {
                        if (read_ready_flags)
                            read_ready_flags[index] = 1;
                        pipe_ready_count++;
                    }
                }
            }
            index++;
        }
        index = 0;
        while (write_file_descriptors && index < write_count)
        {
            if (write_file_descriptors[index] >= 0)
            {
                if (write_ready_flags)
                    write_ready_flags[index] = 1;
                pipe_ready_count++;
            }
            index++;
        }
        index = 0;
        while (read_file_descriptors && index < read_count)
        {
            if (read_file_descriptors[index] < 0
                || read_ready_flags == ft_nullptr
                || read_ready_flags[index] == 0)
                read_file_descriptors[index] = -1;
            index++;
        }
        index = 0;
        while (write_file_descriptors && index < write_count)
        {
            if (write_file_descriptors[index] < 0
                || write_ready_flags == ft_nullptr
                || write_ready_flags[index] == 0)
                write_file_descriptors[index] = -1;
            index++;
        }
        if (read_ready_flags)
            cma_free(read_ready_flags);
        if (write_ready_flags)
            cma_free(write_ready_flags);
        if (timeout_milliseconds < 0 && pipe_ready_count == 0)
        {
            while (pipe_ready_count == 0)
                cmp_thread_sleep(1);
        }
        return (pipe_ready_count);
    }

    FD_ZERO(&read_set);
    FD_ZERO(&write_set);
    index = 0;
    max_descriptor = -1;
    timeout_pointer = NULL;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
        {
            FD_SET(static_cast<SOCKET>(read_file_descriptors[index]), &read_set);
            if (read_file_descriptors[index] > max_descriptor)
                max_descriptor = read_file_descriptors[index];
        }
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
        {
            FD_SET(static_cast<SOCKET>(write_file_descriptors[index]), &write_set);
            if (write_file_descriptors[index] > max_descriptor)
                max_descriptor = write_file_descriptors[index];
        }
        index++;
    }
    if (timeout_milliseconds >= 0)
    {
        timeout.tv_sec = timeout_milliseconds / 1000;
        timeout.tv_usec = (timeout_milliseconds % 1000) * 1000;
        timeout_pointer = &timeout;
    }
    ready_descriptors = select(max_descriptor + 1, &read_set, &write_set, NULL, timeout_pointer);
    if (ready_descriptors <= 0)
    {
        int32_t select_error;

        select_error = WSAGetLastError();
        if (ready_descriptors == 0)
            (void)(FT_ERR_SUCCESS);
        else
            (void)(cmp_map_system_error_to_ft(select_error));
        return (ready_descriptors);
    }
    index = 0;
    total_ready = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] < 0 ||
            !FD_ISSET(static_cast<SOCKET>(read_file_descriptors[index]), &read_set))
            read_file_descriptors[index] = -1;
        else
            total_ready++;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] < 0 ||
            !FD_ISSET(static_cast<SOCKET>(write_file_descriptors[index]), &write_set))
            write_file_descriptors[index] = -1;
        else
            total_ready++;
        index++;
    }
    (void)(FT_ERR_SUCCESS);
    unique_ready = 0;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
            unique_ready++;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        int32_t search_index;
        ft_bool already_counted;

        already_counted = FT_FALSE;
        search_index = 0;
        while (read_file_descriptors && search_index < read_count)
        {
            if (read_file_descriptors[search_index]
                == write_file_descriptors[index])
            {
                already_counted = FT_TRUE;
                break ;
            }
            search_index++;
        }
        if (write_file_descriptors[index] >= 0
            && already_counted == FT_FALSE)
            unique_ready++;
        index++;
    }
    return (unique_ready);
#else
    struct pollfd *poll_descriptors;
    int32_t *poll_index_to_read_index;
    int32_t *poll_index_to_write_index;
    int32_t *read_ready_flags;
    int32_t *write_ready_flags;
    int32_t index;
    int32_t poll_count;
    int32_t ready_descriptors;
    int32_t total_ready;

    poll_descriptors = ft_nullptr;
    poll_index_to_read_index = ft_nullptr;
    poll_index_to_write_index = ft_nullptr;
    read_ready_flags = ft_nullptr;
    write_ready_flags = ft_nullptr;
    poll_count = 0;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
            poll_count++;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
            poll_count++;
        index++;
    }
    if (poll_count == 0)
    {
        index = 0;
        while (read_file_descriptors && index < read_count)
        {
            read_file_descriptors[index] = -1;
            index++;
        }
        index = 0;
        while (write_file_descriptors && index < write_count)
        {
            write_file_descriptors[index] = -1;
            index++;
        }
        return (0);
    }
    poll_descriptors = static_cast<struct pollfd *>(cma_malloc(sizeof(struct pollfd)
                * static_cast<ft_size_t>(poll_count)));
    if (poll_descriptors == ft_nullptr)
        return (-1);
    poll_index_to_read_index = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                * static_cast<ft_size_t>(poll_count)));
    if (poll_index_to_read_index == ft_nullptr)
    {
        cma_free(poll_descriptors);
        return (-1);
    }
    poll_index_to_write_index = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                * static_cast<ft_size_t>(poll_count)));
    if (poll_index_to_write_index == ft_nullptr)
    {
        cma_free(poll_descriptors);
        cma_free(poll_index_to_read_index);
        return (-1);
    }
    index = 0;
    while (index < poll_count)
    {
        poll_descriptors[index].fd = -1;
        poll_descriptors[index].events = 0;
        poll_descriptors[index].revents = 0;
        poll_index_to_read_index[index] = -1;
        poll_index_to_write_index[index] = -1;
        index++;
    }
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        int32_t file_descriptor;
        int32_t poll_index;

        file_descriptor = read_file_descriptors[index];
        if (file_descriptor >= 0)
        {
            poll_index = 0;
            while (poll_index < poll_count)
            {
                if (poll_descriptors[poll_index].fd == file_descriptor)
                    break ;
                poll_index++;
            }
            if (poll_index == poll_count)
            {
                poll_index = 0;
                while (poll_index < poll_count && poll_descriptors[poll_index].fd != -1)
                    poll_index++;
                poll_descriptors[poll_index].fd = file_descriptor;
            }
            poll_descriptors[poll_index].events |= POLLIN;
            poll_index_to_read_index[poll_index] = index;
        }
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        int32_t file_descriptor;
        int32_t poll_index;

        file_descriptor = write_file_descriptors[index];
        if (file_descriptor >= 0)
        {
            poll_index = 0;
            while (poll_index < poll_count)
            {
                if (poll_descriptors[poll_index].fd == file_descriptor)
                    break ;
                poll_index++;
            }
            if (poll_index == poll_count)
            {
                poll_index = 0;
                while (poll_index < poll_count && poll_descriptors[poll_index].fd != -1)
                    poll_index++;
                poll_descriptors[poll_index].fd = file_descriptor;
            }
            poll_descriptors[poll_index].events |= POLLOUT;
            poll_index_to_write_index[poll_index] = index;
        }
        index++;
    }
    ready_descriptors = poll(poll_descriptors, static_cast<nfds_t>(poll_count),
            timeout_milliseconds);
    if (ready_descriptors <= 0)
    {
        cma_free(poll_descriptors);
        cma_free(poll_index_to_read_index);
        cma_free(poll_index_to_write_index);
        return (ready_descriptors);
    }
    read_ready_flags = ft_nullptr;
    write_ready_flags = ft_nullptr;
    if (read_count > 0 && read_file_descriptors != ft_nullptr)
    {
        read_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                    * static_cast<ft_size_t>(read_count)));
        if (read_ready_flags == ft_nullptr)
        {
            cma_free(poll_descriptors);
            cma_free(poll_index_to_read_index);
            cma_free(poll_index_to_write_index);
            return (-1);
        }
        index = 0;
        while (index < read_count)
        {
            read_ready_flags[index] = 0;
            index++;
        }
    }
    if (write_count > 0 && write_file_descriptors != ft_nullptr)
    {
        write_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
                    * static_cast<ft_size_t>(write_count)));
        if (write_ready_flags == ft_nullptr)
        {
            cma_free(poll_descriptors);
            cma_free(poll_index_to_read_index);
            if (read_ready_flags)
                cma_free(read_ready_flags);
            cma_free(poll_index_to_write_index);
            return (-1);
        }
        index = 0;
        while (index < write_count)
        {
            write_ready_flags[index] = 0;
            index++;
        }
    }
    index = 0;
    while (index < poll_count)
    {
        short revents;
        int32_t read_index;
        int32_t write_index;

        revents = poll_descriptors[index].revents;
        if (revents != 0)
        {
            read_index = poll_index_to_read_index[index];
            if (read_index >= 0 && read_ready_flags != ft_nullptr
                && (revents & (POLLIN | POLLPRI | POLLERR | POLLHUP | POLLNVAL)) != 0)
                read_ready_flags[read_index] = 1;
            write_index = poll_index_to_write_index[index];
            if (write_index >= 0 && write_ready_flags != ft_nullptr
                && (revents & (POLLOUT | POLLERR | POLLHUP | POLLNVAL)) != 0)
                write_ready_flags[write_index] = 1;
        }
        index++;
    }
    total_ready = 0;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] < 0
            || read_ready_flags == ft_nullptr
            || read_ready_flags[index] == 0)
            read_file_descriptors[index] = -1;
        else
            total_ready++;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] < 0
            || write_ready_flags == ft_nullptr
            || write_ready_flags[index] == 0)
            write_file_descriptors[index] = -1;
        else
            total_ready++;
        index++;
    }
    cma_free(poll_descriptors);
    cma_free(poll_index_to_read_index);
    cma_free(poll_index_to_write_index);
    if (read_ready_flags)
        cma_free(read_ready_flags);
    if (write_ready_flags)
        cma_free(write_ready_flags);
    /* poll() reports each unique descriptor, even with both interests set. */
    return (ready_descriptors);
#endif
}
