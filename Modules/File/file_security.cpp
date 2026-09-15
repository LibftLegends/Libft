#include "file_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../RNG/rng.hpp"
#include <fcntl.h>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"
#if defined(_WIN32) || defined(_WIN64)
# define FILE_SECURITY_BINARY_FLAG O_BINARY
#else
# define FILE_SECURITY_BINARY_FLAG 0
#endif

static char *file_security_temp_directory(void)
{
    char *directory_path;
    int32_t error_code;

    directory_path = ft_nullptr;
    error_code = cmp_get_temp_directory(&directory_path);
    if (error_code != FT_ERR_SUCCESS)
        return (ft_nullptr);
    return (directory_path);
}

static char file_security_hex_digit(uint8_t value) noexcept
{
    if (value < 10U)
        return (static_cast<char>('0' + value));
    return (static_cast<char>('a' + (value - 10U)));
}

static int32_t file_security_append_random_hex(ft_string *name)
{
    uint8_t random_bytes[3];
    ft_size_t index;
    int32_t error_code;

    if (name == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    error_code = rng_secure_bytes(random_bytes, sizeof(random_bytes));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    index = 0;
    while (index < sizeof(random_bytes))
    {
        error_code = name->append(file_security_hex_digit(
                static_cast<uint8_t>((random_bytes[index] >> 4U) & 0x0FU)));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = name->append(file_security_hex_digit(
                static_cast<uint8_t>(random_bytes[index] & 0x0FU)));
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static void file_security_delete_string(ft_string *string) noexcept
{
    if (string == ft_nullptr)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

static int32_t file_security_build_temp_path(const char *directory_path,
    const char *prefix, ft_string *path_out)
{
    ft_string name;
    ft_string *joined_path;
    char *native_directory_path;
    char *temporary_directory_path;
    int32_t error_code;

    if (path_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    error_code = name.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    if (prefix != ft_nullptr && prefix[0] != '\0')
        error_code = name.append(prefix);
    else
        error_code = name.append("libft_secure");
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_security_append_random_hex(&name);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)name.destroy();
        return (error_code);
    }
    native_directory_path = ft_nullptr;
    if (directory_path == ft_nullptr || directory_path[0] == '\0')
    {
        temporary_directory_path = file_security_temp_directory();
        if (temporary_directory_path == ft_nullptr)
        {
            (void)name.destroy();
            return (FT_ERR_NO_MEMORY);
        }
        joined_path = file_path_join(temporary_directory_path, name.c_str());
        cma_free(temporary_directory_path);
    }
    else
    {
        error_code = cmp_translate_path_to_native(directory_path,
                &native_directory_path);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)name.destroy();
            return (error_code);
        }
        joined_path = file_path_join(native_directory_path, name.c_str());
        cma_free(native_directory_path);
    }
    (void)name.destroy();
    if (joined_path == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    if (joined_path->get_error() != FT_ERR_SUCCESS)
    {
        error_code = joined_path->get_error();
        file_security_delete_string(joined_path);
        return (error_code);
    }
    error_code = path_out->assign(joined_path->c_str(), joined_path->size());
    file_security_delete_string(joined_path);
    return (error_code);
}

int32_t file_secure_temp_file(const char *directory_path, const char *prefix,
    ft_string *path_out, int32_t *file_descriptor_out)
{
    ft_string candidate_path;
    int32_t file_descriptor;
    uint32_t attempt_count;
    int32_t error_code;

    if (path_out == ft_nullptr || file_descriptor_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    error_code = candidate_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    attempt_count = 0;
    while (attempt_count < 64U)
    {
        (void)candidate_path.clear();
        error_code = file_security_build_temp_path(directory_path, prefix,
                &candidate_path);
        if (error_code != FT_ERR_SUCCESS)
        {
            (void)candidate_path.destroy();
            return (error_code);
        }
        file_descriptor = cmp_open(candidate_path.c_str(), O_RDWR | O_CREAT
                | O_EXCL | FILE_SECURITY_BINARY_FLAG, 0600);
        if (file_descriptor >= 0)
        {
            error_code = path_out->assign(candidate_path.c_str(),
                    candidate_path.size());
            if (error_code != FT_ERR_SUCCESS)
            {
                (void)cmp_close(file_descriptor);
                (void)file_delete(candidate_path.c_str());
                (void)candidate_path.destroy();
                return (error_code);
            }
            *file_descriptor_out = file_descriptor;
            (void)candidate_path.destroy();
            return (FT_ERR_SUCCESS);
        }
        /* A random-name collision is the only failure that is safe to
         * retry.  Propagate permission, path, descriptor, and filesystem
         * failures immediately instead of masking them as a collision after
         * 64 needless RNG/open attempts. */
        {
            const int32_t open_error = cmp_get_last_open_error();
#if defined(_WIN32) || defined(_WIN64)
            if (open_error != ERROR_FILE_EXISTS
                && open_error != ERROR_ALREADY_EXISTS)
            {
                (void)candidate_path.destroy();
                return (cmp_file_error_to_errno(open_error));
            }
#else
            if (open_error != EEXIST)
            {
                (void)candidate_path.destroy();
                return (cmp_file_error_to_errno(open_error));
            }
#endif
        }
        ++attempt_count;
    }
    (void)candidate_path.destroy();
    return (FT_ERR_ALREADY_EXISTS);
}

static ft_bool file_security_has_root_boundary(const char *root,
    const char *candidate) noexcept
{
    ft_size_t index;

    index = 0;
    while (root[index] != '\0' && candidate[index] != '\0')
    {
        if (root[index] != candidate[index]
            && !((root[index] == '/' || root[index] == '\\')
                && (candidate[index] == '/' || candidate[index] == '\\')))
            return (FT_FALSE);
        ++index;
    }
    if (root[index] != '\0')
        return (FT_FALSE);
    if (candidate[index] == '\0')
        return (FT_TRUE);
    if (index > 0
        && (root[index - 1] == '/' || root[index - 1] == '\\'))
        return (FT_TRUE);
    if (candidate[index] == '/' || candidate[index] == '\\')
        return (FT_TRUE);
    return (FT_FALSE);
}

ft_bool file_path_is_inside_root(const char *root_path, const char *candidate_path)
{
    ft_string *root_normalized;
    ft_string *candidate_normalized;
    ft_bool result;

    if (root_path == ft_nullptr || candidate_path == ft_nullptr)
        return (FT_FALSE);
    root_normalized = file_path_normalize(root_path);
    if (root_normalized == ft_nullptr)
        return (FT_FALSE);
    candidate_normalized = file_path_normalize(candidate_path);
    if (candidate_normalized == ft_nullptr)
    {
        file_security_delete_string(root_normalized);
        return (FT_FALSE);
    }
    if (root_normalized->get_error() != FT_ERR_SUCCESS
        || candidate_normalized->get_error() != FT_ERR_SUCCESS)
        result = FT_FALSE;
    else
        result = file_security_has_root_boundary(root_normalized->c_str(),
                candidate_normalized->c_str());
    file_security_delete_string(root_normalized);
    file_security_delete_string(candidate_normalized);
    return (result);
}

int32_t file_validate_path_inside_root(const char *root_path,
    const char *candidate_path)
{
    if (root_path == ft_nullptr || candidate_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_path_is_inside_root(root_path, candidate_path) == FT_TRUE)
        return (FT_ERR_SUCCESS);
    return (FT_ERR_INVALID_PATH);
}

int32_t file_validate_regular_file_inside_root(const char *root_path,
    const char *candidate_path)
{
    char *canonical_root;
    char *canonical_candidate;
    int32_t error_code;

    if (root_path == ft_nullptr || candidate_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_get_type(candidate_path) != FILE_TYPE_REGULAR)
        return (FT_ERR_INVALID_PATH);
    canonical_root = ft_nullptr;
    error_code = cmp_path_canonical(root_path, &canonical_root);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    canonical_candidate = ft_nullptr;
    error_code = cmp_path_canonical(candidate_path, &canonical_candidate);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(canonical_root);
        return (error_code);
    }
    if (file_security_has_root_boundary(canonical_root,
            canonical_candidate) == FT_FALSE)
        error_code = FT_ERR_INVALID_PATH;
    cma_free(canonical_root);
    cma_free(canonical_candidate);
    return (error_code);
}

static int32_t file_security_write_descriptor(int32_t file_descriptor,
    const char *data, ft_size_t size)
{
    ft_size_t total_written;
    int64_t bytes_written;
    int32_t error_code;

    if (data == ft_nullptr && size > 0)
        return (FT_ERR_INVALID_POINTER);
    total_written = 0;
    while (total_written < size)
    {
        bytes_written = 0;
        error_code = cmp_write(file_descriptor, data + total_written,
                size - total_written, &bytes_written);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        if (bytes_written <= 0)
            return (FT_ERR_IO);
        total_written = total_written + static_cast<ft_size_t>(bytes_written);
    }
    return (FT_ERR_SUCCESS);
}

int32_t file_replace_safe(const char *path, const char *data, ft_size_t size)
{
    ft_string *directory_name;
    ft_string temporary_path;
    int32_t file_descriptor;
    int32_t error_code;
    int32_t sync_error;

    if (path == ft_nullptr || (data == ft_nullptr && size > 0))
        return (FT_ERR_INVALID_POINTER);
    directory_name = file_path_dirname_string(path);
    if (directory_name == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    error_code = temporary_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        file_security_delete_string(directory_name);
        return (error_code);
    }
    file_descriptor = -1;
    error_code = file_secure_temp_file(directory_name->c_str(), "replace",
            &temporary_path, &file_descriptor);
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_security_write_descriptor(file_descriptor, data, size);
    if (error_code == FT_ERR_SUCCESS)
    {
        sync_error = FT_ERR_SUCCESS;
        error_code = cmp_file_sync(file_descriptor, &sync_error);
        if (sync_error != FT_ERR_SUCCESS)
            error_code = sync_error;
    }
    if (file_descriptor >= 0)
        (void)cmp_close(file_descriptor);
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_move(temporary_path.c_str(), path);
    if (error_code == FT_ERR_SUCCESS)
    {
        sync_error = FT_ERR_SUCCESS;
        error_code = cmp_file_sync_directory(directory_name->c_str(),
            &sync_error);
        if (sync_error != FT_ERR_SUCCESS)
            error_code = sync_error;
    }
    if (error_code != FT_ERR_SUCCESS)
        (void)file_delete(temporary_path.c_str());
    (void)temporary_path.destroy();
    file_security_delete_string(directory_name);
    return (error_code);
}

int32_t file_close_descriptor(int32_t file_descriptor)
{
    return (cmp_close(file_descriptor));
}
