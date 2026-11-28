#include "file_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"

int32_t file_metadata_diff(const char *left_path, const char *right_path,
    uint32_t *difference_out)
{
    if (left_path == ft_nullptr || right_path == ft_nullptr
        || difference_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    cmp_file_metadata left{};
    cmp_file_metadata right{};
    if (cmp_file_read_metadata(left_path, left) != FT_ERR_SUCCESS
        || cmp_file_read_metadata(right_path, right) != FT_ERR_SUCCESS
        || left.type == FILE_TYPE_UNKNOWN || right.type == FILE_TYPE_UNKNOWN)
        return (FT_ERR_IO);
    uint32_t differences = FILE_METADATA_DIFF_NONE;
    if (left.type == FILE_TYPE_MISSING || right.type == FILE_TYPE_MISSING)
    {
        if (left.type != right.type)
            differences = FILE_METADATA_DIFF_MISSING;
        *difference_out = differences;
        return (FT_ERR_SUCCESS);
    }
    if (left.details_error != FT_ERR_SUCCESS)
        return (left.details_error);
    if (right.details_error != FT_ERR_SUCCESS)
        return (right.details_error);
    if (left.type != right.type)
        differences |= FILE_METADATA_DIFF_TYPE;
    if (left.type == FILE_TYPE_REGULAR && right.type == FILE_TYPE_REGULAR && left.size != right.size)
        differences |= FILE_METADATA_DIFF_SIZE;
    if (left.permissions != right.permissions)
        differences |= FILE_METADATA_DIFF_PERMISSIONS;
    *difference_out = differences;
    return (FT_ERR_SUCCESS);
}
