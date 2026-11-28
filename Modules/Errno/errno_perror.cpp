#include "errno.hpp"
#include "errno_internal.hpp"
#include "../Basic/class_nullptr.hpp"

int32_t ft_perror(const char *error_msg, int32_t error_code)
{
    if (error_msg == ft_nullptr)
    {
        const char *const fragments[] = {ft_strerror(error_code), "\n"};
        return (errno_write_stderr_fragments(fragments, 2U));
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        const char *const fragments[] = {error_msg, "\n"};
        return (errno_write_stderr_fragments(fragments, 2U));
    }
    const char *const fragments[] = {error_msg, ": ", ft_strerror(error_code), "\n"};
    return (errno_write_stderr_fragments(fragments, 4U));
}
