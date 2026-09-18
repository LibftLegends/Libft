#include "filesystem.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../File/file_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

static int32_t filesystem_write_descriptor_all(int32_t file_descriptor,
    const void *data,
    ft_size_t size)
{
    const unsigned char *bytes;
    ft_size_t written_total;
    int64_t written_now;
    int32_t error_code;

    if (file_descriptor < 0)
        return (FT_ERR_INVALID_ARGUMENT);
    if (data == ft_nullptr && size > 0)
        return (FT_ERR_INVALID_POINTER);
    bytes = static_cast<const unsigned char *>(data);
    written_total = 0;
    while (written_total < size)
    {
        written_now = 0;
        error_code = cmp_write(file_descriptor, bytes + written_total,
            size - written_total, &written_now);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (written_now <= 0)
            return (FT_ERR_IO);
        written_total = written_total + written_now;
    }
    return (FT_ERR_SUCCESS);
}

int32_t filesystem_atomic_write(const char *path, const void *data, ft_size_t size)
{
    ft_string temporary_path;
    ft_string *directory_name;
    int32_t file_descriptor;
    int32_t error_code;
    int32_t close_error;

    if (path == ft_nullptr || (data == ft_nullptr && size > 0))
    {
        return (FT_ERR_INVALID_POINTER);
    }
    error_code = temporary_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    directory_name = file_path_dirname_string(path);
    if (directory_name == ft_nullptr)
    {
        (void)temporary_path.destroy();
        return (FT_ERR_NO_MEMORY);
    }
    file_descriptor = -1;
    error_code = file_secure_temp_file(directory_name->c_str(), "atomic",
        &temporary_path, &file_descriptor);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)directory_name->destroy();
        delete directory_name;
        (void)temporary_path.destroy();
        return (error_code);
    }
    error_code = filesystem_write_descriptor_all(file_descriptor, data, size);
    close_error = cmp_close(file_descriptor);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)file_delete(temporary_path.c_str());
        (void)directory_name->destroy();
        delete directory_name;
        (void)temporary_path.destroy();
        return (error_code);
    }
    if (close_error != FT_ERR_SUCCESS)
    {
        (void)file_delete(temporary_path.c_str());
        (void)directory_name->destroy();
        delete directory_name;
        (void)temporary_path.destroy();
        return (close_error);
    }
    error_code = file_move(temporary_path.c_str(), path);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)file_delete(temporary_path.c_str());
    }
    (void)directory_name->destroy();
    delete directory_name;
    (void)temporary_path.destroy();
    return (error_code);
}
