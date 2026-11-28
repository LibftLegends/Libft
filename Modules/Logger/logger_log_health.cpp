#include "logger_internal.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/pthread.hpp"
#include "../Time/time.hpp"
#include "../Networking/networking.hpp"
#include "../Networking/socket_handle.hpp"
#include "../Template/move.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include <ctime>
#include <limits>

struct s_log_remote_health_state
{
    uint64_t identity;
    ft_string       host;
    uint16_t  port;
    ft_bool            use_tcp;
    ft_bool            reachable;
    t_time          last_check;
    int32_t             last_error;

    s_log_remote_health_state()
        : identity(0U), host(), port(0), use_tcp(FT_FALSE), reachable(FT_FALSE),
          last_check(0), last_error(FT_ERR_INVALID_STATE)
    {
        return ;
    }
};

struct s_network_sink_snapshot
{
    uint64_t identity;
    int32_t socket_fd;
    int32_t capture_error;
    t_network_send_function send_function;
    ft_socket_handle probe_socket;
    ft_bool has_probe_socket;
    ft_string host;
    uint16_t port;
    ft_bool use_tcp;

    s_network_sink_snapshot()
        : identity(0U), socket_fd(-1), capture_error(FT_ERR_SUCCESS),
          send_function(ft_nullptr), probe_socket(), has_probe_socket(FT_FALSE),
          host(), port(0U), use_tcp(FT_FALSE)
    {
        return ;
    }

    int32_t initialize(s_network_sink_snapshot &&other)
    {
        int32_t error = this->host.move(other.host);
        if (error != FT_ERR_SUCCESS)
            return (error);
        if (other.has_probe_socket != FT_FALSE)
        {
            error = this->probe_socket.move(other.probe_socket);
            if (error != FT_ERR_SUCCESS)
                return (error);
        }
        this->identity = other.identity;
        this->socket_fd = other.socket_fd;
        this->capture_error = other.capture_error;
        this->send_function = other.send_function;
        this->has_probe_socket = other.has_probe_socket;
        other.has_probe_socket = FT_FALSE;
        this->port = other.port;
        this->use_tcp = other.use_tcp;
        return (FT_ERR_SUCCESS);
    }
};

static pthread_mutex_t g_health_mutex;
static pthread_mutex_t g_health_control_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_health_condition = PTHREAD_COND_INITIALIZER;
static thread_local ft_bool g_is_health_worker = FT_FALSE;
static pthread_once_t g_health_mutex_once = PTHREAD_ONCE_INIT;
static int32_t g_health_mutex_init_error = 0;
static ft_bool g_health_thread_running = FT_FALSE;
static pthread_t g_health_thread;
static ft_bool g_health_thread_joinable = FT_FALSE;
static uint64_t g_health_interval_generation = 0U;
static uint32_t g_health_interval_seconds = 30;
static ft_vector<s_log_remote_health_state> g_health_states;

static void logger_health_initialize_mutex()
{
    int32_t initialization_status;

    initialization_status = pthread_mutex_init(&g_health_mutex, nullptr);
    if (initialization_status != 0)
    {
        g_health_mutex_init_error = initialization_status;
        return ;
    }
    g_health_mutex_init_error = 0;
    return ;
}

static int32_t logger_health_lock()
{
    int32_t once_status;
    int32_t lock_status;
    int32_t initialize_status;

    once_status = pthread_once(&g_health_mutex_once, logger_health_initialize_mutex);
    if (once_status != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    if (g_health_mutex_init_error != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_status = pthread_mutex_lock(&g_health_mutex);
    if (lock_status != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    if (g_health_states.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialize_status = g_health_states.initialize();
        if (initialize_status != FT_ERR_SUCCESS)
        {
            (void)pthread_mutex_unlock(&g_health_mutex);
            return (initialize_status);
        }
    }
    return (FT_ERR_SUCCESS);
}

static int32_t logger_health_unlock()
{
    int32_t once_status;
    int32_t unlock_status;

    once_status = pthread_once(&g_health_mutex_once, logger_health_initialize_mutex);
    if (once_status != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    if (g_health_mutex_init_error != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    unlock_status = pthread_mutex_unlock(&g_health_mutex);
    if (unlock_status != 0)
    {
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_SUCCESS);
}

static ft_bool logger_health_deadline(timespec &deadline)
{
    if (std::timespec_get(&deadline, TIME_UTC) != TIME_UTC)
        return (FT_FALSE);
    uint32_t interval = g_health_interval_seconds;
    if (interval == 0U)
        interval = 30U;
    using t_deadline_seconds = decltype(deadline.tv_sec);
    constexpr t_deadline_seconds MAX_SECONDS = std::numeric_limits<t_deadline_seconds>::max();
    const uint64_t seconds = static_cast<uint64_t>(deadline.tv_sec);
    if (seconds >= static_cast<uint64_t>(MAX_SECONDS)
        || interval >= static_cast<uint64_t>(MAX_SECONDS) - seconds)
        deadline.tv_sec = MAX_SECONDS;
    else
        deadline.tv_sec = static_cast<t_deadline_seconds>(seconds + interval);
    return (FT_TRUE);
}

static ft_bool logger_health_wait()
{
    timespec deadline;
    uint64_t interval_generation = g_health_interval_generation;
    if (logger_health_deadline(deadline) == FT_FALSE)
        return (FT_FALSE);
    while (g_health_thread_running != FT_FALSE)
    {
        const int32_t wait_error = pthread_cond_timedwait(&g_health_condition,
            &g_health_mutex, &deadline);
        if (wait_error != 0 && wait_error != ETIMEDOUT)
            return (FT_FALSE);
        if (g_health_thread_running == FT_FALSE)
            return (FT_FALSE);
        if (interval_generation != g_health_interval_generation)
        {
            interval_generation = g_health_interval_generation;
            if (logger_health_deadline(deadline) == FT_FALSE)
                return (FT_FALSE);
        }
        else if (wait_error == ETIMEDOUT)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static void logger_health_probe_sink(const s_network_sink_snapshot &snapshot_entry,
        ft_bool *reachable, int32_t *error_code_value)
{
    ft_size_t total_bytes_sent;
    t_network_send_function send_function;
    int32_t socket_fd;

    if (!reachable || !error_code_value)
    {
        if (error_code_value)
            *error_code_value = FT_ERR_INVALID_ARGUMENT;
        if (reachable)
            *reachable = FT_FALSE;
        return ;
    }
    *reachable = FT_FALSE;
    *error_code_value = FT_ERR_INVALID_ARGUMENT;
    if (snapshot_entry.capture_error != FT_ERR_SUCCESS)
    {
        *error_code_value = snapshot_entry.capture_error;
        return ;
    }
    socket_fd = snapshot_entry.socket_fd;
    if (snapshot_entry.has_probe_socket != FT_FALSE)
        socket_fd = snapshot_entry.probe_socket.get();
    send_function = snapshot_entry.send_function;
    if (socket_fd < 0)
    {
        *error_code_value = FT_ERR_INVALID_HANDLE;
        return ;
    }
    if (!send_function)
        send_function = nw_send;
    static constexpr char PING_MESSAGE[] = "[ft_logger] health probe\n";
    constexpr ft_size_t MESSAGE_LENGTH = sizeof(PING_MESSAGE) - 1U;
    total_bytes_sent = 0;
    while (total_bytes_sent < MESSAGE_LENGTH)
    {
        const ft_size_t remaining_bytes = MESSAGE_LENGTH - total_bytes_sent;
        const ssize_t send_result = send_function(socket_fd,
                PING_MESSAGE + total_bytes_sent, remaining_bytes, 0);
        if (send_result <= 0 || static_cast<ft_size_t>(send_result) > remaining_bytes)
        {
            *error_code_value = FT_ERR_SOCKET_SEND_FAILED;
            return ;
        }
        total_bytes_sent += static_cast<ft_size_t>(send_result);
    }
    *reachable = FT_TRUE;
    *error_code_value = FT_ERR_SUCCESS;
    return ;
}

static int32_t logger_capture_network_sink(const s_log_sink &entry,
    s_network_sink_snapshot &snapshot, ft_bool capture_socket)
{
    const auto *sink = static_cast<s_network_sink *>(entry.user_data);
    ft_bool acquired = FT_FALSE;
    int32_t error = network_sink_lock(sink, &acquired);
    if (error != FT_ERR_SUCCESS)
        return (error);
    snapshot.identity = entry.registration->identity;
    snapshot.port = sink->port;
    snapshot.use_tcp = sink->use_tcp;
    error = snapshot.host.initialize(sink->host);
    if (error == FT_ERR_SUCCESS && capture_socket != FT_FALSE)
    {
        snapshot.send_function = sink->send_function;
        if (sink->socket_fd >= 0 && (sink->send_function == ft_nullptr
            || sink->send_function == nw_send))
        {
            snapshot.capture_error = snapshot.probe_socket.initialize_duplicate(sink->socket_fd);
            snapshot.has_probe_socket = snapshot.capture_error == FT_ERR_SUCCESS;
        }
        else
            snapshot.socket_fd = sink->socket_fd;
    }
    network_sink_unlock(sink, acquired);
    return (error);
}

static int32_t logger_health_sync_states(const ft_vector<s_network_sink_snapshot> &snapshot);

static int32_t logger_snapshot_network_sinks(ft_vector<s_network_sink_snapshot> &snapshot,
    ft_bool capture_socket)
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    const ft_size_t count = g_sinks.size();
    ft_size_t index = 0U;
    int32_t error = FT_ERR_SUCCESS;
    while (index < count && error == FT_ERR_SUCCESS)
    {
        const s_log_sink &entry = g_sinks[index];
        if (entry.function == ft_network_sink && entry.user_data != ft_nullptr)
        {
            s_network_sink_snapshot captured;
            error = logger_capture_network_sink(entry, captured, capture_socket);
            if (error == FT_ERR_SUCCESS)
                error = snapshot.push_back(ft_move(captured));
        }
        index += 1U;
    }
    if (error == FT_ERR_SUCCESS)
        error = logger_health_sync_states(snapshot);
    const int32_t unlock_error = logger_unlock_sinks();
    if (error != FT_ERR_SUCCESS)
        return (error);
    return (unlock_error);
}

static ft_bool logger_health_sink_registered(uint64_t identity)
{
    if (logger_lock_sinks() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    const ft_size_t count = g_sinks.size();
    ft_size_t index = 0U;
    while (index < count && g_sinks[index].registration->identity != identity)
        index += 1U;
    (void)logger_unlock_sinks();
    return (index < count);
}

static int32_t logger_health_sync_states(const ft_vector<s_network_sink_snapshot> &snapshot)
{
    ft_size_t state_count;
    ft_size_t snapshot_count;
    ft_size_t entry_index;
    int32_t lock_status;
    int32_t unlock_status;

    snapshot_count = snapshot.size();
    if (snapshot.get_error() != FT_ERR_SUCCESS)
    {
        return (FT_ERR_INTERNAL);
    }
    lock_status = logger_health_lock();
    if (lock_status != 0)
        return (FT_ERR_INTERNAL);
    state_count = g_health_states.size();
    if (g_health_states.get_error() != FT_ERR_SUCCESS)
    {
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    entry_index = 0;
    while (entry_index < state_count)
    {
        s_log_remote_health_state &state = g_health_states[entry_index];
        ft_bool found;
        ft_size_t snapshot_index;

        if (g_health_states.get_error() != FT_ERR_SUCCESS)
        {
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        
        found = FT_FALSE;
        snapshot_index = 0;
        while (snapshot_index < snapshot_count)
        {
            const s_network_sink_snapshot &snapshot_entry = snapshot[snapshot_index];

            if (snapshot.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            if (snapshot_entry.identity == state.identity)
            {
                state.host = snapshot_entry.host;
                if (state.host.get_error() != FT_ERR_SUCCESS)
                {
                    unlock_status = logger_health_unlock();
                    if (unlock_status != 0)
                        return (FT_ERR_INTERNAL);
                    return (FT_ERR_INTERNAL);
                }
                state.port = snapshot_entry.port;
                state.use_tcp = snapshot_entry.use_tcp;
                found = FT_TRUE;
                break ;
            }
            snapshot_index += 1;
        }
        if (!found)
        {
            g_health_states.erase(g_health_states.begin() + entry_index);
            if (g_health_states.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            state_count = g_health_states.size();
            if (g_health_states.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            continue;
        }
        entry_index += 1;
    }
    entry_index = 0;
    state_count = g_health_states.size();
    if (g_health_states.get_error() != FT_ERR_SUCCESS)
    {
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    while (entry_index < snapshot_count)
    {
        const s_network_sink_snapshot &snapshot_entry = snapshot[entry_index];
        ft_size_t state_index;
        ft_bool exists;

        if (snapshot.get_error() != FT_ERR_SUCCESS)
        {
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        exists = FT_FALSE;
        state_index = 0;
        while (state_index < state_count)
        {
            s_log_remote_health_state &state = g_health_states[state_index];

            if (g_health_states.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            if (state.identity == snapshot_entry.identity)
            {
                exists = FT_TRUE;
                break ;
            }
            state_index += 1;
        }
        if (!exists)
        {
            s_log_remote_health_state new_state;

            new_state.identity = snapshot_entry.identity;
            new_state.host = snapshot_entry.host;
            if (new_state.host.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            new_state.port = snapshot_entry.port;
            new_state.use_tcp = snapshot_entry.use_tcp;
            new_state.reachable = FT_FALSE;
            new_state.last_check = 0;
            new_state.last_error = FT_ERR_INVALID_STATE;
            g_health_states.push_back(new_state);
            if (g_health_states.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
            state_count = g_health_states.size();
            if (g_health_states.get_error() != FT_ERR_SUCCESS)
            {
                unlock_status = logger_health_unlock();
                if (unlock_status != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
        }
        entry_index += 1;
    }
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t logger_health_record_result(uint64_t identity, ft_bool reachable, int32_t error_code_value)
{
    ft_size_t state_count;
    ft_size_t entry_index;
    int32_t lock_status;
    int32_t unlock_status;
    t_time current_time;

    lock_status = logger_health_lock();
    if (lock_status != 0)
        return (FT_ERR_INTERNAL);
    state_count = g_health_states.size();
    if (g_health_states.get_error() != FT_ERR_SUCCESS)
    {
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    current_time = time_now();
    entry_index = 0;
    while (entry_index < state_count)
    {
        s_log_remote_health_state &state = g_health_states[entry_index];

        if (g_health_states.get_error() != FT_ERR_SUCCESS)
        {
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        if (state.identity == identity)
        {
            state.reachable = reachable;
            state.last_check = current_time;
            state.last_error = error_code_value;
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_SUCCESS);
        }
        entry_index += 1;
    }
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_INTERNAL);
}

static void *logger_health_thread(void *argument)
{
    (void)argument;
    g_is_health_worker = FT_TRUE;
    if (logger_health_lock() != FT_ERR_SUCCESS)
        return (ft_nullptr);
    while (g_health_thread_running != FT_FALSE)
    {
        (void)logger_health_unlock();
        (void)ft_log_probe_remote_health();
        if (logger_health_lock() != FT_ERR_SUCCESS)
            return (ft_nullptr);
        if (g_health_thread_running == FT_FALSE || logger_health_wait() == FT_FALSE)
            break ;
    }
    g_health_thread_running = FT_FALSE;
    (void)logger_health_unlock();
    return (ft_nullptr);
}

static void logger_health_request_stop()
{
    if (logger_health_lock() != FT_ERR_SUCCESS)
        return ;
    g_health_thread_running = FT_FALSE;
    (void)pthread_cond_broadcast(&g_health_condition);
    (void)logger_health_unlock();
    return ;
}

static void logger_health_update_worker(ft_bool enable)
{
    if (logger_health_lock() != FT_ERR_SUCCESS)
        return ;
    const ft_bool running = g_health_thread_running;
    (void)logger_health_unlock();
    if (enable != FT_FALSE && running != FT_FALSE)
        return ;
    if (g_health_thread_joinable != FT_FALSE)
    {
        logger_health_request_stop();
        if (pt_thread_join(g_health_thread, ft_nullptr) != 0)
            return ;
        g_health_thread_joinable = FT_FALSE;
    }
    if (enable == FT_FALSE || logger_health_lock() != FT_ERR_SUCCESS)
        return ;
    if (pt_thread_create(&g_health_thread, ft_nullptr, logger_health_thread, ft_nullptr) == 0)
    {
        g_health_thread_joinable = FT_TRUE;
        g_health_thread_running = FT_TRUE;
    }
    (void)logger_health_unlock();
    return ;
}

ft_bool logger_in_health_worker()
{
    return (g_is_health_worker);
}

void ft_log_enable_remote_health(ft_bool enable)
{
    const ft_bool closing = logger_runtime_closing();
    if (enable != FT_FALSE && closing != FT_FALSE)
        return ;
    if (g_is_health_worker != FT_FALSE
        || (closing != FT_FALSE && logger_in_delivery_callback() != FT_FALSE))
    {
        if (enable == FT_FALSE)
            logger_health_request_stop();
        return ;
    }
    if (pthread_mutex_lock(&g_health_control_mutex) != 0)
        return ;
    if (enable == FT_FALSE || logger_runtime_closing() == FT_FALSE)
        logger_health_update_worker(enable);
    (void)pthread_mutex_unlock(&g_health_control_mutex);
    return ;
}

#ifdef LIBFT_TEST_BUILD
void ft_log_destroy_remote_health_for_tests(void)
{
    ft_log_enable_remote_health(FT_FALSE);
    (void)g_health_states.destroy();
    return ;
}
#endif

void ft_log_set_remote_health_interval(uint32_t interval_seconds)
{
    if (logger_health_lock() != FT_ERR_SUCCESS)
        return ;
    if (g_health_interval_seconds != interval_seconds)
    {
        g_health_interval_seconds = interval_seconds;
        g_health_interval_generation += 1U;
        (void)pthread_cond_broadcast(&g_health_condition);
    }
    (void)logger_health_unlock();
    return ;
}

int32_t logger_probe_remote_health()
{
    ft_vector<s_network_sink_snapshot> snapshot;
    int32_t error = snapshot.initialize();
    if (error == FT_ERR_SUCCESS)
        error = logger_snapshot_network_sinks(snapshot, FT_TRUE);
    if (error != FT_ERR_SUCCESS)
        return (error);

    const ft_size_t count = snapshot.size();
    ft_size_t index = 0U;
    int32_t first_error = FT_ERR_SUCCESS;
    while (index < count)
    {
        const s_network_sink_snapshot &entry = snapshot[index];
        if (logger_health_sink_registered(entry.identity) != FT_FALSE)
        {
            ft_bool reachable;
            int32_t probe_error;
            logger_health_probe_sink(entry, &reachable, &probe_error);
            if (logger_health_sink_registered(entry.identity) == FT_FALSE)
                error = FT_ERR_NOT_FOUND;
            else
            {
                error = logger_health_record_result(entry.identity, reachable, probe_error);
                if (error == FT_ERR_SUCCESS && reachable == FT_FALSE)
                    error = probe_error;
            }
            if (first_error == FT_ERR_SUCCESS)
                first_error = error;
        }
        index += 1U;
    }
    return (first_error);
}

int32_t ft_log_probe_remote_health()
{
    return (-static_cast<int32_t>(logger_probe_remote_health() != FT_ERR_SUCCESS));
}

int32_t logger_read_remote_health(s_log_remote_health *statuses, ft_size_t capacity,
    ft_size_t *entry_count)
{
    if (entry_count == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_vector<s_network_sink_snapshot> snapshot;
    int32_t error = snapshot.initialize();
    if (error == FT_ERR_SUCCESS)
        error = logger_snapshot_network_sinks(snapshot, FT_FALSE);
    if (error != FT_ERR_SUCCESS)
        return (error);
    if (logger_health_lock() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);

    const ft_size_t states_count = g_health_states.size();
    error = g_health_states.get_error();
    if (error == FT_ERR_SUCCESS)
    {
        *entry_count = states_count;
        if (statuses != ft_nullptr && capacity < states_count)
            error = FT_ERR_OUT_OF_RANGE;
        else if (statuses != ft_nullptr)
        {
            ft_size_t index = 0U;
            while (index < states_count)
            {
                const s_log_remote_health_state &state = g_health_states[index];
                error = g_health_states.get_error();
                if (error != FT_ERR_SUCCESS)
                    break ;
                statuses[index].host = state.host.c_str();
                statuses[index].port = state.port;
                statuses[index].use_tcp = state.use_tcp;
                statuses[index].reachable = state.reachable;
                statuses[index].last_check = state.last_check;
                statuses[index].last_error = state.last_error;
                index += 1U;
            }
        }
    }
    (void)logger_health_unlock();
    return (error);
}

int32_t ft_log_get_remote_health(s_log_remote_health *statuses, ft_size_t capacity,
    ft_size_t *entry_count)
{
    if (logger_read_remote_health(statuses, capacity, entry_count) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}
