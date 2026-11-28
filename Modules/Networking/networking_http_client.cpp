#include "http_client.hpp"
#include "socket_class.hpp"
#include "socket_handle.hpp"
#include "networking.hpp"
#include "ssl_wrapper.hpp"
#include "openssl_support.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/pair.hpp"
#if NETWORKING_HAS_OPENSSL
#include <cstring>
#include <charconv>
#include <limits>
#include <new>
#include <mutex>
#include <string_view>
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
    ft_size_t           header_search_offset;
    int32_t                 status_code;
    ft_bool                headers_ready;
    ft_bool                keep_alive_allowed;
    ft_bool                has_content_length;
    ft_size_t              expected_body_length;
    ft_size_t              received_body_length;
    http_response_handler handler;
};

struct http_buffer_adapter_state
{
    ft_string   *response;
    ft_bool        header_appended;
    ft_size_t      body_bytes;
    int32_t         status_code;
    int32_t         error_code;
};

static thread_local http_buffer_adapter_state g_http_buffer_adapter_state{};

struct http_client_connection_entry
{
    ft_string               host;
    ft_string               port;
    ft_bool                    use_ssl;
    int32_t                     socket_fd = -1;
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

static std::mutex g_http_client_pool_mutex;
static ft_bool g_http_client_pool_enabled = FT_TRUE;
static std::vector<http_client_connection_entry *> g_http_client_pool_entries;
static ft_size_t g_http_client_pool_max_idle = 8;
static int64_t g_http_client_pool_idle_timeout_ms = 30000;
static ft_size_t g_http_client_pool_acquire_calls = 0;
static ft_size_t g_http_client_pool_reuse_hits = 0;
static ft_size_t g_http_client_pool_acquire_misses = 0;

#ifdef LIBFT_TEST_BUILD
static void http_client_pool_untrack_runtime_leaks(void)
{
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

static void http_client_pool_delete_entry(http_client_connection_entry *entry) noexcept;

int32_t http_client_pool_enable_thread_safety(void)
{
    g_http_client_pool_mutex.lock();
    g_http_client_pool_enabled = FT_TRUE;
    g_http_client_pool_mutex.unlock();
    return (FT_ERR_SUCCESS);
}

int32_t http_client_pool_disable_thread_safety(void)
{
    std::vector<http_client_connection_entry *> detached;
    g_http_client_pool_mutex.lock();
    g_http_client_pool_enabled = FT_FALSE;
    detached.swap(g_http_client_pool_entries);
    g_http_client_pool_mutex.unlock();
    ft_size_t index = 0;
    while (index < detached.size())
        http_client_pool_delete_entry(detached[index++]);
    return (FT_ERR_SUCCESS);
}

ft_bool http_client_pool_is_thread_safe(void)
{
    g_http_client_pool_mutex.lock();
    const ft_bool enabled = g_http_client_pool_enabled;
    g_http_client_pool_mutex.unlock();
    return (enabled);
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
    ft_size_t expired = 0;
    while (expired < g_http_client_pool_entries.size())
    {
        http_client_connection_entry *candidate = g_http_client_pool_entries[expired];
        if (candidate && time_monotonic_point_diff_ms(candidate->last_used, now)
            <= g_http_client_pool_idle_timeout_ms)
            break ;
        http_client_pool_delete_entry(candidate);
        ++expired;
    }
    if (expired != 0)
        g_http_client_pool_entries.erase(g_http_client_pool_entries.begin(),
            g_http_client_pool_entries.begin() + expired);
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
    t_monotonic_time_point now;
    http_client_connection_entry *pooled_entry;

    g_http_client_pool_mutex.lock();
    now = time_monotonic_point_now();
    http_client_pool_prune_locked(now);
    if (!g_http_client_pool_enabled
        || g_http_client_pool_entries.size() >= g_http_client_pool_max_idle)
    {
        g_http_client_pool_mutex.unlock();
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    pooled_entry = new (std::nothrow) http_client_connection_entry();
    if (pooled_entry == NULL)
    {
        g_http_client_pool_mutex.unlock();
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    if (pooled_entry->host.move(connection.entry.host) != FT_ERR_SUCCESS
        || pooled_entry->port.move(connection.entry.port) != FT_ERR_SUCCESS)
    {
        g_http_client_pool_mutex.unlock();
        http_client_pool_delete_entry(pooled_entry);
        http_client_pool_dispose_entry(connection.entry);
        http_client_pool_reset_active(connection);
        return ;
    }
    try
    {
        g_http_client_pool_entries.push_back(pooled_entry);
    }
    catch (const std::bad_alloc &)
    {
        g_http_client_pool_mutex.unlock();
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
#ifdef LIBFT_TEST_BUILD
    http_client_pool_untrack_runtime_leaks();
#endif
    g_http_client_pool_mutex.unlock();
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
    t_monotonic_time_point now;
    ft_size_t index;

    g_http_client_pool_mutex.lock();
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
            int32_t move_error = connection.entry.host.move(candidate->host);
            if (move_error == FT_ERR_SUCCESS)
                move_error = connection.entry.port.move(candidate->port);
            if (move_error != FT_ERR_SUCCESS)
            {
                g_http_client_pool_entries.erase(g_http_client_pool_entries.begin() + index);
                g_http_client_pool_mutex.unlock();
                http_client_pool_delete_entry(candidate);
                return (move_error);
            }
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
            g_http_client_pool_mutex.unlock();
            return (FT_ERR_SUCCESS);
        }
        index++;
    }
    g_http_client_pool_acquire_misses++;
    const ft_bool can_store = g_http_client_pool_enabled;
    g_http_client_pool_mutex.unlock();
    if (http_client_reinitialize_string(connection.entry.host) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    if (http_client_reinitialize_string(connection.entry.port) != FT_ERR_SUCCESS)
        return (FT_ERR_NO_MEMORY);
    int32_t key_error = connection.entry.host.append(host);
    if (key_error == FT_ERR_SUCCESS)
        key_error = connection.entry.port.append(port);
    if (key_error != FT_ERR_SUCCESS)
        return (key_error);
    connection.entry.use_ssl = use_ssl;
    connection.entry.socket_fd = -1;
    connection.entry.ssl_context = NULL;
    connection.entry.ssl_connection = NULL;
    connection.entry.last_used = now;
    connection.from_pool = FT_FALSE;
    connection.store_allowed = can_store;
    reused = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

static std::string_view http_client_trim_header(std::string_view value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
        value.remove_prefix(1);
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t'))
        value.remove_suffix(1);
    return (value);
}

static ft_bool http_client_header_equals(std::string_view value,
    std::string_view lowercase_name)
{
    if (value.size() != lowercase_name.size())
        return (FT_FALSE);
    while (!value.empty())
    {
        char character = value.front();

        if (character >= 'A' && character <= 'Z')
            character = static_cast<char>(character + ('a' - 'A'));
        if (character != lowercase_name.front())
            return (FT_FALSE);
        value.remove_prefix(1);
        lowercase_name.remove_prefix(1);
    }
    return (FT_TRUE);
}

static void http_client_parse_connection(std::string_view value,
    ft_bool &close_connection, ft_bool &keep_alive_requested)
{
    while (!value.empty())
    {
        const std::size_t separator = value.find(',');
        const std::string_view token = http_client_trim_header(value.substr(0, separator));
        if (http_client_header_equals(token, "close"))
            close_connection = FT_TRUE;
        else if (http_client_header_equals(token, "keep-alive"))
            keep_alive_requested = FT_TRUE;
        if (separator == std::string_view::npos)
            break ;
        value.remove_prefix(separator + 1);
    }
    return ;
}

static int32_t http_client_parse_content_length(http_stream_state &state,
    std::string_view value)
{
    while (1)
    {
        const std::size_t separator = value.find(',');
        const std::string_view number = http_client_trim_header(value.substr(0, separator));
        if (number.empty())
            return (FT_ERR_IO);
        ft_size_t parsed_length = 0;
        const char *const number_end = number.data() + number.size();
        const auto result = std::from_chars(number.data(), number_end, parsed_length);
        if (result.ec != std::errc{} || result.ptr != number_end
            || (state.has_content_length && state.expected_body_length != parsed_length))
            return (FT_ERR_IO);
        state.has_content_length = FT_TRUE;
        state.expected_body_length = parsed_length;
        if (separator == std::string_view::npos)
            return (FT_ERR_SUCCESS);
        value.remove_prefix(separator + 1);
    }
}

static int32_t http_client_parse_header_metadata(http_stream_state &state)
{
    std::string_view header(state.headers.c_str(),
        FT_UINT64_TO_NATIVE_SIZE_CAST(state.headers.size()));
    const std::size_t status_line_end = header.find("\r\n");
    if (status_line_end == std::string_view::npos)
        return (FT_ERR_IO);
    ft_bool keep_alive_requested = FT_FALSE;
    if (header.substr(0, 9) == "HTTP/1.1 ")
        keep_alive_requested = FT_TRUE;
    header.remove_prefix(status_line_end + 2);
    ft_bool close_connection = FT_FALSE;
    ft_bool has_transfer_encoding = FT_FALSE;
    while (!header.empty())
    {
        const std::size_t line_end = header.find("\r\n");
        if (line_end == std::string_view::npos || line_end == 0)
            break ;
        const std::string_view line = header.substr(0, line_end);
        header.remove_prefix(line_end + 2);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos)
            continue ;
        const std::string_view name = http_client_trim_header(line.substr(0, colon));
        if (http_client_header_equals(name, "connection"))
            http_client_parse_connection(line.substr(colon + 1), close_connection,
                keep_alive_requested);
        else if (http_client_header_equals(name, "content-length"))
        {
            const int32_t length_error = http_client_parse_content_length(state,
                line.substr(colon + 1));
            if (length_error != FT_ERR_SUCCESS)
                return (length_error);
        }
        else if (http_client_header_equals(name, "transfer-encoding"))
            has_transfer_encoding = FT_TRUE;
    }
    if (has_transfer_encoding && state.has_content_length)
        return (FT_ERR_IO);
    if (state.has_content_length && keep_alive_requested && !close_connection)
        state.keep_alive_allowed = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void http_client_pool_flush(void)
{
    std::vector<http_client_connection_entry *> detached;
    g_http_client_pool_mutex.lock();
    detached.swap(g_http_client_pool_entries);
    g_http_client_pool_mutex.unlock();
    ft_size_t index = 0;
    while (index < detached.size())
        http_client_pool_delete_entry(detached[index++]);
    return ;
}

void http_client_pool_set_max_idle(ft_size_t max_idle)
{

    g_http_client_pool_mutex.lock();
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
    g_http_client_pool_mutex.unlock();
    return ;
}

ft_size_t http_client_pool_get_idle_count(void)
{
    ft_size_t idle_count;

    g_http_client_pool_mutex.lock();
    idle_count = g_http_client_pool_entries.size();
    g_http_client_pool_mutex.unlock();
    return (idle_count);
}

void http_client_pool_debug_reset_counters(void)
{

    g_http_client_pool_mutex.lock();
    g_http_client_pool_acquire_calls = 0;
    g_http_client_pool_reuse_hits = 0;
    g_http_client_pool_acquire_misses = 0;
    g_http_client_pool_mutex.unlock();
    return ;
}

ft_size_t http_client_pool_debug_get_reuse_count(void)
{
    ft_size_t reuse_count;

    g_http_client_pool_mutex.lock();
    reuse_count = g_http_client_pool_reuse_hits;
    g_http_client_pool_mutex.unlock();
    return (reuse_count);
}

ft_size_t http_client_pool_debug_get_miss_count(void)
{
    ft_size_t miss_count;

    g_http_client_pool_mutex.lock();
    miss_count = g_http_client_pool_acquire_misses;
    g_http_client_pool_mutex.unlock();
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

static int32_t http_client_connect_resolved(const ft_vector<networking_resolved_address> &addresses)
{
    const ft_size_t address_count = addresses.size();
    ft_size_t index = 0;
    while (index < address_count)
    {
        const auto &address = addresses[index];
        const int32_t socket_fd = nw_socket(address.address.ss_family, SOCK_STREAM, IPPROTO_TCP);
        if (socket_fd >= 0)
        {
            if (nw_connect(socket_fd, reinterpret_cast<const sockaddr *>(&address.address),
                    address.length) >= 0)
                return (socket_fd);
            nw_close(socket_fd);
        }
        ++index;
    }
    return (-1);
}

static int32_t http_client_open_socket(const char *host, const char *port_string)
{
    ft_vector<networking_resolved_address> addresses;
    if (addresses.initialize() != FT_ERR_SUCCESS)
        return (-1);
#ifdef _WIN32
    // Keep Winsock active from DNS through all connection attempts.
    if (ft_socket_runtime_acquire() != FT_ERR_SUCCESS)
        return (-1);
#endif
    int32_t socket_fd = -1;
    if (networking_dns_resolve(host, port_string, AF_UNSPEC, SOCK_STREAM, IPPROTO_TCP, 0,
            addresses))
        socket_fd = http_client_connect_resolved(addresses);
#ifdef _WIN32
    ft_socket_runtime_release();
#endif
    return (socket_fd);
}

static int32_t http_client_establish_connection(const char *host, const char *port_string,
    ft_bool use_ssl, http_client_active_connection &connection)
{
    if (connection.entry.socket_fd >= 0)
        return (FT_ERR_SUCCESS);
    const int32_t socket_fd = http_client_open_socket(host, port_string);
    if (socket_fd < 0)
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
        const int32_t result = SSL_connect(connection.entry.ssl_connection);
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

static int32_t http_client_send_plain_span(int32_t socket_fd, const char *buffer,
    ft_size_t length, ft_bool &write_started)
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
        write_started = FT_TRUE;
    }
    return (FT_ERR_SUCCESS);
}

int32_t http_client_send_plain_request(int32_t socket_fd, const char *buffer, ft_size_t length)
{
    ft_bool write_started = FT_FALSE;
    return (http_client_send_plain_span(socket_fd, buffer, length, write_started));
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
        int32_t ssl_error;

        send_result = networking_ssl_write_with_status(ssl_connection,
            buffer + total_sent, length - total_sent, ssl_error);
        if (send_result <= 0)
        {
            int32_t wait_result;

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
    state.header_search_offset = 0;
    state.headers_ready = FT_FALSE;
    state.keep_alive_allowed = FT_FALSE;
    state.has_content_length = FT_FALSE;
    state.expected_body_length = 0;
    state.received_body_length = 0;
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

    const char *const buffer = state.header_buffer.c_str();
    const ft_size_t buffer_length = state.header_buffer.size();
    terminator = ft_strstr(buffer + state.header_search_offset, "\r\n\r\n");
    if (terminator == NULL)
    {
        if (buffer_length > 3U)
            state.header_search_offset = buffer_length - 3U;
        return (FT_ERR_SUCCESS);
    }
    header_length = static_cast<ft_size_t>(terminator - buffer) + 4;
    const int32_t copy_error = state.headers.assign(buffer, header_length);
    if (copy_error != FT_ERR_SUCCESS)
        return (copy_error);
    state.status_code = http_client_parse_status(state.headers);
    const int32_t metadata_error = http_client_parse_header_metadata(state);
    if (metadata_error != FT_ERR_SUCCESS)
        return (metadata_error);
    body_length = buffer_length - header_length;
    if (body_length > 0 && state.handler != NULL)
    {
        const char *body_ptr;
        ft_size_t deliver_length;

        body_ptr = buffer + header_length;
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
    return (FT_ERR_SUCCESS);
}

static int32_t http_client_receive_stream(http_client_active_connection &connection,
    http_response_handler handler, ft_bool &allow_keep_alive, ft_bool &response_started)
{
    char                buffer[1024];
    ssize_t             bytes_received;
    http_stream_state   state;
    int32_t                 socket_fd;
    SSL                 *ssl_connection;
    ft_bool                use_ssl;

    allow_keep_alive = FT_FALSE;
    response_started = FT_FALSE;
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
        int32_t ssl_error = SSL_ERROR_NONE;
        if (use_ssl != FT_FALSE)
            bytes_received = networking_ssl_read_with_status(ssl_connection,
                buffer, sizeof(buffer) - 1U, ssl_error);
        else
            bytes_received = nw_recv(socket_fd, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0)
        {
            response_started = FT_TRUE;
            buffer[bytes_received] = '\0';
            if (state.headers_ready == FT_FALSE)
            {
                const int32_t append_error = state.header_buffer.append(
                    buffer, static_cast<ft_size_t>(bytes_received));
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                const int32_t header_error = http_client_stream_handle_header(state);
                if (header_error != FT_ERR_SUCCESS)
                    return (header_error);
                if (state.headers_ready)
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
    if (!state.headers_ready || (state.has_content_length
            && state.received_body_length < state.expected_body_length))
        return (FT_ERR_IO);
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
    if (g_http_buffer_adapter_state.response == NULL
        || g_http_buffer_adapter_state.error_code != FT_ERR_SUCCESS)
        return ;
    if (status_code != 0)
        g_http_buffer_adapter_state.status_code = status_code;
    if (g_http_buffer_adapter_state.header_appended == FT_FALSE && headers.empty() == FT_FALSE)
    {
        g_http_buffer_adapter_state.error_code = g_http_buffer_adapter_state.response->append(headers);
        if (g_http_buffer_adapter_state.error_code != FT_ERR_SUCCESS)
            return ;
        g_http_buffer_adapter_state.header_appended = FT_TRUE;
    }
    if (chunk_size > 0)
    {
        g_http_buffer_adapter_state.error_code = g_http_buffer_adapter_state.response->append(
            body_chunk, chunk_size);
        if (g_http_buffer_adapter_state.error_code == FT_ERR_SUCCESS)
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
    g_http_buffer_adapter_state.error_code = FT_ERR_SUCCESS;
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

    if (result == FT_ERR_SUCCESS)
        result = g_http_buffer_adapter_state.error_code;
    if (!observability_networking_metrics_is_active())
    {
        http_client_reset_buffer_adapter_state();
        return (result);
    }
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
    http_client_reset_buffer_adapter_state();
    observability_networking_metrics_record(sample);
    return (result);
}

static int32_t http_client_build_request_head(ft_string &request, const char *method,
    const char *host, const char *path, const ft_size_t *body_length)
{
    if (!host || !path)
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t error_code = request.initialize();
    const char *const parts[] = {method, " ", path, " HTTP/1.1\r\nHost: ", host};
    ft_size_t index = 0;
    while (error_code == FT_ERR_SUCCESS && index < sizeof(parts) / sizeof(parts[0]))
    {
        error_code = request.append(parts[index]);
        ++index;
    }
    if (error_code == FT_ERR_SUCCESS && body_length)
    {
        constexpr ft_size_t DIGIT_CAPACITY = std::numeric_limits<ft_size_t>::digits10 + 1U;
        char length[DIGIT_CAPACITY];
        const auto text = std::to_chars(length, length + sizeof(length), *body_length);
        if (text.ec != std::errc{})
            return (FT_ERR_OUT_OF_RANGE);
        error_code = request.append("\r\nContent-Length: ");
        if (error_code == FT_ERR_SUCCESS)
            error_code = request.append(length, static_cast<ft_size_t>(text.ptr - length));
    }
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    return (request.append("\r\nConnection: keep-alive\r\n\r\n"));
}

static int32_t http_client_send_span(const http_client_active_connection &connection,
    const char *data, ft_size_t length, ft_bool &write_started)
{
    if (connection.entry.use_ssl)
    {
        // A failed TLS write can still have sent part of a record.
        if (length != 0)
            write_started = FT_TRUE;
        return (http_client_send_ssl_request(connection.entry.ssl_connection, data, length));
    }
    return (http_client_send_plain_span(connection.entry.socket_fd, data, length, write_started));
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
    const int32_t build_error = http_client_build_request_head(request, "GET", host, path,
        ft_nullptr);
    if (build_error != FT_ERR_SUCCESS)
        return (build_error);
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
        ft_bool write_started = FT_FALSE;
        send_result = http_client_send_span(connection, request.c_str(), request.size(),
            write_started);
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
        ft_bool response_started = FT_FALSE;
        receive_result = http_client_receive_stream(connection, handler, allow_keep_alive,
            response_started);
        if (receive_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection && !response_started)
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
    g_http_buffer_adapter_state.error_code = FT_ERR_SUCCESS;
    result = http_get_stream(host, path, http_client_buffering_adapter, use_ssl, custom_port);
    return (http_client_finish_with_metrics("GET", host, path, 0, result, start_time));
}

int32_t http_post(const char *host, const char *path, const ft_string &body, ft_string &response, ft_bool use_ssl, const char *custom_port)
{
    t_monotonic_time_point start_time;
    const char *port_string;
    ft_string request;
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
    g_http_buffer_adapter_state.error_code = FT_ERR_SUCCESS;
    if (custom_port != NULL && custom_port[0] != '\0')
        port_string = custom_port;
    else if (use_ssl)
        port_string = "443";
    else
        port_string = "80";
    ft_string body_snapshot;
    const ft_string *source = &body;
    if (body.is_thread_safe())
    {
        const int32_t snapshot_error = body_snapshot.initialize(body);
        if (snapshot_error != FT_ERR_SUCCESS)
            return (http_client_finish_with_metrics("POST", host, path, 0,
                snapshot_error, start_time));
        source = &body_snapshot;
    }
    body_length = source->size();
    int32_t build_error = http_client_build_request_head(request, "POST", host, path,
        &body_length);
    constexpr ft_size_t INLINE_BODY_LIMIT = 1024;
    const ft_bool send_body_separately = body_length > INLINE_BODY_LIMIT;
    if (build_error == FT_ERR_SUCCESS && !send_body_separately)
        build_error = request.append(*source);
    if (build_error != FT_ERR_SUCCESS)
        return (http_client_finish_with_metrics("POST", host, path, body_length,
            build_error, start_time));
    http_client_pool_reset_active(connection);
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
        ft_bool write_started = FT_FALSE;
        send_result = http_client_send_span(connection, request.c_str(), request.size(),
            write_started);
        if (send_result == FT_ERR_SUCCESS && send_body_separately)
            send_result = http_client_send_span(connection, source->c_str(), body_length,
                write_started);
        if (send_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            if (reused_connection && !write_started)
            {
                attempt++;
                continue ;
            }
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        }
        ft_bool response_started = FT_FALSE;
        receive_result = http_client_receive_stream(connection, http_client_buffering_adapter,
            allow_keep_alive, response_started);
        if (receive_result != FT_ERR_SUCCESS)
        {
            http_client_pool_disable_store(connection);
            http_client_pool_release_connection(connection, FT_FALSE);
            return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
        }
        http_client_pool_release_connection(connection, allow_keep_alive);
        return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_SUCCESS, start_time));
    }
    return (http_client_finish_with_metrics("POST", host, path, body_length, FT_ERR_INVALID_OPERATION, start_time));
}


#endif
