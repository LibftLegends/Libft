#include "../Basic/class_nullptr.hpp"
#include "system_utils.hpp"
#include <cstdlib>
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <csignal>

const char  *su_internal_take_abort_reason(void);

#ifndef LIBFT_TEST_BUILD
static ft_bool su_abort_should_print_diagnostics(void) noexcept
{
    return (FT_TRUE);
}
#endif

void    su_abort(void)
{
#ifndef LIBFT_TEST_BUILD
    const char  *reason;

    reason = su_internal_take_abort_reason();
    if (reason == ft_nullptr)
        reason = "su_abort invoked";
    if (su_abort_should_print_diagnostics() == FT_TRUE)
    {
        su_run_resource_tracers(reason);
        std::fprintf(stderr, "libft abort: %s\n", reason);
    }
    std::fflush(nullptr);
#endif
    (void)std::raise(SIGABRT);
    std::abort();
    return ;
}

void    su_exit(int32_t exit_code)
{
    std::_Exit(exit_code);
    return ;
}
