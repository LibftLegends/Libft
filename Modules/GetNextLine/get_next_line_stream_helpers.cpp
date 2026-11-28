#include "get_next_line.hpp"

#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include "../Template/move.hpp"

#include <cerrno>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/vector.hpp"

static int32_t append_line_to_vector(ft_vector<ft_string> &lines, char *line_buffer)
{
    ft_string line_string;
    int32_t initialize_error;

    if (line_buffer == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    initialize_error = line_string.initialize(line_buffer);
    cma_free(line_buffer);
    if (initialize_error != FT_ERR_SUCCESS)
        return (initialize_error);
    return (lines.push_back(ft_move(line_string)));
}

int32_t ft_read_file_lines(int32_t file_descriptor, ft_vector<ft_string> &lines, ft_size_t buffer_size)
{
    char *line_pointer;
    ft_bool finished;
    int32_t append_status;
    int32_t clear_status;

    if (buffer_size == 0 || file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    finished = FT_FALSE;
    while (finished == FT_FALSE)
    {
        line_pointer = get_next_line(file_descriptor, buffer_size);
        if (!line_pointer)
        {
            finished = FT_TRUE;
        }
        else
        {
            append_status = append_line_to_vector(lines, line_pointer);
            if (append_status != FT_ERR_SUCCESS)
            {
                (void)gnl_clear_stream(file_descriptor);
                return (append_status);
            }
        }
    }
    clear_status = gnl_clear_stream(file_descriptor);
    if (clear_status != FT_ERR_SUCCESS)
        return (clear_status);
    return (FT_ERR_SUCCESS);
}

int32_t ft_open_and_read_file(const char *path, ft_vector<ft_string> &lines, ft_size_t buffer_size)
{
    int32_t file_descriptor;
    int32_t read_result;
    int32_t close_result;
    int32_t open_error;

    if (!path)
        return (FT_ERR_INVALID_ARGUMENT);
    if (buffer_size == 0)
        return (FT_ERR_INVALID_ARGUMENT);
    file_descriptor = cmp_open(path);
    if (file_descriptor < 0)
    {
        open_error = cmp_map_system_error_to_ft(errno);
        if (open_error == FT_ERR_SUCCESS)
            open_error = FT_ERR_IO;
        return (open_error);
    }
    read_result = ft_read_file_lines(file_descriptor, lines, buffer_size);
    close_result = cmp_close(file_descriptor);
    if (close_result != FT_ERR_SUCCESS)
    {
        if (read_result == FT_ERR_SUCCESS)
            return (close_result);
    }
    if (read_result != FT_ERR_SUCCESS)
        return (read_result);
    return (FT_ERR_SUCCESS);
}
