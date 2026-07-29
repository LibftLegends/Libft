#include "logger_internal.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/pthread.hpp"
#include "../Time/time.hpp"
#include "../Networking/networking.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#include "../Errno/errno.hpp"

struct s_log_remote_health_state
{
    s_network_sink *sink;
    ft_string       host;
    uint16_t  port;
    ft_bool            use_tcp;
    ft_bool            reachable;
    t_time          last_check;
    int32_t             last_error;

    s_log_remote_health_state()
        : sink(ft_nullptr), host(), port(0), use_tcp(FT_FALSE), reachable(FT_FALSE),
          last_check(0), last_error(FT_ERR_INVALID_STATE)
    {
        return ;
    }
};

struct s_network_sink_snapshot
{
    s_network_sink         *sink;
    int32_t                     socket_fd;
    t_network_send_function send_function;
    ft_string               host;
    uint16_t          port;
    ft_bool                    use_tcp;

    s_network_sink_snapshot()
        : sink(ft_nullptr), socket_fd(-1), send_function(ft_nullptr), host(),
          port(0), use_tcp(FT_FALSE)
    {
        return ;
    }
};

static pthread_mutex_t g_health_mutex;
static pthread_once_t g_health_mutex_once = PTHREAD_ONCE_INIT;
static int32_t g_health_mutex_init_error = 0;
static ft_bool g_health_thread_running = FT_FALSE;
static pthread_t g_health_thread;
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

static int32_t logger_health_fetch_config(ft_bool *running, uint32_t *interval_seconds)
{
    int32_t lock_status;
    int32_t unlock_status;

    lock_status = logger_health_lock();
    if (lock_status != 0)
        return (FT_ERR_INTERNAL);
    if (running)
        *running = g_health_thread_running;
    if (interval_seconds)
        *interval_seconds = g_health_interval_seconds;
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t logger_health_wait_loop(uint32_t interval_seconds)
{
    uint32_t wait_seconds;
    uint32_t elapsed_seconds;
    ft_bool running;

    wait_seconds = interval_seconds;
    if (wait_seconds == 0)
        wait_seconds = 30;
    elapsed_seconds = 0;
    while (elapsed_seconds < wait_seconds)
    {
        if (logger_health_fetch_config(&running, ft_nullptr) != 0)
            return (FT_ERR_INTERNAL);
        if (!running)
            return (FT_ERR_SUCCESS);
        time_sleep(1);
        elapsed_seconds += 1;
    }
    return (FT_ERR_SUCCESS);
}

static void logger_health_probe_sink(const s_network_sink_snapshot &snapshot_entry,
        ft_bool *reachable, int32_t *error_code_value)
{
    const char *ping_message;
    ft_size_t message_length;
    ft_size_t total_bytes_sent;
    int32_t previous_errno;
    int32_t send_errno;
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
    if (!snapshot_entry.sink)
    {
        return ;
    }
    socket_fd = snapshot_entry.socket_fd;
    send_function = snapshot_entry.send_function;
    if (socket_fd < 0)
    {
        *error_code_value = FT_ERR_INVALID_HANDLE;
        return ;
    }
    if (!send_function)
        send_function = nw_send;
    ping_message = "[ft_logger] health probe\n";
    message_length = ft_strlen(ping_message);
    total_bytes_sent = 0;
    previous_errno = FT_ERR_SUCCESS;
    while (total_bytes_sent < message_length)
    {
        ssize_t send_result;

        send_result = send_function(socket_fd, ping_message + total_bytes_sent,
                message_length - total_bytes_sent, 0);
        if (send_result <= 0)
        {
            send_errno = FT_ERR_SUCCESS;
            if (send_errno == FT_ERR_SUCCESS || send_errno == previous_errno)
                send_errno = FT_ERR_SOCKET_SEND_FAILED;
            *error_code_value = send_errno;
            return ;
        }
        total_bytes_sent += static_cast<ft_size_t>(send_result);
    }
    *reachable = FT_TRUE;
    *error_code_value = FT_ERR_SUCCESS;
    return ;
}

static int32_t logger_snapshot_network_sinks(ft_vector<s_network_sink_snapshot> &snapshot)
{
    ft_size_t sink_count;
    ft_size_t entry_index;

    if (logger_lock_sinks() != 0)
        return (FT_ERR_INTERNAL);
    sink_count = g_sinks.size();
    if (g_sinks.get_error() != FT_ERR_SUCCESS)
    {
        if (logger_unlock_sinks() != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    entry_index = 0;
    while (entry_index < sink_count)
    {
        s_log_sink entry;
        s_log_sink &stored_entry = g_sinks[entry_index];
        ft_bool        log_sink_lock_acquired;

        entry = g_sinks[entry_index];
        if (g_sinks.get_error() != FT_ERR_SUCCESS)
        {
            if (logger_unlock_sinks() != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        log_sink_lock_acquired = FT_FALSE;
        int32_t log_sink_lock_error;

        log_sink_lock_error = log_sink_lock(&stored_entry, &log_sink_lock_acquired);
        if (log_sink_lock_error != FT_ERR_SUCCESS)
        {
            if (logger_unlock_sinks() != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        entry = stored_entry;
        if (entry.function == ft_network_sink)
        {
            s_network_sink *network_sink;
            s_network_sink_snapshot snapshot_entry;
            ft_bool                   network_lock_acquired;

            network_sink = static_cast<s_network_sink *>(entry.user_data);
            snapshot_entry.sink = network_sink;
            snapshot_entry.socket_fd = -1;
            snapshot_entry.send_function = ft_nullptr;
            network_lock_acquired = FT_FALSE;
            if (network_sink)
            {
                int32_t network_lock_error;

                network_lock_error = network_sink_lock(network_sink, &network_lock_acquired);
                if (network_lock_error != FT_ERR_SUCCESS)
                {
                    if (network_lock_acquired)
                        network_sink_unlock(network_sink, network_lock_acquired);
                    if (log_sink_lock_acquired)
                        log_sink_unlock(&stored_entry, log_sink_lock_acquired);
                    if (logger_unlock_sinks() != 0)
                        return (FT_ERR_INTERNAL);
                    return (FT_ERR_INTERNAL);
                }
            }
            if (network_sink)
            {
                snapshot_entry.socket_fd = network_sink->socket_fd;
                snapshot_entry.send_function = network_sink->send_function;
                snapshot_entry.host = network_sink->host;
                if (snapshot_entry.host.get_error() != FT_ERR_SUCCESS)
                {
                    if (network_lock_acquired)
                        network_sink_unlock(network_sink, network_lock_acquired);
                    if (log_sink_lock_acquired)
                        log_sink_unlock(&stored_entry, log_sink_lock_acquired);
                    if (logger_unlock_sinks() != 0)
                        return (FT_ERR_INTERNAL);
                    return (FT_ERR_INTERNAL);
                }
                snapshot_entry.port = network_sink->port;
                snapshot_entry.use_tcp = network_sink->use_tcp;
            }
            if (network_lock_acquired)
                network_sink_unlock(network_sink, network_lock_acquired);
            snapshot.push_back(snapshot_entry);
            if (snapshot.get_error() != FT_ERR_SUCCESS)
            {
                if (log_sink_lock_acquired)
                    log_sink_unlock(&stored_entry, log_sink_lock_acquired);
                if (logger_unlock_sinks() != 0)
                    return (FT_ERR_INTERNAL);
                return (FT_ERR_INTERNAL);
            }
        }
        if (log_sink_lock_acquired)
            log_sink_unlock(&stored_entry, log_sink_lock_acquired);
        entry_index += 1;
    }
    if (logger_unlock_sinks() != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
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
            if (snapshot_entry.sink == state.sink)
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
            if (state.sink == snapshot_entry.sink)
            {
                exists = FT_TRUE;
                break ;
            }
            state_index += 1;
        }
        if (!exists)
        {
            s_log_remote_health_state new_state;

            new_state.sink = snapshot_entry.sink;
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

static int32_t logger_health_record_result(s_network_sink *sink, ft_bool reachable, int32_t error_code_value)
{
    ft_size_t state_count;
    ft_size_t entry_index;
    int32_t lock_status;
    int32_t unlock_status;
    t_time current_time;

    if (!sink)
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
        if (state.sink == sink)
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
    while (1)
    {
        ft_bool running;
        uint32_t interval_seconds;

        if (logger_health_fetch_config(&running, &interval_seconds) != 0)
            break ;
        if (!running)
            break ;
        ft_log_probe_remote_health();
        if (logger_health_wait_loop(interval_seconds) != 0)
            break ;
    }
    return (ft_nullptr);
}

void ft_log_enable_remote_health(ft_bool enable)
{
    int32_t lock_status;
    int32_t unlock_status;

    lock_status = logger_health_lock();
    if (lock_status != 0)
        return ;
    if (enable)
    {
        if (g_health_thread_running)
        {
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return ;
            return ;
        }
        g_health_thread_running = FT_TRUE;
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return ;
        if (pt_thread_create(&g_health_thread, ft_nullptr, logger_health_thread, ft_nullptr) != 0)
        {
            lock_status = logger_health_lock();
            if (lock_status == 0)
            {
                g_health_thread_running = FT_FALSE;
                logger_health_unlock();
            }
            return ;
        }
        return ;
    }
    if (!g_health_thread_running)
    {
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return ;
        return ;
    }
    g_health_thread_running = FT_FALSE;
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return ;
    void *thread_result;

    thread_result = ft_nullptr;
    if (pt_thread_join(g_health_thread, &thread_result) != 0)
        return ;
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
    int32_t lock_status;
    int32_t unlock_status;

    lock_status = logger_health_lock();
    if (lock_status != 0)
        return ;
    g_health_interval_seconds = interval_seconds;
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return ;
    return ;
}

int32_t ft_log_probe_remote_health()
{
    int32_t final_result;

    final_result = 0;
    {
        ft_vector<s_network_sink_snapshot> snapshot;
        ft_size_t snapshot_count;
        ft_size_t entry_index;
        ft_bool failure_detected;

        if (snapshot.initialize() != FT_ERR_SUCCESS)
            return (-1);
        if (logger_snapshot_network_sinks(snapshot) != 0)
        {
            final_result = -1;
            goto cleanup_snapshot;
        }
        if (logger_health_sync_states(snapshot) != 0)
        {
            final_result = -1;
            goto cleanup_snapshot;
        }
        snapshot_count = snapshot.size();
        if (snapshot.get_error() != FT_ERR_SUCCESS)
        {
            final_result = -1;
            goto cleanup_snapshot;
        }
        entry_index = 0;
        failure_detected = FT_FALSE;
        while (entry_index < snapshot_count)
        {
            const s_network_sink_snapshot &snapshot_entry = snapshot[entry_index];
            ft_bool reachable;
            int32_t probe_error;

            if (snapshot.get_error() != FT_ERR_SUCCESS)
            {
                final_result = -1;
                goto cleanup_snapshot;
            }
            if (!snapshot_entry.sink)
            {
                entry_index += 1;
                continue;
            }
            reachable = FT_FALSE;
            probe_error = FT_ERR_INVALID_STATE;
            logger_health_probe_sink(snapshot_entry, &reachable, &probe_error);
            if (logger_health_record_result(snapshot_entry.sink, reachable, probe_error) != 0)
            {
                if (FT_TRUE)
                {
                    final_result = -1;
                    goto cleanup_snapshot;
                }
            }
            if (!reachable && !failure_detected)
            {
                failure_detected = FT_TRUE;
            }
            entry_index += 1;
        }
        if (failure_detected)
        {
            final_result = -1;
            goto cleanup_snapshot;
        }
        final_result = 0;
    cleanup_snapshot:
        ;
    }
    return (final_result);
}

int32_t ft_log_get_remote_health(s_log_remote_health *statuses, ft_size_t capacity, ft_size_t *entry_count)
{
    ft_vector<s_network_sink_snapshot> snapshot;
    ft_size_t states_count;
    ft_size_t entry_index;
    int32_t lock_status;
    int32_t unlock_status;

    if (!entry_count)
    {
        return (FT_ERR_INTERNAL);
    }
    if (snapshot.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (logger_snapshot_network_sinks(snapshot) != 0)
        return (FT_ERR_INTERNAL);
    if (logger_health_sync_states(snapshot) != 0)
        return (FT_ERR_INTERNAL);
    lock_status = logger_health_lock();
    if (lock_status != 0)
        return (FT_ERR_INTERNAL);
    states_count = g_health_states.size();
    if (g_health_states.get_error() != FT_ERR_SUCCESS)
    {
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    if (statuses && capacity < states_count)
    {
        *entry_count = states_count;
        unlock_status = logger_health_unlock();
        if (unlock_status != 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_INTERNAL);
    }
    entry_index = 0;
    while (entry_index < states_count)
    {
        s_log_remote_health_state &state = g_health_states[entry_index];

        if (g_health_states.get_error() != FT_ERR_SUCCESS)
        {
            unlock_status = logger_health_unlock();
            if (unlock_status != 0)
                return (FT_ERR_INTERNAL);
            return (FT_ERR_INTERNAL);
        }
        if (statuses && entry_index < capacity)
        {
            statuses[entry_index].host = ft_nullptr;
            statuses[entry_index].port = 0;
            statuses[entry_index].use_tcp = FT_FALSE;
            statuses[entry_index].reachable = state.reachable;
            statuses[entry_index].last_check = state.last_check;
            statuses[entry_index].last_error = state.last_error;
            if (state.host.c_str())
            {
                statuses[entry_index].host = state.host.c_str();
                statuses[entry_index].port = state.port;
                statuses[entry_index].use_tcp = state.use_tcp;
            }
        }
        entry_index += 1;
    }
    *entry_count = states_count;
    unlock_status = logger_health_unlock();
    if (unlock_status != 0)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}
