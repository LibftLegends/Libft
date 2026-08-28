#include <cerrno>
#include <cstdlib>

#include "networking.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"

#ifdef NETWORKING_USE_EPOLL
# include <sys/epoll.h>
# include <sys/eventfd.h>
# include <unistd.h>
#else
# include <fcntl.h>
# include <unistd.h>
#endif

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

static void sync_legacy_arrays(event_loop *loop)
{
    uint32_t read_count;
    uint32_t write_count;
    int32_t *read_descriptors;
    int32_t *write_descriptors;
    uint32_t index;

    read_count = 0U;
    write_count = 0U;
    index = 0U;
    while (index < loop->registration_count)
    {
        if ((loop->registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_READ) != 0U)
            read_count += 1U;
        if ((loop->registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_WRITE) != 0U)
            write_count += 1U;
        index += 1U;
    }
    if (read_count == 0U)
        read_count = 1U;
    if (write_count == 0U)
        write_count = 1U;
    read_descriptors = static_cast<int32_t *>(cma_malloc(
        sizeof(int32_t) * read_count));
    write_descriptors = static_cast<int32_t *>(cma_malloc(
        sizeof(int32_t) * write_count));
    if (read_descriptors == ft_nullptr || write_descriptors == ft_nullptr)
    {
        if (read_descriptors != ft_nullptr)
            cma_free(read_descriptors);
        if (write_descriptors != ft_nullptr)
            cma_free(write_descriptors);
        return ;
    }
    read_count = 0U;
    write_count = 0U;
    index = 0U;
    while (index < loop->registration_count)
    {
        if ((loop->registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_READ) != 0U)
            read_descriptors[read_count++] = loop->registrations[index].file_descriptor;
        if ((loop->registrations[index].interest_mask
                & EVENT_LOOP_INTEREST_WRITE) != 0U)
            write_descriptors[write_count++] = loop->registrations[index].file_descriptor;
        index += 1U;
    }
    if (loop->read_file_descriptors != ft_nullptr)
        cma_free(loop->read_file_descriptors);
    if (loop->write_file_descriptors != ft_nullptr)
        cma_free(loop->write_file_descriptors);
    loop->read_file_descriptors = read_descriptors;
    loop->write_file_descriptors = write_descriptors;
    loop->read_count = static_cast<int32_t>(read_count);
    loop->write_count = static_cast<int32_t>(write_count);
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
#else
    (void)loop;
    (void)file_descriptor;
#endif
    return (FT_ERR_SUCCESS);
}

static void signal_loop(event_loop *loop)
{
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
}

static void drain_loop_signal(event_loop *loop)
{
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
        close(loop->backend_descriptor);
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
            close(loop->wakeup_read_descriptor);
            close(loop->backend_descriptor);
            loop->wakeup_read_descriptor = -1;
            loop->wakeup_write_descriptor = -1;
            loop->backend_descriptor = -1;
            free_mutex(&loop->wait_mutex);
            free_mutex(&loop->mutex);
            return ;
        }
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
        close(loop->wakeup_read_descriptor);
#ifndef NETWORKING_USE_EPOLL
    if (loop->wakeup_write_descriptor >= 0)
        close(loop->wakeup_write_descriptor);
#endif
    if (loop->backend_descriptor >= 0)
        close(loop->backend_descriptor);
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
        merged_mask = loop->registrations[registration_index].interest_mask
            | interest_mask;
        if (merged_mask == loop->registrations[registration_index].interest_mask)
        {
            event_loop_unlock(loop, FT_TRUE);
            return (FT_ERR_SUCCESS);
        }
        error_code = backend_add_or_modify(loop, file_descriptor, merged_mask,
            loop->registrations[registration_index].generation, FT_TRUE);
        if (error_code == FT_ERR_SUCCESS)
        {
            loop->registrations[registration_index].interest_mask = merged_mask;
            sync_legacy_arrays(loop);
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
            new_ready_events = static_cast<event_loop_ready_event *>(
                cma_realloc(loop->ready_events,
                    sizeof(event_loop_ready_event) * new_count));
            if (new_ready_events == ft_nullptr)
            {
                loop->registrations = new_registrations;
                event_loop_unlock(loop, FT_TRUE);
                return (FT_ERR_NO_MEMORY);
            }
#ifdef NETWORKING_USE_EPOLL
            new_backend_events = cma_realloc(loop->backend_events,
                sizeof(struct epoll_event) * new_count);
            if (new_backend_events == ft_nullptr)
            {
                loop->registrations = new_registrations;
                loop->ready_events = new_ready_events;
                event_loop_unlock(loop, FT_TRUE);
                return (FT_ERR_NO_MEMORY);
            }
#else
            new_backend_events = loop->backend_events;
#endif
            loop->registrations = new_registrations;
            loop->ready_events = new_ready_events;
            loop->backend_events = new_backend_events;
        }
        if (loop->registrations == ft_nullptr || loop->ready_events == ft_nullptr)
        {
            event_loop_unlock(loop, FT_TRUE);
            return (FT_ERR_NO_MEMORY);
        }
        generation = loop->next_generation + 1U;
        loop->next_generation = generation;
        error_code = backend_add_or_modify(loop, file_descriptor,
            interest_mask, generation, FT_FALSE);
        if (error_code == FT_ERR_SUCCESS)
        {
            loop->registrations[loop->registration_count].file_descriptor
                = file_descriptor;
            loop->registrations[loop->registration_count].interest_mask
                = interest_mask;
            loop->registrations[loop->registration_count].generation
                = generation;
            loop->registration_count = new_count;
            loop->registration_capacity = new_count;
            loop->ready_capacity = new_count;
            loop->backend_event_capacity = new_count;
            sync_legacy_arrays(loop);
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
    if (remaining_mask == 0U)
        error_code = backend_remove(loop, file_descriptor);
    else
        error_code = backend_add_or_modify(loop, file_descriptor, remaining_mask,
            loop->registrations[registration_index].generation, FT_TRUE);
    if (error_code == FT_ERR_SUCCESS)
    {
        if (remaining_mask != 0U)
            loop->registrations[registration_index].interest_mask = remaining_mask;
        else
        {
            while (static_cast<uint32_t>(registration_index) + 1U
                < loop->registration_count)
            {
                loop->registrations[registration_index]
                    = loop->registrations[registration_index + 1];
                registration_index += 1;
            }
            loop->registration_count -= 1U;
            sync_legacy_arrays(loop);
        }
        signal_loop(loop);
    }
    event_loop_unlock(loop, FT_TRUE);
    return (error_code);
}

int32_t event_loop_wait(event_loop *loop, event_loop_ready_event *events,
    uint32_t event_capacity, uint32_t *event_count,
    int32_t timeout_milliseconds) noexcept
{
    uint32_t registration_count;
    uint32_t local_count;
    uint32_t index;
#ifdef NETWORKING_USE_EPOLL
    struct epoll_event *backend_events;
    int32_t ready_count;
    uint64_t token;
    int32_t file_descriptor;
    uint32_t backend_mask;
    uint64_t generation;
    int32_t registration_index;
    struct epoll_event empty_backend_event;
#else
    int32_t *read_snapshot;
    int32_t *write_snapshot;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t read_alloc_count;
    uint32_t write_alloc_count;
    uint32_t poll_read_count;
    int32_t poll_result;
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
    event_loop_unlock(loop, FT_TRUE);
    local_count = 0U;
#ifdef NETWORKING_USE_EPOLL
    if (registration_count == 0U)
        registration_count = 1U;
    drain_loop_signal(loop);
    backend_events = static_cast<struct epoll_event *>(loop->backend_events);
    if (loop->registration_count == 0U)
    {
        ft_memset(&empty_backend_event, 0, sizeof(empty_backend_event));
        backend_events = &empty_backend_event;
    }
    else if (backend_events == ft_nullptr
        || loop->backend_event_capacity < registration_count)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_NO_MEMORY);
    }
    ready_count = epoll_wait(loop->backend_descriptor, backend_events,
        static_cast<int32_t>(registration_count), timeout_milliseconds);
    if (ready_count < 0 && errno != EINTR)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    index = 0U;
    if (ready_count < 0)
        ready_count = 0;
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
    event_loop_unlock(loop, FT_TRUE);
    while (index < static_cast<uint32_t>(ready_count))
    {
        token = backend_events[index].data.u64;
        if (token == UINT64_MAX)
            drain_loop_signal(loop);
        else
        {
            generation = token >> 32U;
            file_descriptor = static_cast<int32_t>(token & UINT32_MAX);
            if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
                continue;
            registration_index = find_registration(loop, file_descriptor);
            if (registration_index < 0
                || loop->registrations[registration_index].generation != generation)
            {
                event_loop_unlock(loop, FT_TRUE);
                continue;
            }
            event_loop_unlock(loop, FT_TRUE);
            backend_mask = backend_events[index].events;
            if ((backend_mask & EPOLLIN) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_READ,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_mask & EPOLLOUT) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_WRITE,
                        FT_ERR_SUCCESS) != FT_ERR_SUCCESS)
                {
                    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
                    return (FT_ERR_NO_MEMORY);
                }
            }
            if ((backend_mask & (EPOLLHUP | EPOLLERR)) != 0U)
            {
                if (append_event(events, &local_count, event_capacity,
                        file_descriptor, EVENT_LOOP_READY_ERROR,
                        FT_ERR_INVALID_STATE) != FT_ERR_SUCCESS)
                {
                    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
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
                    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
                    return (FT_ERR_NO_MEMORY);
                }
            }
#endif
        }
        index += 1U;
    }
#else
    if (event_loop_lock(loop, ft_nullptr) != FT_ERR_SUCCESS)
    {
        (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
        return (FT_ERR_THREAD_BUSY);
    }
    read_count = static_cast<uint32_t>(loop->read_count);
    write_count = static_cast<uint32_t>(loop->write_count);
    read_alloc_count = read_count;
    write_alloc_count = write_count;
    poll_read_count = read_count + 1U;
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
        read_snapshot[read_count] = loop->wakeup_read_descriptor;
    }
    event_loop_unlock(loop, FT_TRUE);
    if (read_snapshot == ft_nullptr || write_snapshot == ft_nullptr)
        poll_result = -1;
    else
        poll_result = nw_poll(read_snapshot, static_cast<int32_t>(poll_read_count),
            write_snapshot, static_cast<int32_t>(write_count),
            timeout_milliseconds);
    if (poll_result > 0 && read_snapshot != ft_nullptr
        && read_snapshot[read_count] >= 0)
    {
        if (read_snapshot[read_count] == loop->wakeup_read_descriptor)
            drain_loop_signal(loop);
    }
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
    (void)pt_mutex_unlock_if_not_null(loop->wait_mutex);
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
    uint32_t event_count;
    uint32_t capacity;

    if (loop == ft_nullptr)
        return (-1);
    if (loop->ready_events == ft_nullptr && loop->registration_count == 0U)
        return (0);
    capacity = loop->ready_capacity;
    if (capacity == 0U)
        capacity = 1U;
    if (event_loop_wait(loop, loop->ready_events, capacity, &event_count,
            timeout_milliseconds) != FT_ERR_SUCCESS)
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
