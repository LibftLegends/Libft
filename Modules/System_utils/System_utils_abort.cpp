#include "../Basic/class_nullptr.hpp"
#include "system_utils.hpp"
#include <cstdlib>
#include <cstdio>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include <csignal>

const char  *su_internal_take_abort_reason(void);

static ft_bool su_abort_should_print_diagnostics(void) noexcept
{
#ifdef LIBFT_TEST_BUILD
    return (FT_FALSE);
#endif
    return (FT_TRUE);
}

void    su_abort(void)
{
    const char  *reason;

    reason = su_internal_take_abort_reason();
    if (reason == ft_nullptr)
        reason = "su_abort invoked";
    if (su_abort_should_print_diagnostics() == FT_TRUE)
    {
        su_run_resource_tracers(reason);
        std::fprintf(stderr, "libft abort: %s\n", reason);
    }
#ifndef LIBFT_TEST_BUILD
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
