#include "compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include <cerrno>
#include <csignal>
#include <cstring>

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
# include <process.h>
#else
# include <unistd.h>
# include <sys/stat.h>
#endif

static t_cmp_service_signal_handler g_cmp_service_signal_handler = ft_nullptr;
static void *g_cmp_service_signal_context = ft_nullptr;

#if defined(_WIN32) || defined(_WIN64)
static ft_bool g_cmp_service_console_handler_installed = FT_FALSE;
static void (*g_cmp_service_previous_sigint)(int) = SIG_DFL;
static void (*g_cmp_service_previous_sigterm)(int) = SIG_DFL;

static BOOL WINAPI cmp_service_console_handler(DWORD control_type)
{
    int32_t mapped_signal;

    mapped_signal = 0;
    if (g_cmp_service_signal_handler == ft_nullptr)
        return (FALSE);
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT)
        mapped_signal = SIGINT;
    else if (control_type == CTRL_CLOSE_EVENT
        || control_type == CTRL_SHUTDOWN_EVENT
        || control_type == CTRL_LOGOFF_EVENT)
        mapped_signal = SIGTERM;
    if (mapped_signal == 0)
        return (FALSE);
    g_cmp_service_signal_handler(mapped_signal, g_cmp_service_signal_context);
    return (TRUE);
}

static void cmp_service_c_signal_dispatch(int signal_number)
{
    if (g_cmp_service_signal_handler == ft_nullptr)
        return ;
    g_cmp_service_signal_handler(signal_number,
        g_cmp_service_signal_context);
    return ;
}
#else
static struct sigaction g_cmp_service_previous_sigint;
static struct sigaction g_cmp_service_previous_sigterm;
#ifdef SIGHUP
static struct sigaction g_cmp_service_previous_sighup;
static ft_bool g_cmp_service_sighup_installed = FT_FALSE;
#endif
static ft_bool g_cmp_service_sigint_installed = FT_FALSE;
static ft_bool g_cmp_service_sigterm_installed = FT_FALSE;

static void cmp_service_signal_dispatch(int signal_number)
{
    if (g_cmp_service_signal_handler == ft_nullptr)
        return ;
    g_cmp_service_signal_handler(signal_number,
        g_cmp_service_signal_context);
    return ;
}
#endif

int32_t cmp_service_set_working_directory(const char *working_directory)
{
    if (working_directory == ft_nullptr || working_directory[0] == '\0')
        return (0);
#if defined(_WIN32) || defined(_WIN64)
    if (!SetCurrentDirectoryA(working_directory))
        return (-1);
    return (0);
#else
    if (chdir(working_directory) != 0)
        return (-1);
    return (0);
#endif
}

int32_t cmp_service_detach_process(ft_bool force_no_fork)
{
#if defined(_WIN32) || defined(_WIN64)
    if (force_no_fork == FT_TRUE)
        return (0);
    if (cmp_service_release_console() != 0)
        return (-1);
    return (0);
#else
    pid_t process_id;

    if (force_no_fork == FT_TRUE)
        return (0);
    process_id = fork();
    if (process_id < 0)
        return (-1);
    if (process_id > 0)
        return (1);
    if (setsid() < 0)
        return (-1);
    process_id = fork();
    if (process_id < 0)
        return (-1);
    if (process_id > 0)
        _exit(0);
    umask(0);
    return (0);
#endif
}

int32_t cmp_service_release_console(void)
{
#if defined(_WIN32) || defined(_WIN64)
    DWORD console_error;

    if (!FreeConsole())
    {
        console_error = GetLastError();
        if (console_error != ERROR_INVALID_HANDLE)
            return (-1);
    }
    return (0);
#else
    return (0);
#endif
}

int32_t cmp_service_install_signal_handlers(t_cmp_service_signal_handler handler,
    void *user_context)
{
    if (handler == ft_nullptr)
        return (-1);
    cmp_service_clear_signal_handlers();
    g_cmp_service_signal_handler = handler;
    g_cmp_service_signal_context = user_context;
#if defined(_WIN32) || defined(_WIN64)
    errno = 0;
    g_cmp_service_previous_sigint = std::signal(SIGINT,
            cmp_service_c_signal_dispatch);
    if (g_cmp_service_previous_sigint == SIG_ERR)
    {
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    errno = 0;
    g_cmp_service_previous_sigterm = std::signal(SIGTERM,
            cmp_service_c_signal_dispatch);
    if (g_cmp_service_previous_sigterm == SIG_ERR)
    {
        std::signal(SIGINT, g_cmp_service_previous_sigint);
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    if (!SetConsoleCtrlHandler(cmp_service_console_handler, TRUE))
    {
        std::signal(SIGINT, g_cmp_service_previous_sigint);
        std::signal(SIGTERM, g_cmp_service_previous_sigterm);
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    g_cmp_service_console_handler_installed = FT_TRUE;
#else
    struct sigaction action;

    std::memset(&action, 0, sizeof(action));
    action.sa_handler = cmp_service_signal_dispatch;
    sigemptyset(&action.sa_mask);
    if (sigaction(SIGINT, &action, &g_cmp_service_previous_sigint) != 0)
    {
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    g_cmp_service_sigint_installed = FT_TRUE;
    if (sigaction(SIGTERM, &action, &g_cmp_service_previous_sigterm) != 0)
    {
        sigaction(SIGINT, &g_cmp_service_previous_sigint, ft_nullptr);
        g_cmp_service_sigint_installed = FT_FALSE;
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    g_cmp_service_sigterm_installed = FT_TRUE;
#ifdef SIGHUP
    if (sigaction(SIGHUP, &action, &g_cmp_service_previous_sighup) != 0)
    {
        sigaction(SIGTERM, &g_cmp_service_previous_sigterm, ft_nullptr);
        sigaction(SIGINT, &g_cmp_service_previous_sigint, ft_nullptr);
        g_cmp_service_sigterm_installed = FT_FALSE;
        g_cmp_service_sigint_installed = FT_FALSE;
        g_cmp_service_signal_handler = ft_nullptr;
        g_cmp_service_signal_context = ft_nullptr;
        return (-1);
    }
    g_cmp_service_sighup_installed = FT_TRUE;
#endif
#endif
    return (0);
}

void cmp_service_clear_signal_handlers(void)
{
#if defined(_WIN32) || defined(_WIN64)
    if (g_cmp_service_console_handler_installed == FT_TRUE)
    {
        SetConsoleCtrlHandler(cmp_service_console_handler, FALSE);
        g_cmp_service_console_handler_installed = FT_FALSE;
        std::signal(SIGINT, g_cmp_service_previous_sigint);
        std::signal(SIGTERM, g_cmp_service_previous_sigterm);
    }
    g_cmp_service_previous_sigint = SIG_DFL;
    g_cmp_service_previous_sigterm = SIG_DFL;
#else
    if (g_cmp_service_sigint_installed == FT_TRUE)
    {
        sigaction(SIGINT, &g_cmp_service_previous_sigint, ft_nullptr);
        g_cmp_service_sigint_installed = FT_FALSE;
    }
    if (g_cmp_service_sigterm_installed == FT_TRUE)
    {
        sigaction(SIGTERM, &g_cmp_service_previous_sigterm, ft_nullptr);
        g_cmp_service_sigterm_installed = FT_FALSE;
    }
#ifdef SIGHUP
    if (g_cmp_service_sighup_installed == FT_TRUE)
    {
        sigaction(SIGHUP, &g_cmp_service_previous_sighup, ft_nullptr);
        g_cmp_service_sighup_installed = FT_FALSE;
    }
#endif
#endif
    g_cmp_service_signal_handler = ft_nullptr;
    g_cmp_service_signal_context = ft_nullptr;
    return ;
}
