#include <cerrno>
#include <chrono>
#include <cstdlib>

#include "networking_event_loop_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"

#ifdef NETWORKING_USE_EPOLL
# include <sys/epoll.h>
# include <sys/eventfd.h>
# include <unistd.h>
#elif defined(NETWORKING_USE_KQUEUE)
# include <fcntl.h>
# include <sys/event.h>
# include <sys/time.h>
# include <unistd.h>
#elif !defined(_WIN32)
# include <fcntl.h>
# include <unistd.h>
#endif

static void event_loop_close_descriptor(int32_t file_descriptor)
{
#ifdef _WIN32
    (void)nw_close(file_descriptor);
#else
    (void)close(file_descriptor);
#endif
    return ;
}

static int32_t find_registration(const event_loop *loop, int32_t file_descriptor)
{
    uint32_t index;

    index = 0U;
    while (index < loop->registration_count)
    {
        if (loop->registrations[index].file_descriptor == file_descriptor)
            return (static_cast<int32_t>(index));
        index += 1U;
    }
    return (-1);
}

static int32_t event_loop_reserve_registrations(event_loop *loop, uint32_t required)
{
    if (required > loop->registration_capacity)
    {
        uint32_t capacity = loop->registration_capacity;
        if (capacity == 0)
            capacity = 4;
        while (capacity < required)
        {
            if (capacity > FT_INT32_MAX / 2)
                capacity = FT_INT32_MAX;
            else
                capacity *= 2;
        }
        auto *registrations = static_cast<event_loop_registration *>(cma_realloc(
                loop->registrations, sizeof(event_loop_registration) * capacity));
        if (!registrations)
            return (FT_ERR_NO_MEMORY);
        loop->registrations = registrations;
        auto *read_descriptors = static_cast<int32_t *>(cma_realloc(
                loop->read_file_descriptors, sizeof(int32_t) * capacity));
        if (!read_descriptors)
            return (FT_ERR_NO_MEMORY);
        loop->read_file_descriptors = read_descriptors;
        auto *write_descriptors = static_cast<int32_t *>(cma_realloc(
                loop->write_file_descriptors, sizeof(int32_t) * capacity));
        if (!write_descriptors)
            return (FT_ERR_NO_MEMORY);
        loop->write_file_descriptors = write_descriptors;
        auto *ready_events = static_cast<event_loop_ready_event *>(cma_realloc(
                loop->ready_events, sizeof(event_loop_ready_event) * capacity));
        if (!ready_events)
            return (FT_ERR_NO_MEMORY);
        loop->ready_events = ready_events;
        loop->ready_capacity = capacity;
        loop->registration_capacity = capacity;
    }
#if defined(NETWORKING_USE_EPOLL) || defined(NETWORKING_USE_KQUEUE)
    if (!loop->wait_active && required > loop->backend_event_capacity)
    {
# ifdef NETWORKING_USE_EPOLL
        constexpr ft_size_t EVENT_SIZE = sizeof(struct epoll_event);
# else
        constexpr ft_size_t EVENT_SIZE = sizeof(struct kevent);
# endif
        void *events = cma_realloc(loop->backend_events, EVENT_SIZE * loop->registration_capacity);
        if (!events)
            return (FT_ERR_NO_MEMORY);
        loop->backend_events = events;
        loop->backend_event_capacity = loop->registration_capacity;
    }
#endif
    return (FT_ERR_SUCCESS);
}

static void event_loop_insert_interest(const event_loop *loop, int32_t *descriptors,
    int32_t &count, uint32_t registration_index, uint32_t mask, int32_t file_descriptor)
{
    int32_t index = count;
    if (registration_index != loop->registration_count)
    {
        index = 0;
        uint32_t previous = 0;
        while (previous < registration_index)
        {
            if ((loop->registrations[previous].interest_mask & mask) != 0U)
                ++index;
            ++previous;
        }
        if (index < count)
            ft_memmove(descriptors + index + 1, descriptors + index,
                sizeof(int32_t) * static_cast<ft_size_t>(count - index));
    }
    descriptors[index] = file_descriptor;
    ++count;
    return ;
}

static void event_loop_erase_interest(int32_t *descriptors, int32_t &count,
    int32_t file_descriptor)
{
    int32_t index = 0;
    while (index < count && descriptors[index] != file_descriptor)
        ++index;
    if (index == count)
        return ;
    --count;
    if (index < count)
        ft_memmove(descriptors + index, descriptors + index + 1,
            sizeof(int32_t) * static_cast<ft_size_t>(count - index));
    return ;
}

static int32_t make_mutex(pt_mutex **mutex_pointer)
{
    void *memory;

    memory = std::malloc(sizeof(pt_mutex));
    if (memory == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    *mutex_pointer = new(memory) pt_mutex();
    if ((*mutex_pointer)->initialize() != FT_ERR_SUCCESS)
    {
        (*mutex_pointer)->~pt_mutex();
        std::free(memory);
        *mutex_pointer = ft_nullptr;
        return (FT_ERR_NO_MEMORY);
    }
    return (FT_ERR_SUCCESS);
}

static void free_mutex(pt_mutex **mutex_pointer)
{
    if (mutex_pointer == ft_nullptr || *mutex_pointer == ft_nullptr)
        return ;
    (void)(*mutex_pointer)->destroy();
    (*mutex_pointer)->~pt_mutex();
    std::free(*mutex_pointer);
    *mutex_pointer = ft_nullptr;
    return ;
}

static void release_event_loop_wait(event_loop *loop)
{
    if (loop == ft_nullptr)
        return ;
    if (loop->mutex != ft_nullptr
        && pt_mutex_lock_if_not_null(loop->mutex) == FT_ERR_SUCCESS)
    {
        loop->wait_active = FT_FALSE;
        (void)pt_mutex_unlock_if_not_null(loop->mutex);
    }
    if (loop->wait_mutex != ft_nullptr)
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
    return ;
}

#ifndef _WIN32
static int32_t event_loop_remaining_timeout(
    const std::chrono::steady_clock::time_point &deadline)
{
    int64_t remaining_milliseconds;

    remaining_milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
        deadline - std::chrono::steady_clock::now()).count();
    if (remaining_milliseconds <= 0)
        return (0);
    if (remaining_milliseconds > static_cast<int64_t>(INT32_MAX))
        return (INT32_MAX);
    return (static_cast<int32_t>(remaining_milliseconds));
}
#endif

#ifdef NETWORKING_USE_EPOLL
static uint32_t epoll_interest(uint32_t interest_mask)
{
    uint32_t events;

    events = 0U;
    if ((interest_mask & EVENT_LOOP_INTEREST_READ) != 0U)
        events |= EPOLLIN;
    if ((interest_mask & EVENT_LOOP_INTEREST_WRITE) != 0U)
        events |= EPOLLOUT;
# ifdef EPOLLRDHUP
    events |= EPOLLRDHUP;
# endif
    return (events);
}
#elif defined(NETWORKING_USE_KQUEUE)
static int32_t kqueue_change(event_loop *loop, int32_t file_descriptor,
    int16_t filter, uint16_t flags, uint64_t token)
{
    struct kevent change_event;

    EV_SET(&change_event, file_descriptor, filter, flags, 0U, 0,
        reinterpret_cast<void *>(static_cast<uintptr_t>(token)));
    if (kevent(loop->backend_descriptor, &change_event, 1, ft_nullptr, 0,
            ft_nullptr) != 0)
    {
        if ((flags & EV_DELETE) != 0U && errno == ENOENT)
            return (FT_ERR_SUCCESS);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    return (FT_ERR_SUCCESS);
}
#endif

static int32_t backend_add_or_modify(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask, uint64_t generation, ft_bool existing)
{
#ifdef NETWORKING_USE_EPOLL
    struct epoll_event event;
    uint64_t token;
    int32_t operation;

    ft_memset(&event, 0, sizeof(event));
    token = (generation << 32U) | static_cast<uint32_t>(file_descriptor);
    event.data.u64 = token;
    event.events = epoll_interest(interest_mask);
    if (existing != FT_FALSE)
        operation = EPOLL_CTL_MOD;
    else
        operation = EPOLL_CTL_ADD;
    if (epoll_ctl(loop->backend_descriptor, operation, file_descriptor,
            &event) != 0)
        return (FT_ERR_INVALID_ARGUMENT);
#elif defined(NETWORKING_USE_KQUEUE)
    uint64_t token;
    int32_t error_code;

    token = (generation << 32U) | static_cast<uint32_t>(file_descriptor);
    if ((interest_mask & EVENT_LOOP_INTEREST_READ) != 0U)
        error_code = kqueue_change(loop, file_descriptor, EVFILT_READ,
            static_cast<uint16_t>(EV_ADD | EV_ENABLE), token);
    else if (existing != FT_FALSE)
        error_code = kqueue_change(loop, file_descriptor, EVFILT_READ,
            EV_DELETE, token);
    else
        error_code = FT_ERR_SUCCESS;
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if ((interest_mask & EVENT_LOOP_INTEREST_WRITE) != 0U)
        error_code = kqueue_change(loop, file_descriptor, EVFILT_WRITE,
            static_cast<uint16_t>(EV_ADD | EV_ENABLE), token);
    else if (existing != FT_FALSE)
        error_code = kqueue_change(loop, file_descriptor, EVFILT_WRITE,
            EV_DELETE, token);
    else
        error_code = FT_ERR_SUCCESS;
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
#else
    (void)loop;
    (void)file_descriptor;
    (void)interest_mask;
    (void)generation;
    (void)existing;
#endif
    return (FT_ERR_SUCCESS);
}

static int32_t backend_remove(event_loop *loop, int32_t file_descriptor)
{
#ifdef NETWORKING_USE_EPOLL
    if (epoll_ctl(loop->backend_descriptor, EPOLL_CTL_DEL, file_descriptor,
            ft_nullptr) != 0 && errno != ENOENT)
        return (FT_ERR_INVALID_ARGUMENT);
#elif defined(NETWORKING_USE_KQUEUE)
    if (kqueue_change(loop, file_descriptor, EVFILT_READ, EV_DELETE, 0U)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
    if (kqueue_change(loop, file_descriptor, EVFILT_WRITE, EV_DELETE, 0U)
        != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_ARGUMENT);
#else
    (void)loop;
    (void)file_descriptor;
#endif
    return (FT_ERR_SUCCESS);
}

#ifdef _WIN32
static ft_bool event_loop_open_wakeup_sockets(event_loop *loop)
{
    const int32_t listener = nw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener < 0)
        return (FT_FALSE);
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    sockaddr *socket_address = reinterpret_cast<sockaddr *>(&address);
    socklen_t address_size = sizeof(address);
    if (nw_bind(listener, socket_address, address_size) != 0 || nw_listen(listener, 1) != 0
        || getsockname(static_cast<SOCKET>(listener), socket_address, &address_size) != 0)
    {
        nw_close(listener);
        return (FT_FALSE);
    }

    const int32_t sender = nw_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    int32_t receiver = -1;
    if (sender >= 0 && nw_connect(sender, socket_address, address_size) == 0)
        receiver = nw_accept(listener, ft_nullptr, ft_nullptr);
    nw_close(listener);
    const int32_t no_delay = 1;
    if (receiver < 0 || nw_set_nonblocking(sender) != 0 || nw_set_nonblocking(receiver) != 0
        || setsockopt(static_cast<SOCKET>(sender), IPPROTO_TCP, TCP_NODELAY,
            reinterpret_cast<const char *>(&no_delay), sizeof(no_delay)) != 0)
    {
        if (receiver >= 0)
            nw_close(receiver);
        if (sender >= 0)
            nw_close(sender);
        return (FT_FALSE);
    }
    loop->wakeup_read_descriptor = receiver;
    loop->wakeup_write_descriptor = sender;
    return (FT_TRUE);
}
#endif

static void signal_loop(event_loop *loop)
{
#ifdef _WIN32
    uint8_t value;

    value = 1U;
    if (loop->wakeup_write_descriptor >= 0)
        (void)send(static_cast<SOCKET>(loop->wakeup_write_descriptor),
            reinterpret_cast<const char *>(&value), sizeof(value), 0);
    return ;
#else
#ifdef NETWORKING_USE_EPOLL
    uint64_t value;

    value = 1U;
    if (loop->wakeup_write_descriptor >= 0
        && write(loop->wakeup_write_descriptor, &value, sizeof(value)) < 0
        && errno != EAGAIN)
        return ;
#else
    uint8_t value;

    value = 1U;
    if (loop->wakeup_write_descriptor >= 0
        && write(loop->wakeup_write_descriptor, &value, 1U) < 0
        && errno != EAGAIN)
        return ;
#endif
    return ;
#endif
}

static void drain_loop_signal(event_loop *loop)
{
#ifdef _WIN32
    char signals[256];

    if (loop->wakeup_read_descriptor >= 0)
    {
        while (recv(static_cast<SOCKET>(loop->wakeup_read_descriptor),
                signals, sizeof(signals), 0) > 0)
            ;
    }
    return ;
#else
#ifdef NETWORKING_USE_EPOLL
    uint64_t value;

    while (read(loop->wakeup_read_descriptor, &value, sizeof(value)) > 0)
        ;
#else
    uint8_t values[64];

    while (read(loop->wakeup_read_descriptor, values, sizeof(values)) > 0)
        ;
#endif
    return ;
#endif
}

static int32_t append_event(event_loop_ready_event *events,
    uint32_t *event_count, uint32_t event_capacity, int32_t file_descriptor,
    uint32_t ready_mask, int32_t error_code)
{
    uint32_t index;

    index = 0U;
    while (index < *event_count)
    {
        if (events[index].file_descriptor == file_descriptor)
        {
            events[index].ready_mask |= ready_mask;
            if (events[index].error_code == FT_ERR_SUCCESS)
                events[index].error_code = error_code;
            return (FT_ERR_SUCCESS);
        }
        index += 1U;
    }
    if (*event_count >= event_capacity)
        return (FT_ERR_NO_MEMORY);
    events[*event_count].file_descriptor = file_descriptor;
    events[*event_count].ready_mask = ready_mask;
    events[*event_count].error_code = error_code;
    *event_count += 1U;
    return (FT_ERR_SUCCESS);
}

void event_loop_init(event_loop *loop)
{
    int32_t error_code;

    if (loop == ft_nullptr)
        return ;
    ft_memset(loop, 0, sizeof(*loop));
    loop->backend_descriptor = -1;
    loop->wakeup_read_descriptor = -1;
    loop->wakeup_write_descriptor = -1;
    loop->wait_active = FT_FALSE;
    error_code = make_mutex(&loop->mutex);
    if (error_code != FT_ERR_SUCCESS)
        return ;
    error_code = make_mutex(&loop->wait_mutex);
    if (error_code != FT_ERR_SUCCESS)
    {
        free_mutex(&loop->mutex);
        return ;
    }
#ifdef NETWORKING_USE_EPOLL
    loop->backend_descriptor = epoll_create1(EPOLL_CLOEXEC);
    if (loop->backend_descriptor < 0)
    {
        free_mutex(&loop->wait_mutex);
        free_mutex(&loop->mutex);
        return ;
    }
    loop->wakeup_read_descriptor = eventfd(0U, EFD_CLOEXEC | EFD_NONBLOCK);
    loop->wakeup_write_descriptor = loop->wakeup_read_descriptor;
    if (loop->wakeup_read_descriptor < 0)
    {
        event_loop_close_descriptor(loop->backend_descriptor);
        loop->backend_descriptor = -1;
        free_mutex(&loop->wait_mutex);
        free_mutex(&loop->mutex);
        return ;
    }
    {
        struct epoll_event event;

        ft_memset(&event, 0, sizeof(event));
        event.events = EPOLLIN;
        event.data.u64 = UINT64_MAX;
        if (epoll_ctl(loop->backend_descriptor, EPOLL_CTL_ADD,
                loop->wakeup_read_descriptor, &event) != 0)
        {
            event_loop_close_descriptor(loop->wakeup_read_descriptor);
            event_loop_close_descriptor(loop->backend_descriptor);
            loop->wakeup_read_descriptor = -1;
            loop->wakeup_write_descriptor = -1;
            loop->backend_descriptor = -1;
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
    }
#elif defined(NETWORKING_USE_KQUEUE)
    loop->backend_descriptor = kqueue();
    if (loop->backend_descriptor < 0)
    {
        free_mutex(&loop->wait_mutex);
        free_mutex(&loop->mutex);
        return ;
    }
    {
        int32_t descriptors[2];

        if (pipe(descriptors) != 0)
        {
            event_loop_close_descriptor(loop->backend_descriptor);
            loop->backend_descriptor = -1;
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
        loop->wakeup_read_descriptor = descriptors[0];
        loop->wakeup_write_descriptor = descriptors[1];
        (void)fcntl(loop->wakeup_read_descriptor, F_SETFL, O_NONBLOCK);
        (void)fcntl(loop->wakeup_write_descriptor, F_SETFL, O_NONBLOCK);
    }
    {
        struct kevent event;

        EV_SET(&event, loop->wakeup_read_descriptor, EVFILT_READ,
            EV_ADD | EV_ENABLE, 0U, 0, ft_nullptr);
        if (kevent(loop->backend_descriptor, &event, 1, ft_nullptr, 0,
                ft_nullptr) != 0)
        {
            event_loop_close_descriptor(loop->wakeup_read_descriptor);
            event_loop_close_descriptor(loop->wakeup_write_descriptor);
            event_loop_close_descriptor(loop->backend_descriptor);
            loop->wakeup_read_descriptor = -1;
            loop->wakeup_write_descriptor = -1;
            loop->backend_descriptor = -1;
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
    }
#elif defined(_WIN32)
    if (!event_loop_open_wakeup_sockets(loop))
    {
        free_mutex(&loop->wait_mutex);
        free_mutex(&loop->mutex);
        return ;
    }
#else
    {
        int32_t descriptors[2];

        if (pipe(descriptors) != 0)
        {
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
        loop->wakeup_read_descriptor = descriptors[0];
        loop->wakeup_write_descriptor = descriptors[1];
        (void)fcntl(loop->wakeup_read_descriptor, F_SETFL, O_NONBLOCK);
        (void)fcntl(loop->wakeup_write_descriptor, F_SETFL, O_NONBLOCK);
    }
#endif
    loop->thread_safe_enabled = FT_TRUE;
    return ;
}

void event_loop_clear(event_loop *loop)
{
    uint32_t index;

    if (loop == ft_nullptr)
        return ;
    if (loop->mutex != ft_nullptr)
    {
        (void)pt_mutex_lock_if_not_null(loop->mutex);
        loop->stopping = FT_TRUE;
        signal_loop(loop);
        (void)pt_mutex_unlock_if_not_null(loop->mutex);
    }
    if (loop->wait_mutex != ft_nullptr)
    {
        (void)pt_mutex_lock_if_not_null(loop->wait_mutex);
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
    }
    index = 0U;
    while (index < loop->registration_count)
    {
        (void)backend_remove(loop, loop->registrations[index].file_descriptor);
        index += 1U;
    }
    if (loop->wakeup_read_descriptor >= 0)
        event_loop_close_descriptor(loop->wakeup_read_descriptor);
#ifndef NETWORKING_USE_EPOLL
    if (loop->wakeup_write_descriptor >= 0)
        event_loop_close_descriptor(loop->wakeup_write_descriptor);
#endif
    if (loop->backend_descriptor >= 0)
        event_loop_close_descriptor(loop->backend_descriptor);
    if (loop->registrations != ft_nullptr)
        cma_free(loop->registrations);
    if (loop->ready_events != ft_nullptr)
        cma_free(loop->ready_events);
    if (loop->backend_events != ft_nullptr)
        cma_free(loop->backend_events);
    if (loop->read_file_descriptors != ft_nullptr)
        cma_free(loop->read_file_descriptors);
    if (loop->write_file_descriptors != ft_nullptr)
        cma_free(loop->write_file_descriptors);
    loop->registrations = ft_nullptr;
    loop->ready_events = ft_nullptr;
    loop->backend_events = ft_nullptr;
    loop->read_file_descriptors = ft_nullptr;
    loop->write_file_descriptors = ft_nullptr;
    loop->backend_descriptor = -1;
    loop->wakeup_read_descriptor = -1;
    loop->wakeup_write_descriptor = -1;
    loop->registration_count = 0U;
    loop->registration_capacity = 0U;
    loop->ready_capacity = 0U;
    loop->backend_event_capacity = 0U;
    free_mutex(&loop->wait_mutex);
    free_mutex(&loop->mutex);
    loop->thread_safe_enabled = FT_FALSE;
    return ;
}

int32_t event_loop_initialize(event_loop *loop) noexcept
{
    if (loop == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    event_loop_init(loop);
    if (loop->mutex == ft_nullptr || loop->wait_mutex == ft_nullptr
        || loop->wakeup_read_descriptor < 0
        || loop->wakeup_write_descriptor < 0)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

int32_t event_loop_destroy(event_loop *loop) noexcept
{
    if (loop == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    event_loop_clear(loop);
    return (FT_ERR_SUCCESS);
}

int32_t event_loop_add_interest(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask) noexcept
{
    if (loop == ft_nullptr || file_descriptor < 0 || interest_mask == 0U
        || (interest_mask & ~(EVENT_LOOP_INTEREST_READ
            | EVENT_LOOP_INTEREST_WRITE)) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
        return (FT_ERR_THREAD_BUSY);
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t existing = find_registration(loop, file_descriptor);
    uint32_t index = loop->registration_count;
    uint32_t previous_mask = 0;
    const uint64_t generation = loop->next_generation + 1;
    int32_t error_code = FT_ERR_SUCCESS;
    if (existing >= 0)
    {
        index = static_cast<uint32_t>(existing);
        previous_mask = loop->registrations[index].interest_mask;
    }
    const uint32_t added_mask = interest_mask & ~previous_mask;
    if (added_mask == 0)
    {
        event_loop_unlock(loop, FT_TRUE);
        return (FT_ERR_SUCCESS);
    }
    if (existing < 0)
    {
        if (index >= FT_INT32_MAX)
            error_code = FT_ERR_OUT_OF_RANGE;
        else
            error_code = event_loop_reserve_registrations(loop, index + 1U);
    }
    const uint32_t merged_mask = previous_mask | added_mask;
    if (error_code == FT_ERR_SUCCESS)
        error_code = backend_add_or_modify(loop, file_descriptor, merged_mask,
                generation, existing >= 0);
    if (error_code == FT_ERR_SUCCESS)
    {
        if ((added_mask & EVENT_LOOP_INTEREST_READ) != 0U)
            event_loop_insert_interest(loop, loop->read_file_descriptors, loop->read_count,
                    index, EVENT_LOOP_INTEREST_READ, file_descriptor);
        if ((added_mask & EVENT_LOOP_INTEREST_WRITE) != 0U)
            event_loop_insert_interest(loop, loop->write_file_descriptors, loop->write_count,
                    index, EVENT_LOOP_INTEREST_WRITE, file_descriptor);
        loop->registrations[index] = {file_descriptor, merged_mask, generation};
        if (existing < 0)
            ++loop->registration_count;
        loop->next_generation = generation;
    }
#ifdef _WIN32
    if (error_code == FT_ERR_SUCCESS && loop->wait_active != FT_FALSE)
        signal_loop(loop);
    event_loop_unlock(loop, FT_TRUE);
#else
    event_loop_unlock(loop, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS)
        signal_loop(loop);
#endif
    return (error_code);
}

int32_t event_loop_remove_interest(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask) noexcept
{
    int32_t registration_index;
    int32_t error_code;

    if (loop == ft_nullptr || file_descriptor < 0 || interest_mask == 0U
        || (interest_mask & ~(EVENT_LOOP_INTEREST_READ
            | EVENT_LOOP_INTEREST_WRITE)) != 0U)
        return (FT_ERR_INVALID_ARGUMENT);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
        return (FT_ERR_THREAD_BUSY);
    registration_index = find_registration(loop, file_descriptor);
    if (registration_index < 0)
    {
        event_loop_unlock(loop, FT_TRUE);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    const uint32_t index = static_cast<uint32_t>(registration_index);
    const uint32_t previous_mask = loop->registrations[index].interest_mask;
    const uint32_t removed_mask = previous_mask & interest_mask;
    if (removed_mask == 0)
    {
        event_loop_unlock(loop, FT_TRUE);
        return (FT_ERR_SUCCESS);
    }
    const uint32_t remaining_mask = previous_mask & ~removed_mask;
    const uint64_t generation = loop->next_generation + 1;
    if (remaining_mask != 0U)
        error_code = backend_add_or_modify(loop, file_descriptor, remaining_mask,
                generation, FT_TRUE);
    else
        error_code = backend_remove(loop, file_descriptor);
    if (error_code == FT_ERR_SUCCESS)
    {
        if ((removed_mask & EVENT_LOOP_INTEREST_READ) != 0U)
            event_loop_erase_interest(loop->read_file_descriptors, loop->read_count, file_descriptor);
        if ((removed_mask & EVENT_LOOP_INTEREST_WRITE) != 0U)
            event_loop_erase_interest(loop->write_file_descriptors, loop->write_count, file_descriptor);
        if (remaining_mask != 0U)
        {
            loop->registrations[index].interest_mask = remaining_mask;
            loop->registrations[index].generation = generation;
        }
        else
        {
            --loop->registration_count;
            if (index < loop->registration_count)
                ft_memmove(loop->registrations + index, loop->registrations + index + 1,
                    sizeof(event_loop_registration) * (loop->registration_count - index));
        }
        loop->next_generation = generation;
    }
#ifdef _WIN32
    if (error_code == FT_ERR_SUCCESS && loop->wait_active != FT_FALSE)
        signal_loop(loop);
    event_loop_unlock(loop, FT_TRUE);
#else
    event_loop_unlock(loop, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS)
        signal_loop(loop);
#endif
    return (error_code);
}

#if !defined(NETWORKING_USE_EPOLL) && !defined(NETWORKING_USE_KQUEUE)
static ft_bool event_loop_snapshot_is_current(const event_loop *loop, int32_t file_descriptor,
    uint32_t interest, uint64_t snapshot_generation)
{
    const int32_t index = find_registration(loop, file_descriptor);
    if (index < 0)
        return (FT_FALSE);
    const event_loop_registration &current = loop->registrations[index];
    return (current.generation <= snapshot_generation && (current.interest_mask & interest) != 0U);
}
#endif

int32_t event_loop_wait(event_loop *loop, event_loop_ready_event *events,
    uint32_t event_capacity, uint32_t *event_count,
    int32_t timeout_milliseconds) noexcept
{
    uint32_t registration_count;
    uint32_t local_count;
    uint32_t index;
    ft_bool has_registrations;
#ifdef NETWORKING_USE_EPOLL
    struct epoll_event *backend_events;
    int32_t ready_count;
    int32_t wait_timeout;
    uint64_t token;
    int32_t file_descriptor;
    uint32_t backend_mask;
    uint64_t generation;
    int32_t registration_index;
    struct epoll_event empty_backend_event;
    std::chrono::steady_clock::time_point deadline;
#elif defined(NETWORKING_USE_KQUEUE)
    struct kevent *backend_events;
    int32_t ready_count;
    struct timespec timeout;
    struct timespec *timeout_pointer;
    uint64_t token;
    int32_t file_descriptor;
    uint64_t generation;
    int32_t registration_index;
    struct kevent empty_backend_event;
    std::chrono::steady_clock::time_point deadline;
#else
    constexpr uint32_t STACK_INTERESTS = 32;
    int32_t read_stack[STACK_INTERESTS];
    int32_t write_stack[STACK_INTERESTS];
    int32_t *read_snapshot = read_stack;
    int32_t *write_snapshot = write_stack;
    int32_t *heap_snapshot = ft_nullptr;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t poll_read_count;
    int32_t poll_result;
#endif
#if !defined(NETWORKING_USE_EPOLL) && !defined(NETWORKING_USE_KQUEUE)
    (void)has_registrations;
#endif

    if (loop == ft_nullptr || events == ft_nullptr || event_count == ft_nullptr
        || event_capacity == 0U || timeout_milliseconds < -1)
        return (FT_ERR_INVALID_ARGUMENT);
    *event_count = 0U;
    if (loop->wait_mutex == ft_nullptr
        || loop->wait_mutex->try_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_THREAD_BUSY);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_THREAD_BUSY);
    }
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    registration_count = loop->registration_count;
    has_registrations = FT_FALSE;
    if (registration_count > 0U)
        has_registrations = FT_TRUE;
    if (registration_count > loop->backend_event_capacity)
    {
#if defined(NETWORKING_USE_EPOLL) || defined(NETWORKING_USE_KQUEUE)
        void *new_backend_events;

# ifdef NETWORKING_USE_EPOLL
        new_backend_events = cma_realloc(loop->backend_events,
            sizeof(struct epoll_event) * registration_count);
# else
        new_backend_events = cma_realloc(loop->backend_events,
            sizeof(struct kevent) * registration_count);
# endif
        if (new_backend_events == ft_nullptr)
        {
            event_loop_unlock(loop, FT_TRUE);
            (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
            return (FT_ERR_NO_MEMORY);
        }
        loop->backend_events = new_backend_events;
        loop->backend_event_capacity = registration_count;
#endif
    }
#ifdef _WIN32
    drain_loop_signal(loop);
#endif
    loop->wait_active = FT_TRUE;
    event_loop_unlock(loop, FT_TRUE);
    local_count = 0U;
#ifdef NETWORKING_USE_EPOLL
    if (registration_count == 0U)
        registration_count = 1U;
    drain_loop_signal(loop);
    backend_events = static_cast<struct epoll_event *>(loop->backend_events);
    if (has_registrations == FT_FALSE)
    {
        ft_memset(&empty_backend_event, 0, sizeof(empty_backend_event));
        backend_events = &empty_backend_event;
    }
    else if (backend_events == ft_nullptr
        || loop->backend_event_capacity < registration_count)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_NO_MEMORY);
    }
    wait_timeout = timeout_milliseconds;
    if (timeout_milliseconds >= 0)
        deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeout_milliseconds);
    ready_count = epoll_wait(loop->backend_descriptor, backend_events,
        static_cast<int32_t>(registration_count), wait_timeout);
    while (ready_count < 0 && errno == EINTR)
    {
        if (timeout_milliseconds < 0)
            wait_timeout = -1;
        else
        {
            wait_timeout = event_loop_remaining_timeout(deadline);
            if (wait_timeout == 0)
            {
                ready_count = 0;
                break ;
            }
        }
        ready_count = epoll_wait(loop->backend_descriptor, backend_events,
            static_cast<int32_t>(registration_count), wait_timeout);
    }
    if (ready_count < 0 && errno != EINTR)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    index = 0U;
    if (ready_count < 0)
        ready_count = 0;
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_THREAD_BUSY);
    }
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    while (index < static_cast<uint32_t>(ready_count))
    {
        token = backend_events[index].data.u64;
        if (token == UINT64_MAX)
            drain_loop_signal(loop);
        else
        {
            generation = token >> 32U;
            file_descriptor = static_cast<int32_t>(token & UINT32_MAX);
            registration_index = find_registration(loop, file_descriptor);
            if (registration_index < 0
                || static_cast<uint32_t>(loop->registrations[registration_index].generation)
                    != generation)
            {
                index += 1U;
                continue;
            }
            backend_mask = backend_events[index].events;
            if ((backend_mask & EPOLLIN) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_READ,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_mask & EPOLLOUT) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_WRITE,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_mask & (EPOLLHUP | EPOLLERR)) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_ERROR,
                        FT_ERR_INVALID_STATE) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
#ifdef EPOLLRDHUP
            if ((backend_mask & EPOLLRDHUP) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_HANGUP,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
#endif
        }
        index += 1U;
    }
    event_loop_unlock(loop, FT_TRUE);
#elif defined(NETWORKING_USE_KQUEUE)
    if (registration_count == 0U)
        registration_count = 1U;
    drain_loop_signal(loop);
    backend_events = static_cast<struct kevent *>(loop->backend_events);
    if (has_registrations == FT_FALSE)
    {
        ft_memset(&empty_backend_event, 0, sizeof(empty_backend_event));
        backend_events = &empty_backend_event;
    }
    else if (backend_events == ft_nullptr
        || loop->backend_event_capacity < registration_count)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_NO_MEMORY);
    }
    timeout_pointer = ft_nullptr;
    if (timeout_milliseconds >= 0)
    {
        timeout.tv_sec = timeout_milliseconds / 1000;
        timeout.tv_nsec = (timeout_milliseconds % 1000) * 1000000;
        timeout_pointer = &timeout;
        deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeout_milliseconds);
    }
    ready_count = kevent(loop->backend_descriptor, ft_nullptr, 0,
        backend_events, static_cast<int32_t>(registration_count),
        timeout_pointer);
    while (ready_count < 0 && errno == EINTR)
    {
        if (timeout_milliseconds < 0)
            timeout_pointer = ft_nullptr;
        else
        {
            int32_t remaining_milliseconds;

            remaining_milliseconds = event_loop_remaining_timeout(deadline);
            if (remaining_milliseconds == 0)
            {
                ready_count = 0;
                break ;
            }
            timeout.tv_sec = remaining_milliseconds / 1000;
            timeout.tv_nsec = (remaining_milliseconds % 1000) * 1000000;
            timeout_pointer = &timeout;
        }
        ready_count = kevent(loop->backend_descriptor, ft_nullptr, 0,
            backend_events, static_cast<int32_t>(registration_count),
            timeout_pointer);
    }
    if (ready_count < 0)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    index = 0U;
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        release_event_loop_wait(loop);
        return (FT_ERR_THREAD_BUSY);
    }
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    while (index < static_cast<uint32_t>(ready_count))
    {
        if (backend_events[index].udata == ft_nullptr)
            drain_loop_signal(loop);
        else
        {
            token = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(
                backend_events[index].udata));
            generation = token >> 32U;
            file_descriptor = static_cast<int32_t>(token & UINT32_MAX);
            registration_index = find_registration(loop, file_descriptor);
            if (registration_index < 0
                || static_cast<uint32_t>(loop->registrations[registration_index].generation)
                    != generation)
            {
                index += 1U;
                continue;
            }
            if (backend_events[index].filter == EVFILT_READ)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_READ,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if (backend_events[index].filter == EVFILT_WRITE)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_WRITE,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_events[index].flags & EV_EOF) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_HANGUP,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_events[index].flags & EV_ERROR) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_ERROR,
                        FT_ERR_INVALID_STATE) != FT_ERR_SUCCESS)
                {
                    event_loop_unlock(loop, FT_TRUE);
                    release_event_loop_wait(loop);
                    return (FT_ERR_NO_MEMORY);
                }
            }
        }
        index += 1U;
    }
    event_loop_unlock(loop, FT_TRUE);
#else
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_THREAD_BUSY);
    }
    const uint64_t snapshot_generation = loop->next_generation;
    read_count = static_cast<uint32_t>(loop->read_count);
    write_count = static_cast<uint32_t>(loop->write_count);
    poll_read_count = read_count + 1U;
    if (poll_read_count > STACK_INTERESTS || write_count > STACK_INTERESTS)
    {
        const ft_size_t interest_count = static_cast<ft_size_t>(poll_read_count) + write_count;
        heap_snapshot = static_cast<int32_t *>(cma_malloc(sizeof(int32_t) * interest_count));
        if (!heap_snapshot)
        {
            event_loop_unlock(loop, FT_TRUE);
            release_event_loop_wait(loop);
            return (FT_ERR_NO_MEMORY);
        }
        read_snapshot = heap_snapshot;
        write_snapshot = heap_snapshot + poll_read_count;
    }
    if (read_count != 0U)
        ft_memcpy(read_snapshot, loop->read_file_descriptors, sizeof(int32_t) * read_count);
    if (write_count != 0U)
        ft_memcpy(write_snapshot, loop->write_file_descriptors, sizeof(int32_t) * write_count);
    read_snapshot[read_count] = loop->wakeup_read_descriptor;
    event_loop_unlock(loop, FT_TRUE);

    poll_result = nw_poll(read_snapshot, static_cast<int32_t>(poll_read_count), write_snapshot,
            static_cast<int32_t>(write_count), timeout_milliseconds);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        if (heap_snapshot)
            cma_free(heap_snapshot);
        release_event_loop_wait(loop);
        return (FT_ERR_THREAD_BUSY);
    }
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        if (heap_snapshot)
            cma_free(heap_snapshot);
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    const ft_bool registrations_changed = (loop->next_generation != snapshot_generation);
    if (poll_result > 0 && read_snapshot[read_count] == loop->wakeup_read_descriptor)
        drain_loop_signal(loop);
    index = 0U;
    while (poll_result > 0 && index < read_count)
    {
        if (read_snapshot[index] >= 0 && (!registrations_changed
            || event_loop_snapshot_is_current(loop, read_snapshot[index],
                EVENT_LOOP_INTEREST_READ, snapshot_generation)))
#ifdef _WIN32
        {
            u_long available_bytes;

            available_bytes = 0U;
            if (ioctlsocket(static_cast<SOCKET>(read_snapshot[index]),
                FIONREAD, &available_bytes) != 0)
                (void)append_event(events, &local_count, event_capacity,
                    read_snapshot[index], EVENT_LOOP_READY_ERROR,
                    FT_ERR_INVALID_STATE);
            else if (available_bytes == 0U)
                (void)append_event(events, &local_count, event_capacity,
                    read_snapshot[index], EVENT_LOOP_READY_HANGUP,
                    FT_ERR_SUCCESS);
            else
                (void)append_event(events, &local_count, event_capacity,
                    read_snapshot[index], EVENT_LOOP_READY_READ,
                    FT_ERR_SUCCESS);
        }
#else
            (void)append_event(events, &local_count, event_capacity,
                read_snapshot[index], EVENT_LOOP_READY_READ, FT_ERR_SUCCESS);
#endif
        index += 1U;
    }
    index = 0U;
    while (poll_result > 0 && index < write_count)
    {
        if (write_snapshot[index] >= 0 && (!registrations_changed
            || event_loop_snapshot_is_current(loop, write_snapshot[index],
                EVENT_LOOP_INTEREST_WRITE, snapshot_generation)))
            (void)append_event(events, &local_count, event_capacity,
                write_snapshot[index], EVENT_LOOP_READY_WRITE,
                FT_ERR_SUCCESS);
        index += 1U;
    }
    event_loop_unlock(loop, FT_TRUE);
    if (heap_snapshot)
        cma_free(heap_snapshot);
#endif
    *event_count = local_count;
    release_event_loop_wait(loop);
    return (FT_ERR_SUCCESS);
}

int32_t event_loop_wait_for_socket(event_loop *loop, int32_t socket_descriptor,
    ft_bool wait_for_write, int32_t timeout_milliseconds) noexcept
{
    if (loop == ft_nullptr || socket_descriptor < 0 || timeout_milliseconds < -1
        || loop->wait_mutex == ft_nullptr
        || loop->wait_mutex->try_lock() != FT_ERR_SUCCESS)
        return (-1);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (-1);
    }
    if (loop->stopping || loop->wakeup_read_descriptor < 0)
    {
        event_loop_unlock(loop, FT_TRUE);
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (-1);
    }
    drain_loop_signal(loop);
    loop->wait_active = FT_TRUE;
    int32_t read_descriptors[] = {loop->wakeup_read_descriptor, socket_descriptor};
    event_loop_unlock(loop, FT_TRUE);

    int32_t read_count = 2;
    int32_t write_count = 0;
    if (wait_for_write)
    {
        read_count = 1;
        write_count = 1;
    }
    int32_t write_descriptor = socket_descriptor;
#ifndef _WIN32
    std::chrono::steady_clock::time_point deadline;
    if (timeout_milliseconds > 0)
        deadline = std::chrono::steady_clock::now()
            + std::chrono::milliseconds(timeout_milliseconds);
#endif
    int32_t poll_result = nw_poll(read_descriptors, read_count, &write_descriptor,
        write_count, timeout_milliseconds);
#ifndef _WIN32
    while (poll_result < 0 && errno == EINTR)
    {
        if (timeout_milliseconds > 0)
            timeout_milliseconds = event_loop_remaining_timeout(deadline);
        if (timeout_milliseconds == 0)
        {
            poll_result = 0;
            break ;
        }
        poll_result = nw_poll(read_descriptors, read_count, &write_descriptor,
            write_count, timeout_milliseconds);
    }
#endif

    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        release_event_loop_wait(loop);
        return (-1);
    }
    const ft_bool stopping = loop->stopping;
    if (poll_result > 0 && read_descriptors[0] >= 0)
        drain_loop_signal(loop);
    loop->wait_active = FT_FALSE;
    event_loop_unlock(loop, FT_TRUE);
    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
    if (stopping || poll_result < 0)
        return (-1);
    if (poll_result == 0)
        return (0);
    if (wait_for_write)
        return (write_descriptor >= 0);
    return (read_descriptors[1] >= 0);
}

int32_t event_loop_interrupt(event_loop *loop) noexcept
{
    if (loop == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    signal_loop(loop);
    return (FT_ERR_SUCCESS);
}

int32_t event_loop_add_socket(event_loop *loop, int32_t socket_fd,
    ft_bool is_write)
{
    if (is_write != FT_FALSE)
        return (event_loop_add_interest(loop, socket_fd,
            EVENT_LOOP_INTEREST_WRITE));
    return (event_loop_add_interest(loop, socket_fd,
        EVENT_LOOP_INTEREST_READ));
}

int32_t event_loop_remove_socket(event_loop *loop, int32_t socket_fd,
    ft_bool is_write)
{
    if (is_write != FT_FALSE)
        return (event_loop_remove_interest(loop, socket_fd,
            EVENT_LOOP_INTEREST_WRITE));
    return (event_loop_remove_interest(loop, socket_fd,
        EVENT_LOOP_INTEREST_READ));
}

int32_t event_loop_run(event_loop *loop, int32_t timeout_milliseconds)
{
    constexpr uint32_t STACK_EVENTS = 32;
    event_loop_ready_event stack_events[STACK_EVENTS];
    event_loop_ready_event *events = stack_events;
    uint32_t event_count;
    uint32_t capacity;
    int32_t result_code;

    if (loop == ft_nullptr)
        return (-1);
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
        return (-1);
    capacity = loop->registration_count;
    event_loop_unlock(loop, FT_TRUE);
    if (capacity == 0U)
        capacity = 1U;
    if (capacity > STACK_EVENTS)
    {
        events = static_cast<event_loop_ready_event *>(cma_malloc(
                sizeof(event_loop_ready_event) * capacity));
        if (!events)
            return (-1);
    }
    result_code = event_loop_wait(loop, events, capacity, &event_count,
        timeout_milliseconds);
    if (events != stack_events)
        cma_free(events);
    if (result_code != FT_ERR_SUCCESS)
        return (-1);
    return (static_cast<int32_t>(event_count));
}

int32_t event_loop_prepare_thread_safety(event_loop *loop)
{
    if (loop == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (loop->mutex != ft_nullptr && loop->wait_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    return (FT_ERR_INVALID_STATE);
}

void event_loop_teardown_thread_safety(event_loop *loop)
{
    if (loop == ft_nullptr)
        return ;
    free_mutex(&loop->wait_mutex);
    free_mutex(&loop->mutex);
    loop->thread_safe_enabled = FT_FALSE;
    return ;
}

int32_t event_loop_lock(event_loop *loop, ft_bool *lock_acquired)
{
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_FALSE;
    if (loop == ft_nullptr || loop->mutex == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    if (pt_mutex_lock_if_not_null(loop->mutex) != FT_ERR_SUCCESS)
        return (FT_ERR_THREAD_BUSY);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void event_loop_unlock(event_loop *loop, ft_bool lock_acquired)
{
    if (loop == ft_nullptr || lock_acquired == FT_FALSE)
        return ;
    (void)pt_mutex_unlock_if_not_null(loop->mutex);
    return ;
}
