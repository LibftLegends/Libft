#include "system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include <cstdlib>
#include <new>
#include <limits>
#include "../Basic/limits.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef LIBFT_TEST_BUILD
static ft_bool g_force_file_stream_allocation_failure = FT_FALSE;
#endif

int32_t su_file_prepare_thread_safety(su_file *stream)
{
    pt_mutex    *mutex_pointer;
    int32_t         mutex_error;

    if (stream == ft_nullptr)
        return (-1);
    if (stream->mutex != ft_nullptr)
        return (0);
    mutex_pointer = new (std::nothrow) pt_mutex;
    if (mutex_pointer == ft_nullptr)
        return (-1);
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        return (-1);
    }
    stream->mutex = mutex_pointer;
    return (0);
}

void su_file_teardown_thread_safety(su_file *stream)
{
    if (stream == ft_nullptr)
        return ;
    if (stream->mutex == ft_nullptr)
        return ;
    (void)stream->mutex->destroy();
    delete stream->mutex;
    stream->mutex = ft_nullptr;
    return ;
}

#ifdef LIBFT_TEST_BUILD
void su_force_file_stream_allocation_failure(ft_bool should_fail)
{
    g_force_file_stream_allocation_failure = should_fail;
    return ;
}
#endif

int32_t su_file_lock(su_file *stream, ft_bool *lock_acquired)
{
    int32_t lock_error;

    if (stream == ft_nullptr || lock_acquired == ft_nullptr)
        return (-1);
    *lock_acquired = FT_FALSE;
    lock_error = pt_mutex_lock_if_not_null(stream->mutex);
    if (lock_error != FT_ERR_SUCCESS)
        return (-1);
    *lock_acquired = FT_TRUE;
    return (0);
}

void su_file_unlock(su_file *stream, ft_bool lock_acquired)
{
    if (stream == ft_nullptr)
        return ;
    if (lock_acquired == FT_FALSE)
        return ;
    (void)pt_mutex_unlock_if_not_null(stream->mutex);
    return ;
}

static su_file *create_file_stream(int32_t file_descriptor)
{
    su_file  *file_stream;

    if (file_descriptor < 0)
        return (ft_nullptr);
#ifdef LIBFT_TEST_BUILD
    if (g_force_file_stream_allocation_failure == FT_TRUE)
    {
        (void)cmp_close(file_descriptor);
        return (ft_nullptr);
    }
#endif
    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
    {
        (void)cmp_close(file_descriptor);
        return (ft_nullptr);
    }
    file_stream->mutex = ft_nullptr;
    file_stream->closed = FT_FALSE;
    file_stream->_descriptor = file_descriptor;
    if (su_file_prepare_thread_safety(file_stream) != 0)
    {
        (void)cmp_close(file_descriptor);
        std::free(file_stream);
        return (ft_nullptr);
    }
    return (file_stream);
}

su_file *su_fopen(const char *path_name)
{
    int32_t file_descriptor;

    if (path_name == ft_nullptr)
        return (ft_nullptr);
    file_descriptor = su_open(path_name);
    if (file_descriptor < 0)
        return (ft_nullptr);
    return (create_file_stream(file_descriptor));
}

su_file *su_fopen(const char *path_name, int32_t flags)
{
    int32_t file_descriptor;

    if (path_name == ft_nullptr)
        return (ft_nullptr);
    file_descriptor = su_open(path_name, flags);
    if (file_descriptor < 0)
        return (ft_nullptr);
    return (create_file_stream(file_descriptor));
}

su_file *su_fopen(const char *path_name, int32_t flags, mode_t mode)
{
    int32_t file_descriptor;

    if (path_name == ft_nullptr)
        return (ft_nullptr);
    file_descriptor = su_open(path_name, flags, mode);
    if (file_descriptor < 0)
        return (ft_nullptr);
    return (create_file_stream(file_descriptor));
}

int32_t su_fclose(su_file *stream)
{
    ft_bool    lock_acquired;
    int32_t     close_result;

    if (stream == ft_nullptr)
        return (-1);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (-1);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    stream->closed = FT_TRUE;
    close_result = cmp_close(stream->_descriptor);
    if (close_result != 0)
    {
        stream->closed = FT_FALSE;
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    su_file_unlock(stream, lock_acquired);
    su_file_teardown_thread_safety(stream);
    std::free(stream);
    return (0);
}

ft_size_t su_fread(void *buffer, ft_size_t size, ft_size_t count, su_file *stream)
{
    ft_size_t   total_size;
    ft_size_t   total_read;
    char        *byte_buffer;
    int64_t     bytes_read;
    ft_size_t   maximum_size;
    ft_bool        lock_acquired;

    if (buffer == ft_nullptr || stream == ft_nullptr)
        return (0);
    if (size == 0 || count == 0)
        return (0);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (0);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (0);
    }
    maximum_size = std::numeric_limits<ft_size_t>::max();
    if (count > maximum_size / size)
    {
        su_file_unlock(stream, lock_acquired);
        return (0);
    }
    total_size = size * count;
    total_read = 0;
    byte_buffer = static_cast<char*>(buffer);
    while (total_read < total_size)
    {
        bytes_read = su_read(stream->_descriptor,
            byte_buffer + total_read, total_size - total_read);
        if (bytes_read <= 0)
            break ;
        total_read += static_cast<ft_size_t>(bytes_read);
    }
    su_file_unlock(stream, lock_acquired);
    return (total_read / size);
}

ft_size_t su_fwrite(const void *buffer, ft_size_t size, ft_size_t count, su_file *stream)
{
    ft_size_t   total_size;
    ft_size_t   maximum_size;
    ft_bool        lock_acquired;
    int64_t     bytes_written;

    if (buffer == ft_nullptr || stream == ft_nullptr)
        return (0);
    if (size == 0 || count == 0)
        return (0);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (0);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (0);
    }
    maximum_size = std::numeric_limits<ft_size_t>::max();
    if (count > maximum_size / size)
    {
        su_file_unlock(stream, lock_acquired);
        return (0);
    }
    total_size = size * count;
    bytes_written = su_write(stream->_descriptor, buffer, total_size);
    su_file_unlock(stream, lock_acquired);
    if (bytes_written < 0)
        return (0);
    return (static_cast<ft_size_t>(bytes_written) / size);
}

int32_t su_fseek(su_file *stream, int64_t offset, int32_t origin)
{
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER distance_to_move;
    LARGE_INTEGER new_position;
    HANDLE file_handle;
    ft_bool lock_acquired;

    if (stream == ft_nullptr)
        return (-1);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (-1);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    file_handle = cmp_retrieve_handle(stream->_descriptor);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    distance_to_move.QuadPart = static_cast<LONGLONG>(offset);
    if (SetFilePointerEx(file_handle, distance_to_move, &new_position, origin) == 0)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    su_file_unlock(stream, lock_acquired);
    return (0);
#else
    off_t   result;
    ft_bool    lock_acquired;

    if (stream == ft_nullptr)
        return (-1);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (-1);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1);
    }
    result = su_lseek(stream->_descriptor, offset, origin);
    su_file_unlock(stream, lock_acquired);
    if (result < 0)
        return (-1);
    return (0);
#endif
}

int64_t su_ftell(su_file *stream)
{
#if defined(_WIN32) || defined(_WIN64)
    LARGE_INTEGER distance_to_move;
    LARGE_INTEGER new_position;
    HANDLE file_handle;
    ft_bool lock_acquired;

    if (stream == ft_nullptr)
        return (-1L);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (-1L);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1L);
    }
    file_handle = cmp_retrieve_handle(stream->_descriptor);
    if (file_handle == INVALID_HANDLE_VALUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1L);
    }
    distance_to_move.QuadPart = 0;
    if (SetFilePointerEx(file_handle, distance_to_move, &new_position, FILE_CURRENT) == 0)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1L);
    }
    su_file_unlock(stream, lock_acquired);
    return (static_cast<int64_t>(new_position.QuadPart));
#else
    off_t   position;
    ft_bool    lock_acquired;

    if (stream == ft_nullptr)
        return (-1L);
    if (su_file_lock(stream, &lock_acquired) != 0)
        return (-1L);
    if (stream->closed == FT_TRUE)
    {
        su_file_unlock(stream, lock_acquired);
        return (-1L);
    }
    position = su_lseek(stream->_descriptor, 0, SEEK_CUR);
    su_file_unlock(stream, lock_acquired);
    if (position < 0)
        return (-1L);
    return (position);
#endif
}
