#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"
#include <algorithm>
#include <cerrno>
#include <unistd.h>
#include <sys/epoll.h>

static void nw_epoll_collect(epoll_event *events, int32_t &count,
    const int32_t *descriptors, int32_t descriptor_count, uint32_t interest)
{
    int32_t index = 0;
    while (descriptors && index < descriptor_count)
    {
        if (descriptors[index] >= 0)
        {
            events[count].events = interest;
            events[count].data = {};
            events[count].data.fd = descriptors[index];
            ++count;
        }
        ++index;
    }
    return ;
}

static ft_bool nw_epoll_by_descriptor(const epoll_event &left, const epoll_event &right)
{
    return (left.data.fd < right.data.fd);
}

static int32_t nw_epoll_wait(epoll_event *events, int32_t count, int32_t timeout)
{
    std::sort(events, events + count, nw_epoll_by_descriptor);
    const int32_t descriptor = epoll_create1(EPOLL_CLOEXEC);
    if (descriptor < 0)
        return (-1);

    int32_t index = 0;
    int32_t registered = 0;
    int32_t result = 0;
    while (index < count)
    {
        epoll_event interest = events[index++];
        while (index < count && events[index].data.fd == interest.data.fd)
            interest.events |= events[index++].events;
        if (epoll_ctl(descriptor, EPOLL_CTL_ADD, interest.data.fd, &interest) < 0)
        {
            result = -1;
            break ;
        }
        ++registered;
    }
    if (result == 0)
        result = epoll_wait(descriptor, events, registered, timeout);
    const int32_t saved_error = errno;
    close(descriptor);
    if (result < 0)
        errno = saved_error;
    return (result);
}

static void nw_epoll_filter_ready(int32_t *descriptors, int32_t count, uint32_t interest,
    const epoll_event *events, int32_t ready_count)
{
    int32_t index = 0;
    while (descriptors && index < count)
    {
        int32_t &descriptor = descriptors[index];
        if (ready_count == 0 || descriptor < 0)
        {
            descriptor = -1;
            ++index;
            continue ;
        }
        epoll_event target{};
        target.data.fd = descriptor;
        const epoll_event *ready = std::lower_bound(events, events + ready_count,
            target, nw_epoll_by_descriptor);
        if (ready == events + ready_count || ready->data.fd != descriptor
            || (ready->events & (interest | EPOLLERR | EPOLLHUP)) == 0)
            descriptor = -1;
        ++index;
    }
    return ;
}

static int32_t nw_epoll_count_active(const int32_t *descriptors, int32_t count)
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

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
    int32_t *write_file_descriptors, int32_t write_count, int32_t timeout_milliseconds)
{
    constexpr int32_t STACK_EVENTS = 32;
    epoll_event stack_events[STACK_EVENTS];
    const int64_t active = static_cast<int64_t>(
        nw_epoll_count_active(read_file_descriptors, read_count))
        + nw_epoll_count_active(write_file_descriptors, write_count);
    if (active > FT_INT32_MAX)
    {
        errno = EINVAL;
        return (-1);
    }
    if (active == 0)
    {
        nw_epoll_filter_ready(read_file_descriptors, read_count, EPOLLIN, ft_nullptr, 0);
        nw_epoll_filter_ready(write_file_descriptors, write_count, EPOLLOUT, ft_nullptr, 0);
        return (0);
    }

    epoll_event *events = stack_events;
    if (active > STACK_EVENTS)
    {
        events = static_cast<epoll_event *>(cma_malloc(
                sizeof(epoll_event) * static_cast<ft_size_t>(active)));
        if (!events)
        {
            errno = ENOMEM;
            return (-1);
        }
    }
    int32_t count = 0;
    nw_epoll_collect(events, count, read_file_descriptors, read_count, EPOLLIN);
    nw_epoll_collect(events, count, write_file_descriptors, write_count, EPOLLOUT);
    const int32_t ready = nw_epoll_wait(events, count, timeout_milliseconds);
    const int32_t saved_error = errno;
    if (ready > 0)
    {
        std::sort(events, events + ready, nw_epoll_by_descriptor);
        nw_epoll_filter_ready(read_file_descriptors, read_count, EPOLLIN, events, ready);
        nw_epoll_filter_ready(write_file_descriptors, write_count, EPOLLOUT, events, ready);
    }
    if (events != stack_events)
        cma_free(events);
    if (ready < 0)
        errno = saved_error;
    return (ready);
}
