#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"
#include <algorithm>
#include <cerrno>
#include <unistd.h>
#include <sys/event.h>
#include <sys/time.h>

static int32_t nw_kqueue_count_active(const int32_t *descriptors, int32_t count)
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

static void nw_kqueue_collect(struct kevent *events, int32_t &count,
    const int32_t *descriptors, int32_t descriptor_count, int32_t filter)
{
    int32_t index = 0;
    while (descriptors && index < descriptor_count)
    {
        if (descriptors[index] >= 0)
        {
            events[count] = {};
            EV_SET(&events[count], descriptors[index], filter, EV_ADD, 0, 0, nullptr);
            ++count;
        }
        ++index;
    }
    return ;
}

static ft_bool nw_kqueue_event_order(const struct kevent &left, const struct kevent &right)
{
    if (left.ident != right.ident)
        return (left.ident < right.ident);
    return (left.filter < right.filter);
}

static int32_t nw_kqueue_ready_count(struct kevent *events, int32_t event_count)
{
    int32_t index = 0;
    while (index < event_count)
    {
        if ((events[index].flags & EV_ERROR) != 0)
        {
            errno = static_cast<int32_t>(events[index].data);
            if (errno == 0)
                errno = EIO;
            return (-1);
        }
        ++index;
    }
    std::sort(events, events + event_count, nw_kqueue_event_order);
    int32_t unique_count = 1;
    index = 1;
    while (index < event_count)
    {
        if (events[index - 1].ident != events[index].ident)
            ++unique_count;
        ++index;
    }
    return (unique_count);
}

static void nw_kqueue_filter_ready(int32_t *descriptors, int32_t count, int32_t filter,
    const struct kevent *events, int32_t event_count)
{
    int32_t index = 0;
    while (descriptors && index < count)
    {
        int32_t &descriptor = descriptors[index++];
        if (descriptor < 0)
            continue ;
        struct kevent target{};
        EV_SET(&target, descriptor, filter, 0, 0, 0, nullptr);
        const struct kevent *ready = std::lower_bound(events, events + event_count,
            target, nw_kqueue_event_order);
        if (ready == events + event_count || ready->ident != target.ident
            || ready->filter != target.filter)
            descriptor = -1;
    }
    return ;
}

static int32_t nw_kqueue_wait(struct kevent *events, int32_t count, int32_t timeout_milliseconds)
{
    const int32_t queue = kqueue();
    if (queue < 0)
        return (-1);
    timespec timeout;
    timespec *timeout_pointer = nullptr;
    if (timeout_milliseconds >= 0)
    {
        timeout.tv_sec = timeout_milliseconds / 1000;
        timeout.tv_nsec = (timeout_milliseconds % 1000) * 1000000;
        timeout_pointer = &timeout;
    }
    const int32_t ready = kevent(queue, events, count, events, count, timeout_pointer);
    const int32_t saved_error = errno;
    close(queue);
    if (ready < 0)
        errno = saved_error;
    return (ready);
}

int32_t nw_poll(int32_t *read_file_descriptors, int32_t read_count,
    int32_t *write_file_descriptors, int32_t write_count, int32_t timeout_milliseconds)
{
    constexpr int32_t STACK_EVENTS = 32;
    struct kevent stack_events[STACK_EVENTS];
    const int64_t active = static_cast<int64_t>(
        nw_kqueue_count_active(read_file_descriptors, read_count))
        + nw_kqueue_count_active(write_file_descriptors, write_count);
    if (active > FT_INT32_MAX)
    {
        errno = EINVAL;
        return (-1);
    }
    if (active == 0)
        return (0);

    struct kevent *events = stack_events;
    if (active > STACK_EVENTS)
    {
        events = static_cast<struct kevent *>(cma_malloc(
                sizeof(struct kevent) * static_cast<ft_size_t>(active)));
        if (!events)
        {
            errno = ENOMEM;
            return (-1);
        }
    }
    int32_t count = 0;
    nw_kqueue_collect(events, count, read_file_descriptors, read_count, EVFILT_READ);
    nw_kqueue_collect(events, count, write_file_descriptors, write_count, EVFILT_WRITE);
    const int32_t ready = nw_kqueue_wait(events, count, timeout_milliseconds);
    int32_t result = ready;
    if (ready > 0)
    {
        result = nw_kqueue_ready_count(events, ready);
        if (result > 0)
        {
            nw_kqueue_filter_ready(read_file_descriptors, read_count, EVFILT_READ, events, ready);
            nw_kqueue_filter_ready(write_file_descriptors, write_count, EVFILT_WRITE, events, ready);
        }
    }
    const int32_t saved_error = errno;
    if (events != stack_events)
        cma_free(events);
    if (result < 0)
        errno = saved_error;
    return (result);
}
