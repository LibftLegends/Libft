#include "filesystem_internal.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../File/file_internal.hpp"
#include "../Time/time.hpp"
#include <atomic>
#include <cstring>

static char *filesystem_write_temp_number(char *end, uint64_t value) noexcept
{
    constexpr char DIGITS[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    do
    {
        *--end = DIGITS[value % 36U];
        value /= 36U;
    }
    while (value != 0);
    return (end);
}

static int32_t filesystem_append_temp_suffix(ft_string &name)
{
    static std::atomic<uint64_t> counter{0};
    static const uint64_t timestamp = static_cast<uint64_t>(time_now_ms());
    const uint64_t ordinal = counter.fetch_add(1, std::memory_order_relaxed);
    constexpr ft_size_t MAX_DIGITS = 13;
    char suffix[3 * MAX_DIGITS + 3];
    char *end = suffix + sizeof(suffix);
    char *cursor = filesystem_write_temp_number(end, ordinal);
    *--cursor = '-';
    cursor = filesystem_write_temp_number(cursor, cmp_get_process_id());
    *--cursor = '-';
    cursor = filesystem_write_temp_number(cursor, timestamp);
    *--cursor = '_';
    return (name.append(cursor, static_cast<ft_size_t>(end - cursor)));
}

static ft_string *filesystem_join_temp_name(const char *directory, ft_string &name)
{
    if (std::strpbrk(name.c_str(), "/\\:") != ft_nullptr)
        return (file_path_join(directory, name.c_str()));
    ft_string *path = file_path_normalize(directory);
    if (path != ft_nullptr && file_path_append_normalized_relative(*path, name) != FT_ERR_SUCCESS)
    {
        delete path;
        return (ft_nullptr);
    }
    return (path);
}

int32_t filesystem_temp_path(const char *prefix, const char *extension,
    ft_string *output)
{
    if (output == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (prefix == ft_nullptr || prefix[0] == '\0')
        prefix = "libft";
    ft_string name;
    int32_t error_code = name.initialize(prefix);
    if (error_code == FT_ERR_SUCCESS)
        error_code = filesystem_append_temp_suffix(name);
    if (error_code == FT_ERR_SUCCESS && extension != ft_nullptr && extension[0] != '\0')
    {
        if (extension[0] != '.')
            error_code = name.append('.');
        if (error_code == FT_ERR_SUCCESS)
            error_code = name.append(extension);
    }
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);

    char *native_directory = ft_nullptr;
    error_code = cmp_get_temp_directory(&native_directory);
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    ft_string *path = filesystem_join_temp_name(native_directory, name);
    cma_free(native_directory);
    if (path == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    filesystem_normalize_portable_slashes(path);
    error_code = output->move(*path);
    delete path;
    return (error_code);
}
