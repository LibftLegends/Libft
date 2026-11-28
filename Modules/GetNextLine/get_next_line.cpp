#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Template/unordered_map.hpp"
#include "../Errno/errno.hpp"
#include "get_next_line.hpp"
#include "gnl_stream.hpp"
#include <new>
#include <unistd.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static ft_bool map_has_new_error(ft_unordered_map<int32_t, char*> &map, int32_t previous_error, int32_t *current_error)
{
    int32_t updated_error;

    updated_error = map.get_error();
    if (current_error)
        *current_error = updated_error;
    if (updated_error != FT_ERR_SUCCESS)
    {
        if (previous_error == FT_ERR_SUCCESS)
            return (FT_TRUE);
        if (updated_error != previous_error)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static ft_unordered_map<int32_t, char*> g_gnl_leftovers;
static ft_unordered_map<int32_t, gnl_stream*> g_gnl_streams;

#ifdef LIBFT_TEST_BUILD
static void gnl_untrack_runtime_leaks(void)
{
    if (g_gnl_leftovers.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_leftovers._data != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_leftovers._data);
    if (g_gnl_leftovers.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_leftovers._occupied != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_leftovers._occupied);
    if (g_gnl_leftovers.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_leftovers._mutex != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_leftovers._mutex);
    if (g_gnl_streams.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_streams._data != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_streams._data);
    if (g_gnl_streams.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_streams._occupied != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_streams._occupied);
    if (g_gnl_streams.is_initialised() == FT_CLASS_STATE_INITIALISED
        && g_gnl_streams._mutex != ft_nullptr)
        (void)cma_untrack_leak(g_gnl_streams._mutex);
    return ;
}
#endif

static int32_t gnl_ensure_maps_initialised(void)
{
    int32_t initialise_error;

    if (g_gnl_leftovers.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = g_gnl_leftovers.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (initialise_error);
#ifdef LIBFT_TEST_BUILD
        gnl_untrack_runtime_leaks();
#endif
    }
    if (g_gnl_streams.is_initialised() != FT_CLASS_STATE_INITIALISED)
    {
        initialise_error = g_gnl_streams.initialize();
        if (initialise_error != FT_ERR_SUCCESS)
            return (initialise_error);
#ifdef LIBFT_TEST_BUILD
        gnl_untrack_runtime_leaks();
#endif
    }
    return (FT_ERR_SUCCESS);
}

static ft_bool stream_map_has_new_error(ft_unordered_map<int32_t, gnl_stream*> &map,
    int32_t previous_error, int32_t *current_error)
{
    int32_t updated_error;

    updated_error = map.get_error();
    if (current_error)
        *current_error = updated_error;
    if (updated_error != FT_ERR_SUCCESS)
    {
        if (previous_error == FT_ERR_SUCCESS)
            return (FT_TRUE);
        if (updated_error != previous_error)
            return (FT_TRUE);
    }
    return (FT_FALSE);
}

static gnl_stream *gnl_acquire_stream(int32_t file_descriptor, int32_t *stream_error)
{
    int32_t map_error_before;
    int32_t map_error_after;
    gnl_stream *existing_stream;
    void *memory;
    gnl_stream *new_stream;
    int32_t init_error;

    if (stream_error)
        *stream_error = FT_ERR_SUCCESS;
    if (file_descriptor < 0)
    {
        if (stream_error)
            *stream_error = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    init_error = gnl_ensure_maps_initialised();
    if (init_error != FT_ERR_SUCCESS)
    {
        if (stream_error)
            *stream_error = init_error;
        return (ft_nullptr);
    }
    map_error_before = g_gnl_streams.get_error();
    ft_unordered_map<int32_t, gnl_stream*>::iterator stream_iterator(g_gnl_streams.find(file_descriptor));
    if (stream_map_has_new_error(g_gnl_streams, map_error_before, &map_error_after))
    {
        if (stream_error)
            *stream_error = map_error_after;
        return (ft_nullptr);
    }
    if (stream_iterator != g_gnl_streams.end())
        return (stream_iterator->second);
    memory = cma_malloc(sizeof(gnl_stream));
    if (!memory)
    {
        if (stream_error)
            *stream_error = FT_ERR_NO_MEMORY;
        return (ft_nullptr);
    }
    new_stream = new(memory) gnl_stream();
    init_error = new_stream->initialize();
    if (init_error == FT_ERR_SUCCESS)
        init_error = new_stream->init_from_fd(file_descriptor);
    if (init_error != FT_ERR_SUCCESS)
    {
        if (new_stream != ft_nullptr)
            (void)new_stream->destroy();
        new_stream->~gnl_stream();
        cma_free(memory);
        if (stream_error)
            *stream_error = init_error;
        return (ft_nullptr);
    }
    map_error_before = g_gnl_streams.get_error();
    g_gnl_streams.insert(file_descriptor, new_stream);
    if (stream_map_has_new_error(g_gnl_streams, map_error_before, &map_error_after))
    {
        new_stream->reset();
        new_stream->~gnl_stream();
        cma_free(memory);
        if (stream_error)
            *stream_error = map_error_after;
        return (ft_nullptr);
    }
#ifdef LIBFT_TEST_BUILD
    gnl_untrack_runtime_leaks();
#endif
    existing_stream = new_stream;
    if (stream_error)
        *stream_error = FT_ERR_SUCCESS;
    return (existing_stream);
}

#ifdef LIBFT_TEST_BUILD
static void    *(*g_gnl_leftover_alloc_hook)(ft_size_t size) = cma_malloc;

void    gnl_set_leftover_alloc_hook(void *(*hook)(ft_size_t size))
{
    if (hook)
        g_gnl_leftover_alloc_hook = hook;
    else
        g_gnl_leftover_alloc_hook = cma_malloc;
    return ;
}

void    gnl_reset_leftover_alloc_hook(void)
{
    g_gnl_leftover_alloc_hook = cma_malloc;
    return ;
}
#endif

void    gnl_reset_all_streams(void)
{
    if (gnl_ensure_maps_initialised() != FT_ERR_SUCCESS)
        return ;
    if (g_gnl_leftovers.has_valid_storage())
    {
        ft_unordered_map<int32_t, char*>::iterator map_iterator = g_gnl_leftovers.begin();

        while (map_iterator != g_gnl_leftovers.end())
        {
            char    *buffer;

            buffer = map_iterator->second;
            if (buffer)
                cma_free(buffer);
            ++map_iterator;
        }
        g_gnl_leftovers.clear();
    }
    if (g_gnl_streams.has_valid_storage())
    {
        ft_unordered_map<int32_t, gnl_stream*>::iterator stream_iterator = g_gnl_streams.begin();

        while (stream_iterator != g_gnl_streams.end())
        {
            gnl_stream *stream_pointer;

            stream_pointer = stream_iterator->second;
            if (stream_pointer)
            {
                stream_pointer->reset();
                stream_pointer->~gnl_stream();
                cma_free(stream_pointer);
            }
            ++stream_iterator;
        }
        g_gnl_streams.clear();
    }
    return ;
}

#ifdef LIBFT_TEST_BUILD
void    gnl_destroy_streams_for_tests(void)
{
    (void)g_gnl_leftovers.destroy();
    (void)g_gnl_streams.destroy();
    return ;
}
#endif

char *ft_strjoin_gnl(char *string_one, char *string_two)
{
    if (string_one == ft_nullptr && string_two == ft_nullptr)
        return (ft_nullptr);
    const ft_size_t first_length = ft_strlen_size_t(string_one);
    const ft_size_t second_length = ft_strlen_size_t(string_two);
    if (second_length >= SIZE_MAX - first_length)
        return (ft_nullptr);
    char *result = static_cast<char *>(cma_malloc(first_length + second_length + 1U));
    if (result == ft_nullptr)
        return (ft_nullptr);
    if (first_length != 0U)
        ft_memcpy(result, string_one, first_length);
    if (second_length != 0U)
        ft_memcpy(result + first_length, string_two, second_length);
    result[first_length + second_length] = '\0';
    cma_free(string_one);
    return (result);
}

static char *copy_leftover(const char *buffer, ft_size_t length)
{
    char *leftover;
#ifdef LIBFT_TEST_BUILD
    leftover = static_cast<char *>(g_gnl_leftover_alloc_hook(length + 2U));
#else
    leftover = static_cast<char *>(cma_malloc(length + 2U));
#endif
    if (leftover != ft_nullptr)
        ft_memcpy(leftover, buffer, length + 1U);
    return (leftover);
}

static int32_t append_read_chunk(char *&combined, ft_size_t &length,
    ft_size_t &capacity, const char *chunk, ft_size_t chunk_length)
{
    if (chunk_length >= SIZE_MAX - length)
        return (FT_ERR_OUT_OF_RANGE);
    const ft_size_t required = length + chunk_length + 1U;
    if (required > capacity)
    {
        ft_size_t new_capacity = required;
        if (capacity <= SIZE_MAX / 2U && capacity * 2U > required)
            new_capacity = capacity * 2U;
        char *resized;
        if (combined == ft_nullptr)
            resized = static_cast<char *>(cma_malloc(new_capacity));
        else
            resized = static_cast<char *>(cma_realloc(combined, new_capacity));
        if (resized == ft_nullptr && new_capacity != required)
        {
            resized = static_cast<char *>(cma_realloc(combined, required));
            new_capacity = required;
        }
        if (resized == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        combined = resized;
        capacity = new_capacity;
    }
    if (chunk_length != 0U)
        ft_memcpy(combined + length, chunk, chunk_length);
    length += chunk_length;
    combined[length] = '\0';
    return (FT_ERR_SUCCESS);
}

static char *read_stream(gnl_stream &stream, char *combined, ft_size_t buffer_size,
    ft_size_t &length, ft_size_t &line_length, ft_bool &has_newline)
{
    length = ft_strlen_size_t(combined);
    line_length = length;
    has_newline = FT_FALSE;
    if (length != 0U)
    {
        const char *newline = static_cast<const char *>(ft_memchr(combined, '\n', length));
        if (newline != ft_nullptr)
        {
            line_length = static_cast<ft_size_t>(newline - combined) + 1U;
            has_newline = FT_TRUE;
            return (combined);
        }
    }
    char *chunk = static_cast<char *>(cma_malloc(buffer_size));
    if (chunk == ft_nullptr)
    {
        cma_free(combined);
        return (ft_nullptr);
    }
    ft_size_t capacity = 0U;
    if (combined != ft_nullptr)
        capacity = length + 1U;
    while (FT_TRUE)
    {
        const int64_t bytes_read = stream.read(chunk, buffer_size);
        if (bytes_read < 0 || static_cast<ft_size_t>(bytes_read) > buffer_size)
        {
            cma_free(combined);
            combined = ft_nullptr;
            break ;
        }
        if (bytes_read == 0)
            break ;
        const ft_size_t chunk_length = ft_strnlen(chunk, static_cast<ft_size_t>(bytes_read));
        const char *newline = static_cast<const char *>(ft_memchr(chunk, '\n', chunk_length));
        const ft_size_t previous_length = length;
        if (append_read_chunk(combined, length, capacity, chunk, chunk_length) != FT_ERR_SUCCESS)
        {
            cma_free(combined);
            combined = ft_nullptr;
            break ;
        }
        if (newline != ft_nullptr)
        {
            line_length = previous_length + static_cast<ft_size_t>(newline - chunk) + 1U;
            has_newline = FT_TRUE;
            break ;
        }
        line_length = length;
    }
    cma_free(chunk);
    return (combined);
}

int32_t gnl_clear_stream(int32_t file_descriptor)
{
    int32_t init_error;
    int32_t map_error_before;
    int32_t map_error_after;
    char *leftover;
    gnl_stream *stream_pointer;

    init_error = gnl_ensure_maps_initialised();
    if (init_error != FT_ERR_SUCCESS)
        return (init_error);
    map_error_before = g_gnl_leftovers.get_error();
    ft_unordered_map<int32_t, char*>::iterator map_iterator(g_gnl_leftovers.find(file_descriptor));
    if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
        return (map_error_after);
    if (map_iterator != g_gnl_leftovers.end())
    {
        leftover = map_iterator->second;
        map_error_before = g_gnl_leftovers.get_error();
        g_gnl_leftovers.erase(file_descriptor);
        if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
        {
            if (leftover)
                cma_free(leftover);
            return (map_error_after);
        }
        if (leftover)
            cma_free(leftover);
    }
    map_error_before = g_gnl_streams.get_error();
    ft_unordered_map<int32_t, gnl_stream*>::iterator stream_iterator(g_gnl_streams.find(file_descriptor));
    if (stream_map_has_new_error(g_gnl_streams, map_error_before, &map_error_after))
        return (map_error_after);
    if (stream_iterator != g_gnl_streams.end())
    {
        stream_pointer = stream_iterator->second;
        map_error_before = g_gnl_streams.get_error();
        g_gnl_streams.erase(file_descriptor);
        if (stream_map_has_new_error(g_gnl_streams, map_error_before, &map_error_after))
            return (map_error_after);
        if (stream_pointer)
        {
            stream_pointer->reset();
            stream_pointer->~gnl_stream();
            cma_free(stream_pointer);
        }
    }
    return (FT_ERR_SUCCESS);
}

char *get_next_line(int32_t file_descriptor, ft_size_t buffer_size)
{
    if (buffer_size == 0U || buffer_size >= SIZE_MAX || file_descriptor < 0)
        return (ft_nullptr);
    gnl_stream *stream = gnl_acquire_stream(file_descriptor, ft_nullptr);
    if (stream == ft_nullptr)
        return (ft_nullptr);
    int32_t map_error_before = g_gnl_leftovers.get_error();
    auto iterator = g_gnl_leftovers.find(file_descriptor);
    if (map_has_new_error(g_gnl_leftovers, map_error_before, ft_nullptr))
        return (ft_nullptr);
    char *combined = ft_nullptr;
    if (iterator != g_gnl_leftovers.end())
    {
        combined = iterator->second;
        map_error_before = g_gnl_leftovers.get_error();
        g_gnl_leftovers.erase(file_descriptor);
        if (map_has_new_error(g_gnl_leftovers, map_error_before, ft_nullptr))
        {
            cma_free(combined);
            return (ft_nullptr);
        }
    }
    ft_size_t length;
    ft_size_t line_length;
    ft_bool has_newline;
    combined = read_stream(*stream, combined, buffer_size, length, line_length, has_newline);
    if (combined == ft_nullptr)
        return (ft_nullptr);
    char *line = ft_nullptr;
    if (line_length != 0U)
    {
        line = static_cast<char *>(cma_malloc(line_length + 1U));
        if (line != ft_nullptr)
        {
            ft_memcpy(line, combined, line_length);
            line[line_length] = '\0';
        }
    }
    char *leftover = ft_nullptr;
    if (has_newline == FT_TRUE)
    {
        leftover = copy_leftover(combined + line_length, length - line_length);
        if (leftover == ft_nullptr)
        {
            ft_bzero(combined, length);
            cma_free(combined);
            cma_free(line);
            return (ft_nullptr);
        }
    }
    cma_free(combined);
    if (leftover != ft_nullptr)
    {
        map_error_before = g_gnl_leftovers.get_error();
        g_gnl_leftovers.insert(file_descriptor, leftover);
        if (map_has_new_error(g_gnl_leftovers, map_error_before, ft_nullptr))
        {
            cma_free(leftover);
            cma_free(line);
            return (ft_nullptr);
        }
#ifdef LIBFT_TEST_BUILD
        gnl_untrack_runtime_leaks();
#endif
    }
    return (line);
}
