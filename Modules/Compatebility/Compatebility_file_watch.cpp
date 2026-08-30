#include "compatebility_internal.hpp"
#include "compatebility_file_watch.hpp"
#include "../Errno/errno.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <mutex>
#include <thread>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(__linux__)
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
    std::atomic<ft_bool>    running;
    std::mutex              mutex;
#if defined(__linux__)
    int32_t                  file_descriptor;
    int32_t                  watch_descriptor;
    char                 buffer[4096];
    ft_size_t               buffer_offset;
    ft_size_t               buffer_size;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    int32_t                  kqueue_fd;
    int32_t                  watch_fd;
#elif defined(_WIN32)
    HANDLE               directory;
    char                 buffer[4096];
    DWORD                buffer_offset;
    DWORD                buffer_size;
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

cmp_file_watch_context *cmp_file_watch_create(void)
{
    cmp_file_watch_context *context = new cmp_file_watch_context();

    context->running.store(FT_FALSE);
#if defined(__linux__)
    context->file_descriptor = -1;
    context->watch_descriptor = -1;
    context->buffer_offset = 0;
    context->buffer_size = 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    context->kqueue_fd = -1;
    context->watch_fd = -1;
#elif defined(_WIN32)
    context->directory = INVALID_HANDLE_VALUE;
    context->buffer_offset = 0;
    context->buffer_size = 0;
#endif
    return (context);
}

void cmp_file_watch_destroy(cmp_file_watch_context *context)
{
    if (!context)
        return ;
    cmp_file_watch_stop(context);
    delete context;
    return ;
}

int32_t cmp_file_watch_start(cmp_file_watch_context *context, const char *path)
{
    int32_t error_code;

    if (!context || !path)
        return (FT_ERR_INVALID_ARGUMENT);
    cmp_file_watch_stop(context);
#if defined(__linux__)
    context->file_descriptor = inotify_init1(IN_NONBLOCK);
    if (context->file_descriptor < 0)
    {
        error_code = cmp_file_watch_translate_error();
        return (error_code);
    }
    context->watch_descriptor = inotify_add_watch(context->file_descriptor, path, IN_CREATE | IN_MODIFY | IN_DELETE);
    if (context->watch_descriptor < 0)
    {
        error_code = cmp_file_watch_translate_error();
        close(context->file_descriptor);
        context->file_descriptor = -1;
        return (error_code);
    }
    context->buffer_offset = 0;
    context->buffer_size = 0;
#elif defined(__APPLE__) || defined(__FreeBSD__)
    context->watch_fd = open(path, O_EVTONLY);
    if (context->watch_fd < 0)
    {
        error_code = cmp_file_watch_translate_error();
        return (error_code);
    }
    context->kqueue_fd = kqueue();
    if (context->kqueue_fd < 0)
    {
        error_code = cmp_file_watch_translate_error();
        close(context->watch_fd);
        context->watch_fd = -1;
        return (error_code);
    }
    struct kevent configuration;

    EV_SET(&configuration, context->watch_fd, EVFILT_VNODE, EV_ADD | EV_CLEAR,
        NOTE_WRITE | NOTE_EXTEND | NOTE_DELETE | NOTE_RENAME, 0, ft_nullptr);
    if (kevent(context->kqueue_fd, &configuration, 1, ft_nullptr, 0, ft_nullptr) < 0)
    {
        error_code = cmp_file_watch_translate_error();
        close(context->kqueue_fd);
        context->kqueue_fd = -1;
        close(context->watch_fd);
        context->watch_fd = -1;
        return (error_code);
    }
#elif defined(_WIN32)
    context->directory = CreateFileA(path, FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        ft_nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, ft_nullptr);
    if (context->directory == INVALID_HANDLE_VALUE)
    {
        error_code = cmp_file_watch_translate_error();
        return (error_code);
    }
    context->buffer_offset = 0;
    context->buffer_size = 0;
#endif
    context->running.store(FT_TRUE);
    return (FT_ERR_SUCCESS);
}

void cmp_file_watch_stop(cmp_file_watch_context *context)
{
    if (!context)
        return ;
    std::lock_guard<std::mutex> lock(context->mutex);
    context->running.store(FT_FALSE);
#if defined(__linux__)
    if (context->watch_descriptor >= 0 && context->file_descriptor >= 0)
        inotify_rm_watch(context->file_descriptor, context->watch_descriptor);
    if (context->file_descriptor >= 0)
        close(context->file_descriptor);
    context->watch_descriptor = -1;
    context->file_descriptor = -1;
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
    if (context->directory != INVALID_HANDLE_VALUE)
    {
        CancelIo(context->directory);
        CloseHandle(context->directory);
    }
    context->directory = INVALID_HANDLE_VALUE;
    context->buffer_offset = 0;
    context->buffer_size = 0;
#endif
    return ;
}

ft_bool cmp_file_watch_wait_event(cmp_file_watch_context *context,
    cmp_file_watch_event *event)
{
    if (!context || !event)
        return (FT_FALSE);
    if (!context->running.load())
        return (FT_FALSE);
#if defined(__linux__)
    while (context->running.load())
    {
        std::unique_lock<std::mutex> lock(context->mutex);

        if (!context->running.load())
            return (FT_FALSE);
        if (context->buffer_offset < context->buffer_size)
        {
            const struct inotify_event *notify_event =
                reinterpret_cast<const struct inotify_event *>(
                    context->buffer + context->buffer_offset);
            ft_size_t entry_size = sizeof(struct inotify_event) + static_cast<ft_size_t>(notify_event->len);

            if (entry_size > 0)
                context->buffer_offset += entry_size;
            else
                context->buffer_offset += sizeof(struct inotify_event);
            event->event_type = cmp_file_watch_translate_inotify_mask(notify_event->mask);
            event->has_name = (notify_event->len > 0);
            if (event->has_name)
            {
                ft_size_t name_length = notify_event->len;
                if (name_length >= sizeof(event->name))
                    name_length = sizeof(event->name) - 1;
                std::memcpy(event->name, notify_event->name, name_length);
                event->name[name_length] = '\0';
            }
            else
            {
                event->name[0] = '\0';
            }
            return (FT_TRUE);
        }
        int64_t read_result = ::read(context->file_descriptor, context->buffer,
                sizeof(context->buffer));
        if (read_result < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            lock.unlock();
            std::this_thread::yield();
            continue ;
        }
        if (read_result <= 0)
        {
            context->running.store(FT_FALSE);
            return (FT_FALSE);
        }
        context->buffer_size = static_cast<ft_size_t>(read_result);
        context->buffer_offset = 0;
    }
#elif defined(__APPLE__) || defined(__FreeBSD__)
    while (context->running.load())
    {
        struct kevent observed_event;
        int32_t event_count = kevent(context->kqueue_fd, ft_nullptr, 0, &observed_event, 1, ft_nullptr);
        if (event_count <= 0)
        {
            context->running.store(FT_FALSE);
            return (FT_FALSE);
        }
        event->event_type = cmp_file_watch_translate_bsd_flags(observed_event.fflags);
        event->has_name = FT_FALSE;
        event->name[0] = '\0';
        return (FT_TRUE);
    }
#elif defined(_WIN32)
    while (context->running.load())
    {
        if (context->buffer_offset < context->buffer_size)
        {
            FILE_NOTIFY_INFORMATION *notification =
                reinterpret_cast<FILE_NOTIFY_INFORMATION *>(context->buffer + context->buffer_offset);
            DWORD next_offset = notification->NextEntryOffset;
            if (next_offset == 0)
                next_offset = context->buffer_size - context->buffer_offset;
            context->buffer_offset += next_offset;
            event->event_type = cmp_file_watch_translate_windows_action(notification->Action);
            if (notification->FileNameLength > 0)
            {
                int32_t converted = WideCharToMultiByte(CP_UTF8, 0, notification->FileName,
                    static_cast<int32_t>(notification->FileNameLength / sizeof(WCHAR)), event->name,
                    static_cast<int32_t>(sizeof(event->name) - 1), ft_nullptr, ft_nullptr);
                if (converted > 0)
                {
                    event->name[converted] = '\0';
                    event->has_name = FT_TRUE;
                }
                else
                {
                    event->name[0] = '\0';
                    event->has_name = FT_FALSE;
                }
            }
            else
            {
                event->name[0] = '\0';
                event->has_name = FT_FALSE;
            }
            return (FT_TRUE);
        }
        DWORD bytes_returned = 0;
        BOOL success = ReadDirectoryChangesW(context->directory, context->buffer,
            static_cast<DWORD>(sizeof(context->buffer)), FALSE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE,
            &bytes_returned, ft_nullptr, ft_nullptr);
        if (success == FALSE)
        {
            context->running.store(FT_FALSE);
            return (FT_FALSE);
        }
        if (bytes_returned == 0)
            continue ;
        context->buffer_size = bytes_returned;
        context->buffer_offset = 0;
    }
#endif
    return (FT_FALSE);
}
