#include "system_utils.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#include <cerrno>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

#ifdef LIBFT_TEST_BUILD
static thread_local int32_t g_su_test_open_error = 0;

void su_test_inject_next_open_error(int32_t system_error)
{
    g_su_test_open_error = system_error;
    return ;
}

void su_test_clear_open_error(void)
{
    g_su_test_open_error = 0;
    return ;
}

static thread_local uint32_t g_su_test_write_interrupts = 0U;

void su_test_inject_write_interrupts(uint32_t count)
{
    g_su_test_write_interrupts = count;
    return ;
}

void su_test_clear_write_interrupts(void)
{
    g_su_test_write_interrupts = 0U;
    return ;
}

ft_bool su_test_take_write_interrupt(void)
{
    if (g_su_test_write_interrupts == 0U)
        return (FT_FALSE);
    g_su_test_write_interrupts -= 1U;
    return (FT_TRUE);
}
#endif

int32_t su_open(const char *path_name)
{
    int32_t result;

    result = cmp_open(path_name);
    if (result < 0)
    {
        if (cmp_map_system_error_to_ft(errno) == FT_ERR_SUCCESS)
            return (-1);
    }
    return (result);
}

int32_t su_open(const char *path_name, int32_t flags)
{
    int32_t result;

    result = cmp_open(path_name, flags);
    if (result < 0)
    {
        if (cmp_map_system_error_to_ft(errno) == FT_ERR_SUCCESS)
            return (-1);
    }
    return (result);
}

int32_t su_open(const char *path_name, int32_t flags, mode_t mode)
{
    int32_t result;

    result = cmp_open(path_name, flags, mode);
    if (result < 0)
    {
        if (cmp_map_system_error_to_ft(errno) == FT_ERR_SUCCESS)
            return (-1);
    }
    return (result);
}

int32_t su_open_with_error(const char *path_name, int32_t flags,
    mode_t mode, int32_t *file_descriptor_out, int32_t *error_code_out)
{
    int32_t file_descriptor;
    int32_t system_error;
    int32_t mapped_error;

    if (file_descriptor_out != ft_nullptr)
        *file_descriptor_out = -1;
    if (error_code_out != ft_nullptr)
        *error_code_out = FT_ERR_SUCCESS;
    if (file_descriptor_out == ft_nullptr || error_code_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (path_name == ft_nullptr)
    {
        *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (path_name[0] == '\0')
    {
        *error_code_out = FT_ERR_INVALID_PATH;
        return (FT_ERR_INVALID_PATH);
    }
#ifdef LIBFT_TEST_BUILD
    if (g_su_test_open_error != 0)
    {
        system_error = g_su_test_open_error;
        g_su_test_open_error = 0;
        mapped_error = cmp_map_system_error_to_ft(system_error);
        if (mapped_error == FT_ERR_SUCCESS)
            mapped_error = FT_ERR_FILE_OPEN_FAILED;
        *error_code_out = mapped_error;
        return (mapped_error);
    }
#endif
    file_descriptor = cmp_open(path_name, flags, mode);
    if (file_descriptor < 0)
    {
        mapped_error = cmp_get_last_open_error_code();
        if (mapped_error == FT_ERR_SUCCESS)
        {
            system_error = cmp_get_last_open_error();
            mapped_error = cmp_map_system_error_to_ft(system_error);
        }
        if (mapped_error == FT_ERR_SUCCESS)
            mapped_error = FT_ERR_FILE_OPEN_FAILED;
        *error_code_out = mapped_error;
        return (mapped_error);
    }
    *file_descriptor_out = file_descriptor;
    return (FT_ERR_SUCCESS);
}
