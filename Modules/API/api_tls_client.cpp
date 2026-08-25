#include "tls_client.hpp"
#include "../PThread/pthread_internal.hpp"
#include "api_internal.hpp"
#include "../Printf/printf.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Networking/socket_class.hpp"
#include "../Networking/ssl_wrapper.hpp"
#include "../Networking/networking.hpp"
#include "../Networking/openssl_support.hpp"
#include "../Networking/networking_ssl_compat.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Logger/logger.hpp"
#include "../Template/move.hpp"
#include "../System_utils/system_utils.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
#else
# include <netdb.h>
# include <arpa/inet.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <unistd.h>
#endif
#if NETWORKING_HAS_OPENSSL
#include <openssl/err.h>
#include <openssl/x509v3.h>
#include <openssl/x509.h>
#include <openssl/bio.h>
#include <openssl/asn1.h>
#include <openssl/evp.h>
#include <cstdint>
#include <utility>
#endif

#if NETWORKING_HAS_OPENSSL

static const ft_size_t TLS_STRING_NPOS = static_cast<ft_size_t>(-1);

static char tls_string_char_at(const ft_string &value, ft_size_t index)
{
    const char *data;

    if (index >= value.size())
        return ('\0');
    data = value.c_str();
    return (data[index]);
}

static int32_t tls_string_substr(const ft_string &value, ft_size_t start_index,
    ft_size_t length, ft_string &output)
{
    ft_size_t total_length;
    ft_size_t copy_length;
    const char *data;
    ft_size_t index;
    int32_t string_error;

    if (output.is_initialised() == FT_FALSE)
    {
        string_error = output.initialize();
        if (string_error != FT_ERR_SUCCESS)
            return (string_error);
    }
    else
    {
        string_error = output.clear();
        if (string_error != FT_ERR_SUCCESS)
            return (string_error);
    }
    total_length = value.size();
    if (start_index >= total_length)
        return (FT_ERR_SUCCESS);
    data = value.c_str();
    copy_length = length;
    if (start_index + copy_length > total_length)
        copy_length = total_length - start_index;
    index = 0;
    while (index < copy_length)
    {
        string_error = output.append(data[start_index + index]);
        if (string_error != FT_ERR_SUCCESS)
            return (string_error);
        index++;
    }
    return (FT_ERR_SUCCESS);
}

static ft_size_t tls_string_find_substring(const ft_string &value, const char *needle, ft_size_t start_index)
{
    const char *haystack;
    const char *search_start;
    const char *found;

    if (needle == ft_nullptr)
        return (TLS_STRING_NPOS);
    if (start_index > value.size())
        return (TLS_STRING_NPOS);
    haystack = value.c_str();
    search_start = haystack + start_index;
    found = ft_strstr(search_start, needle);
    if (found == ft_nullptr)
        return (TLS_STRING_NPOS);
    return (static_cast<ft_size_t>(found - haystack));
}

static ft_size_t tls_string_find_char(const ft_string &value, char needle, ft_size_t start_index)
{
    const char *haystack;
    const char *search_start;
    const char *found;

    if (start_index > value.size())
        return (TLS_STRING_NPOS);
    haystack = value.c_str();
    search_start = haystack + start_index;
    found = ft_strchr(search_start, needle);
    if (found == ft_nullptr)
        return (TLS_STRING_NPOS);
    return (static_cast<ft_size_t>(found - haystack));
}

static char tls_ascii_to_lower(char character)
{
    if (character >= 'A' && character <= 'Z')
        character = static_cast<char>(character + 32);
    return (character);
}

static void tls_trim_whitespace(ft_string &value)
{
    ft_size_t start_index;

    start_index = 0;
    while (start_index < value.size())
    {
        char current_char;

        current_char = tls_string_char_at(value, start_index);
        if (current_char != ' ' && current_char != '\t')
            break ;
        value.erase(start_index, 1);
    }
    ft_size_t end_index;

    end_index = value.size();
    while (end_index > 0)
    {
        char current_char;

        current_char = tls_string_char_at(value, end_index - 1);
        if (current_char != ' ' && current_char != '\t')
            break ;
        value.erase(end_index - 1, 1);
        end_index--;
    }
    return ;
}

static void tls_string_to_lower(ft_string &value)
{
    ft_string lowered_value;
    ft_size_t index;
    ft_size_t length;

    length = value.size();
    index = 0;
    while (index < length)
    {
        char current_char;

        current_char = tls_string_char_at(value, index);
        lowered_value.append(tls_ascii_to_lower(current_char));
        index++;
    }
    value = lowered_value;
    return ;
}

static ft_bool tls_header_equals(const ft_string &header_name, const char *target_name)
{
    ft_size_t index;

    if (target_name == ft_nullptr)
        return (FT_FALSE);
    index = 0;
    while (index < header_name.size() && target_name[index] != '\0')
    {
        char left_character;
        char right_character;

        left_character = tls_ascii_to_lower(tls_string_char_at(header_name, index));
        right_character = tls_ascii_to_lower(target_name[index]);
        if (left_character != right_character)
            return (FT_FALSE);
        index++;
    }
    if (index != header_name.size() || target_name[index] != '\0')
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool ssl_pointer_supports_network_checks(SSL *ssl_session)
{
    uintptr_t ssl_address;
    static const uintptr_t minimum_valid_address = 0x1000;

    if (ssl_session == ft_nullptr)
        return (FT_FALSE);
    ssl_address = reinterpret_cast<uintptr_t>(ssl_session);
    if (ssl_address < minimum_valid_address)
        return (FT_FALSE);
    if ((ssl_address & (sizeof(void *) - 1)) != 0)
        return (FT_FALSE);
    return (FT_TRUE);
}

static ssize_t ssl_send_all(SSL *ssl_session, const void *data, ft_size_t size)
{
    ft_size_t total = 0;
    const char *data_pointer = static_cast<const char*>(data);
    while (total < size)
    {
        ssize_t sent = nw_ssl_write(ssl_session, data_pointer + total, size - total);
        if (sent > 0)
        {
            total += sent;
            continue ;
        }
        if (sent < 0)
            return (-1);
        int32_t last_error = FT_ERR_CONFIGURATION;
        if (last_error == FT_ERR_SSL_WANT_READ || last_error == FT_ERR_SSL_WANT_WRITE)
        {
            if (ssl_pointer_supports_network_checks(ssl_session))
            {
                if (networking_check_ssl_after_send(ssl_session) != FT_ERR_SUCCESS)
                    return (-1);
            }
            continue ;
        }
        return (-1);
    }
    return (static_cast<ssize_t>(total));
}

static ft_bool tls_copy_bio_to_string(BIO *memory, ft_string &output)
{
    char *memory_data;
    int64_t memory_length;

    output.clear();
    if (output.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (memory == ft_nullptr)
    {
                return (FT_FALSE);
    }
    memory_length = BIO_get_mem_data(memory, &memory_data);
    if (memory_length < 0)
    {
                return (FT_FALSE);
    }
    if (memory_length == 0)
    {
                return (FT_TRUE);
    }
    output.append(memory_data, static_cast<ft_size_t>(memory_length));
    int32_t string_error = output.get_error();
    if (string_error != FT_ERR_SUCCESS)
    {
                return (FT_FALSE);
    }
        return (FT_TRUE);
}

static ft_bool tls_extract_x509_name(X509_NAME *name, ft_string &output)
{
    BIO *memory;
    ft_bool copy_result;

    if (name == ft_nullptr)
    {
        output.clear();
        int32_t string_error = output.get_error();
                return (string_error == FT_ERR_SUCCESS);
    }
    memory = BIO_new(BIO_s_mem());
    if (memory == ft_nullptr)
    {
                return (FT_FALSE);
    }
    if (X509_NAME_print_ex(memory, name, 0, XN_FLAG_RFC2253) < 0)
    {
        BIO_free(memory);
                return (FT_FALSE);
    }
    copy_result = tls_copy_bio_to_string(memory, output);
    BIO_free(memory);
    if (!copy_result)
    {
        int32_t last_error = FT_ERR_CONFIGURATION;
        if (last_error == FT_ERR_SUCCESS)
                    return (FT_FALSE);
    }
        return (FT_TRUE);
}

static ft_bool tls_extract_asn1_time(const ASN1_TIME *time_value, ft_string &output)
{
    BIO *memory;
    ft_bool copy_result;

    if (time_value == ft_nullptr)
    {
        output.clear();
        int32_t string_error = output.get_error();
                return (string_error == FT_ERR_SUCCESS);
    }
    memory = BIO_new(BIO_s_mem());
    if (memory == ft_nullptr)
    {
                return (FT_FALSE);
    }
    if (ASN1_TIME_print(memory, time_value) != 1)
    {
        BIO_free(memory);
                return (FT_FALSE);
    }
    copy_result = tls_copy_bio_to_string(memory, output);
    BIO_free(memory);
    if (!copy_result)
    {
        int32_t last_error = FT_ERR_CONFIGURATION;
        if (last_error == FT_ERR_SUCCESS)
                    return (FT_FALSE);
    }
        return (FT_TRUE);
}

static ft_bool tls_extract_serial_number(const ASN1_INTEGER *serial_value,
    ft_string &output)
{
    BIO *memory;
    ft_bool copy_result;

    if (serial_value == ft_nullptr)
    {
        output.clear();
        int32_t string_error = output.get_error();
                return (string_error == FT_ERR_SUCCESS);
    }
    memory = BIO_new(BIO_s_mem());
    if (memory == ft_nullptr)
    {
                return (FT_FALSE);
    }
    if (i2a_ASN1_INTEGER(memory, const_cast<ASN1_INTEGER*>(serial_value)) <= 0)
    {
        BIO_free(memory);
                return (FT_FALSE);
    }
    copy_result = tls_copy_bio_to_string(memory, output);
    BIO_free(memory);
    if (!copy_result)
    {
        int32_t last_error = FT_ERR_CONFIGURATION;
        if (last_error == FT_ERR_SUCCESS)
                    return (FT_FALSE);
    }
        return (FT_TRUE);
}

static ft_bool tls_compute_certificate_fingerprint(X509 *certificate,
    ft_string &output)
{
    unsigned char digest[EVP_MAX_MD_SIZE];
    uint32_t digest_length;
    ft_size_t index;
    char byte_buffer[3];

    output.clear();
    if (output.get_error() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (certificate == ft_nullptr)
    {
                return (FT_FALSE);
    }
    if (X509_digest(certificate, EVP_sha256(), digest, &digest_length) != 1)
    {
                return (FT_FALSE);
    }
    index = 0;
    while (index < digest_length)
    {
        if (pf_snprintf(byte_buffer, sizeof(byte_buffer), "%02X",
                digest[index]) < 0)
        {
                        return (FT_FALSE);
        }
        output.append(byte_buffer, 2);
        int32_t string_error = output.get_error();
        if (string_error != FT_ERR_SUCCESS)
        {
                        return (FT_FALSE);
        }
        if (index + 1 < digest_length)
        {
            output.append(':');
            int32_t separator_error = output.get_error();
            if (separator_error != FT_ERR_SUCCESS)
            {
                                return (FT_FALSE);
            }
        }
        index++;
    }
        return (FT_TRUE);
}

static ft_bool tls_fill_certificate_diagnostic(X509 *certificate,
    api_tls_certificate_diagnostics &diagnostic)
{
    const ASN1_TIME *not_before;
    const ASN1_TIME *not_after;
    const ASN1_INTEGER *serial_number;

    if (certificate == ft_nullptr)
    {
                return (FT_FALSE);
    }
    if (!tls_extract_x509_name(X509_get_subject_name(certificate),
            diagnostic.subject))
        return (FT_FALSE);
    if (!tls_extract_x509_name(X509_get_issuer_name(certificate),
            diagnostic.issuer))
        return (FT_FALSE);
    serial_number = X509_get_serialNumber(certificate);
    if (!tls_extract_serial_number(serial_number, diagnostic.serial_number))
        return (FT_FALSE);
    not_before = X509_get0_notBefore(certificate);
    if (!tls_extract_asn1_time(not_before, diagnostic.not_before))
        return (FT_FALSE);
    not_after = X509_get0_notAfter(certificate);
    if (!tls_extract_asn1_time(not_after, diagnostic.not_after))
        return (FT_FALSE);
    if (!tls_compute_certificate_fingerprint(certificate,
            diagnostic.fingerprint_sha256))
        return (FT_FALSE);
    return (FT_TRUE);
}

static ft_bool tls_append_certificate_diagnostic(
    api_tls_handshake_diagnostics &diagnostics, X509 *certificate)
{
    api_tls_certificate_diagnostics entry;
    ft_size_t size_before;
    ft_size_t size_after;

    if (!tls_fill_certificate_diagnostic(certificate, entry))
        return (FT_FALSE);
    size_before = diagnostics.certificates.size();
    diagnostics.certificates.push_back(ft_move(entry));
    size_after = diagnostics.certificates.size();
    if (size_after < size_before + 1)
        return (FT_FALSE);
    return (FT_TRUE);
}

static void tls_log_handshake_diagnostics(
    const api_tls_handshake_diagnostics &diagnostics, const ft_string &host)
{
    ft_size_t certificate_index;
    ft_size_t certificate_count;

    if (!ft_log_get_api_logging())
        return ;
    ft_log_info("api_tls_client::handshake host=%s protocol=%s cipher=%s "
                "certificate_count=%llu",
        host.c_str(), diagnostics.protocol.c_str(),
        diagnostics.cipher.c_str(),
        static_cast<unsigned long long>(diagnostics.certificates.size()));
    certificate_index = 0;
    certificate_count = diagnostics.certificates.size();
    while (certificate_index < certificate_count)
    {
        const api_tls_certificate_diagnostics &entry =
            diagnostics.certificates[certificate_index];
        ft_log_info("api_tls_client::certificate[%llu] subject=%s issuer=%s "
                    "serial=%s not_before=%s not_after=%s fingerprint_sha256=%s",
            static_cast<unsigned long long>(certificate_index),
            entry.subject.c_str(), entry.issuer.c_str(),
            entry.serial_number.c_str(), entry.not_before.c_str(),
            entry.not_after.c_str(), entry.fingerprint_sha256.c_str());
        certificate_index++;
    }
    return ;
}

api_tls_client::api_tls_client() noexcept
: _initialised_state(FT_CLASS_STATE_UNINITIALISED), _ctx(ft_nullptr),
  _ssl(ft_nullptr), _sock(-1), _host(), _port(0), _timeout(60000),
  _mutex(ft_nullptr), _is_shutting_down(FT_FALSE), _async_workers(),
  _handshake_diagnostics()
{
    return ;
}

api_tls_client::~api_tls_client()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

void api_tls_client::abort_lifecycle_error(const char *method_name,
    const char *reason) const noexcept
{
    errno_abort_lifecycle(this->_initialised_state, method_name, reason);
    return ;
}

void api_tls_client::abort_if_not_initialised(const char *method_name) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, method_name);
    return ;
}

int32_t api_tls_client::enable_thread_safety() noexcept
{
    pt_recursive_mutex *new_mutex;
    int32_t initialize_result;

    this->abort_if_not_initialised("api_tls_client::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    new_mutex = new (std::nothrow) pt_recursive_mutex();
    if (new_mutex == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    initialize_result = new_mutex->initialize();
    if (initialize_result != FT_ERR_SUCCESS)
    {
        delete new_mutex;
        return (initialize_result);
    }
    this->_mutex = new_mutex;
    return (FT_ERR_SUCCESS);
}

int32_t api_tls_client::disable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t destroy_result;

    this->abort_if_not_initialised("api_tls_client::disable_thread_safety");
    mutex_pointer = this->_mutex;
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_SUCCESS);
    this->_mutex = ft_nullptr;
    destroy_result = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_result != FT_ERR_SUCCESS)
        return (destroy_result);
    return (FT_ERR_SUCCESS);
}

ft_bool api_tls_client::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t api_tls_client::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        this->abort_lifecycle_error("api_tls_client::initialize",
            "initialize called on initialised instance");
    this->_ctx = ft_nullptr;
    this->_ssl = ft_nullptr;
    this->_sock = -1;
    this->_is_shutting_down = FT_FALSE;
    this->_async_workers.clear();
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    if (this->_host.empty())
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (!OPENSSL_init_ssl(0, ft_nullptr))
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_TERMINATED);
    }
    this->_ctx = SSL_CTX_new(TLS_client_method());
    if (this->_ctx == ft_nullptr)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_NO_MEMORY);
    }
    if (SSL_CTX_set_default_verify_paths(this->_ctx) != 1)
    {
        SSL_CTX_free(this->_ctx);
        this->_ctx = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_CONFIGURATION);
    }
    SSL_CTX_set_verify(this->_ctx, SSL_VERIFY_PEER, ft_nullptr);
    return (FT_ERR_SUCCESS);
}

int32_t api_tls_client::initialize(const char *host, uint16_t port,
    int32_t timeout) noexcept
{
    if (host == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    this->_port = port;
    this->_timeout = timeout;
    if (this->_host.initialize(host) != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (this->_host.get_error());
    }
    return (this->initialize());
}

int32_t api_tls_client::initialize(const api_tls_client &other) noexcept
{
    int32_t destroy_result;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        this->abort_lifecycle_error("api_tls_client::initialize(copy)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        destroy_result = this->destroy();
        if (destroy_result != FT_ERR_SUCCESS)
            return (destroy_result);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    return (this->initialize(other._host.c_str(), other._port, other._timeout));
}

int32_t api_tls_client::initialize(api_tls_client &&other) noexcept
{
    int32_t initialize_result;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        this->abort_lifecycle_error("api_tls_client::initialize(move)",
            "source is uninitialised");
    initialize_result = this->initialize(static_cast<const api_tls_client &>(other));
    if (initialize_result != FT_ERR_SUCCESS)
        return (initialize_result);
    if (other._initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)other.destroy();
    return (FT_ERR_SUCCESS);
}

int32_t api_tls_client::destroy() noexcept
{
    SSL *ssl_pointer;
    SSL_CTX *ctx_pointer;
    int32_t socket_fd;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    (void)this->disable_thread_safety();
    this->_is_shutting_down = FT_TRUE;
    ssl_pointer = this->_ssl;
    ctx_pointer = this->_ctx;
    socket_fd = this->_sock;
    this->_ssl = ft_nullptr;
    this->_ctx = ft_nullptr;
    this->_sock = -1;
    ft_size_t worker_index;

    worker_index = 0;
    while (worker_index < this->_async_workers.size())
    {
        if (this->_async_workers[worker_index].joinable())
            this->_async_workers[worker_index].join();
        worker_index += 1;
    }
    this->_async_workers.clear();
    if (ssl_pointer != ft_nullptr)
    {
        SSL_shutdown(ssl_pointer);
        SSL_free(ssl_pointer);
    }
    if (socket_fd >= 0)
        nw_close(socket_fd);
    if (ctx_pointer != ft_nullptr)
        SSL_CTX_free(ctx_pointer);
    (void)this->_host.destroy();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

uint32_t api_tls_client::move(api_tls_client &other) noexcept
{
    return (static_cast<uint32_t>(this->initialize(ft_move(other))));
}

ft_bool api_tls_client::is_valid() const
{
    ft_bool is_valid_value;

    this->abort_if_not_initialised("api_tls_client::is_valid");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    is_valid_value = (this->_ssl != ft_nullptr);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (is_valid_value);
}

char *api_tls_client::request(const char *method, const char *path, json_group *payload,
                              const char *headers, int32_t *status)
{

    this->abort_if_not_initialised("api_tls_client::request");
    if (method == ft_nullptr || path == ft_nullptr)
        return (ft_nullptr);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    if (this->_is_shutting_down)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    if (this->_ssl == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (ft_nullptr);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (ft_log_get_api_logging())
    {
        const char *log_method = "(null)";
        const char *log_path = "(null)";
        if (method)
            log_method = method;
        if (path)
            log_path = path;
        ft_log_debug("api_tls_client::request %s %s",
            log_method, log_path);
    }

    ft_string request;
    if (request.initialize(method) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    request += " ";
    request += path;
    request += " HTTP/1.1\r\nHost: ";
    request += this->_host.c_str();
    if (headers && headers[0])
    {
        request += "\r\n";
        request += headers;
    }

    ft_string body_string;
    if (payload)
    {
        char *temporary_string = json_write_to_string(payload);
        if (!temporary_string)
            return (ft_nullptr);
        body_string = temporary_string;
        cma_free(temporary_string);
        request += "\r\nContent-Type: application/json";
        if (!api_append_content_length_header(request, body_string.size()))
            return (ft_nullptr);
    }
    request += "\r\nConnection: keep-alive\r\n\r\n";
    if (payload)
        request += body_string.c_str();

    if (ssl_send_all(this->_ssl, request.c_str(), request.size()) < 0)
        return (ft_nullptr);

    ft_string response;
    char buffer[1024];
    ssize_t bytes_received;
    const char *header_end_ptr = ft_nullptr;

    while (!header_end_ptr)
    {
        bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
        if (bytes_received <= 0)
            return (ft_nullptr);
        buffer[bytes_received] = '\0';
        response += buffer;
        header_end_ptr = ft_strstr(response.c_str(), "\r\n\r\n");
    }

    if (status)
    {
        *status = -1;
        const char *space = ft_strchr(response.c_str(), ' ');
        if (space)
            *status = ft_atoi(space + 1);
    }

    ft_size_t header_len = static_cast<ft_size_t>(header_end_ptr - response.c_str()) + 4;
    ft_size_t content_length = 0;
    ft_bool has_content_length = FT_FALSE;
    ft_bool is_chunked = FT_FALSE;
    ft_string headers_section;
    if (tls_string_substr(response, 0, header_len, headers_section) != FT_ERR_SUCCESS)
        return (ft_nullptr);
    ft_size_t line_offset = 0;
    while (line_offset < headers_section.size())
    {
        ft_size_t line_end = tls_string_find_substring(headers_section, "\r\n", line_offset);
        if (line_end == TLS_STRING_NPOS)
            break ;
        ft_string header_line;
        if (tls_string_substr(headers_section, line_offset, line_end - line_offset,
                header_line) != FT_ERR_SUCCESS)
            return (ft_nullptr);
        line_offset = line_end + 2;
        if (header_line.empty())
            continue ;
        ft_size_t colon_index = tls_string_find_char(header_line, ':', 0);
        if (colon_index == TLS_STRING_NPOS)
            continue ;
        ft_string header_name;
        if (tls_string_substr(header_line, 0, colon_index, header_name) != FT_ERR_SUCCESS)
            return (ft_nullptr);
        tls_trim_whitespace(header_name);
        if (header_name.empty())
            continue ;
        ft_string header_value;
        if (tls_string_substr(header_line, colon_index + 1, header_line.size(),
                header_value) != FT_ERR_SUCCESS)
            return (ft_nullptr);
        tls_trim_whitespace(header_value);
        if (tls_header_equals(header_name, "Content-Length") && !has_content_length)
        {
            uint64_t parsed_length;
            uint64_t parsed_length_ull;
            const char *header_value_cstr;
            char *header_parse_end;

            header_value_cstr = header_value.c_str();
            header_parse_end = ft_nullptr;
            parsed_length = ft_strtoul(header_value_cstr, &header_parse_end, 10);
            if (!header_parse_end || header_parse_end == header_value_cstr)
                return (ft_nullptr);
            while (*header_parse_end != '\0')
            {
                if (*header_parse_end != ' ' && *header_parse_end != '\t')
                    return (ft_nullptr);
                header_parse_end += 1;
            }
            parsed_length_ull = static_cast<uint64_t>(parsed_length);
            if (parsed_length_ull > FT_SYSTEM_SIZE_MAX)
                return (ft_nullptr);
            content_length = static_cast<ft_size_t>(parsed_length);
            has_content_length = FT_TRUE;
        }
        if (tls_header_equals(header_name, "Transfer-Encoding"))
        {
            ft_string lowered_value;
            lowered_value = header_value;
            tls_string_to_lower(lowered_value);
            ft_size_t token_start = 0;
            while (token_start <= lowered_value.size())
            {
                ft_size_t token_end = token_start;
                while (token_end < lowered_value.size()
                    && tls_string_char_at(lowered_value, token_end) != ',')
                    token_end++;
                ft_string token_value;
                if (tls_string_substr(lowered_value, token_start, token_end - token_start,
                        token_value) != FT_ERR_SUCCESS)
                    return (ft_nullptr);
                tls_trim_whitespace(token_value);
                if (token_value == "chunked")
                    is_chunked = FT_TRUE;
                if (token_end == lowered_value.size())
                    break ;
                token_start = token_end + 1;
            }
        }
    }

    ft_string body;
    body += response.c_str() + header_len;

    if (is_chunked)
    {
        ft_string chunk_buffer;
        chunk_buffer += body.c_str();
        body.clear();
        ft_size_t parse_offset = 0;
        while (FT_TRUE)
        {
            const char *chunk_start = chunk_buffer.c_str() + parse_offset;
            const char *line_end = ft_strstr(chunk_start, "\r\n");
            while (!line_end)
            {
                bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
                if (bytes_received <= 0)
                    return (ft_nullptr);
                buffer[bytes_received] = '\0';
                chunk_buffer += buffer;
                chunk_start = chunk_buffer.c_str() + parse_offset;
                line_end = ft_strstr(chunk_start, "\r\n");
            }
            ft_size_t line_length = static_cast<ft_size_t>(line_end - chunk_start);
            ft_size_t index = 0;
            while (index < line_length && (chunk_start[index] == ' ' || chunk_start[index] == '\t'))
            {
                index++;
            }
            ft_string size_string;
            while (index < line_length && chunk_start[index] != ';')
            {
                size_string.append(chunk_start[index]);
                index++;
            }
            ft_size_t size_trim = size_string.size();
            while (size_trim > 0)
            {
                const char *size_cstr = size_string.c_str();
                if (size_cstr[size_trim - 1] != ' ' && size_cstr[size_trim - 1] != '\t')
                    break ;
                size_string.erase(size_trim - 1, 1);
                size_trim--;
            }
            uint64_t chunk_length = ft_strtoul(size_string.c_str(),
                    ft_nullptr, 16);
            parse_offset += line_length + 2;
            while (chunk_buffer.size() < parse_offset + chunk_length + 2)
            {
                bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
                if (bytes_received <= 0)
                    return (ft_nullptr);
                buffer[bytes_received] = '\0';
                chunk_buffer += buffer;
            }
            const char *data_ptr = chunk_buffer.c_str() + parse_offset;
            ft_size_t copy_index = 0;
            while (copy_index < chunk_length)
            {
                body.append(data_ptr[copy_index]);
                copy_index++;
            }
            parse_offset += chunk_length;
            if (chunk_buffer.size() < parse_offset + 2)
            {
                while (chunk_buffer.size() < parse_offset + 2)
                {
                    bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
                    if (bytes_received <= 0)
                        return (ft_nullptr);
                    buffer[bytes_received] = '\0';
                    chunk_buffer += buffer;
                }
            }
            if (chunk_length == 0)
            {
                ft_bool trailers_complete = FT_FALSE;
                while (!trailers_complete)
                {
                    const char *trailer_ptr = chunk_buffer.c_str() + parse_offset;
                    const char *trailer_end = ft_strstr(trailer_ptr, "\r\n");
                    while (!trailer_end)
                    {
                        bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
                        if (bytes_received <= 0)
                            return (ft_nullptr);
                        buffer[bytes_received] = '\0';
                        chunk_buffer += buffer;
                        trailer_ptr = chunk_buffer.c_str() + parse_offset;
                        trailer_end = ft_strstr(trailer_ptr, "\r\n");
                    }
                    ft_size_t trailer_length = static_cast<ft_size_t>(trailer_end - trailer_ptr);
                    parse_offset += trailer_length + 2;
                    if (trailer_length == 0)
                        trailers_complete = FT_TRUE;
                }
                break ;
            }
            parse_offset += 2;
        }
    }
    else if (has_content_length)
    {
        while (body.size() < content_length)
        {
            bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1);
            if (bytes_received <= 0)
                return (ft_nullptr);
            buffer[bytes_received] = '\0';
            body += buffer;
        }
        if (body.size() > content_length)
            body.erase(content_length, body.size() - content_length);
    }
    else
    {
        while ((bytes_received = nw_ssl_read(this->_ssl, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[bytes_received] = '\0';
            body += buffer;
        }
        if (bytes_received < 0)
            return (ft_nullptr);
    }

    char *result_body = adv_strdup(body.c_str());
    if (!result_body)
        return (ft_nullptr);
    return (result_body);
}

json_group *api_tls_client::request_json(const char *method, const char *path,
                                         json_group *payload,
                                         const char *headers, int32_t *status)
{
    char *body = this->request(method, path, payload, headers, status);
    if (!body)
        return (ft_nullptr);
    json_group *result = json_read_from_string(body);
    cma_free(body);
    if (!result)
        return (ft_nullptr);
    return (result);
}

ft_bool api_tls_client::request_async(const char *method, const char *path,
                                   json_group *payload,
                                   const char *headers,
                                   api_callback callback,
                                   void *user_data)
{

    this->abort_if_not_initialised("api_tls_client::request_async");
    if (!callback)
        return (FT_FALSE);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_is_shutting_down)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    ft_thread worker([this, method, path, payload, headers, callback, user_data]()
    {
        int32_t status_local = -1;
        char *result_body = this->request(method, path, payload, headers, &status_local);
        callback(result_body, status_local, user_data);
    });
    if (!worker.joinable())
        return (FT_FALSE);
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_is_shutting_down)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        if (worker.joinable())
            worker.join();
        return (FT_FALSE);
    }
    ft_size_t worker_count_before;
    ft_size_t worker_count_after;

    worker_count_before = this->_async_workers.size();
    this->_async_workers.push_back(ft_move(worker));
    worker_count_after = this->_async_workers.size();
    if (worker_count_after < worker_count_before + 1)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        if (worker.joinable())
            worker.join();
        return (FT_FALSE);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_TRUE);
}

ft_bool api_tls_client::populate_handshake_diagnostics()
{
    const char *protocol_name;
    const SSL_CIPHER *cipher;
    const char *cipher_name;
    STACK_OF(X509) *certificate_chain;
    int32_t certificate_count;
    int32_t certificate_index;
    X509 *certificate;
    X509 *leaf_certificate;

    this->abort_if_not_initialised("api_tls_client::populate_handshake_diagnostics");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_is_shutting_down)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    if (this->_ssl == ft_nullptr)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    this->_handshake_diagnostics.protocol.clear();
    if (this->_handshake_diagnostics.protocol.get_error() != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    this->_handshake_diagnostics.cipher.clear();
    if (this->_handshake_diagnostics.cipher.get_error() != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    this->_handshake_diagnostics.certificates.clear();
    if (this->_handshake_diagnostics.certificates.get_error() != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    protocol_name = SSL_get_version(this->_ssl);
    if (protocol_name != ft_nullptr)
    {
        this->_handshake_diagnostics.protocol.append(protocol_name);
        if (this->_handshake_diagnostics.protocol.get_error() != FT_ERR_SUCCESS)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (FT_FALSE);
        }
    }
    cipher = SSL_get_current_cipher(this->_ssl);
    if (cipher != ft_nullptr)
    {
        cipher_name = SSL_CIPHER_get_name(cipher);
        if (cipher_name != ft_nullptr)
        {
            this->_handshake_diagnostics.cipher.append(cipher_name);
            if (this->_handshake_diagnostics.cipher.get_error() != FT_ERR_SUCCESS)
            {
                (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
                return (FT_FALSE);
            }
        }
    }
    certificate_chain = SSL_get_peer_cert_chain(this->_ssl);
    if (certificate_chain != ft_nullptr)
    {
        certificate_count = sk_X509_num(certificate_chain);
        certificate_index = 0;
        while (certificate_index < certificate_count)
        {
            certificate = sk_X509_value(certificate_chain, certificate_index);
            if (certificate != ft_nullptr)
            {
                if (!tls_append_certificate_diagnostic(this->_handshake_diagnostics,
                        certificate))
                {
                    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
                    return (FT_FALSE);
                }
            }
            certificate_index++;
        }
    }
    else
    {
        leaf_certificate = SSL_get1_peer_certificate(this->_ssl);
        if (leaf_certificate == ft_nullptr)
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (FT_FALSE);
        }
        if (!tls_append_certificate_diagnostic(this->_handshake_diagnostics,
                leaf_certificate))
        {
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            X509_free(leaf_certificate);
            return (FT_FALSE);
        }
        X509_free(leaf_certificate);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    tls_log_handshake_diagnostics(this->_handshake_diagnostics, this->_host);
    return (FT_TRUE);
}

ft_bool api_tls_client::refresh_handshake_diagnostics()
{

    this->abort_if_not_initialised("api_tls_client::refresh_handshake_diagnostics");
    if (pt_recursive_mutex_lock_if_not_null(this->_mutex) != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_is_shutting_down)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_FALSE);
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (!this->populate_handshake_diagnostics())
        return (FT_FALSE);
    return (FT_TRUE);
}

const api_tls_handshake_diagnostics &api_tls_client::get_handshake_diagnostics() const noexcept
{
    this->abort_if_not_initialised("api_tls_client::get_handshake_diagnostics");
    return (this->_handshake_diagnostics);
}

#ifdef LIBFT_TEST_BUILD
pt_recursive_mutex *api_tls_client::get_mutex_for_validation() const noexcept
{
    this->abort_if_not_initialised("api_tls_client::get_mutex_for_validation");
    return (this->_mutex);
}
#endif

#endif
