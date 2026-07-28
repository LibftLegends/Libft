#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>
#include <cerrno>

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
            int32_t *write_file_descriptors, int32_t write_count,
            int32_t timeout_milliseconds)
{
    int32_t kqueue_descriptor;
    struct kevent change_event;
    struct kevent *event_list;
    int32_t index;
    int32_t maximum_events;
    int32_t ready_descriptors;
    int32_t ready_index;
    int32_t descriptor;
    int32_t search_index;
    timespec timeout;
    timespec *timeout_pointer;

    kqueue_descriptor = kqueue();
    if (kqueue_descriptor == -1)
    {
        (void)(cmp_map_system_error_to_ft(errno));
        return (-1);
    }
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] < 0)
        {
            index++;
            continue ;
        }
        EV_SET(&change_event, read_file_descriptors[index], EVFILT_READ, EV_ADD, 0, 0, NULL);
        if (kevent(kqueue_descriptor, &change_event, 1, NULL, 0, NULL) == -1)
        {
            int32_t last_error;

            last_error = errno;
            close(kqueue_descriptor);
            (void)(cmp_map_system_error_to_ft(last_error));
            return (-1);
        }
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] < 0)
        {
            index++;
            continue ;
        }
        EV_SET(&change_event, write_file_descriptors[index], EVFILT_WRITE, EV_ADD, 0, 0, NULL);
        if (kevent(kqueue_descriptor, &change_event, 1, NULL, 0, NULL) == -1)
        {
            int32_t last_error;

            last_error = errno;
            close(kqueue_descriptor);
            (void)(cmp_map_system_error_to_ft(last_error));
            return (-1);
        }
        index++;
    }
    maximum_events = 0;
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
            maximum_events++;
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
            maximum_events++;
        index++;
    }
    if (maximum_events == 0)
    {
        close(kqueue_descriptor);
        (void)(FT_ERR_SUCCESS);
        return (0);
    }
    event_list = static_cast<struct kevent *>(cma_malloc(sizeof(struct kevent) * maximum_events));
    if (!event_list)
    {
        close(kqueue_descriptor);
        (void)(FT_ERR_NO_MEMORY);
        return (-1);
    }
    timeout_pointer = NULL;
    if (timeout_milliseconds >= 0)
    {
        timeout.tv_sec = timeout_milliseconds / 1000;
        timeout.tv_nsec = (timeout_milliseconds % 1000) * 1000000;
        timeout_pointer = &timeout;
    }
    ready_descriptors = kevent(kqueue_descriptor, NULL, 0, event_list, maximum_events, timeout_pointer);
    if (ready_descriptors <= 0)
    {
        int32_t wait_error;

        wait_error = errno;
        cma_free(event_list);
        close(kqueue_descriptor);
        if (ready_descriptors == 0)
            (void)(FT_ERR_SUCCESS);
        else
            (void)(cmp_map_system_error_to_ft(wait_error));
        return (ready_descriptors);
    }
    ready_index = 0;
    while (ready_index < ready_descriptors)
    {
        descriptor = static_cast<int32_t>(event_list[ready_index].ident);
        search_index = 0;
        while (read_file_descriptors && search_index < read_count)
        {
            if (read_file_descriptors[search_index] == descriptor)
                break ;
            search_index++;
        }
        if (read_file_descriptors && search_index < read_count)
            read_file_descriptors[search_index] = descriptor;
        search_index = 0;
        while (write_file_descriptors && search_index < write_count)
        {
            if (write_file_descriptors[search_index] == descriptor)
                break ;
            search_index++;
        }
        if (write_file_descriptors && search_index < write_count)
            write_file_descriptors[search_index] = descriptor;
        ready_index++;
    }
    index = 0;
    while (read_file_descriptors && index < read_count)
    {
        if (read_file_descriptors[index] >= 0)
        {
            ready_index = 0;
            while (ready_index < ready_descriptors)
            {
                if (static_cast<int32_t>(event_list[ready_index].ident)
                    == read_file_descriptors[index]
                    && event_list[ready_index].filter == EVFILT_READ)
                    break ;
                ready_index++;
            }
            if (ready_index == ready_descriptors)
                read_file_descriptors[index] = -1;
        }
        index++;
    }
    index = 0;
    while (write_file_descriptors && index < write_count)
    {
        if (write_file_descriptors[index] >= 0)
        {
            ready_index = 0;
            while (ready_index < ready_descriptors)
            {
                if (static_cast<int32_t>(event_list[ready_index].ident)
                    == write_file_descriptors[index]
                    && event_list[ready_index].filter == EVFILT_WRITE)
                    break ;
                ready_index++;
            }
            if (ready_index == ready_descriptors)
                write_file_descriptors[index] = -1;
        }
        index++;
    }
    cma_free(event_list);
    close(kqueue_descriptor);
    (void)(FT_ERR_SUCCESS);
    return (ready_descriptors);
}
