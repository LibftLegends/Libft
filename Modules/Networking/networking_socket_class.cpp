#include "socket_class.hpp"
#include "networking.hpp"
#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"
#include "../Errno/errno_internal.hpp"
#include "../Printf/printf.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <new>
#include <cstdio>
#include "../PThread/pthread.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"
#ifdef _WIN32
# include <winsock2.h>
# include <ws2tcpip.h>
# include <io.h>
#else
# include <arpa/inet.h>
# include <sys/types.h>
# include <unistd.h>
# include <sys/socket.h>
#endif

void ft_socket::sleep_backoff()
{
    pt_thread_sleep(1);
    return ;
}

int32_t ft_socket::enable_thread_safety() noexcept
{
    if (this->_mutex != ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (mutex_pointer == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    int32_t mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::disable_thread_safety() noexcept
{
    if (this->_mutex == ft_nullptr)
        return (FT_ERR_SUCCESS);
    pt_recursive_mutex *mutex_pointer = this->_mutex;
    this->_mutex = ft_nullptr;
    int32_t destroy_error = mutex_pointer->destroy();
    delete mutex_pointer;
    if (destroy_error != FT_ERR_SUCCESS && destroy_error != FT_ERR_INVALID_STATE)
        return (destroy_error);
    return (FT_ERR_SUCCESS);
}

ft_bool ft_socket::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

struct networking_error_entry
{
    int32_t                    error_code;
};

static networking_error_entry networking_consume_last_error(void) noexcept
{
    networking_error_entry entry;

    entry.error_code = FT_ERR_SUCCESS;
    return (entry);
}

ssize_t ft_socket::send_data_locked(const void *data, ft_size_t size, int32_t flags)
{
    if (this->_socket_file_descriptor < 0)
    {
        (void)(FT_ERR_CONFIGURATION);
        return (-1);
    }
    ssize_t bytes_sent;

    bytes_sent = nw_send(this->_socket_file_descriptor, data, size, flags);
    return (bytes_sent);
}

ssize_t ft_socket::send_all_locked(const void *data, ft_size_t size, int32_t flags)
{
    if (this->_socket_file_descriptor < 0)
    {
        (void)(FT_ERR_CONFIGURATION);
        return (-1);
    }
    ft_size_t total_sent;
    const char *buffer;

    total_sent = 0;
    buffer = static_cast<const char *>(data);
    while (total_sent < size)
    {
        ssize_t bytes_sent;

        bytes_sent = nw_send(this->_socket_file_descriptor, buffer + total_sent,
                size - total_sent, flags);
        if (bytes_sent < 0)
        {
            FILE *file_pointer;
            int32_t last_error;

            last_error = 0;
#ifdef _WIN32
            last_error = WSAGetLastError();
#else
            last_error = errno;
#endif
            file_pointer = fopen("socket_send_debug.log", "a");
            if (file_pointer)
            {
                fprintf(file_pointer,
                    "send_all_locked fail fd=%d errno=%d\n",
                    this->_socket_file_descriptor, last_error);
                fclose(file_pointer);
            }
            return (-1);
        }
        if (bytes_sent == 0)
        {
            FILE *file_pointer;
            int32_t last_error;

            last_error = 0;
#ifdef _WIN32
            last_error = WSAGetLastError();
#else
            last_error = errno;
#endif
            file_pointer = fopen("socket_send_debug.log", "a");
            if (file_pointer)
            {
                fprintf(file_pointer,
                    "send_all_locked zero fd=%d errno=%d\n",
                    this->_socket_file_descriptor, last_error);
                fclose(file_pointer);
            }
            (void)(FT_ERR_SOCKET_SEND_FAILED);
            return (-1);
        }
        total_sent += bytes_sent;
    }
    networking_consume_last_error();
    (void)(FT_ERR_SUCCESS);
    return (static_cast<ssize_t>(total_sent));
}

ssize_t ft_socket::receive_data_locked(void *buffer, ft_size_t size, int32_t flags)
{
    if (this->_socket_file_descriptor < 0)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        return (-1);
    }
    ssize_t bytes_received;

    bytes_received = nw_recv(this->_socket_file_descriptor, buffer, size, flags);
    return (bytes_received);
}

ft_bool ft_socket::close_socket_locked()
{
    if (this->_socket_file_descriptor >= 0)
    {
        if (nw_close(this->_socket_file_descriptor) == 0)
        {
            this->_socket_file_descriptor = -1;
            (void)(FT_ERR_SUCCESS);
            return (FT_TRUE);
        }
        return (FT_FALSE);
    }
    (void)(FT_ERR_SUCCESS);
    return (FT_TRUE);
}

void ft_socket::reset_to_empty_state_locked()
{
    ft_size_t connection_index;

    connection_index = 0;
    while (connection_index < this->_connected.size())
    {
        int32_t client_fd;

        client_fd = this->_connected[connection_index];
        if (client_fd >= 0)
            (void)nw_close(client_fd);
        connection_index++;
    }
    this->_connected.clear();
    ft_bzero(&this->_address, sizeof(this->_address));
    this->_socket_file_descriptor = -1;
    (void)(FT_ERR_SUCCESS);
    return ;
}

ft_socket::ft_socket() noexcept
    : _initialised_state(FT_CLASS_STATE_UNINITIALISED), _address(), _connected(), _socket_file_descriptor(-1), _mutex(ft_nullptr)
{
    ft_bzero(&this->_address, sizeof(this->_address));
    this->_connected.clear();
    this->_socket_file_descriptor = -1;
    return ;
}

ft_socket::~ft_socket() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t ft_socket::move(ft_socket &other) noexcept
{
    return (this->initialize(static_cast<ft_socket &&>(other)));
}

int32_t ft_socket::initialize() noexcept
{
    int32_t connected_error;

    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_socket::initialize()",
            "initialize called on initialised instance");
    ft_bzero(&this->_address, sizeof(this->_address));
    connected_error = this->_connected.destroy();
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    connected_error = this->_connected.initialize();
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    this->_socket_file_descriptor = -1;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::initialize(const ft_socket &other) noexcept
{
    int32_t connected_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "ft_socket::initialize(const ft_socket &)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_address = other._address;
    this->_socket_file_descriptor = -1;
    connected_error = this->_connected.destroy();
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    connected_error = this->_connected.initialize();
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    return (FT_ERR_SUCCESS);
}

int32_t ft_socket::initialize(ft_socket &&other) noexcept
{
    int32_t connected_error;

    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "ft_socket::initialize(ft_socket &&)",
            "source is uninitialised");
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (FT_ERR_SUCCESS);
    }
    this->_address = other._address;
    this->_socket_file_descriptor = other._socket_file_descriptor;
    connected_error = this->_connected.destroy();
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    connected_error = this->_connected.move(other._connected);
    if (connected_error != FT_ERR_SUCCESS)
    {
        this->_socket_file_descriptor = -1;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    this->_mutex = other._mutex;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    ft_bzero(&other._address, sizeof(other._address));
    other._socket_file_descriptor = -1;
    other._mutex = ft_nullptr;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

ssize_t ft_socket::send_data(const void *data, ft_size_t size, int32_t flags)
{
    ssize_t bytes_sent;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::send_data");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    bytes_sent = this->send_data_locked(data, size, flags);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (bytes_sent);
}

ssize_t ft_socket::send_all(const void *data, ft_size_t size, int32_t flags)
{
    ssize_t result;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::send_all");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    result = this->send_all_locked(data, size, flags);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

ssize_t ft_socket::receive_data(void *buffer, ft_size_t size, int32_t flags)
{
    ssize_t result;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::receive_data");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    result = this->receive_data_locked(buffer, size, flags);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

ft_bool ft_socket::close_socket()
{
    int32_t socket_fd;
    ft_bool closed;
    ft_bool shutdown_success;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::close_socket");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    if (this->_socket_file_descriptor < 0)
    {
        (void)(FT_ERR_SUCCESS);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (FT_TRUE);
    }
    shutdown_success = FT_TRUE;
#ifdef _WIN32
    if (nw_shutdown(this->_socket_file_descriptor, SD_BOTH) != 0)
#else
    if (nw_shutdown(this->_socket_file_descriptor, SHUT_RDWR) != 0)
#endif
    {
        shutdown_success = FT_FALSE;
    }
    socket_fd = this->_socket_file_descriptor;
    if (nw_close(socket_fd) == 0)
    {
        closed = FT_TRUE;
        networking_consume_last_error();
    }
    else
    {
        closed = FT_FALSE;
        networking_consume_last_error();
    }
    if (closed)
        this->_socket_file_descriptor = -1;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    if (closed && shutdown_success)
        return (FT_TRUE);
    return (FT_FALSE);
}

ssize_t ft_socket::send_data(const void *data, ft_size_t size, int32_t flags, int32_t file_descriptor)
{
    ft_size_t index;
    ft_bool found;
    ssize_t result;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::send_data(file_descriptor)");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    index = 0;
    found = FT_FALSE;
    result = -1;
    while (index < this->_connected.size())
    {
        if (this->_connected[index] == file_descriptor)
        {
            result = nw_send(file_descriptor, data, size, flags);
            found = FT_TRUE;
            break ;
        }
        index++;
    }
    if (!found)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        result = -1;
    }
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (result);
}

ssize_t ft_socket::broadcast_data(const void *data, ft_size_t size, int32_t flags, int32_t exception)
{
    ssize_t total_bytes_sent;
    ft_bool send_failed;
    ft_size_t index;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::broadcast_data");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    total_bytes_sent = 0;
    send_failed = FT_FALSE;
    index = 0;
    while (index < this->_connected.size())
    {
        int32_t client_fd;
        ssize_t bytes_sent;

        client_fd = this->_connected[index];
        if (exception == client_fd)
        {
            index++;
            continue ;
        }
        bytes_sent = nw_send(client_fd, data, size, flags);
        if (bytes_sent < 0)
        {
            send_failed = FT_TRUE;
        }
        else
            total_bytes_sent += bytes_sent;
        index++;
    }
    if (!send_failed)
        (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (total_bytes_sent);
}

ssize_t ft_socket::broadcast_data(const void *data, ft_size_t size, int32_t flags)
{
    return (this->broadcast_data(data, size, flags, -1));
}

int32_t ft_socket::accept_connection()
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::accept_connection");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    if (this->_socket_file_descriptor < 0)
    {
        (void)(FT_ERR_INVALID_ARGUMENT);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    int32_t new_file_descriptor;

    new_file_descriptor = nw_accept(this->_socket_file_descriptor, ft_nullptr, ft_nullptr);
    if (new_file_descriptor < 0)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    ft_size_t previous_size;
    ft_size_t current_size;

    previous_size = this->_connected.size();
    this->_connected.push_back(new_file_descriptor);
    current_size = this->_connected.size();
    if (current_size != previous_size + 1)
    {
        (void)nw_close(new_file_descriptor);
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        return (-1);
    }
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (new_file_descriptor);
}

ft_bool ft_socket::disconnect_client(int32_t file_descriptor)
{
    ft_size_t index;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::disconnect_client");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    index = 0;
    while (index < this->_connected.size())
    {
        if (this->_connected[index] == file_descriptor)
        {
            ft_size_t last;

            last = this->_connected.size() - 1;
            if (index != last)
                this->_connected[index] = this->_connected[last];
            if (file_descriptor >= 0)
                (void)nw_close(file_descriptor);
            this->_connected.pop_back();
            (void)(FT_ERR_SUCCESS);
            (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
            return (FT_TRUE);
        }
        index++;
    }
    (void)(FT_ERR_INVALID_ARGUMENT);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (FT_FALSE);
}

void ft_socket::disconnect_all_clients()
{
    ft_size_t index;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::disconnect_all_clients");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    index = 0;
    while (index < this->_connected.size())
    {
        if (this->_connected[index] >= 0)
            (void)nw_close(this->_connected[index]);
        index++;
    }
    this->_connected.clear();
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

ft_size_t ft_socket::get_client_count() const
{
    ft_size_t count;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::get_client_count");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (0);
    count = this->_connected.size();
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (count);
}

ft_bool ft_socket::is_client_connected(int32_t file_descriptor) const
{
    ft_size_t index;
    ft_bool connected;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::is_client_connected");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (FT_FALSE);
    index = 0;
    connected = FT_FALSE;
    while (index < this->_connected.size())
    {
        if (this->_connected[index] == file_descriptor)
        {
            connected = FT_TRUE;
            break ;
        }
        index++;
    }
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (connected);
}

int32_t ft_socket::get_file_descriptor() const
{
    int32_t descriptor;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::get_fd");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    descriptor = this->_socket_file_descriptor;
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (descriptor);
}

const struct sockaddr_storage &ft_socket::get_address() const
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::get_address");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (this->_address);
    (void)(FT_ERR_SUCCESS);
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return (this->_address);
}

void ft_socket::reset_to_empty_state()
{
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "ft_socket::reset_to_empty_state");
    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return ;
    this->reset_to_empty_state_locked();
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    return ;
}

int32_t ft_socket::initialize(const SocketConfig &config)
{
    int32_t lock_error;
    int32_t initialize_result;
    int32_t connected_error;

    lock_error = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "ft_socket::initialize",
            "initialize called on initialised instance");
    connected_error = this->_connected.destroy();
    if (connected_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    connected_error = this->_connected.initialize();
    if (connected_error != FT_ERR_SUCCESS)
    {
        (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        return (connected_error);
    }
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    initialize_result = FT_ERR_UNSUPPORTED_TYPE;
    if (config._type == SocketType::SERVER)
        initialize_result = this->setup_server(config);
    else if (config._type == SocketType::CLIENT)
        initialize_result = this->setup_client(config);
    if (initialize_result != FT_ERR_SUCCESS)
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (initialize_result);
}

int32_t ft_socket::destroy() noexcept
{
    int32_t disable_error;
    int32_t connected_error;

    if (this->_initialised_state == FT_CLASS_STATE_UNINITIALISED
        || this->_initialised_state == FT_CLASS_STATE_DESTROYED)
        return (FT_ERR_SUCCESS);
    disable_error = this->disable_thread_safety();
    if (disable_error != FT_ERR_SUCCESS)
        return (disable_error);
    this->disconnect_all_clients();
    connected_error = this->_connected.destroy();
    if (connected_error != FT_ERR_SUCCESS)
        return (connected_error);
    (void)this->close_socket();
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    return (FT_ERR_SUCCESS);
}

#ifdef LIBFT_TEST_BUILD
pt_recursive_mutex *ft_socket::get_mutex_for_validation() const noexcept
{
    if (this->_mutex == ft_nullptr)
    {
        pt_recursive_mutex *mutex_pointer = new (std::nothrow) pt_recursive_mutex();
        if (mutex_pointer == ft_nullptr)
            return (ft_nullptr);
        int32_t mutex_error = mutex_pointer->initialize();
        if (mutex_error != FT_ERR_SUCCESS)
        {
            delete mutex_pointer;
            return (ft_nullptr);
        }
        this->_mutex = mutex_pointer;
    }
    return (this->_mutex);
}

#endif
