#include "file_watch.hpp"
#include "../Compatebility/compatebility_file_watch.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Template/move.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno_internal.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "file_watch_event.hpp"

static thread_local ft_file_watch *g_file_watch_worker = ft_nullptr;

ft_file_watch::ft_file_watch()
    : _path(), _callback(ft_nullptr), _typed_callback(ft_nullptr),
    _user_data(ft_nullptr), _thread(), _running(FT_FALSE), _stopped(FT_TRUE),
    _debounce_milliseconds(0), _mutex(ft_nullptr), _state(ft_nullptr),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_file_watch::~ft_file_watch()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t ft_file_watch::initialize()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_file_watch::initialize",
            "initialize called while already initialised");
    if (this->_path.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    this->_callback = ft_nullptr;
    this->_typed_callback = ft_nullptr;
    this->_user_data = ft_nullptr;
    this->_running = FT_FALSE;
    this->_stopped = FT_TRUE;
    this->_debounce_milliseconds = 0;
    this->_state = cmp_file_watch_create();
    if (this->_state == ft_nullptr)
    {
        (void)this->_path.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_file_watch::initialize(const ft_file_watch &other)
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_file_watch::initialize(const ft_file_watch &) source",
            "source is not initialised");
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    this->_callback = other._callback;
    this->_typed_callback = other._typed_callback;
    this->_user_data = other._user_data;
    this->_debounce_milliseconds.store(other._debounce_milliseconds.load());
    this->_path = other._path;
    return (FT_ERR_SUCCESS);
}

int32_t ft_file_watch::initialize(ft_file_watch &&other)
{
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state,
            "ft_file_watch::initialize(ft_file_watch &&) source",
            "source is not initialised");
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    if (this->initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    this->_callback = other._callback;
    this->_typed_callback = other._typed_callback;
    this->_user_data = other._user_data;
    this->_debounce_milliseconds.store(other._debounce_milliseconds.load());
    this->_path = other._path;
    other._callback = ft_nullptr;
    other._typed_callback = ft_nullptr;
    other._user_data = ft_nullptr;
    other._debounce_milliseconds = 0;
    other._path.clear();
    return (FT_ERR_SUCCESS);
}

int32_t ft_file_watch::destroy()
{
    int32_t disable_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    if (g_file_watch_worker == this)
        return (FT_ERR_INVALID_OPERATION);
    this->stop();
    if (this->_stopped.load() == FT_FALSE)
        return (FT_ERR_INVALID_STATE);
    if (this->_state != ft_nullptr)
    {
        cmp_file_watch_destroy(this->_state);
        this->_state = ft_nullptr;
    }
    disable_error = this->disable_thread_safety();
    const int32_t destroy_error = this->_path.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (destroy_error);
}

uint32_t ft_file_watch::move(ft_file_watch &other)
{
    return (static_cast<uint32_t>(this->initialize(ft_move(other))));
}

int32_t ft_file_watch::enable_thread_safety()
{
    pt_recursive_mutex *mutex_pointer;
    void *memory_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    memory_pointer = cma_malloc(sizeof(pt_recursive_mutex));
    if (memory_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    mutex_pointer = new(memory_pointer) pt_recursive_mutex();
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        mutex_pointer->~pt_recursive_mutex();
        cma_free(memory_pointer);
        return (initialize_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_file_watch::disable_thread_safety()
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::disable_thread_safety");
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    this->_mutex->~pt_recursive_mutex();
    cma_free(this->_mutex);
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_file_watch::is_thread_safe() const
{
    return (this->_mutex != ft_nullptr);
}

int32_t ft_file_watch::set_debounce_milliseconds(uint32_t debounce_milliseconds)
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::set_debounce_milliseconds");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    this->_debounce_milliseconds = debounce_milliseconds;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

uint32_t ft_file_watch::get_debounce_milliseconds() const
{
    uint32_t debounce_milliseconds;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::get_debounce_milliseconds");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    debounce_milliseconds = this->_debounce_milliseconds;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (debounce_milliseconds);
}

int32_t ft_file_watch::watch_directory(const char *path,
    file_watch_legacy_callback callback, void *user_data)
{
    return (this->watch_directory_internal(path, callback, ft_nullptr, user_data));
}

int32_t ft_file_watch::watch_directory(const char *path,
    file_watch_callback callback, void *user_data)
{
    return (this->watch_directory_internal(path, ft_nullptr, callback, user_data));
}

int32_t ft_file_watch::watch_directory_internal(const char *path,
    file_watch_legacy_callback legacy_callback, file_watch_callback typed_callback,
    void *user_data)
{
    ft_thread new_thread;
    int32_t lock_error;
    int32_t initialization_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_INVALID_STATE);
    if (g_file_watch_worker == this)
        return (FT_ERR_INVALID_OPERATION);
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (path == ft_nullptr || (legacy_callback == ft_nullptr
            && typed_callback == ft_nullptr))
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (!this->_stopped)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->stop();
        lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
        if (lock_error != FT_ERR_SUCCESS)
            return (lock_error);
    }
    if (this->_stopped == FT_FALSE)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    initialization_error = this->_path.assign(path, ft_strlen_size_t(path));
    if (initialization_error != FT_ERR_SUCCESS)
    {
        this->_callback = ft_nullptr;
        this->_typed_callback = ft_nullptr;
        this->_user_data = ft_nullptr;
        this->_path.clear();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (initialization_error);
    }
    this->_callback = legacy_callback;
    this->_typed_callback = typed_callback;
    this->_user_data = user_data;
    if (cmp_file_watch_start(this->_state, path) != FT_ERR_SUCCESS)
    {
        this->_callback = ft_nullptr;
        this->_typed_callback = ft_nullptr;
        this->_user_data = ft_nullptr;
        this->_path.clear();
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    this->_running = FT_TRUE;
    this->_stopped = FT_FALSE;
    new_thread = ft_thread(&ft_file_watch::event_loop, this);
    if (!new_thread.joinable())
    {
        this->_running.store(FT_FALSE);
        this->_stopped = FT_TRUE;
        cmp_file_watch_stop(this->_state);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_ERR_INVALID_STATE);
    }
    this->_thread = ft_move(new_thread);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

void ft_file_watch::stop()
{
    ft_thread thread_to_join;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::stop");
    int32_t lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_running.store(FT_FALSE);
    cmp_file_watch_request_stop(this->_state);
    if (g_file_watch_worker == this || !this->_thread.joinable())
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return ;
    }
    thread_to_join = ft_move(this->_thread);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    thread_to_join.join();
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    cmp_file_watch_stop(this->_state);
    this->_stopped = FT_TRUE;
    this->_callback = ft_nullptr;
    this->_typed_callback = ft_nullptr;
    this->_user_data = ft_nullptr;
    this->_path.clear();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

ft_bool ft_file_watch::snapshot_callback(file_watch_legacy_callback *legacy_callback,
    file_watch_callback *typed_callback, void *&user_data, ft_string *path_snapshot,
    uint32_t *debounce_milliseconds) const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::snapshot_callback");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (!this->_running)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    *legacy_callback = this->_callback;
    *typed_callback = this->_typed_callback;
    *debounce_milliseconds = this->_debounce_milliseconds;
    user_data = this->_user_data;
    if (path_snapshot != ft_nullptr
        && path_snapshot->assign(this->_path.c_str(), this->_path.size()) != FT_ERR_SUCCESS)
        (void)path_snapshot->clear();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_TRUE);
}

void ft_file_watch::event_loop()
{
    cmp_file_watch_event event;
    file_watch_legacy_callback local_legacy_callback;
    file_watch_callback local_typed_callback;
    void *local_user_data;
    ft_string path_snapshot;
    const char *event_name;
    uint32_t debounce_milliseconds;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state,
        "ft_file_watch::event_loop");
    if (path_snapshot.initialize() != FT_ERR_SUCCESS)
        return ;
    g_file_watch_worker = this;
    while (cmp_file_watch_wait_event(this->_state, &event) == FT_TRUE)
    {
        ft_string *fallback_path = ft_nullptr;
        if (event.has_name == FT_FALSE)
            fallback_path = &path_snapshot;
        if (!this->snapshot_callback(&local_legacy_callback, &local_typed_callback,
                local_user_data, fallback_path, &debounce_milliseconds))
            break ;
        if (debounce_milliseconds > 0)
            (void)cmp_thread_sleep(debounce_milliseconds);
        if (this->_running.load() == FT_FALSE)
            break ;
        event_name = event.name;
        if (fallback_path != ft_nullptr)
            event_name = fallback_path->c_str();
        if (local_typed_callback != ft_nullptr)
            local_typed_callback(event_name, event.event_type, local_user_data);
        else if (local_legacy_callback != ft_nullptr)
            local_legacy_callback(event_name,
                static_cast<int32_t>(event.event_type), local_user_data);
    }
    this->_running.store(FT_FALSE);
    g_file_watch_worker = ft_nullptr;
    return ;
}
