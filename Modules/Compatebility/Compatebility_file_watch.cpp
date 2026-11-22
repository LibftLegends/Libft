#include "compatebility_internal.hpp"
#include "compatebility_file_watch.hpp"
#include "../Errno/errno.hpp"

#include <atomic>
#include <cerrno>
#include <new>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(__linux__)
# include <poll.h>
# include <sys/eventfd.h>
# include <sys/inotify.h>
# include <unistd.h>
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
# include <fcntl.h>
# include <sys/event.h>
# include <unistd.h>
#endif

#if defined(_WIN32)
# include <windows.h>
#endif

struct cmp_file_watch_context
{
    std::atomic<ft_bool> running{FT_FALSE};
#if defined(__linux__)
    int32_t file_descriptor = -1;
    int32_t stop_descriptor = -1;
    alignas(struct inotify_event) char buffer[4096];
    ft_size_t buffer_offset = 0;
    ft_size_t buffer_size = 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    int32_t kqueue_fd = -1;
    int32_t watch_fd = -1;
#elif defined(_WIN32)
    HANDLE directory = INVALID_HANDLE_VALUE;
    HANDLE stop_event = ft_nullptr;
    HANDLE read_event = ft_nullptr;
    OVERLAPPED read_operation{};
    ft_bool read_pending = FT_FALSE;
    alignas(DWORD) char buffer[4096];
    DWORD buffer_offset = 0;
    DWORD buffer_size = 0;
#endif
};

static int32_t cmp_file_watch_translate_error(void)
{
    int32_t error_code = FT_ERR_IO;

#if defined(_WIN32)
    DWORD last_error = GetLastError();

    if (last_error != 0)
        error_code = cmp_map_system_error_to_ft(static_cast<int32_t>(last_error));
    else
        error_code = FT_ERR_IO;
#else
    if (errno != 0)
        error_code = cmp_map_system_error_to_ft(errno);
#endif
    return (error_code);
}

#if defined(__linux__)
static file_watch_event_type cmp_file_watch_translate_inotify_mask(uint32_t mask)
{
    if (mask & IN_CREATE)
        return (FILE_WATCH_EVENT_CREATE);
    if (mask & IN_DELETE)
        return (FILE_WATCH_EVENT_DELETE);
    return (FILE_WATCH_EVENT_MODIFY);
}
#endif

#if defined(__APPLE__) || defined(__FreeBSD__)
static file_watch_event_type cmp_file_watch_translate_bsd_flags(uint32_t flags)
{
    if (flags & NOTE_DELETE)
        return (FILE_WATCH_EVENT_DELETE);
    return (FILE_WATCH_EVENT_MODIFY);
}
#endif

#if defined(_WIN32)
static file_watch_event_type cmp_file_watch_translate_windows_action(DWORD action)
{
    if (action == FILE_ACTION_ADDED)
        return (FILE_WATCH_EVENT_CREATE);
    if (action == FILE_ACTION_REMOVED)
        return (FILE_WATCH_EVENT_DELETE);
    return (FILE_WATCH_EVENT_MODIFY);
}
#endif

#if defined(_WIN32)
static int32_t cmp_file_watch_issue_read(cmp_file_watch_context &context)
{
    context.read_operation = {};
    context.read_operation.hEvent = context.read_event;
    ResetEvent(context.read_event);
    if (ReadDirectoryChangesW(context.directory, context.buffer,
            static_cast<DWORD>(sizeof(context.buffer)), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            ft_nullptr, &context.read_operation, ft_nullptr) == FALSE
        && GetLastError() != ERROR_IO_PENDING)
        return (cmp_file_watch_translate_error());
    context.read_pending = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

static void cmp_file_watch_cancel_read(cmp_file_watch_context &context)
{
    if (context.read_pending == FT_TRUE)
    {
        (void)CancelIoEx(context.directory, &context.read_operation);
        DWORD transferred = 0;
        (void)GetOverlappedResult(context.directory, &context.read_operation, &transferred, TRUE);
        context.read_pending = FT_FALSE;
    }
    return ;
}
#endif

cmp_file_watch_context *cmp_file_watch_create(void)
{
    return (new (std::nothrow) cmp_file_watch_context());
}

void cmp_file_watch_destroy(cmp_file_watch_context *context)
{
    cmp_file_watch_stop(context);
    delete context;
    return ;
}

int32_t cmp_file_watch_start(cmp_file_watch_context *context, const char *path)
{
    if (context == ft_nullptr || path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    cmp_file_watch_stop(context);
#if defined(__linux__)
    context->file_descriptor = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (context->file_descriptor < 0)
        return (cmp_file_watch_translate_error());
    context->stop_descriptor = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (context->stop_descriptor < 0 || inotify_add_watch(context->file_descriptor,
            path, IN_CREATE | IN_MODIFY | IN_DELETE) < 0)
    {
        const int32_t error_code = cmp_file_watch_translate_error();
        cmp_file_watch_stop(context);
        return (error_code);
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
#if defined(__APPLE__)
    context->watch_fd = open(path, O_EVTONLY);
#else
    context->watch_fd = open(path, O_RDONLY | O_CLOEXEC);
#endif
    if (context->watch_fd < 0)
        return (cmp_file_watch_translate_error());
    context->kqueue_fd = kqueue();
    if (context->kqueue_fd < 0)
    {
        const int32_t error_code = cmp_file_watch_translate_error();
        cmp_file_watch_stop(context);
        return (error_code);
    }
    struct kevent configuration[2];
    EV_SET(&configuration[0], context->watch_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
        NOTE_WRITE | NOTE_EXTEND | NOTE_DELETE | NOTE_RENAME, 0, ft_nullptr);
    EV_SET(&configuration[1], 1, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, ft_nullptr);
    if (kevent(context->kqueue_fd, configuration, 2, ft_nullptr, 0, ft_nullptr) < 0)
    {
        const int32_t error_code = cmp_file_watch_translate_error();
        cmp_file_watch_stop(context);
        return (error_code);
    }
#elif defined(_WIN32)
    context->directory = CreateFileA(path, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, ft_nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, ft_nullptr);
    if (context->directory == INVALID_HANDLE_VALUE)
        return (cmp_file_watch_translate_error());
    context->stop_event = CreateEventW(ft_nullptr, TRUE, FALSE, ft_nullptr);
    if (context->stop_event == ft_nullptr)
    {
        const int32_t error_code = cmp_file_watch_translate_error();
        cmp_file_watch_stop(context);
        return (error_code);
    }
    context->read_event = CreateEventW(ft_nullptr, TRUE, FALSE, ft_nullptr);
    if (context->read_event == ft_nullptr)
    {
        const int32_t error_code = cmp_file_watch_translate_error();
        cmp_file_watch_stop(context);
        return (error_code);
    }
    const int32_t read_error = cmp_file_watch_issue_read(*context);
    if (read_error != FT_ERR_SUCCESS)
    {
        cmp_file_watch_stop(context);
        return (read_error);
    }
#endif
    context->running.store(FT_TRUE);
    return (FT_ERR_SUCCESS);
}

void cmp_file_watch_request_stop(cmp_file_watch_context *context)
{
    if (context == ft_nullptr || context->running.exchange(FT_FALSE) == FT_FALSE)
        return ;
#if defined(__linux__)
    const uint64_t wake = 1;
    while (::write(context->stop_descriptor, &wake, sizeof(wake)) < 0 && errno == EINTR)
        continue ;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    struct kevent stop_event;
    EV_SET(&stop_event, 1, EVFILT_USER, 0, NOTE_TRIGGER, 0, ft_nullptr);
    while (kevent(context->kqueue_fd, &stop_event, 1, ft_nullptr, 0, ft_nullptr) < 0
        && errno == EINTR)
        continue ;
#elif defined(_WIN32)
    (void)SetEvent(context->stop_event);
#endif
    return ;
}

void cmp_file_watch_stop(cmp_file_watch_context *context)
{
    if (context == ft_nullptr)
        return ;
    context->running.store(FT_FALSE);
#if defined(__linux__)
    if (context->file_descriptor >= 0)
        close(context->file_descriptor);
    if (context->stop_descriptor >= 0)
        close(context->stop_descriptor);
    context->file_descriptor = -1;
    context->stop_descriptor = -1;
    context->buffer_offset = 0;
    context->buffer_size = 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    if (context->watch_fd >= 0)
        close(context->watch_fd);
    if (context->kqueue_fd >= 0)
        close(context->kqueue_fd);
    context->watch_fd = -1;
    context->kqueue_fd = -1;
#elif defined(_WIN32)
    cmp_file_watch_cancel_read(*context);
    if (context->directory != INVALID_HANDLE_VALUE)
        CloseHandle(context->directory);
    if (context->stop_event != ft_nullptr)
        CloseHandle(context->stop_event);
    if (context->read_event != ft_nullptr)
        CloseHandle(context->read_event);
    context->directory = INVALID_HANDLE_VALUE;
    context->stop_event = ft_nullptr;
    context->read_event = ft_nullptr;
    context->buffer_offset = 0;
    context->buffer_size = 0;
#endif
    return ;
}

#if defined(__linux__)
static ft_bool cmp_file_watch_receive(cmp_file_watch_context &context)
{
    struct pollfd descriptors[2] = {
        {context.stop_descriptor, POLLIN, 0},
        {context.file_descriptor, POLLIN, 0}
    };
    while (context.running.load() == FT_TRUE)
    {
        const int32_t ready = poll(descriptors, 2, -1);
        if (ready < 0 && errno == EINTR)
            continue ;
        if (ready <= 0 || descriptors[0].revents != 0
            || (descriptors[1].revents & POLLIN) == 0)
            return (FT_FALSE);
        const ssize_t count = ::read(context.file_descriptor, context.buffer, sizeof(context.buffer));
        if (count < 0 && (errno == EAGAIN || errno == EINTR))
            continue ;
        if (count <= 0)
            return (FT_FALSE);
        context.buffer_size = static_cast<ft_size_t>(count);
        context.buffer_offset = 0;
        return (FT_TRUE);
    }
    return (FT_FALSE);
}
#elif defined(_WIN32)
static ft_bool cmp_file_watch_receive(cmp_file_watch_context &context)
{
    if (context.read_pending == FT_FALSE && cmp_file_watch_issue_read(context) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    HANDLE events[2] = {context.stop_event, context.read_event};
    if (WaitForMultipleObjects(2, events, FALSE, INFINITE) != WAIT_OBJECT_0 + 1)
    {
        cmp_file_watch_cancel_read(context);
        return (FT_FALSE);
    }
    DWORD transferred = 0;
    const ft_bool success = GetOverlappedResult(context.directory, &context.read_operation,
        &transferred, FALSE) != FALSE;
    context.read_pending = FT_FALSE;
    if (success == FT_FALSE)
        return (FT_FALSE);
    context.buffer_size = transferred;
    context.buffer_offset = 0;
    return (FT_TRUE);
}
#endif

ft_bool cmp_file_watch_wait_event(cmp_file_watch_context *context, cmp_file_watch_event *event)
{
    if (context == ft_nullptr || event == ft_nullptr)
        return (FT_FALSE);
    while (context->running.load() == FT_TRUE)
    {
#if defined(__linux__)
        if (context->buffer_offset == context->buffer_size && cmp_file_watch_receive(*context) == FT_FALSE)
            break ;
        const auto *notification = reinterpret_cast<const struct inotify_event *>(
            context->buffer + context->buffer_offset);
        context->buffer_offset += sizeof(*notification) + static_cast<ft_size_t>(notification->len);
        event->event_type = cmp_file_watch_translate_inotify_mask(notification->mask);
        event->has_name = notification->len > 0;
        ft_size_t name_length = notification->len;
        if (name_length >= sizeof(event->name))
            name_length = sizeof(event->name) - 1;
        if (name_length > 0)
            ft_memcpy(event->name, notification->name, name_length);
        event->name[name_length] = '\0';
        return (FT_TRUE);
#elif defined(__APPLE__) || defined(__FreeBSD__)
        struct kevent observed_event;
        const int32_t count = kevent(context->kqueue_fd, ft_nullptr, 0, &observed_event, 1, ft_nullptr);
        if (count < 0 && errno == EINTR)
            continue ;
        if (count <= 0 || observed_event.filter == EVFILT_USER)
            break ;
        event->event_type = cmp_file_watch_translate_bsd_flags(observed_event.fflags);
        event->has_name = FT_FALSE;
        event->name[0] = '\0';
        return (FT_TRUE);
#elif defined(_WIN32)
        if (context->buffer_offset == context->buffer_size && cmp_file_watch_receive(*context) == FT_FALSE)
            break ;
        if (context->buffer_size == 0)
            continue ;
        const auto *notification = reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(
            context->buffer + context->buffer_offset);
        if (notification->NextEntryOffset == 0)
            context->buffer_offset = context->buffer_size;
        else
            context->buffer_offset += notification->NextEntryOffset;
        event->event_type = cmp_file_watch_translate_windows_action(notification->Action);
        int32_t name_length = 0;
        if (notification->FileNameLength > 0)
            name_length = WideCharToMultiByte(CP_UTF8, 0, notification->FileName,
                static_cast<int32_t>(notification->FileNameLength / sizeof(WCHAR)), event->name,
                static_cast<int32_t>(sizeof(event->name) - 1), ft_nullptr, ft_nullptr);
        event->has_name = name_length > 0;
        event->name[name_length] = '\0';
        return (FT_TRUE);
#else
        break ;
#endif
    }
    context->running.store(FT_FALSE);
    return (FT_FALSE);
}
