#ifndef FILE_WATCH_HPP
#define FILE_WATCH_HPP

#include "../Errno/errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"

#include "../Compatebility/compatebility_file_watch.hpp"
#include "../Threading/thread.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "file_watch_event.hpp"
#include <atomic>

typedef void (*file_watch_callback)(const char *path, file_watch_event_type event_type, void *user_data);
typedef void (*file_watch_legacy_callback)(const char *path, int32_t event_type, void *user_data);

class ft_file_watch
{
#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_string _path;
        file_watch_legacy_callback _callback;
        file_watch_callback _typed_callback;
        void *_user_data;
        ft_thread _thread;
        std::atomic<ft_bool> _running;
        std::atomic<ft_bool> _stopped;
        std::atomic<uint32_t> _debounce_milliseconds;
        mutable pt_recursive_mutex *_mutex;
        void event_loop();
        int32_t watch_directory_internal(const char *path,
            file_watch_legacy_callback legacy_callback,
            file_watch_callback typed_callback, void *user_data);
        ft_bool snapshot_callback(file_watch_legacy_callback *legacy_callback,
            file_watch_callback *typed_callback, void *&user_data,
            ft_string *path_snapshot, uint32_t *debounce_milliseconds) const;
        cmp_file_watch_context *_state;
        uint8_t _initialised_state;

    public:
        ft_file_watch();
        ft_file_watch(const ft_file_watch &other) = delete;
        ft_file_watch(ft_file_watch &&other) = delete;
        ~ft_file_watch();

        ft_file_watch &operator=(const ft_file_watch &) = delete;
        ft_file_watch &operator=(ft_file_watch &&) = delete;

        int32_t initialize();
        int32_t initialize(const ft_file_watch &other);
        int32_t initialize(ft_file_watch &&other);
        int32_t destroy();
        uint32_t move(ft_file_watch &other);
        int32_t enable_thread_safety();
        int32_t disable_thread_safety();
        ft_bool is_thread_safe() const;
        int32_t watch_directory(const char *path, file_watch_legacy_callback callback, void *user_data);
        int32_t watch_directory(const char *path, file_watch_callback callback, void *user_data);
        int32_t set_debounce_milliseconds(uint32_t debounce_milliseconds);
        uint32_t get_debounce_milliseconds() const;
        void stop();
};

#endif
