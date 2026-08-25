#include "http_client.hpp"
#include "http2_client.hpp"
#include "socket_class.hpp"
#include "networking.hpp"
#include "ssl_wrapper.hpp"
#include "openssl_support.hpp"
#include "networking_ssl_compat.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/pair.hpp"
#if NETWORKING_HAS_OPENSSL
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <vector>
#include <openssl/x509v3.h>
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include "../Time/time.hpp"
#include "../Observability/observability_networking_metrics.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Threading/unique_lock.hpp"

#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <arpa/inet.h>
# include <unistd.h>
#endif

struct http_stream_state
{
    ft_string           header_buffer;
    ft_string           headers;
    int32_t                 status_code;
    ft_bool                headers_ready;
    ft_bool                keep_alive_allowed;
    ft_bool                has_content_length;
    ft_size_t              expected_body_length;
    ft_size_t              received_body_length;
    ft_bool                saw_chunked_encoding;
    ft_bool                http_1_1;
    http_response_handler handler;
};

struct http_buffer_adapter_state
{
    ft_string   *response;
    ft_bool        header_appended;
    ft_size_t      body_bytes;
    int32_t         status_code;
};

static http_buffer_adapter_state g_http_buffer_adapter_state = { NULL, FT_FALSE, 0, 0 };

struct http_client_connection_entry
{
    ft_string               host;
    ft_string               port;
    ft_bool                    use_ssl;
    int32_t                     socket_fd;
    SSL_CTX                 *ssl_context;
    SSL                     *ssl_connection;
    t_monotonic_time_point  last_used;
};

struct http_client_active_connection
{
    http_client_connection_entry   entry;
    ft_bool                            from_pool;
    ft_bool                            store_allowed;
};

static pt_recursive_mutex *g_http_client_pool_mutex = ft_nullptr;
static std::vector<http_client_connection_entry *> g_http_client_pool_entries;
static ft_size_t g_http_client_pool_max_idle = 8;
static int64_t g_http_client_pool_idle_timeout_ms = 30000;
static ft_size_t g_http_client_pool_acquire_calls = 0;
static ft_size_t g_http_client_pool_reuse_hits = 0;
static ft_size_t g_http_client_pool_acquire_misses = 0;

#ifdef LIBFT_TEST_BUILD
static void http_client_pool_untrack_runtime_leaks(void)
{
    if (g_http_client_pool_mutex != ft_nullptr)
        (void)cma_untrack_leak(g_http_client_pool_mutex);
    if (!g_http_client_pool_entries.empty()
        && g_http_client_pool_entries.data() != NULL)
        (void)cma_untrack_leak(g_http_client_pool_entries.data());
    return ;
}
#endif

static int32_t http_client_reinitialize_string(ft_string &value)
{
    int32_t destroy_error;
    int32_t initialize_error;

    destroy_error = value.destroy();
    if (destroy_error != FT_ERR_SUCCESS)
        return (destroy_error);
    initialize_error = value.initialize();
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    return (FT_ERR_SUCCESS);
}

int32_t http_client_pool_enable_thread_safety(void)
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    if (g_http_client_pool_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_error = mutex_pointer->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (initialize_error);
    }
    g_http_client_pool_mutex = mutex_pointer;
#ifdef LIBFT_TEST_BUILD
    http_client_pool_untrack_runtime_leaks();
#endif
    return (FT_ERR_SUCCESS);
}

int32_t http_client_pool_disable_thread_safety(void)
{
    int32_t destroy_error;
    pt_recursive_mutex *mutex_pointer;

    mutex_pointer = g_http_client_pool_mutex;
    g_http_client_pool_mutex = ft_nullptr;
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    return (destroy_error);
}

ft_bool http_client_pool_is_thread_safe(void)
{
    if (g_http_client_pool_mutex != ft_nullptr)
        return (FT_TRUE);
    return (FT_FALSE);
}

static void http_client_pool_reset_active(http_client_active_connection &connection)
{
    if (http_client_reinitialize_string(connection.entry.host) != FT_ERR_SUCCESS)
        return ;
    if (http_client_reinitialize_string(connection.entry.port) != FT_ERR_SUCCESS)
        return ;
    connection.entry.socket_fd = -1;
    connection.entry.ssl_context = NULL;
    connection.entry.ssl_connection = NULL;
    connection.entry.use_ssl = FT_FALSE;
    connection.from_pool = FT_FALSE;
    connection.store_allowed = FT_TRUE;
    return ;
}

static void http_client_pool_dispose_entry(http_client_connection_entry &entry)
{
    if (entry.ssl_connection != NULL)
    {
        SSL_shutdown(entry.ssl_connection);
        SSL_free(entry.ssl_connection);
        entry.ssl_connection = NULL;
    }
    if (entry.ssl_context != NULL)
    {
        SSL_CTX_free(entry.ssl_context);
        entry.ssl_context = NULL;
    }
    if (entry.socket_fd >= 0)
    {
        nw_close(entry.socket_fd);
        entry.socket_fd = -1;
    }
    if (http_client_reinitialize_string(entry.host) != FT_ERR_SUCCESS)
        return ;
    if (http_client_reinitialize_string(entry.port) != FT_ERR_SUCCESS)
        return ;
    entry.use_ssl = FT_FALSE;
    entry.last_used = time_monotonic_point_now();
    return ;
}

static void http_client_pool_delete_entry(http_client_connection_entry *entry) noexcept
{
    if (entry == NULL)
        return ;
    http_client_pool_dispose_entry(*entry);
    (void)entry->host.destroy();
    (void)entry->port.destroy();
    delete entry;
    return ;
}

static void http_client_pool_prune_locked(t_monotonic_time_point now)
{
    ft_size_t index;

    index = 0;
    while (index < g_http_client_pool_entries.size())
    {
        http_client_connection_entry *candidate = g_http_client_pool_entries[index];
        int64_t idle_time_ms;

        if (candidate == NULL)
        {
            g_http_client_pool_entries.erase(g_http_client_pool_entries.begin() + index);
            continue ;
        }
        idle_time_ms = time_monotonic_point_diff_ms(candidate->last_used, now);
        if (idle_time_ms < 0)
            idle_time_ms = 0;
        if (idle_time_ms > g_http_client_pool_idle_timeout_ms)
        {
            http_client_pool_delete_entry(candidate);
            g_http_client_pool_entries.erase(g_http_client_pool_entries.begin() + index);
            continue ;
        }
        index++;
    }
    return ;
}

static void http_client_pool_release_connection(http_client_active_connection &connection, ft_bool allow_reuse)
{
    if (connection.entry.socket_fd < 0)
    {
        http_client_pool_reset_active(connection);
        return ;
    }
    if (allow_reuse == FT_FALSE || connection.store_allowed == FT_FALSE)
    {
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    int32_t lock_error;
    t_monotonic_time_point now;
    http_client_connection_entry *pooled_entry;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
    {
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    now = time_monotonic_point_now();
    http_client_pool_prune_locked(now);
    if (g_http_client_pool_entries.size() >= g_http_client_pool_max_idle)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    pooled_entry = new (std::nothrow) http_client_connection_entry();
    if (pooled_entry == NULL)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    if (pooled_entry->host.initialize(connection.entry.host) != FT_ERR_SUCCESS
        || pooled_entry->port.initialize(connection.entry.port) != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
        http_client_pool_delete_entry(pooled_entry);
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    pooled_entry->use_ssl = connection.entry.use_ssl;
    pooled_entry->socket_fd = connection.entry.socket_fd;
    pooled_entry->ssl_context = connection.entry.ssl_context;
    pooled_entry->ssl_connection = connection.entry.ssl_connection;
    pooled_entry->last_used = now;
    connection.entry.socket_fd = -1;
    connection.entry.ssl_context = NULL;
    connection.entry.ssl_connection = NULL;
    g_http_client_pool_entries.push_back(pooled_entry);
#ifdef LIBFT_TEST_BUILD
    http_client_pool_untrack_runtime_leaks();
#endif
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    http_client_pool_reset_active(connection);
    return ;
}

static void http_client_pool_disable_store(http_client_active_connection &connection)
{
    connection.store_allowed = FT_FALSE;
    return ;
}

static int32_t http_client_pool_acquire_connection(const char *host, const char *port,
    ft_bool use_ssl, http_client_active_connection &connection, ft_bool &reused)
{
    int32_t lock_error;
    t_monotonic_time_point now;
    ft_size_t index;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    now = time_monotonic_point_now();
    http_client_pool_prune_locked(now);
    g_http_client_pool_acquire_calls++;
    index = 0;
    while (index < g_http_client_pool_entries.size())
    {
        http_client_connection_entry *candidate = g_http_client_pool_entries[index];

        if (candidate != NULL && candidate->use_ssl == use_ssl
            && candidate->host == host && candidate->port == port)
        {
            if (http_client_reinitialize_string(connection.entry.host) != FT_ERR_SUCCESS
                || http_client_reinitialize_string(connection.entry.port) != FT_ERR_SUCCESS)
            {
                (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
                return (FT_ERR_NO_MEMORY);
            }
            connection.entry.host = candidate->host;
            connection.entry.port = candidate->port;
            connection.entry.use_ssl = candidate->use_ssl;
            connection.entry.socket_fd = candidate->socket_fd;
            connection.entry.ssl_context = candidate->ssl_context;
            connection.entry.ssl_connection = candidate->ssl_connection;
            connection.entry.last_used = candidate->last_used;
            candidate->socket_fd = -1;
            candidate->ssl_context = NULL;
            candidate->ssl_connection = NULL;
            connection.from_pool = FT_TRUE;
            connection.store_allowed = FT_TRUE;
            g_http_client_pool_entries.erase(g_http_client_pool_entries.begin() + index);
            http_client_pool_delete_entry(candidate);
            g_http_client_pool_reuse_hits++;
            reused = FT_TRUE;
            (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
            return (FT_ERR_SUCCESS);
        }
        index++;
    }
    g_http_client_pool_acquire_misses++;
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    if (http_client_reinitialize_string(connection.entry.host) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (http_client_reinitialize_string(connection.entry.port) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    connection.entry.host = host;
    connection.entry.port = port;
    connection.entry.use_ssl = use_ssl;
    connection.entry.socket_fd = -1;
    connection.entry.ssl_context = NULL;
    connection.entry.ssl_connection = NULL;
    connection.entry.last_used = now;
    connection.from_pool = FT_FALSE;
    connection.store_allowed = FT_TRUE;
    reused = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static void http_client_trim_string(ft_string &value)
{
    ft_size_t start_index;
    ft_size_t end_index;

    start_index = 0;
    while (start_index < value.size()
        && ft_isspace(static_cast<unsigned char>(value[start_index])) != 0)
        start_index++;
    if (start_index > 0)
        value.erase(0, start_index);
    end_index = value.size();
    while (end_index > 0
        && ft_isspace(static_cast<unsigned char>(value[end_index - 1])) != 0)
        end_index--;
    if (end_index < value.size())
        value.erase(end_index, value.size() - end_index);
    return ;
}

static void http_client_to_lower(ft_string &value)
{
    ft_size_t index;
    char *mutable_data;

    index = 0;
    mutable_data = value.data();
    if (mutable_data == NULL)
        return ;
    while (index < value.size())
    {
        char character;

        character = mutable_data[index];
        if (character >= 'A' && character <= 'Z')
            mutable_data[index] = static_cast<char>(character + 32);
        index++;
    }
    return ;
}

static void http_client_parse_header_metadata(http_stream_state &state)
{
    const char  *header_data;
    ft_size_t      header_limit;
    ft_size_t      offset;
    ft_string   status_prefix;
    ft_string   header_name;
    ft_string   header_value;
    ft_string   lowered_value;

    if (status_prefix.initialize() != FT_ERR_SUCCESS)
        return ;
    if (header_name.initialize() != FT_ERR_SUCCESS)
        return ;
    if (header_value.initialize() != FT_ERR_SUCCESS)
        return ;
    if (lowered_value.initialize() != FT_ERR_SUCCESS)
        return ;

    state.keep_alive_allowed = FT_FALSE;
    state.has_content_length = FT_FALSE;
    state.expected_body_length = 0;
    state.received_body_length = 0;
    state.saw_chunked_encoding = FT_FALSE;
    state.http_1_1 = FT_FALSE;
    header_limit = state.headers.size();
    header_data = state.headers.c_str();
    if (header_limit >= 8)
    {
        status_prefix.assign(header_data, 8);
        if (status_prefix == "HTTP/1.1")
        {
            state.http_1_1 = FT_TRUE;
            state.keep_alive_allowed = FT_TRUE;
        }
    }
    offset = 0;
    while (offset < header_limit)
    {
        ft_size_t line_length;
        const char *line_start;
        ft_size_t colon_index;

        line_start = header_data + offset;
        line_length = 0;
        while (offset + line_length + 1 < header_limit)
        {
            if (line_start[line_length] == '\r'
                && line_start[line_length + 1] == '\n')
                break ;
            line_length++;
        }
        if (offset + line_length + 1 >= header_limit)
            break ;
        offset += line_length + 2;
        if (line_length == 0)
            continue ;
        colon_index = 0;
        while (colon_index < line_length && line_start[colon_index] != ':')
            colon_index++;
        if (colon_index >= line_length)
            continue ;
        header_name.clear();
        header_value.clear();
        lowered_value.clear();
        header_name.assign(line_start, colon_index);
        header_value.assign(line_start + colon_index + 1, line_length - colon_index - 1);
        http_client_trim_string(header_name);
        http_client_trim_string(header_value);
        http_client_to_lower(header_name);
        if (header_name == "connection")
        {
            lowered_value = header_value;
            http_client_to_lower(lowered_value);
            if (lowered_value.find("close") != ft_string::npos)
                state.keep_alive_allowed = FT_FALSE;
            else if (lowered_value.find("keep-alive") != ft_string::npos)
                state.keep_alive_allowed = FT_TRUE;
            continue ;
        }
        if (header_name == "content-length")
        {
            ft_size_t digit_index;
            uint64_t parsed_length;

            digit_index = 0;
            parsed_length = 0;
            while (digit_index < header_value.size()
                && ft_isdigit(static_cast<unsigned char>(header_value[digit_index])) != 0)
            {
                parsed_length = (parsed_length * 10)
                    + static_cast<uint64_t>(header_value[digit_index] - '0');
                digit_index++;
            }
            state.has_content_length = FT_TRUE;
            state.expected_body_length = static_cast<ft_size_t>(parsed_length);
            continue ;
        }
        if (header_name == "transfer-encoding")
        {
            lowered_value = header_value;
            http_client_to_lower(lowered_value);
            if (lowered_value.find("chunked") != ft_string::npos)
            {
                state.saw_chunked_encoding = FT_TRUE;
                state.keep_alive_allowed = FT_FALSE;
            }
        }
    }
    if (state.has_content_length == FT_FALSE)
        state.keep_alive_allowed = FT_FALSE;
    if (state.saw_chunked_encoding != FT_FALSE)
        state.keep_alive_allowed = FT_FALSE;
    return ;
}

void http_client_pool_flush(void)
{
    int32_t lock_error;
    ft_size_t index;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    index = 0;
    while (index < g_http_client_pool_entries.size())
    {
        http_client_pool_delete_entry(g_http_client_pool_entries[index]);
        index++;
    }
    g_http_client_pool_entries.clear();
#ifdef LIBFT_TEST_BUILD
    http_client_pool_untrack_runtime_leaks();
#endif
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return ;
}

void http_client_pool_set_max_idle(ft_size_t max_idle)
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    g_http_client_pool_max_idle = max_idle;
    if (g_http_client_pool_entries.size() > g_http_client_pool_max_idle)
    {
        ft_size_t index;

        index = g_http_client_pool_max_idle;
        while (index < g_http_client_pool_entries.size())
        {
            http_client_pool_delete_entry(g_http_client_pool_entries[index]);
            index++;
        }
        g_http_client_pool_entries.resize(g_http_client_pool_max_idle);
#ifdef LIBFT_TEST_BUILD
        http_client_pool_untrack_runtime_leaks();
#endif
    }
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return ;
}

ft_size_t http_client_pool_get_idle_count(void)
{
    int32_t lock_error;
    ft_size_t idle_count;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    idle_count = g_http_client_pool_entries.size();
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return (idle_count);
}

void http_client_pool_debug_reset_counters(void)
{
    int32_t lock_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    g_http_client_pool_acquire_calls = 0;
    g_http_client_pool_reuse_hits = 0;
    g_http_client_pool_acquire_misses = 0;
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return ;
}

ft_size_t http_client_pool_debug_get_reuse_count(void)
{
    int32_t lock_error;
    ft_size_t reuse_count;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    reuse_count = g_http_client_pool_reuse_hits;
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return (reuse_count);
}

ft_size_t http_client_pool_debug_get_miss_count(void)
{
    int32_t lock_error;
    ft_size_t miss_count;

    lock_error = pt_recursive_mutex_lock_if_not_null(g_http_client_pool_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    miss_count = g_http_client_pool_acquire_misses;
    (void)pt_recursive_mutex_unlock_if_not_null(g_http_client_pool_mutex);
    return (miss_count);
}

static int32_t http_client_wait_for_socket_ready(int32_t socket_fd, ft_bool wait_for_write)
{
    int32_t poll_descriptor;
    int32_t poll_result;

    if (socket_fd < 0)
    {
        return (-1);
    }
    poll_descriptor = socket_fd;
    if (wait_for_write != FT_FALSE)
        poll_result = nw_poll(NULL, 0, &poll_descriptor, 1, 1000);
    else
        poll_result = nw_poll(&poll_descriptor, 1, NULL, 0, 1000);
    if (poll_result < 0)
    {
#ifdef _WIN32
#else
#endif
        return (-1);
    }
    if (poll_result == 0)
        return (1);
    return (0);
}

static int32_t http_client_initialize_ssl(int32_t socket_fd, const char *host, SSL_CTX **ssl_context, SSL **ssl_connection)
{
    SSL_CTX *local_context;
    SSL *local_connection;

    if (ssl_context == NULL || ssl_connection == NULL)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    *ssl_context = NULL;
    *ssl_connection = NULL;
    SSL_library_init();
    local_context = SSL_CTX_new(TLS_client_method());
    if (local_context == NULL)
    {
        return (FT_ERR_INVALID_OPERATION);
    }
    SSL_CTX_set_verify(local_context, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_default_verify_paths(local_context) != 1)
    {
        SSL_CTX_free(local_context);
        return (FT_ERR_INVALID_OPERATION);
    }
    local_connection = SSL_new(local_context);
    if (local_connection == NULL)
    {
        SSL_CTX_free(local_context);
        return (FT_ERR_INVALID_OPERATION);
    }
    ft_bool selected_http2;
    int32_t alpn_error;

    if (!http2_select_alpn_protocol(local_connection, selected_http2, alpn_error))
    (void)selected_http2;
    (void)alpn_error;
    if (host != NULL && host[0] != '\0')
    {
        int64_t control_result;

        control_result = SSL_ctrl(local_connection, SSL_CTRL_SET_TLSEXT_HOSTNAME,
            TLSEXT_NAMETYPE_host_name, const_cast<char *>(host));
        if (control_result != 1)
        {
            SSL_free(local_connection);
            SSL_CTX_free(local_context);
            return (FT_ERR_INVALID_OPERATION);
        }
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
        X509_VERIFY_PARAM *verify_params;

        verify_params = SSL_get0_param(local_connection);
        if (verify_params == NULL)
        {
            SSL_free(local_connection);
            SSL_CTX_free(local_context);
            return (FT_ERR_INVALID_OPERATION);
        }
        X509_VERIFY_PARAM_set_hostflags(verify_params, 0);
        if (X509_VERIFY_PARAM_set1_host(verify_params, host, 0) != 1)
        {
            SSL_free(local_connection);
            SSL_CTX_free(local_context);
            return (FT_ERR_INVALID_OPERATION);
        }
#endif
    }
    if (SSL_set_fd(local_connection, socket_fd) != 1)
    {
        SSL_free(local_connection);
        SSL_CTX_free(local_context);
        return (FT_ERR_INVALID_OPERATION);
    }
    *ssl_context = local_context;
    *ssl_connection = local_connection;
    return (FT_ERR_SUCCESS);
}

static int32_t http_client_establish_connection(const char *host, const char *port_string,
    ft_bool use_ssl, http_client_active_connection &connection)
{
    struct addrinfo address_hints;
    struct addrinfo *address_info;
    struct addrinfo *current_info;
    int32_t socket_fd;
    int32_t result;
    int32_t resolver_status;

    if (connection.entry.socket_fd >= 0)
        return (FT_ERR_SUCCESS);
    ft_memset(&address_hints, 0, sizeof(address_hints));
    address_hints.ai_family = AF_UNSPEC;
    address_hints.ai_socktype = SOCK_STREAM;
    resolver_status = getaddrinfo(host, port_string, &address_hints, &address_info);
    if (resolver_status != 0)
    {
        networking_dns_set_error(resolver_status);
        http_client_pool_disable_store(connection);
        return (FT_ERR_INVALID_OPERATION);
    }
    socket_fd = -1;
    result = -1;
    current_info = address_info;
    while (current_info != NULL)
    {
        socket_fd = nw_socket(current_info->ai_family, current_info->ai_socktype, current_info->ai_protocol);
        if (socket_fd >= 0)
        {
            result = nw_connect(socket_fd, current_info->ai_addr,
                FT_NETWORKING_SOCKLEN_CAST(current_info->ai_addrlen));
            if (result >= 0)
                break ;
            nw_close(socket_fd);
            socket_fd = -1;
        }
        else
        {
        }
        current_info = current_info->ai_next;
    }
    freeaddrinfo(address_info);
    if (socket_fd < 0 || result < 0)
    {
        http_client_pool_disable_store(connection);
        return (FT_ERR_INVALID_OPERATION);
    }
    connection.entry.socket_fd = socket_fd;
    connection.entry.use_ssl = use_ssl;
    if (use_ssl != FT_FALSE)
    {
        if (http_client_initialize_ssl(socket_fd, host, &connection.entry.ssl_context,
            &connection.entry.ssl_connection) != FT_ERR_SUCCESS)
        {
            nw_close(socket_fd);
            connection.entry.socket_fd = -1;
            http_client_pool_disable_store(connection);
            return (FT_ERR_INVALID_OPERATION);
        }
        result = SSL_connect(connection.entry.ssl_connection);
        if (result != 1)
        {
            SSL_free(connection.entry.ssl_connection);
            SSL_CTX_free(connection.entry.ssl_context);
            connection.entry.ssl_connection = NULL;
            connection.entry.ssl_context = NULL;
            nw_close(socket_fd);
            connection.entry.socket_fd = -1;
            http_client_pool_disable_store(connection);
            return (FT_ERR_INVALID_OPERATION);
        }
#if OPENSSL_VERSION_NUMBER >= 0x10002000L
        if (SSL_get_verify_result(connection.entry.ssl_connection) != X509_V_OK)
        {
            SSL_shutdown(connection.entry.ssl_connection);
            SSL_free(connection.entry.ssl_connection);
            SSL_CTX_free(connection.entry.ssl_context);
            connection.entry.ssl_connection = NULL;
            connection.entry.ssl_context = NULL;
            nw_close(socket_fd);
            connection.entry.socket_fd = -1;
            http_client_pool_disable_store(connection);
            return (FT_ERR_INVALID_OPERATION);
        }
#endif
    }
    return (FT_ERR_SUCCESS);
}

int32_t http_client_send_plain_request(int32_t socket_fd, const char *buffer, ft_size_t length)
{
    ft_size_t total_sent;
    ssize_t send_result;

    total_sent = 0;
    while (total_sent < length)
    {
        send_result = nw_send(socket_fd, buffer + total_sent, length - total_sent, 0);
        if (send_result < 0)
        {
            int32_t wait_result;

#ifdef _WIN32
            int32_t last_error;

            last_error = WSAGetLastError();
            if (last_error == WSAEINTR)
                continue ;
            if (last_error == WSAEWOULDBLOCK)
            {
                wait_result = http_client_wait_for_socket_ready(socket_fd, FT_TRUE);
                if (wait_result < 0)
                    return (FT_ERR_INVALID_OPERATION);
                continue ;
            }
#else
            if (errno == EINTR)
                continue ;
            if (errno == EWOULDBLOCK || errno == EAGAIN)
            {
                wait_result = http_client_wait_for_socket_ready(socket_fd, FT_TRUE);
                if (wait_result < 0)
                    return (FT_ERR_INVALID_OPERATION);
                continue ;
            }
#endif
            return (FT_ERR_INVALID_OPERATION);
        }
        if (send_result == 0)
        {
            return (FT_ERR_INVALID_OPERATION);
        }
        total_sent += static_cast<ft_size_t>(send_result);
    }
    return (FT_ERR_SUCCESS);
}

int32_t http_client_send_ssl_request(SSL *ssl_connection, const char *buffer, ft_size_t length)
{
    ft_size_t total_sent;
    ssize_t send_result;
    int32_t socket_fd;

    socket_fd = -1;
    if (ssl_connection != NULL)
        socket_fd = SSL_get_fd(ssl_connection);
    total_sent = 0;
    while (total_sent < length)
    {
        send_result = nw_ssl_write(ssl_connection, buffer + total_sent, length - total_sent);
        if (send_result <= 0)
        {
            int32_t ssl_error;
            int32_t wait_result;

            ssl_error = SSL_get_error(ssl_connection, static_cast<int32_t>(send_result));
            if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
            {
                ft_bool wait_for_write;

                wait_for_write = (ssl_error == SSL_ERROR_WANT_WRITE);
                wait_result = http_client_wait_for_socket_ready(socket_fd, wait_for_write);
                if (wait_result < 0)
                    return (FT_ERR_INVALID_OPERATION);
                continue ;
            }
            if (ssl_error == SSL_ERROR_SYSCALL)
            {
#ifdef _WIN32
                int32_t last_error;

                last_error = WSAGetLastError();
                if (last_error == WSAEINTR)
                    continue ;
                if (last_error == WSAEWOULDBLOCK)
                {
                    wait_result = http_client_wait_for_socket_ready(socket_fd, FT_TRUE);
                    if (wait_result < 0)
                        return (FT_ERR_INVALID_OPERATION);
                    continue ;
                }
                if (last_error != 0)
                {
                    return (FT_ERR_INVALID_OPERATION);
                }
#else
                if (errno == EINTR)
                    continue ;
                if (errno == EWOULDBLOCK || errno == EAGAIN)
                {
                    wait_result = http_client_wait_for_socket_ready(socket_fd, FT_TRUE);
                    if (wait_result < 0)
                        return (FT_ERR_INVALID_OPERATION);
                    continue ;
                }
                if (errno != 0)
                {
                    return (FT_ERR_INVALID_OPERATION);
                }
#endif
            }
            return (FT_ERR_INVALID_OPERATION);
        }
        total_sent += static_cast<ft_size_t>(send_result);
    }
    if (networking_check_ssl_after_send(ssl_connection) != 0)
        return (FT_ERR_INVALID_OPERATION);
    return (FT_ERR_SUCCESS);
}

static int32_t http_client_stream_state_init(http_stream_state &state, http_response_handler handler)
{
    if (state.header_buffer.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (state.headers.initialize() != FT_ERR_SUCCESS)
    {
        (void)state.header_buffer.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    state.status_code = 0;
    state.headers_ready = FT_FALSE;
    state.keep_alive_allowed = FT_FALSE;
    state.has_content_length = FT_FALSE;
    state.expected_body_length = 0;
    state.received_body_length = 0;
    state.saw_chunked_encoding = FT_FALSE;
    state.http_1_1 = FT_FALSE;
    state.handler = handler;
    return (FT_ERR_SUCCESS);
}

static int32_t http_client_parse_status(const ft_string &headers)
{
    const char  *header_cstr;
    ft_size_t      index;

    header_cstr = headers.c_str();
    index = 0;
    while (header_cstr[index] != '\0' && header_cstr[index] != ' ')
        index++;
    while (header_cstr[index] == ' ')
        index++;
    if (header_cstr[index] == '\0')
        return (0);
    return (ft_atoi(header_cstr + index));
}

static int32_t http_client_stream_handle_header(http_stream_state &state)
{
    const char  *terminator;
    ft_size_t      header_length;
    ft_size_t      body_length;

    terminator = ft_strstr(state.header_buffer.c_str(), "\r\n\r\n");
    if (terminator == NULL)
        return (0);
    header_length = static_cast<ft_size_t>(terminator - state.header_buffer.c_str()) + 4;
    state.headers = state.header_buffer;
    if (state.headers.size() > header_length)
        state.headers.erase(header_length, state.headers.size() - header_length);
    state.status_code = http_client_parse_status(state.headers);
    http_client_parse_header_metadata(state);
    body_length = state.header_buffer.size() - header_length;
    if (body_length > 0 && state.handler != NULL)
    {
        const char *body_ptr;
        ft_size_t deliver_length;

        body_ptr = state.header_buffer.c_str() + header_length;
        deliver_length = body_length;
        if (state.has_content_length != FT_FALSE)
        {
            ft_size_t remaining_bytes;

            if (state.expected_body_length > state.received_body_length)
                remaining_bytes = state.expected_body_length - state.received_body_length;
            else
                remaining_bytes = 0;
            if (deliver_length > remaining_bytes)
            {
                deliver_length = remaining_bytes;
                state.keep_alive_allowed = FT_FALSE;
            }
            state.received_body_length += deliver_length;
        }
        if (deliver_length > 0)
            state.handler(state.status_code, state.headers, body_ptr, deliver_length, FT_FALSE);
        if (deliver_length < body_length)
            state.keep_alive_allowed = FT_FALSE;
    }
    state.header_buffer.clear();
    state.headers_ready = FT_TRUE;
    return (1);
}

static int32_t http_client_receive_stream(http_client_active_connection &connection,
    http_response_handler handler, ft_bool &allow_keep_alive)
{
    char                buffer[1024];
    ssize_t             bytes_received;
    http_stream_state   state;
    int32_t                 socket_fd;
    SSL                 *ssl_connection;
    ft_bool                use_ssl;

    allow_keep_alive = FT_FALSE;
    if (handler == NULL)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (http_client_stream_state_init(state, handler) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    socket_fd = connection.entry.socket_fd;
    ssl_connection = connection.entry.ssl_connection;
    use_ssl = connection.entry.use_ssl;
    while (1)
    {
        if (use_ssl != FT_FALSE)
            bytes_received = nw_ssl_read(ssl_connection, buffer, sizeof(buffer) - 1);
        else
            bytes_received = nw_recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0';
            if (state.headers_ready == FT_FALSE)
            {
                state.header_buffer.append(buffer, static_cast<ft_size_t>(bytes_received));
                if (http_client_stream_handle_header(state) != 0)
                {
                    if (state.has_content_length != FT_FALSE
                        && state.received_body_length >= state.expected_body_length)
                        break ;
                    continue ;
                }
            }
            else
            {
                ft_size_t deliver_size;

                deliver_size = static_cast<ft_size_t>(bytes_received);
                if (state.has_content_length != FT_FALSE)
                {
                    ft_size_t remaining_bytes;

                    if (state.expected_body_length > state.received_body_length)
                        remaining_bytes = state.expected_body_length - state.received_body_length;
                    else
                        remaining_bytes = 0;
                    if (deliver_size > remaining_bytes)
                    {
                        deliver_size = remaining_bytes;
                        state.keep_alive_allowed = FT_FALSE;
                    }
                    state.received_body_length += deliver_size;
                }
                if (deliver_size > 0)
                    state.handler(state.status_code, state.headers, buffer, deliver_size, FT_FALSE);
            }
            if (state.has_content_length != FT_FALSE
                && state.received_body_length >= state.expected_body_length)
                break ;
        }
        else
        {
            if (use_ssl != FT_FALSE)
            {
                int32_t ssl_error;

                ssl_error = SSL_get_error(ssl_connection, static_cast<int32_t>(bytes_received));
                if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
                {
                    int32_t wait_result;
                    ft_bool wait_for_write;

                    wait_for_write = (ssl_error == SSL_ERROR_WANT_WRITE);
                    wait_result = http_client_wait_for_socket_ready(socket_fd, wait_for_write);
                    if (wait_result < 0)
                        return (FT_ERR_INVALID_OPERATION);
                    continue ;
                }
                if (ssl_error == SSL_ERROR_ZERO_RETURN)
                {
                    state.keep_alive_allowed = FT_FALSE;
                    break ;
                }
                if (ssl_error == SSL_ERROR_SYSCALL)
                {
#ifdef _WIN32
                    int32_t last_error;

                    last_error = WSAGetLastError();
                    if (last_error == WSAEINTR)
                        continue ;
                    if (last_error == WSAEWOULDBLOCK)
                    {
                        int32_t wait_result;

                        wait_result = http_client_wait_for_socket_ready(socket_fd, FT_FALSE);
                        if (wait_result < 0)
                            return (FT_ERR_INVALID_OPERATION);
                        continue ;
                    }
#else
                    int32_t last_error;

                    last_error = errno;
                    if (last_error == EINTR)
                        continue ;
                    if (last_error == EWOULDBLOCK || last_error == EAGAIN)
                    {
                        int32_t wait_result;

                        wait_result = http_client_wait_for_socket_ready(socket_fd, FT_FALSE);
                        if (wait_result < 0)
                            return (FT_ERR_INVALID_OPERATION);
                        continue ;
                    }
#endif
                    state.keep_alive_allowed = FT_FALSE;
                    return (FT_ERR_INVALID_OPERATION);
                }
                state.keep_alive_allowed = FT_FALSE;
                return (FT_ERR_INVALID_OPERATION);
            }
            if (bytes_received < 0)
            {
#ifdef _WIN32
                int32_t last_error;

                last_error = WSAGetLastError();
                if (last_error == WSAEINTR)
                    continue ;
                if (last_error == WSAEWOULDBLOCK)
                {
                    int32_t wait_result;

                    wait_result = http_client_wait_for_socket_ready(socket_fd, FT_FALSE);
                    if (wait_result < 0)
                        return (FT_ERR_INVALID_OPERATION);
                    continue ;
                }
                if (last_error == WSAECONNRESET)
                    break ;
#else
                int32_t last_error;

                last_error = errno;
                if (last_error == EINTR)
                    continue ;
                if (last_error == EWOULDBLOCK || last_error == EAGAIN)
                {
                    int32_t wait_result;

                    wait_result = http_client_wait_for_socket_ready(socket_fd, FT_FALSE);
                    if (wait_result < 0)
                        return (FT_ERR_INVALID_OPERATION);
                    continue ;
                }
                if (last_error == ECONNRESET)
                    break ;
#endif
                state.keep_alive_allowed = FT_FALSE;
                return (FT_ERR_INVALID_OPERATION);
            }
            state.keep_alive_allowed = FT_FALSE;
            break ;
        }
    }
    if (state.headers_ready == FT_FALSE && state.header_buffer.empty() == FT_FALSE)
    {
        const char *body_ptr;

        body_ptr = state.header_buffer.c_str();
        state.handler(state.status_code, state.headers, body_ptr,
            state.header_buffer.size(), FT_FALSE);
    }
    state.handler(state.status_code, state.headers, "", 0, FT_TRUE);
    if (state.headers_ready != FT_FALSE && state.keep_alive_allowed != FT_FALSE
        && state.has_content_length != FT_FALSE
        && state.received_body_length >= state.expected_body_length)
        allow_keep_alive = FT_TRUE;
    else
        allow_keep_alive = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static void http_client_buffering_adapter(int32_t status_code, const ft_string &headers,
    const char *body_chunk, ft_size_t chunk_size, ft_bool finished)
{
    if (g_http_buffer_adapter_state.response == NULL)
        return ;
    if (status_code != 0)
        g_http_buffer_adapter_state.status_code = status_code;
    if (g_http_buffer_adapter_state.header_appended == FT_FALSE && headers.empty() == FT_FALSE)
    {
        g_http_buffer_adapter_state.response->append(headers);
        g_http_buffer_adapter_state.header_appended = FT_TRUE;
    }
    if (chunk_size > 0)
    {
        g_http_buffer_adapter_state.response->append(body_chunk);
        g_http_buffer_adapter_state.body_bytes += chunk_size;
    }
    if (finished != FT_FALSE && status_code != 0)
        g_http_buffer_adapter_state.status_code = status_code;
    return ;
}

static void http_client_reset_buffer_adapter_state(void)
{
    g_http_buffer_adapter_state.response = NULL;
    g_http_buffer_adapter_state.header_appended = FT_FALSE;
    g_http_buffer_adapter_state.body_bytes = 0;
    g_http_buffer_adapter_state.status_code = 0;
    return ;
}

static int32_t http_client_finish_with_metrics(const char *method, const char *host,
    const char *path, ft_size_t request_bytes, int32_t result,
    t_monotonic_time_point start_time)
{
    ft_networking_observability_sample sample;
    t_monotonic_time_point finish_time;
    int64_t duration_ms;
    int32_t error_code;

    finish_time = time_monotonic_point_now();
    duration_ms = time_monotonic_point_diff_ms(start_time, finish_time);
    if (duration_ms < 0)
        duration_ms = 0;
    error_code = FT_ERR_SUCCESS;
    if (result != FT_ERR_SUCCESS)
        error_code = FT_ERR_IO;
    sample.labels.component = "http_client";
    sample.labels.operation = method;
    sample.labels.target = host;
    sample.labels.resource = path;
    sample.duration_ms = duration_ms;
    sample.request_bytes = request_bytes;
    sample.response_bytes = g_http_buffer_adapter_state.body_bytes;
    sample.status_code = g_http_buffer_adapter_state.status_code;
    sample.error_code = error_code;
    if (error_code == FT_ERR_SUCCESS)
    {
        sample.success = FT_TRUE;
        sample.error_tag = "ok";
    }
    else
    {
        sample.success = FT_FALSE;
        sample.error_tag = ft_strerror(error_code);
    }
    observability_networking_metrics_record(sample);
    http_client_reset_buffer_adapter_state();
    return (result);
}

int32_t http_get_stream(const char *host, const char *path, http_response_handler handler,
    ft_bool use_ssl, const char *custom_port)
{
    const char *port_string;
    ft_string request;
    http_client_active_connection connection;
    ft_bool allow_keep_alive;
    int32_t attempt;

    if (handler == NULL)
    {
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (custom_port != NULL && custom_port[0] != '\0')
        port_string = custom_port;
    else if (use_ssl)
        port_string = "443";
    else
        port_string = "80";
    if (request.initialize() != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    http_client_pool_reset_active(connection);
    attempt = 0;
    while (attempt < 2)
    {
        ft_bool reused_connection;
        int32_t send_result;
        int32_t receive_result;

        if (http_client_pool_acquire_connection(host, port_string, use_ssl,
            connection, reused_connection) != FT_ERR_SUCCESS)
            return (FT_ERR_INVALID_OPERATION);
        if (http_client_establish_connection(host, port_string, use_ssl, connection) != FT_ERR_SUCCESS)
        {
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (FT_ERR_INVALID_OPERATION);
        }
        request.clear();
        request.append("GET ");
        request.append(path);
        request.append(" HTTP/1.1\r\nHost: ");
        request.append(host);
        request.append("\r\nConnection: keep-alive\r\n\r\n");
        if (use_ssl != FT_FALSE)
            send_result = http_client_send_ssl_request(connection.entry.ssl_connection,
                request.c_str(), request.size());
        else
            send_result = http_client_send_plain_request(connection.entry.socket_fd,
                request.c_str(), request.size());
        if (send_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (FT_ERR_INVALID_OPERATION);
        }
        receive_result = http_client_receive_stream(connection, handler, allow_keep_alive);
        if (receive_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (FT_ERR_INVALID_OPERATION);
        }
        http_client_pool_release_connection(connection, allow_keep_alive);
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_INVALID_OPERATION);
}

int32_t http_get(const char *host, const char *path, ft_string &response, ft_bool use_ssl, const char *custom_port)
{
    t_monotonic_time_point start_time;
    int32_t result;

    start_time = time_monotonic_point_now();
    response.clear();
    g_http_buffer_adapter_state.response = &response;
    g_http_buffer_adapter_state.header_appended = FT_FALSE;
    g_http_buffer_adapter_state.body_bytes = 0;
    g_http_buffer_adapter_state.status_code = 0;
    result = http_get_stream(host, path, http_client_buffering_adapter, use_ssl, custom_port);
    return (http_client_finish_with_metrics("GET", host, path, 0, result, start_time));
}

int32_t http_post(const char *host, const char *path, const ft_string &body, ft_string &response, ft_bool use_ssl, const char *custom_port)
{
    t_monotonic_time_point start_time;
    const char *port_string;
    ft_string request;
    char length_string[32];
    http_client_active_connection connection;
    ft_bool allow_keep_alive;
    int32_t attempt;
    ft_size_t body_length;

    start_time = time_monotonic_point_now();
    response.clear();
    g_http_buffer_adapter_state.response = &response;
    g_http_buffer_adapter_state.header_appended = FT_FALSE;
    g_http_buffer_adapter_state.body_bytes = 0;
    g_http_buffer_adapter_state.status_code = 0;
    if (custom_port != NULL && custom_port[0] != '\0')
        port_string = custom_port;
    else if (use_ssl)
        port_string = "443";
    else
        port_string = "80";
    if (request.initialize() != FT_ERR_SUCCESS)
        return (http_client_finish_with_metrics("POST", host, path, body.size(),
            FT_ERR_NO_MEMORY, start_time));
    http_client_pool_reset_active(connection);
    body_length = body.size();
    std::snprintf(length_string, sizeof(length_string), "%llu",
        static_cast<unsigned long long>(body_length));
    attempt = 0;
    while (attempt < 2)
    {
        ft_bool reused_connection;
        int32_t send_result;
        int32_t receive_result;

        if (http_client_pool_acquire_connection(host, port_string, use_ssl,
            connection, reused_connection) != FT_ERR_SUCCESS)
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        if (http_client_establish_connection(host, port_string, use_ssl, connection) != FT_ERR_SUCCESS)
        {
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        }
        request.clear();
        request.append("POST ");
        request.append(path);
        request.append(" HTTP/1.1\r\nHost: ");
        request.append(host);
        request.append("\r\nContent-Length: ");
        request.append(length_string);
        request.append("\r\nConnection: keep-alive\r\n\r\n");
        request.append(body);
        if (use_ssl != FT_FALSE)
            send_result = http_client_send_ssl_request(connection.entry.ssl_connection,
                request.c_str(), request.size());
        else
            send_result = http_client_send_plain_request(connection.entry.socket_fd,
                request.c_str(), request.size());
        if (send_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        }
        receive_result = http_client_receive_stream(connection, http_client_buffering_adapter,
            allow_keep_alive);
        if (receive_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection != FT_FALSE)
            {
                attempt++;
                continue ;
            }
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        }
        http_client_pool_release_connection(connection, allow_keep_alive);
        return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_SUCCESS, start_time));
    }
    return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
}


#endif
