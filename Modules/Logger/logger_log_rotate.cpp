#include "logger_internal.hpp"
#include "../Basic/basic.hpp"
#include "../Time/time.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <inttypes.h>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Errno/errno.hpp"

static int32_t logger_build_rotation_path(const ft_string &rotation_base,
    ft_size_t entry_index, ft_string &path)
{
    char index_buffer[32];

    std::snprintf(index_buffer, sizeof(index_buffer), "%" PRIu64,
        entry_index);
    path = rotation_base + index_buffer;
    if (path.get_error() != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t logger_remove_oldest_rotation(const ft_string &rotation_base,
    ft_size_t retention_count)
{
    ft_string oldest_path;
    int32_t delete_error;

    if (logger_build_rotation_path(rotation_base, retention_count,
            oldest_path) != 0)
        return (FT_ERR_INTERNAL);
    delete_error = FT_ERR_SUCCESS;
    if (cmp_file_delete(oldest_path.c_str(), &delete_error) == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);
    if (delete_error != FT_ERR_IO)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}

static int32_t logger_shift_rotation_chain(const ft_string &rotation_base,
    ft_size_t retention_count)
{
    ft_size_t current_index;
    ft_string source_path;
    ft_string destination_path;
    int32_t move_error;

    current_index = retention_count;
    while (current_index > 1)
    {
        if (logger_build_rotation_path(rotation_base, current_index - 1,
                source_path) != 0)
            return (FT_ERR_INTERNAL);
        if (logger_build_rotation_path(rotation_base, current_index,
                destination_path) != 0)
            return (FT_ERR_INTERNAL);
        move_error = FT_ERR_SUCCESS;
        if (cmp_file_move(source_path.c_str(), destination_path.c_str(),
                &move_error) != FT_ERR_SUCCESS)
        {
            if (move_error != FT_ERR_IO)
                return (FT_ERR_INTERNAL);
        }
        current_index -= 1;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t logger_prepare_rotation_internal(s_file_sink *sink,
    ft_bool *rotate_for_size, ft_bool *rotate_for_age)
{
    t_time current_time;
    int64_t age_seconds;
    ft_size_t file_size;
    t_time modification_time;
    ft_size_t retention_count;
    int32_t metadata_error;

    if (!sink || !rotate_for_size || !rotate_for_age)
        return (FT_ERR_INTERNAL);
    if (sink->path.empty())
        return (FT_ERR_INTERNAL);
    if (sink->max_size == 0 && sink->max_age_seconds == 0)
    {
        *rotate_for_size = FT_FALSE;
        *rotate_for_age = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    metadata_error = cmp_file_get_size(sink->path.c_str(), &file_size, ft_nullptr);
    if (metadata_error != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    retention_count = sink->retention_count;
    if (retention_count > 1024)
        retention_count = 0;
    if (retention_count == 0)
        retention_count = 1;
    sink->retention_count = retention_count;
    *rotate_for_size = FT_FALSE;
    if (sink->max_size > 0
        && file_size >= sink->max_size)
        *rotate_for_size = FT_TRUE;
    *rotate_for_age = FT_FALSE;
    if (sink->max_age_seconds > 0)
    {
        metadata_error = cmp_file_get_modification_time(sink->path.c_str(), &modification_time,
                ft_nullptr);
        if (metadata_error != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        current_time = time_now();
        if (current_time == static_cast<t_time>(-1))
            return (FT_ERR_INTERNAL);
        if (current_time >= modification_time)
        {
            age_seconds = current_time - modification_time;
            if (age_seconds >= static_cast<int64_t>(sink->max_age_seconds))
                *rotate_for_age = FT_TRUE;
        }
    }
    return (FT_ERR_SUCCESS);
}

int32_t logger_prepare_rotation(s_file_sink *sink, ft_bool *rotate_for_size,
    ft_bool *rotate_for_age)
{
    ft_bool lock_acquired;
    int32_t operation_result;

    lock_acquired = FT_FALSE;
    if (file_sink_lock(sink, &lock_acquired) != 0)
        return (FT_ERR_INTERNAL);
    operation_result = logger_prepare_rotation_internal(sink, rotate_for_size,
            rotate_for_age);
    if (lock_acquired)
        file_sink_unlock(sink, lock_acquired);
    return (operation_result);
}

void logger_execute_rotation(s_file_sink *sink)
{
    ft_string rotation_base;
    ft_string rotated_path;
    int32_t close_result;
    int32_t reopen_flags;
    int32_t rotation_errno;
    ft_size_t retention_count;
    ft_bool lock_acquired;
    ft_bool should_unlock;
    ft_bool rotation_failed;

    if (!sink)
        return ;
    lock_acquired = FT_FALSE;
    should_unlock = FT_FALSE;
    rotation_errno = 0;
    rotation_failed = FT_FALSE;
    if (file_sink_lock(sink, &lock_acquired) != 0)
        return ;
    if (lock_acquired)
        should_unlock = FT_TRUE;
    retention_count = sink->retention_count;
    reopen_flags = O_CREAT | O_WRONLY | O_APPEND;
    if (retention_count > 0)
    {
        rotation_base = sink->path + ".";
        if (rotation_base.get_error() != FT_ERR_SUCCESS)
        {
            rotation_failed = FT_TRUE;
            rotation_errno = ENAMETOOLONG;
        }
        else if (logger_remove_oldest_rotation(rotation_base, retention_count) != 0)
        {
            rotation_failed = FT_TRUE;
            rotation_errno = (errno != 0) ? errno : ENAMETOOLONG;
        }
        if (rotation_failed == FT_FALSE
            && logger_shift_rotation_chain(rotation_base, retention_count) != 0)
        {
            rotation_failed = FT_TRUE;
            rotation_errno = (errno != 0) ? errno : ENAMETOOLONG;
        }
        rotated_path = sink->path + ".1";
        if (rotated_path.get_error() != FT_ERR_SUCCESS)
        {
            rotation_failed = FT_TRUE;
            rotation_errno = ENAMETOOLONG;
        }
        else if (rotation_failed == FT_FALSE
            && cmp_file_move(sink->path.c_str(), rotated_path.c_str(), ft_nullptr) != FT_ERR_SUCCESS)
        {
            rotation_failed = FT_TRUE;
            rotation_errno = (errno != 0) ? errno : ENAMETOOLONG;
        }
    }
    else
    {
        reopen_flags = O_CREAT | O_WRONLY | O_TRUNC | O_APPEND;
    }
    if (rotation_failed != FT_FALSE && retention_count > 0)
        goto cleanup;
    close_result = cmp_close(sink->file_descriptor);
    if (close_result == -1)
    {
        sink->file_descriptor = -1;
        goto cleanup;
    }
    sink->file_descriptor = -1;
    sink->file_descriptor = cmp_open(sink->path.c_str(), reopen_flags, 0644);
    if (sink->file_descriptor == -1)
        goto cleanup;
    if (rotation_failed != FT_FALSE && rotation_errno != 0)
        errno = rotation_errno;

cleanup:
    if (rotation_failed != FT_FALSE && sink->file_descriptor != -1 && rotation_errno != 0)
        errno = rotation_errno;
    if (should_unlock)
        file_sink_unlock(sink, lock_acquired);
    return ;
}

void ft_log_rotate(s_file_sink *sink)
{
    ft_bool rotate_for_size;
    ft_bool rotate_for_age;

    rotate_for_size = FT_FALSE;
    rotate_for_age = FT_FALSE;
    if (logger_prepare_rotation(sink, &rotate_for_size, &rotate_for_age) != 0)
        return ;
    if (!rotate_for_size && !rotate_for_age)
        return ;
    logger_execute_rotation(sink);
    return ;
}
