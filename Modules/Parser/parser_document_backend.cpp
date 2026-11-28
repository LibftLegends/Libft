#include "document_backend.hpp"
#include "../System_utils/system_utils.hpp"
#include "../CMA/CMA.hpp"
#include "../Networking/http_client.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../Printf/printf.hpp"
#include "../Basic/basic.hpp"
#include "../Observability/observability.hpp"
#include <fcntl.h>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

static void parser_observability_emit(const char *operation, const char *resource,
    ft_observability_trace_phase phase, int32_t error_code,
    ft_size_t bytes_read, ft_size_t bytes_written)
{
    ft_observability_trace_event event;
    ft_bool success;

    event.module = FT_OBSERVABILITY_MODULE_PARSER;
    event.phase = phase;
    event.operation = operation;
    event.resource = resource;
    event.error_code = error_code;
    event.bytes_read = bytes_read;
    event.bytes_written = bytes_written;
    (void)observability_trace_emit(&event);
    if (phase == FT_OBSERVABILITY_TRACE_FINISH)
    {
        success = FT_FALSE;
        if (error_code == FT_ERR_SUCCESS)
            success = FT_TRUE;
        (void)observability_record_operation(FT_OBSERVABILITY_MODULE_PARSER,
            success, bytes_read, bytes_written);
    }
    return ;
}

ft_document_source::ft_document_source() noexcept
    : _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_document_source::~ft_document_source()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_document_source::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_document_source::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_source::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t ft_document_source::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_source::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
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
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_source::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_document_source::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t ft_document_source::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_source::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == false)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_source::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_source::lock");
    return (this->lock_internal(lock_acquired));
}

void ft_document_source::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_source::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}


ft_document_sink::ft_document_sink() noexcept
    : _mutex(ft_nullptr), _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

ft_document_sink::~ft_document_sink()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_document_sink::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_document_sink::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_sink::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (disable_error);
}

int32_t ft_document_sink::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t initialize_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_sink::enable_thread_safety");
    if (this->_mutex != ft_nullptr)
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
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_sink::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    return (destroy_error);
}

ft_bool ft_document_sink::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t ft_document_sink::lock_internal(ft_bool *lock_acquired) const noexcept
{
    int32_t lock_error;

    if (lock_acquired != ft_nullptr)
        *lock_acquired = false;
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (lock_acquired != ft_nullptr)
        *lock_acquired = true;
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_sink::unlock_internal(ft_bool lock_acquired) const noexcept
{
    if (lock_acquired == false)
        return (FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t ft_document_sink::lock(ft_bool *lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_sink::lock");
    return (this->lock_internal(lock_acquired));
}

void ft_document_sink::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_document_sink::unlock");
    (void)this->unlock_internal(lock_acquired);
    return ;
}


ft_file_document_source::ft_file_document_source() noexcept
    : _path()
{
    return ;
}

ft_file_document_source::~ft_file_document_source()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_file_document_source::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_file_document_source::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t status = this->_path.initialize();
    if (status != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (status);
    }
    return (ft_document_source::initialize());
}

int32_t ft_file_document_source::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = ft_document_source::destroy();
    const int32_t path_error = this->_path.destroy();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (path_error);
}

void ft_file_document_source::set_path(const char *file_path) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_source::set_path");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (file_path == ft_nullptr)
        this->_path.clear();
    else
        this->_path = file_path;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const char *ft_file_document_source::get_path() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_source::get_path");
    return (this->_path.c_str());
}

int32_t ft_file_document_source::read_all(ft_string &output)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_source::read_all");
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    su_file *stream = su_fopen(this->_path.c_str());
    (void)this->unlock_internal(lock_acquired);
    if (!stream)
        return (FT_ERR_IO);

    int64_t stream_size = -1;
    if (su_fseek(stream, 0, SEEK_END) == 0)
        stream_size = su_ftell(stream);
    if (stream_size < 0 || su_fseek(stream, 0, SEEK_SET) != 0)
        status = FT_ERR_IO;

    const ft_size_t size = static_cast<ft_size_t>(stream_size);
    char *buffer = ft_nullptr;
    if (status == FT_ERR_SUCCESS && size >= SIZE_MAX)
        status = FT_ERR_OUT_OF_RANGE;
    if (status == FT_ERR_SUCCESS)
    {
        buffer = static_cast<char *>(cma_malloc(size + 1));
        if (!buffer)
            status = FT_ERR_NO_MEMORY;
        else if (su_fread(buffer, 1, size, stream) != size)
            status = FT_ERR_IO;
    }
    if (su_fclose(stream) != 0 && status == FT_ERR_SUCCESS)
        status = FT_ERR_IO;
    if (status == FT_ERR_SUCCESS)
    {
        buffer[size] = '\0';
        status = ft_string_take_owned_buffer(output, buffer, size);
        if (status == FT_ERR_SUCCESS)
            return (FT_ERR_SUCCESS);
    }
    cma_free(buffer);
    return (status);
}


ft_file_document_sink::ft_file_document_sink() noexcept
    : _path()
{
    return ;
}

ft_file_document_sink::~ft_file_document_sink()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_file_document_sink::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_file_document_sink::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    const int32_t status = this->_path.initialize();
    if (status != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (status);
    }
    return (ft_document_sink::initialize());
}

int32_t ft_file_document_sink::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = ft_document_sink::destroy();
    const int32_t path_error = this->_path.destroy();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    return (path_error);
}

void ft_file_document_sink::set_path(const char *file_path) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_sink::set_path");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    if (file_path == ft_nullptr)
        this->_path.clear();
    else
        this->_path = file_path;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const char *ft_file_document_sink::get_path() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_sink::get_path");
    return (this->_path.c_str());
}

int32_t ft_file_document_sink::write_all(const char *data_pointer, ft_size_t data_length)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_file_document_sink::write_all");
    if (data_pointer == ft_nullptr && data_length != 0)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    su_file *stream = su_fopen(this->_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    (void)this->unlock_internal(lock_acquired);
    if (!stream)
        return (FT_ERR_IO);

    if (data_length != 0 && su_fwrite(data_pointer, 1, data_length, stream) != data_length)
        status = FT_ERR_IO;
    if (su_fclose(stream) != 0 && status == FT_ERR_SUCCESS)
        status = FT_ERR_IO;
    return (status);
}

ft_memory_document_source::ft_memory_document_source() noexcept
    : _data_pointer(ft_nullptr), _data_length(0)
{
    return ;
}

ft_memory_document_source::~ft_memory_document_source()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_memory_document_source::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = ft_document_source::destroy();
    this->_data_pointer = ft_nullptr;
    this->_data_length = 0;
    return (disable_error);
}

void ft_memory_document_source::set_data(const char *data_pointer,
    ft_size_t data_length) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_source::set_data");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_data_pointer = data_pointer;
    this->_data_length = data_length;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const char *ft_memory_document_source::get_data() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_source::get_data");
    return (this->_data_pointer);
}

ft_size_t ft_memory_document_source::get_length() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_source::get_length");
    return (this->_data_length);
}

int32_t ft_memory_document_source::read_all(ft_string &output)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_source::read_all");
    parser_observability_emit("memory_source_read_all", "memory",
        FT_OBSERVABILITY_TRACE_START, FT_ERR_SUCCESS, 0, 0);
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    ft_size_t bytes_read = 0;
    if (status == FT_ERR_SUCCESS)
    {
        if (!this->_data_pointer && this->_data_length != 0)
            status = FT_ERR_INVALID_ARGUMENT;
        else
        {
            status = output.assign(this->_data_pointer, this->_data_length);
            if (status == FT_ERR_SUCCESS)
                bytes_read = this->_data_length;
        }
        (void)this->unlock_internal(lock_acquired);
    }
    parser_observability_emit("memory_source_read_all", "memory",
        FT_OBSERVABILITY_TRACE_FINISH, status, bytes_read, 0);
    return (status);
}


ft_memory_document_sink::ft_memory_document_sink() noexcept
    : _storage_pointer(ft_nullptr)
{
    return ;
}

ft_memory_document_sink::~ft_memory_document_sink()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_memory_document_sink::destroy() noexcept
{
    int32_t disable_error;

    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    disable_error = ft_document_sink::destroy();
    this->_storage_pointer = ft_nullptr;
    return (disable_error);
}

void ft_memory_document_sink::set_storage(ft_string *storage_pointer) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_sink::set_storage");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->_storage_pointer = storage_pointer;
    (void)this->unlock_internal(lock_acquired);
    return ;
}

ft_string *ft_memory_document_sink::get_storage() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_sink::get_storage");
    return (this->_storage_pointer);
}

int32_t ft_memory_document_sink::write_all(const char *data_pointer, ft_size_t data_length)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_memory_document_sink::write_all");
    parser_observability_emit("memory_sink_write_all", "memory",
        FT_OBSERVABILITY_TRACE_START, FT_ERR_SUCCESS, 0, 0);
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    ft_size_t bytes_written = 0;
    if (status == FT_ERR_SUCCESS)
    {
        if (!this->_storage_pointer)
            status = FT_ERR_INVALID_STATE;
        else if (!data_pointer && data_length != 0)
            status = FT_ERR_INVALID_ARGUMENT;
        else
        {
            status = this->_storage_pointer->assign(data_pointer, data_length);
            if (status == FT_ERR_SUCCESS)
                bytes_written = data_length;
        }
        (void)this->unlock_internal(lock_acquired);
    }
    parser_observability_emit("memory_sink_write_all", "memory",
        FT_OBSERVABILITY_TRACE_FINISH, status, 0, bytes_written);
    return (status);
}


#if NETWORKING_HAS_OPENSSL
struct parser_http_endpoint
{
    ft_string host_storage;
    ft_string path_storage;
    ft_string port_storage;
    const char *host;
    const char *path;
    const char *port;
    ft_bool use_ssl;
};

static int32_t parser_http_capture_endpoint(parser_http_endpoint &endpoint,
    const ft_string &host, const ft_string &path, const ft_string &port,
    ft_bool use_ssl, ft_bool copy_configuration) noexcept
{
    if (host.empty() || path.empty())
        return (FT_ERR_INVALID_STATE);
    endpoint.host = host.c_str();
    endpoint.path = path.c_str();
    endpoint.port = ft_nullptr;
    endpoint.use_ssl = use_ssl;
    if (!port.empty())
        endpoint.port = port.c_str();
    if (!copy_configuration)
        return (FT_ERR_SUCCESS);

    int32_t status = endpoint.host_storage.initialize(host);
    if (status == FT_ERR_SUCCESS)
        status = endpoint.path_storage.initialize(path);
    if (status == FT_ERR_SUCCESS && endpoint.port)
        status = endpoint.port_storage.initialize(port);
    if (status != FT_ERR_SUCCESS)
        return (status);
    endpoint.host = endpoint.host_storage.c_str();
    endpoint.path = endpoint.path_storage.c_str();
    if (endpoint.port)
        endpoint.port = endpoint.port_storage.c_str();
    return (FT_ERR_SUCCESS);
}

static int32_t parser_http_initialize_option(ft_string &option, const char *value) noexcept
{
    if (!value || value[0] == '\0')
        return (option.initialize());
    return (option.initialize(value));
}

static void parser_http_configure(ft_string &host_storage, ft_string &path_storage,
    ft_string &port_storage, ft_bool &ssl_storage, const char *host,
    const char *path, const char *port, ft_bool use_ssl) noexcept
{
    ft_string new_host;
    ft_string new_path;
    ft_string new_port;
    if (parser_http_initialize_option(new_host, host) != FT_ERR_SUCCESS
        || parser_http_initialize_option(new_path, path) != FT_ERR_SUCCESS
        || parser_http_initialize_option(new_port, port) != FT_ERR_SUCCESS)
        return ;
    (void)host_storage.move(new_host);
    (void)path_storage.move(new_path);
    (void)port_storage.move(new_port);
    ssl_storage = use_ssl;
    return ;
}

ft_http_document_source::ft_http_document_source() noexcept
    : _host(), _path(), _port(), _use_ssl(false)
{
    return ;
}

ft_http_document_source::~ft_http_document_source()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_http_document_source::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_http_document_source::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t status = this->_host.initialize();
    if (status == FT_ERR_SUCCESS)
        status = this->_path.initialize();
    if (status == FT_ERR_SUCCESS)
        status = this->_port.initialize();
    if (status != FT_ERR_SUCCESS)
    {
        (void)this->_host.destroy();
        (void)this->_path.destroy();
        (void)this->_port.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (status);
    }
    return (ft_document_source::initialize());
}

int32_t ft_http_document_source::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t disable_error = ft_document_source::destroy();
    const int32_t host_error = this->_host.destroy();
    const int32_t path_error = this->_path.destroy();
    const int32_t port_error = this->_port.destroy();
    this->_use_ssl = FT_FALSE;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    if (host_error != FT_ERR_SUCCESS)
        return (host_error);
    if (path_error != FT_ERR_SUCCESS)
        return (path_error);
    return (port_error);
}

void ft_http_document_source::configure(const char *host, const char *path,
    ft_bool use_ssl, const char *port) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::configure");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    parser_http_configure(this->_host, this->_path, this->_port, this->_use_ssl,
        host, path, port, use_ssl);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const char *ft_http_document_source::get_host() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::get_host");
    return (this->_host.c_str());
}

const char *ft_http_document_source::get_path() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::get_path");
    return (this->_path.c_str());
}

const char *ft_http_document_source::get_port() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::get_port");
    if (this->_port.size() == 0)
        return (ft_nullptr);
    return (this->_port.c_str());
}

ft_bool ft_http_document_source::is_ssl_enabled() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::is_ssl_enabled");
    return (this->_use_ssl);
}

int32_t ft_http_document_source::read_all(ft_string &output)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_source::read_all");
    parser_http_endpoint endpoint;
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = parser_http_capture_endpoint(endpoint, this->_host, this->_path,
        this->_port, this->_use_ssl, this->is_thread_safe());
    (void)this->unlock_internal(lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);

    ft_string response;
    status = response.initialize();
    if (status == FT_ERR_SUCCESS)
        status = http_get_body(endpoint.host, endpoint.path, response, endpoint.use_ssl, endpoint.port);
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (output.move(response));
}


ft_http_document_sink::ft_http_document_sink() noexcept
    : _host(), _path(), _port(), _use_ssl(false)
{
    return ;
}

ft_http_document_sink::~ft_http_document_sink()
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}


int32_t ft_http_document_sink::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        errno_abort_lifecycle(this->_initialised_state, "ft_http_document_sink::initialize", "called while object is already initialised");
        return (FT_ERR_INVALID_STATE);
    }
    int32_t status = this->_host.initialize();
    if (status == FT_ERR_SUCCESS)
        status = this->_path.initialize();
    if (status == FT_ERR_SUCCESS)
        status = this->_port.initialize();
    if (status != FT_ERR_SUCCESS)
    {
        (void)this->_host.destroy();
        (void)this->_path.destroy();
        (void)this->_port.destroy();
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (status);
    }
    return (ft_document_sink::initialize());
}

int32_t ft_http_document_sink::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t disable_error = ft_document_sink::destroy();
    const int32_t host_error = this->_host.destroy();
    const int32_t path_error = this->_path.destroy();
    const int32_t port_error = this->_port.destroy();
    this->_use_ssl = FT_FALSE;
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    if (host_error != FT_ERR_SUCCESS)
        return (host_error);
    if (path_error != FT_ERR_SUCCESS)
        return (path_error);
    return (port_error);
}

void ft_http_document_sink::configure(const char *host, const char *path,
    ft_bool use_ssl, const char *port) noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::configure");
    lock_acquired = false;
    lock_error = this->lock_internal(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    parser_http_configure(this->_host, this->_path, this->_port, this->_use_ssl,
        host, path, port, use_ssl);
    (void)this->unlock_internal(lock_acquired);
    return ;
}

const char *ft_http_document_sink::get_host() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::get_host");
    return (this->_host.c_str());
}

const char *ft_http_document_sink::get_path() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::get_path");
    return (this->_path.c_str());
}

const char *ft_http_document_sink::get_port() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::get_port");
    if (this->_port.size() == 0)
        return (ft_nullptr);
    return (this->_port.c_str());
}

ft_bool ft_http_document_sink::is_ssl_enabled() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::is_ssl_enabled");
    return (this->_use_ssl);
}

int32_t ft_http_document_sink::write_all(const char *data_pointer, ft_size_t data_length)
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_http_document_sink::write_all");
    parser_http_endpoint endpoint;
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = this->lock_internal(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = parser_http_capture_endpoint(endpoint, this->_host, this->_path,
        this->_port, this->_use_ssl, this->is_thread_safe());
    (void)this->unlock_internal(lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (!data_pointer && data_length != 0)
        return (FT_ERR_INVALID_ARGUMENT);

    return (http_post_body(endpoint.host, endpoint.path, data_pointer, data_length,
        endpoint.use_ssl, endpoint.port));
}

#endif
