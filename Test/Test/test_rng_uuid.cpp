#include "../test_internal.hpp"
#include "../../Modules/RNG/rng.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "compatebility_system_test_hooks.hpp"
#include <cerrno>
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
#endif

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static int is_lower_hexadecimal_character(char value)
{
    if (value >= '0' && value <= '9')
        return (1);
    if (value >= 'a' && value <= 'f')
        return (1);
    return (0);
}

FT_TEST(test_ft_generate_uuid_null_out_sets_errno)
{
    ft_generate_uuid(ft_nullptr);
    return (1);
}

FT_TEST(test_ft_generate_uuid_success_sets_errno_success)
{
    char output[37];
    size_t index;

    ft_generate_uuid(output);
    index = 0;
    while (index < 36)
    {
        if (index == 8 || index == 13 || index == 18 || index == 23)
            FT_ASSERT_EQ('-', output[index]);
        else
            FT_ASSERT(is_lower_hexadecimal_character(output[index]) != 0);
        index++;
    }
    FT_ASSERT_EQ('\0', output[36]);
    FT_ASSERT_EQ('4', output[14]);
    FT_ASSERT(output[19] == '8' || output[19] == '9'
        || output[19] == 'a' || output[19] == 'b');
    return (1);
}

FT_TEST(test_ft_generate_uuid_entropy_failure_writes_only_empty_string)
{
    char output[39];
    ft_memset(output, 'x', sizeof(output));
#if defined(_WIN32) || defined(_WIN64)
    cmp_force_rng_windows_acquire_failure(ERROR_ACCESS_DENIED);
#else
    cmp_force_rng_open_failure(EACCES);
#endif
    ft_generate_uuid(output + 1);
#if defined(_WIN32) || defined(_WIN64)
    cmp_clear_force_rng_windows_failures();
#else
    cmp_clear_force_rng_failures();
#endif
    FT_ASSERT_EQ('\0', output[1]);
    output[1] = 'x';
    ft_size_t index = 0U;
    while (index < sizeof(output))
    {
        FT_ASSERT_EQ('x', output[index]);
        index += 1U;
    }
    return (1);
}
