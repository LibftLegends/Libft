#include "errno.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include <cerrno>

void    su_exit(int32_t exit_code);

void    ft_exit(const char *error_msg, int32_t exit_code)
{
    const int32_t system_error = errno;
    int32_t error_code = FT_ERR_SUCCESS;
    if (system_error != 0)
        error_code = cmp_map_system_error_to_ft(system_error);
    if (error_msg != ft_nullptr || error_code != FT_ERR_SUCCESS)
        ft_perror(error_msg, error_code);
    su_exit(exit_code);
    return ;
}
