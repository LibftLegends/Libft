#include "file_internal.hpp"
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

static int32_t file_security_append_random_hex(ft_string &name)
{
    static constexpr char HEX_DIGITS[] = "0123456789abcdef";
    uint8_t random_bytes[3];
    char suffix[sizeof(random_bytes) * 2];

    const int32_t error_code = rng_secure_bytes(random_bytes, sizeof(random_bytes));
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_size_t index = 0;
    while (index < sizeof(random_bytes))
    {
        suffix[index * 2] = HEX_DIGITS[random_bytes[index] >> 4U];
        suffix[index * 2 + 1] = HEX_DIGITS[random_bytes[index] & 0x0FU];
        ++index;
    }
    return (name.append(suffix, sizeof(suffix)));
}

static void file_security_delete_string(ft_string *string) noexcept
{
    if (string == ft_nullptr)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

static int32_t file_security_build_temp_path(const char *native_directory_path,
    const char *prefix, ft_string **path_out)
{
    ft_string name;

    int32_t error_code = name.initialize(prefix);
    if (error_code == FT_ERR_SUCCESS)
        error_code = file_security_append_random_hex(name);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    *path_out = file_path_join(native_directory_path, name.c_str());
    if (*path_out == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

static int32_t file_security_open_temp_candidate(const char *path, int32_t *file_descriptor_out)
{
    const int32_t file_descriptor = cmp_open(path, O_RDWR | O_CREAT | O_EXCL
        | FILE_SECURITY_BINARY_FLAG, 0600);
    if (file_descriptor >= 0)
    {
        *file_descriptor_out = file_descriptor;
        return (FT_ERR_SUCCESS);
    }
    const int32_t open_error = cmp_get_last_open_error();
#if defined(_WIN32) || defined(_WIN64)
    if (open_error == ERROR_FILE_EXISTS || open_error == ERROR_ALREADY_EXISTS)
        return (FT_ERR_ALREADY_EXISTS);
#else
    if (open_error == EEXIST)
        return (FT_ERR_ALREADY_EXISTS);
#endif
    if (open_error == 0)
    {
        const int32_t error_code = cmp_get_last_open_error_code();
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        return (FT_ERR_IO);
    }
    return (cmp_file_error_to_errno(open_error));
}

static int32_t file_security_open_temp(const char *native_directory_path,
    const char *prefix, ft_string &path_out, int32_t *file_descriptor_out)
{
    uint32_t attempt_count = 0;
    while (attempt_count < 64U)
    {
        ft_string *candidate_path = ft_nullptr;
        int32_t error_code = file_security_build_temp_path(native_directory_path,
            prefix, &candidate_path);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        int32_t file_descriptor;
        error_code = file_security_open_temp_candidate(candidate_path->c_str(), &file_descriptor);
        if (error_code == FT_ERR_SUCCESS)
        {
            error_code = path_out.move(*candidate_path);
            if (error_code == FT_ERR_SUCCESS)
                *file_descriptor_out = file_descriptor;
            else
            {
                (void)cmp_close(file_descriptor);
                (void)file_delete(candidate_path->c_str());
            }
            delete candidate_path;
            return (error_code);
        }
        delete candidate_path;
        if (error_code != FT_ERR_ALREADY_EXISTS)
            return (error_code);
        ++attempt_count;
    }
    return (FT_ERR_ALREADY_EXISTS);
}

int32_t file_secure_temp_file(const char *directory_path, const char *prefix,
    ft_string *path_out, int32_t *file_descriptor_out)
{
    char *native_directory_path = ft_nullptr;
    int32_t error_code;

    if (path_out == ft_nullptr || file_descriptor_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (prefix == ft_nullptr || prefix[0] == '\0')
        prefix = "libft_secure";
    if (directory_path == ft_nullptr || directory_path[0] == '\0')
    {
        if (cmp_get_temp_directory(&native_directory_path) != FT_ERR_SUCCESS)
            return (FT_ERR_NO_MEMORY);
    }
    else
    {
        error_code = cmp_translate_path_to_native(directory_path, &native_directory_path);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
    }
    error_code = file_security_open_temp(native_directory_path, prefix, *path_out,
        file_descriptor_out);
    cma_free(native_directory_path);
    return (error_code);
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

static ft_bool file_security_is_normalized_relative(const char *path) noexcept
{
    const char *component;
    const char *cursor;

    component = path;
    cursor = path;
    while (FT_TRUE)
    {
        while (*cursor != '\0' && *cursor != '/')
        {
            if (*cursor == '\\' || *cursor == ':')
                return (FT_FALSE);
            ++cursor;
        }
        if (cursor == component || (*component == '.'
                && (cursor - component == 1 || (cursor - component == 2
                        && component[1] == '.'))))
            return (FT_FALSE);
        if (*cursor == '\0')
            return (FT_TRUE);
        ++cursor;
        component = cursor;
    }
}

ft_bool file_path_is_inside_root(const char *root_path, const char *candidate_path)
{
    ft_string *root_normalized;
    ft_string *candidate_normalized;
    ft_bool result;

    if (root_path == ft_nullptr || candidate_path == ft_nullptr)
        return (FT_FALSE);
    /* The common relative path has no components that normalization would
     * change. Compare it directly and leave resolved-path checks to callers. */
    if (file_security_is_normalized_relative(root_path) == FT_TRUE
        && file_security_is_normalized_relative(candidate_path) == FT_TRUE)
        return (file_security_has_root_boundary(root_path, candidate_path));
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

int32_t file_validate_path_resolution_inside_root(const char *root_path,
    const char *candidate_path)
{
    ft_string *parent_path;
    char *canonical_root;
    char *canonical_parent;
    char *canonical_candidate;
    file_type candidate_type;
    int32_t error_code;

    if (root_path == ft_nullptr || candidate_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (file_path_is_inside_root(root_path, candidate_path) == FT_FALSE)
        return (FT_ERR_INVALID_PATH);
    parent_path = file_path_dirname_string(candidate_path);
    if (parent_path == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    canonical_root = ft_nullptr;
    error_code = cmp_path_canonical(root_path, &canonical_root);
    if (error_code != FT_ERR_SUCCESS)
    {
        file_security_delete_string(parent_path);
        return (error_code);
    }
    canonical_parent = ft_nullptr;
    error_code = cmp_path_canonical(parent_path->c_str(), &canonical_parent);
    file_security_delete_string(parent_path);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(canonical_root);
        return (error_code);
    }
    if (file_security_has_root_boundary(canonical_root,
            canonical_parent) == FT_FALSE)
    {
        cma_free(canonical_root);
        cma_free(canonical_parent);
        return (FT_ERR_INVALID_PATH);
    }
    candidate_type = file_get_type(candidate_path);
    if (candidate_type == FILE_TYPE_MISSING)
    {
        cma_free(canonical_root);
        cma_free(canonical_parent);
        return (FT_ERR_SUCCESS);
    }
    if (candidate_type == FILE_TYPE_SYMLINK
        || candidate_type == FILE_TYPE_UNKNOWN)
    {
        cma_free(canonical_root);
        cma_free(canonical_parent);
        return (FT_ERR_INVALID_PATH);
    }
    canonical_candidate = ft_nullptr;
    error_code = cmp_path_canonical(candidate_path, &canonical_candidate);
    if (error_code != FT_ERR_SUCCESS)
    {
        cma_free(canonical_root);
        cma_free(canonical_parent);
        return (error_code);
    }
    if (file_security_has_root_boundary(canonical_root,
            canonical_candidate) == FT_FALSE)
        error_code = FT_ERR_INVALID_PATH;
    cma_free(canonical_root);
    cma_free(canonical_parent);
    cma_free(canonical_candidate);
    return (error_code);
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

static ft_size_t file_security_parent_prefix(const char *path)
{
    // Preserve components as provided so the OS resolves the temp and target alike.
    const ft_bool windows_separators = cmp_path_separator() == '\\';
    ft_size_t index = ft_strlen_size_t(path);
    while (index > 0)
    {
        const char previous = path[index - 1];
        if (previous == '/' || (windows_separators == FT_TRUE && previous == '\\'))
            break ;
        --index;
    }
    if (windows_separators == FT_TRUE && index < 2
        && path[index] != '\0' && path[index + 1] == ':'
        && file_path_is_absolute(path + index) == FT_TRUE)
        index += 2;
    return (index);
}

static int32_t file_security_open_sibling(const char *target, ft_size_t parent_length,
    const char *prefix, ft_string &temporary_path, int32_t *file_descriptor_out)
{
    uint32_t attempt = 0;
    while (attempt < 64U)
    {
        int32_t error_code = temporary_path.assign(target, parent_length);
        if (error_code == FT_ERR_SUCCESS)
            error_code = temporary_path.append(prefix);
        if (error_code == FT_ERR_SUCCESS)
            error_code = file_security_append_random_hex(temporary_path);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        error_code = file_security_open_temp_candidate(temporary_path.c_str(),
            file_descriptor_out);
        if (error_code != FT_ERR_ALREADY_EXISTS)
            return (error_code);
        ++attempt;
    }
    return (FT_ERR_ALREADY_EXISTS);
}

int32_t file_replace_stream(const char *path, ft_bool durable,
    file_replace_write_callback write_contents, void *context)
{
    if (path == ft_nullptr || write_contents == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (path[0] == '\0')
        return (FT_ERR_INVALID_PATH);
    const ft_size_t parent_length = file_security_parent_prefix(path);
    ft_string directory;
    const char *prefix = "atomic";
    if (durable == FT_TRUE)
    {
        int32_t error_code = directory.initialize(".");
        if (error_code == FT_ERR_SUCCESS && parent_length > 0)
            error_code = directory.assign(path, parent_length);
        if (error_code != FT_ERR_SUCCESS)
            return (error_code);
        prefix = "replace";
    }
    // A new temporary name must be distinct even if the target does not exist.
    const char target_initial = path[parent_length];
    if (target_initial == prefix[0] || target_initial == prefix[0] - 'a' + 'A')
        prefix = "temp";
    ft_string temporary_path;
    int32_t error_code = temporary_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    int32_t file_descriptor;
    error_code = file_security_open_sibling(path, parent_length, prefix, temporary_path,
        &file_descriptor);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = write_contents(file_descriptor, context);
    if (error_code == FT_ERR_SUCCESS && durable == FT_TRUE)
        error_code = cmp_file_sync(file_descriptor, ft_nullptr);
    const int32_t close_error = cmp_close(file_descriptor);
    if (error_code == FT_ERR_SUCCESS)
        error_code = close_error;
    if (error_code == FT_ERR_SUCCESS)
        error_code = cmp_file_replace_atomic(temporary_path.c_str(), path);
    if (error_code != FT_ERR_SUCCESS)
        (void)file_delete(temporary_path.c_str());
    else if (durable == FT_TRUE)
        error_code = cmp_file_sync_directory(directory.c_str(), ft_nullptr);
    return (error_code);
}

int32_t file_replace_contents(const char *path, const char *data, ft_size_t size,
    ft_bool durable)
{
    if (data == ft_nullptr && size != 0)
        return (FT_ERR_INVALID_POINTER);
    struct write_context
    {
        const char *data;
        ft_size_t size;
    };
    write_context context{data, size};
    return (file_replace_stream(path, durable, [](int32_t descriptor, void *opaque) -> int32_t
    {
        const auto &contents = *static_cast<write_context *>(opaque);
        return (file_write_all_to_descriptor(descriptor, contents.data, contents.size));
    }, &context));
}

int32_t file_replace_safe(const char *path, const char *data, ft_size_t size)
{
    return (file_replace_contents(path, data, size, FT_TRUE));
}

int32_t file_close_descriptor(int32_t file_descriptor)
{
    return (cmp_close(file_descriptor));
}
