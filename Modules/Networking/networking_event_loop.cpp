#include <cerrno>
#include <chrono>
#include <cstdlib>

#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"

#ifdef _WIN32
# include <fcntl.h>
# include <io.h>
#endif

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
    (void)_close(file_descriptor);
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

static int32_t build_legacy_arrays(const event_loop_registration *registrations,
    uint32_t registration_count, int32_t **read_descriptors,
    uint32_t *read_count, int32_t **write_descriptors,
    uint32_t *write_count)
{
    uint32_t index;

    *read_count = 0U;
    *write_count = 0U;
    index = 0U;
    while (index < registration_count)
    {
        if ((registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_READ) != 0U)
            *read_count += 1U;
        if ((registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_WRITE) != 0U)
            *write_count += 1U;
        index += 1U;
    }
    *read_descriptors = ft_nullptr;
    *write_descriptors = ft_nullptr;
    if (*read_count > 0U)
        *read_descriptors = static_cast<int32_t *>(cma_malloc(
            sizeof(int32_t) * *read_count));
    if (*write_count > 0U)
        *write_descriptors = static_cast<int32_t *>(cma_malloc(
            sizeof(int32_t) * *write_count));
    if ((*read_count > 0U && *read_descriptors == ft_nullptr)
        || (*write_count > 0U && *write_descriptors == ft_nullptr))
    {
        if (*read_descriptors != ft_nullptr)
            cma_free(*read_descriptors);
        if (*write_descriptors != ft_nullptr)
            cma_free(*write_descriptors);
        *read_descriptors = ft_nullptr;
        *write_descriptors = ft_nullptr;
        return (FT_ERR_NO_MEMORY);
    }
    *read_count = 0U;
    *write_count = 0U;
    index = 0U;
    while (index < registration_count)
    {
        if ((registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_READ) != 0U)
            (*read_descriptors)[(*read_count)++] = registrations[index].file_descriptor;
        if ((registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_WRITE) != 0U)
            (*write_descriptors)[(*write_count)++] = registrations[index].file_descriptor;
        index += 1U;
    }
    return (FT_ERR_SUCCESS);
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

#if defined(NETWORKING_USE_EPOLL) || defined(NETWORKING_USE_KQUEUE)
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

static void signal_loop(event_loop *loop)
{
#ifdef _WIN32
    uint8_t value;

    value = 1U;
    if (loop->wakeup_write_descriptor >= 0)
        (void)_write(loop->wakeup_write_descriptor, &value, 1U);
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

#ifndef _WIN32
static void drain_loop_signal(event_loop *loop)
{
#ifdef _WIN32
    uint8_t value;

    if (loop->wakeup_read_descriptor >= 0)
        (void)_read(loop->wakeup_read_descriptor, &value, 1U);
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
#endif

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
    {
        int32_t descriptors[2];

        if (_pipe(descriptors, 512, _O_BINARY | _O_NOINHERIT) != 0)
        {
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
        loop->wakeup_read_descriptor = descriptors[0];
        loop->wakeup_write_descriptor = descriptors[1];
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
    int32_t registration_index;
    uint32_t merged_mask;
    uint32_t new_count;
    uint64_t generation;
    int32_t error_code;

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
    registration_index = find_registration(loop, file_descriptor);
    if (registration_index >= 0)
    {
        uint32_t previous_mask;
        int32_t *new_read_descriptors;
        int32_t *new_write_descriptors;
        uint32_t new_read_count;
        uint32_t new_write_count;

        merged_mask = loop->registrations[registration_index].interest_mask
            | interest_mask;
        if (merged_mask == loop->registrations[registration_index].interest_mask)
        {
            event_loop_unlock(loop, FT_TRUE);
            return (FT_ERR_SUCCESS);
        }
        previous_mask = loop->registrations[registration_index].interest_mask;
        loop->registrations[registration_index].interest_mask = merged_mask;
        error_code = build_legacy_arrays(loop->registrations,
            loop->registration_count, &new_read_descriptors, &new_read_count,
            &new_write_descriptors, &new_write_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            loop->registrations[registration_index].interest_mask = previous_mask;
            event_loop_unlock(loop, FT_TRUE);
            return (error_code);
        }
        error_code = backend_add_or_modify(loop, file_descriptor, merged_mask,
            loop->registrations[registration_index].generation, FT_TRUE);
        if (error_code != FT_ERR_SUCCESS)
        {
            loop->registrations[registration_index].interest_mask = previous_mask;
            cma_free(new_read_descriptors);
            cma_free(new_write_descriptors);
        }
        else
        {
            if (loop->read_file_descriptors != ft_nullptr)
                cma_free(loop->read_file_descriptors);
            if (loop->write_file_descriptors != ft_nullptr)
                cma_free(loop->write_file_descriptors);
            loop->read_file_descriptors = new_read_descriptors;
            loop->write_file_descriptors = new_write_descriptors;
            loop->read_count = static_cast<int32_t>(new_read_count);
            loop->write_count = static_cast<int32_t>(new_write_count);
        }
    }
    else
    {
        new_count = loop->registration_count + 1U;
        {
            event_loop_registration *new_registrations;
            event_loop_ready_event *new_ready_events;
            void *new_backend_events;

            new_registrations = static_cast<event_loop_registration *>(
                cma_realloc(loop->registrations,
                    sizeof(event_loop_registration) * new_count));
            if (new_registrations == ft_nullptr)
            {
                event_loop_unlock(loop, FT_TRUE);
                return (FT_ERR_NO_MEMORY);
            }
            new_ready_events = loop->ready_events;
            if (loop->wait_active == FT_FALSE || loop->ready_events == ft_nullptr)
            {
                new_ready_events = static_cast<event_loop_ready_event *>(
                    cma_realloc(loop->ready_events,
                        sizeof(event_loop_ready_event) * new_count));
                if (new_ready_events == ft_nullptr)
                {
                    loop->registrations = new_registrations;
                    event_loop_unlock(loop, FT_TRUE);
                    return (FT_ERR_NO_MEMORY);
                }
            }
#if defined(NETWORKING_USE_EPOLL) || defined(NETWORKING_USE_KQUEUE)
            new_backend_events = loop->backend_events;
            if (loop->wait_active == FT_FALSE)
            {
# ifdef NETWORKING_USE_EPOLL
                new_backend_events = cma_realloc(loop->backend_events,
                    sizeof(struct epoll_event) * new_count);
# else
                new_backend_events = cma_realloc(loop->backend_events,
                    sizeof(struct kevent) * new_count);
# endif
                if (new_backend_events == ft_nullptr)
                {
                    loop->registrations = new_registrations;
                    loop->ready_events = new_ready_events;
                    event_loop_unlock(loop, FT_TRUE);
                    return (FT_ERR_NO_MEMORY);
                }
            }
#else
            new_backend_events = loop->backend_events;
#endif
            loop->registrations = new_registrations;
            loop->ready_events = new_ready_events;
            if (loop->wait_active == FT_FALSE)
                loop->backend_events = new_backend_events;
        }
        if (loop->registrations == ft_nullptr || loop->ready_events == ft_nullptr)
        {
            event_loop_unlock(loop, FT_TRUE);
            return (FT_ERR_NO_MEMORY);
        }
        generation = loop->next_generation + 1U;
        error_code = backend_add_or_modify(loop, file_descriptor,
            interest_mask, generation, FT_FALSE);
        if (error_code == FT_ERR_SUCCESS)
        {
            int32_t *new_read_descriptors;
            int32_t *new_write_descriptors;
            uint32_t new_read_count;
            uint32_t new_write_count;

            loop->registrations[loop->registration_count].file_descriptor
                = file_descriptor;
            loop->registrations[loop->registration_count].interest_mask
                = interest_mask;
            loop->registrations[loop->registration_count].generation
                = generation;
            error_code = build_legacy_arrays(loop->registrations, new_count,
                &new_read_descriptors, &new_read_count,
                &new_write_descriptors, &new_write_count);
            if (error_code != FT_ERR_SUCCESS)
            {
                (void)backend_remove(loop, file_descriptor);
                event_loop_unlock(loop, FT_TRUE);
                return (error_code);
            }
            loop->registration_count = new_count;
            loop->registration_capacity = new_count;
            loop->next_generation = generation;
            if (loop->wait_active == FT_FALSE)
            {
                loop->ready_capacity = new_count;
                loop->backend_event_capacity = new_count;
            }
            if (loop->read_file_descriptors != ft_nullptr)
                cma_free(loop->read_file_descriptors);
            if (loop->write_file_descriptors != ft_nullptr)
                cma_free(loop->write_file_descriptors);
            loop->read_file_descriptors = new_read_descriptors;
            loop->write_file_descriptors = new_write_descriptors;
            loop->read_count = static_cast<int32_t>(new_read_count);
            loop->write_count = static_cast<int32_t>(new_write_count);
        }
    }
    event_loop_unlock(loop, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS)
        signal_loop(loop);
    return (error_code);
}

int32_t event_loop_remove_interest(event_loop *loop, int32_t file_descriptor,
    uint32_t interest_mask) noexcept
{
    int32_t registration_index;
    uint32_t remaining_mask;
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
    remaining_mask = loop->registrations[registration_index].interest_mask
        & ~interest_mask;
    if (remaining_mask != 0U)
    {
        uint32_t previous_mask;
        int32_t *new_read_descriptors;
        int32_t *new_write_descriptors;
        uint32_t new_read_count;
        uint32_t new_write_count;

        previous_mask = loop->registrations[registration_index].interest_mask;
        loop->registrations[registration_index].interest_mask = remaining_mask;
        error_code = build_legacy_arrays(loop->registrations,
            loop->registration_count, &new_read_descriptors, &new_read_count,
            &new_write_descriptors, &new_write_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            loop->registrations[registration_index].interest_mask = previous_mask;
            event_loop_unlock(loop, FT_TRUE);
            return (error_code);
        }
        error_code = backend_add_or_modify(loop, file_descriptor, remaining_mask,
            loop->registrations[registration_index].generation, FT_TRUE);
        if (error_code != FT_ERR_SUCCESS)
        {
            loop->registrations[registration_index].interest_mask = previous_mask;
            cma_free(new_read_descriptors);
            cma_free(new_write_descriptors);
        }
        else
        {
            if (loop->read_file_descriptors != ft_nullptr)
                cma_free(loop->read_file_descriptors);
            if (loop->write_file_descriptors != ft_nullptr)
                cma_free(loop->write_file_descriptors);
            loop->read_file_descriptors = new_read_descriptors;
            loop->write_file_descriptors = new_write_descriptors;
            loop->read_count = static_cast<int32_t>(new_read_count);
            loop->write_count = static_cast<int32_t>(new_write_count);
        }
    }
    else
    {
        event_loop_registration *new_registrations;
        int32_t *new_read_descriptors;
        int32_t *new_write_descriptors;
        uint32_t new_read_count;
        uint32_t new_write_count;
        uint32_t new_count;
        uint32_t source_index;
        uint32_t destination_index;

        new_count = loop->registration_count - 1U;
        new_registrations = ft_nullptr;
        if (new_count > 0U)
        {
            new_registrations = static_cast<event_loop_registration *>(
                cma_malloc(sizeof(event_loop_registration) * new_count));
            if (new_registrations == ft_nullptr)
            {
                event_loop_unlock(loop, FT_TRUE);
                return (FT_ERR_NO_MEMORY);
            }
        }
        source_index = 0U;
        destination_index = 0U;
        while (source_index < loop->registration_count)
        {
            if (source_index != static_cast<uint32_t>(registration_index))
            {
                new_registrations[destination_index] =
                    loop->registrations[source_index];
                destination_index += 1U;
            }
            source_index += 1U;
        }
        error_code = build_legacy_arrays(new_registrations, new_count,
            &new_read_descriptors, &new_read_count,
            &new_write_descriptors, &new_write_count);
        if (error_code != FT_ERR_SUCCESS)
        {
            if (new_registrations != ft_nullptr)
                cma_free(new_registrations);
            event_loop_unlock(loop, FT_TRUE);
            return (error_code);
        }
        error_code = backend_remove(loop, file_descriptor);
        if (error_code == FT_ERR_SUCCESS)
        {
            cma_free(loop->registrations);
            loop->registrations = new_registrations;
            loop->registration_count = new_count;
            if (loop->read_file_descriptors != ft_nullptr)
                cma_free(loop->read_file_descriptors);
            if (loop->write_file_descriptors != ft_nullptr)
                cma_free(loop->write_file_descriptors);
            loop->read_file_descriptors = new_read_descriptors;
            loop->write_file_descriptors = new_write_descriptors;
            loop->read_count = static_cast<int32_t>(new_read_count);
            loop->write_count = static_cast<int32_t>(new_write_count);
        }
        else
        {
            if (new_registrations != ft_nullptr)
                cma_free(new_registrations);
            cma_free(new_read_descriptors);
            cma_free(new_write_descriptors);
        }
    }
    event_loop_unlock(loop, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS)
        signal_loop(loop);
    return (error_code);
}

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
    int32_t *read_snapshot;
    int32_t *write_snapshot;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t read_alloc_count;
    uint32_t write_alloc_count;
    uint32_t poll_read_count;
    int32_t poll_result;
    int32_t poll_timeout;
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
                || loop->registrations[registration_index].generation != generation)
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
                || loop->registrations[registration_index].generation != generation)
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
    read_count = static_cast<uint32_t>(loop->read_count);
    write_count = static_cast<uint32_t>(loop->write_count);
    read_alloc_count = read_count + 1U;
    write_alloc_count = write_count;
    poll_read_count = read_count;
#ifndef _WIN32
    poll_read_count = read_count + 1U;
#endif
    if (read_alloc_count == 0U)
        read_alloc_count = 1U;
    if (write_alloc_count == 0U)
        write_alloc_count = 1U;
    read_snapshot = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
        * read_alloc_count));
    write_snapshot = static_cast<int32_t *>(cma_malloc(sizeof(int32_t)
        * write_alloc_count));
    if (read_snapshot != ft_nullptr && write_snapshot != ft_nullptr)
    {
        index = 0U;
        while (index < read_count)
        {
            read_snapshot[index] = loop->read_file_descriptors[index];
            index += 1U;
        }
        index = 0U;
        while (index < write_count)
        {
            write_snapshot[index] = loop->write_file_descriptors[index];
            index += 1U;
        }
#ifndef _WIN32
        read_snapshot[read_count] = loop->wakeup_read_descriptor;
#endif
    }
    event_loop_unlock(loop, FT_TRUE);
    if (read_snapshot == ft_nullptr || write_snapshot == ft_nullptr)
    {
        if (read_snapshot != ft_nullptr)
            cma_free(read_snapshot);
        if (write_snapshot != ft_nullptr)
            cma_free(write_snapshot);
        release_event_loop_wait(loop);
        return (FT_ERR_NO_MEMORY);
    }
    else
    {
        poll_timeout = timeout_milliseconds;
#ifdef _WIN32
        if (poll_timeout < 0)
            poll_timeout = 50;
#endif
        poll_result = nw_poll(read_snapshot,
            static_cast<int32_t>(poll_read_count), write_snapshot,
            static_cast<int32_t>(write_count), poll_timeout);
    }
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        cma_free(read_snapshot);
        cma_free(write_snapshot);
        release_event_loop_wait(loop);
        return (FT_ERR_THREAD_BUSY);
    }
    if (loop->stopping != FT_FALSE)
    {
        event_loop_unlock(loop, FT_TRUE);
        cma_free(read_snapshot);
        cma_free(write_snapshot);
        release_event_loop_wait(loop);
        return (FT_ERR_INVALID_STATE);
    }
    event_loop_unlock(loop, FT_TRUE);
#ifndef _WIN32
    if (poll_result > 0 && read_snapshot != ft_nullptr
        && read_snapshot[read_count] >= 0)
    {
        if (read_snapshot[read_count] == loop->wakeup_read_descriptor)
            drain_loop_signal(loop);
    }
#endif
    index = 0U;
    while (poll_result > 0 && index < read_count)
    {
        if (read_snapshot[index] >= 0)
            (void)append_event(events, &local_count, event_capacity,
                read_snapshot[index], EVENT_LOOP_READY_READ, FT_ERR_SUCCESS);
        index += 1U;
    }
    index = 0U;
    while (poll_result > 0 && index < write_count)
    {
        if (write_snapshot[index] >= 0)
            (void)append_event(events, &local_count, event_capacity,
                write_snapshot[index], EVENT_LOOP_READY_WRITE,
                FT_ERR_SUCCESS);
        index += 1U;
    }
    if (read_snapshot != ft_nullptr)
        cma_free(read_snapshot);
    if (write_snapshot != ft_nullptr)
        cma_free(write_snapshot);
#endif
    *event_count = local_count;
    release_event_loop_wait(loop);
    return (FT_ERR_SUCCESS);
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
    event_loop_ready_event *events;
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
    events = static_cast<event_loop_ready_event *>(cma_malloc(
        sizeof(event_loop_ready_event) * capacity));
    if (events == ft_nullptr)
        return (-1);
    result_code = event_loop_wait(loop, events, capacity, &event_count,
        timeout_milliseconds);
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
