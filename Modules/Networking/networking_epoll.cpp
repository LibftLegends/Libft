#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include <unistd.h>
#include <sys/epoll.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool descriptor_is_present(const int32_t *descriptors,
    int32_t descriptor_count, int32_t file_descriptor)
{
    int32_t index;

    index = 0;
    while (descriptors != ft_nullptr && index < descriptor_count)
    {
        if (descriptors[index] == file_descriptor)
            return (FT_TRUE);
        index += 1;
    }
    return (FT_FALSE);
}

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
            int32_t *write_file_descriptors, int32_t write_count,
            int32_t timeout_milliseconds)
{
    int32_t epoll_descriptor;
    epoll_event event;
    epoll_event *events;
    int32_t index;
    int32_t maximum_events;
    int32_t valid_read_count;
    int32_t valid_write_count;
    int32_t ready_descriptors;
    int32_t ready_index;
    int32_t file_descriptor;
    int32_t search_index;
    int32_t *read_ready_flags;
    int32_t *write_ready_flags;

    epoll_descriptor = epoll_create1(0);
    if (epoll_descriptor == -1)
    {
        return (-1);
    }
    index = 0;
    valid_read_count = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
        {
            if (descriptor_is_present(read_file_descriptors, index,
                    read_file_descriptors[index]) == FT_TRUE)
            {
                index++;
                continue ;
            }
            event.events = EPOLLIN;
            event.data.fd = read_file_descriptors[index];
            if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD, read_file_descriptors[index], &event) == -1)
            {
                close(epoll_descriptor);
                return (-1);
            }
            valid_read_count++;
        }
        index++;
    }
    index = 0;
    valid_write_count = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
        {
            if (descriptor_is_present(write_file_descriptors, index,
                    write_file_descriptors[index]) == FT_TRUE)
            {
                index++;
                continue ;
            }
            event.events = EPOLLOUT;
            event.data.fd = write_file_descriptors[index];
            if (descriptor_is_present(read_file_descriptors, read_count,
                    write_file_descriptors[index]) == FT_TRUE)
            {
                event.events |= EPOLLIN;
                if (epoll_ctl(epoll_descriptor, EPOLL_CTL_MOD,
                        write_file_descriptors[index], &event) == -1)
                {
                    close(epoll_descriptor);
                    return (-1);
                }
            }
            else if (epoll_ctl(epoll_descriptor, EPOLL_CTL_ADD,
                    write_file_descriptors[index], &event) == -1)
            {
                close(epoll_descriptor);
                return (-1);
            }
            valid_write_count++;
        }
        index++;
    }
    maximum_events = valid_read_count + valid_write_count;
    if (maximum_events == 0)
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
        close(epoll_descriptor);
        return (0);
    }
    events = static_cast<epoll_event *>(cma_malloc(sizeof(epoll_event) * maximum_events));
    if (!events)
    {
        close(epoll_descriptor);
        return (-1);
    }
    read_ready_flags = ft_nullptr;
    write_ready_flags = ft_nullptr;
    if (read_file_descriptors && read_count > 0)
    {
        read_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t) * read_count));
        if (!read_ready_flags)
        {
            cma_free(events);
            close(epoll_descriptor);
            return (-1);
        }
        index = 0;
        while (index < read_count)
        {
            read_ready_flags[index] = 0;
            index++;
        }
    }
    if (write_file_descriptors && write_count > 0)
    {
        write_ready_flags = static_cast<int32_t *>(cma_malloc(sizeof(int32_t) * write_count));
        if (!write_ready_flags)
        {
            if (read_ready_flags)
                cma_free(read_ready_flags);
            cma_free(events);
            close(epoll_descriptor);
            return (-1);
        }
        index = 0;
        while (index < write_count)
        {
            write_ready_flags[index] = 0;
            index++;
        }
    }
    ready_descriptors = epoll_wait(epoll_descriptor, events, maximum_events, timeout_milliseconds);
    if (ready_descriptors <= 0)
    {
        if (read_ready_flags)
            cma_free(read_ready_flags);
        if (write_ready_flags)
            cma_free(write_ready_flags);
        cma_free(events);
        close(epoll_descriptor);
        return (ready_descriptors);
    }
    ready_index = 0;
    while (ready_index < ready_descriptors)
    {
        file_descriptor = events[ready_index].data.fd;
        search_index = 0;
        while (read_file_descriptors && search_index < read_count)
        {
            if (read_file_descriptors[search_index] == file_descriptor)
                break ;
            search_index++;
        }
        if (read_file_descriptors && search_index < read_count)
        {
            read_file_descriptors[search_index] = file_descriptor;
            if (read_ready_flags)
                read_ready_flags[search_index] = 1;
        }
        search_index = 0;
        while (write_file_descriptors && search_index < write_count)
        {
            if (write_file_descriptors[search_index] == file_descriptor)
                break ;
            search_index++;
        }
        if (write_file_descriptors && search_index < write_count)
        {
            write_file_descriptors[search_index] = file_descriptor;
            if (write_ready_flags)
                write_ready_flags[search_index] = 1;
        }
        ready_index++;
    }
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (!read_ready_flags || read_ready_flags[index] == 0)
            read_file_descriptors[index] = -1;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (!write_ready_flags || write_ready_flags[index] == 0)
            write_file_descriptors[index] = -1;
        index++;
    }
    cma_free(events);
    if (read_ready_flags)
        cma_free(read_ready_flags);
    if (write_ready_flags)
        cma_free(write_ready_flags);
    close(epoll_descriptor);
    return (ready_descriptors);
}
