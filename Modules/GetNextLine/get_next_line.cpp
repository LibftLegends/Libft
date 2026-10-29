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

static char* allocate_new_string(char* string_one, char* string_two, int32_t *allocation_error)
{
    int32_t total_length = 0;
    char* new_string;

    if (allocation_error)
        *allocation_error = FT_ERR_SUCCESS;
    if (string_one)
        total_length += ft_strlen(string_one);
    if (string_two)
        total_length += ft_strlen(string_two);
    new_string = static_cast<char*>(cma_malloc(total_length + 1));
    if (!new_string)
    {
        if (allocation_error)
            *allocation_error = FT_ERR_NO_MEMORY;
        return (ft_nullptr);
    }
    return (new_string);
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

char* ft_strjoin_gnl(char* string_one, char* string_two)
{
    char* result;
    char* original_string = string_one;
    int32_t index;
    int32_t error_code;

    if (!string_one && !string_two)
        return (ft_nullptr);
    error_code = FT_ERR_SUCCESS;
    result = allocate_new_string(string_one, string_two, &error_code);
    if (!result)
    {
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_NO_MEMORY;
        return (ft_nullptr);
    }
    index = 0;
    if (string_one)
        while (*string_one)
            result[index++] = *string_one++;
    if (string_two)
        while (*string_two)
            result[index++] = *string_two++;
    result[index] = '\0';
    cma_free(original_string);
    return (result);
}

static char* leftovers(char* readed_string, ft_bool *buffer_was_freed, int32_t *leftover_error)
{
    int32_t read_index = 0;
    int32_t write_index = 0;
    char* string;

    if (buffer_was_freed)
        *buffer_was_freed = FT_FALSE;
    if (leftover_error)
        *leftover_error = FT_ERR_SUCCESS;
    while (readed_string[read_index] && readed_string[read_index] != '\n')
        read_index++;
    if (!readed_string[read_index])
    {
        cma_free(readed_string);
        if (buffer_was_freed)
            *buffer_was_freed = FT_TRUE;
        return (ft_nullptr);
    }
#ifdef LIBFT_TEST_BUILD
    string = static_cast<char*>(g_gnl_leftover_alloc_hook(
            ft_strlen(readed_string) - read_index + 1));
#else
    string = static_cast<char*>(cma_malloc(ft_strlen(readed_string)
            - read_index + 1));
#endif
    if (!string)
    {
        if (leftover_error)
            *leftover_error = FT_ERR_NO_MEMORY;
        if (readed_string)
        {
            ft_size_t combined_length;

            combined_length = ft_strlen(readed_string);
            if (combined_length > 0)
                ft_bzero(readed_string, combined_length);
        }
        cma_free(readed_string);
        if (buffer_was_freed)
            *buffer_was_freed = FT_TRUE;
        return (ft_nullptr);
    }
    read_index++;
    while (readed_string[read_index])
        string[write_index++] = readed_string[read_index++];
    string[write_index] = '\0';
    cma_free(readed_string);
    if (buffer_was_freed)
        *buffer_was_freed = FT_TRUE;
    return (string);
}

static char* malloc_gnl(char* readed_string, ft_size_t length, int32_t *allocation_error)
{
    char* string;

    if (allocation_error)
        *allocation_error = FT_ERR_SUCCESS;
    if (readed_string && readed_string[length] == '\n')
        string = static_cast<char*>(cma_malloc(length + 2));
    else
        string = static_cast<char*>(cma_malloc(length + 1));
    if (!string)
    {
        if (allocation_error)
            *allocation_error = FT_ERR_NO_MEMORY;
        return (ft_nullptr);
    }
    return (string);
}

static char* fetch_line(char* readed_string, int32_t *line_error)
{
    ft_size_t index;
    char* string;
    int32_t error_code;

    index = 0;
    if (line_error)
        *line_error = FT_ERR_SUCCESS;
    if (!readed_string[index])
    {
        if (line_error)
            *line_error = FT_ERR_END_OF_FILE;
        return (ft_nullptr);
    }
    while (readed_string[index] && readed_string[index] != '\n')
        index++;
    error_code = FT_ERR_SUCCESS;
    string = malloc_gnl(readed_string, index, &error_code);
    if (!string)
    {
        if (line_error)
            *line_error = error_code;
        return (ft_nullptr);
    }
    index = 0;
    while (readed_string[index] && readed_string[index] != '\n')
    {
        string[index] = readed_string[index];
        index++;
    }
    if (readed_string[index] == '\n')
    {
        string[index] = '\n';
        index++;
    }
    string[index] = '\0';
    return (string);
}

static char* read_stream(gnl_stream *stream, char* readed_string, ft_size_t buffer_size,
    int32_t *read_error)
{
    char* buffer;
    int64_t readed_bytes;
    ft_bool has_read_bytes;
    int32_t error_code;

    if (read_error)
        *read_error = FT_ERR_SUCCESS;
    if (!stream)
    {
        if (readed_string)
            cma_free(readed_string);
        if (read_error)
            *read_error = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    if (readed_string && ft_strchr(readed_string, '\n'))
        return (readed_string);
    buffer = static_cast<char*>(cma_malloc(buffer_size + 1));
    if (!buffer)
    {
        if (read_error)
            *read_error = FT_ERR_NO_MEMORY;
        if (readed_string)
            cma_free(readed_string);
        return (ft_nullptr);
    }
    readed_bytes = 1;
    has_read_bytes = FT_FALSE;
    while (((readed_string == ft_nullptr) || !ft_strchr(readed_string, '\n'))
        && readed_bytes != 0)
    {
        readed_bytes = stream->read(buffer, buffer_size);
        if (readed_bytes < 0)
        {
            cma_free(buffer);
            if (readed_string)
            {
                cma_free(readed_string);
            }
            if (read_error)
                *read_error = FT_ERR_IO;
            return (ft_nullptr);
        }
        buffer[static_cast<ft_size_t>(readed_bytes)] = '\0';
        if (readed_bytes > 0)
        {
            char* previous_string;

            has_read_bytes = FT_TRUE;
            previous_string = readed_string;
            readed_string = ft_strjoin_gnl(readed_string, buffer);
            if (!readed_string)
            {
                error_code = FT_ERR_NO_MEMORY;
                if (previous_string)
                    cma_free(previous_string);
                cma_free(buffer);
                if (read_error)
                    *read_error = error_code;
                return (ft_nullptr);
            }
        }
    }
    cma_free(buffer);
    if (!has_read_bytes && !readed_string)
    {
        if (read_error)
            *read_error = FT_ERR_END_OF_FILE;
    }
    return (readed_string);
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

char    *get_next_line(int32_t file_descriptor, ft_size_t buffer_size)
{
    char                                   *line;
    char                                   *combined_buffer;
    char                                   *leftover_string;
    ft_bool                                    combined_buffer_was_freed;
    int32_t                                     map_error_before;
    int32_t                                     map_error_after;
    int32_t                                     leftovers_error;
    gnl_stream                              *stream;
    int32_t                                     stream_error;
    int32_t                                     error_code;
    int32_t                                     read_error;
    int32_t                                     line_error;
    int32_t                                     init_error;

    line = ft_nullptr;
    combined_buffer = ft_nullptr;
    leftover_string = ft_nullptr;
    if (buffer_size == 0 || file_descriptor < 0)
        return (ft_nullptr);
    init_error = gnl_ensure_maps_initialised();
    if (init_error != FT_ERR_SUCCESS)
        return (ft_nullptr);
    stream = gnl_acquire_stream(file_descriptor, &stream_error);
    if (!stream)
    {
        error_code = stream_error;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    map_error_before = g_gnl_leftovers.get_error();
    ft_unordered_map<int32_t, char*>::iterator map_iterator = g_gnl_leftovers.find(file_descriptor);
    if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
        return (ft_nullptr);
    if (map_iterator != g_gnl_leftovers.end())
    {
        combined_buffer = map_iterator->second;
        map_error_before = g_gnl_leftovers.get_error();
        g_gnl_leftovers.erase(file_descriptor);
        if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
        {
            if (combined_buffer)
                cma_free(combined_buffer);
            return (ft_nullptr);
        }
    }
    read_error = FT_ERR_SUCCESS;
    combined_buffer = read_stream(stream, combined_buffer, buffer_size, &read_error);
    if (!combined_buffer)
    {
        error_code = read_error;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_IO;
        return (ft_nullptr);
    }
    line_error = FT_ERR_SUCCESS;
    line = fetch_line(combined_buffer, &line_error);
    combined_buffer_was_freed = FT_FALSE;
    leftover_string = leftovers(combined_buffer, &combined_buffer_was_freed, &leftovers_error);
    if (leftovers_error != FT_ERR_SUCCESS)
    {
        if (line)
            cma_free(line);
        if (combined_buffer && !combined_buffer_was_freed)
        {
            ft_size_t combined_length;

            combined_length = ft_strlen(combined_buffer);
            if (combined_length > 0)
                ft_bzero(combined_buffer, combined_length);
            cma_free(combined_buffer);
        }
        return (ft_nullptr);
    }
    if (!line)
    {
        if (leftover_string)
        {
            map_error_before = g_gnl_leftovers.get_error();
            g_gnl_leftovers.insert(file_descriptor, leftover_string);
            if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
            {
                cma_free(leftover_string);
                return (ft_nullptr);
            }
#ifdef LIBFT_TEST_BUILD
            gnl_untrack_runtime_leaks();
#endif
        }
        error_code = line_error;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_END_OF_FILE;
        return (ft_nullptr);
    }
    if (leftover_string)
    {
        map_error_before = g_gnl_leftovers.get_error();
        g_gnl_leftovers.insert(file_descriptor, leftover_string);
        if (map_has_new_error(g_gnl_leftovers, map_error_before, &map_error_after))
        {
            cma_free(leftover_string);
            cma_free(line);
            return (ft_nullptr);
        }
#ifdef LIBFT_TEST_BUILD
        gnl_untrack_runtime_leaks();
#endif
    }
    return (line);
}
