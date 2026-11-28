#include "compatebility_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#if defined(_WIN32) || defined(_WIN64)
# include <windows.h>
static DWORD g_orig_mode;
static ft_bool g_raw_mode_active;

static int32_t cmp_map_last_error_to_ft_error()
{
    DWORD last_error;
    int32_t error_code;

    last_error = GetLastError();
    if (last_error == 0)
        error_code = FT_ERR_TERMINATED;
    else
        error_code = cmp_map_system_error_to_ft(static_cast<int32_t>(last_error));
    return (error_code);
}

int32_t cmp_readline_enable_raw_mode(ft_bool *activated)
{
    HANDLE handle;
    DWORD mode;

    if (activated != ft_nullptr)
        *activated = FT_FALSE;
    if (g_raw_mode_active == FT_TRUE)
        return (FT_ERR_SUCCESS);
    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
    {
        int32_t error_code = cmp_map_last_error_to_ft_error();

        if (error_code == FT_ERR_INVALID_HANDLE)
        {
            g_raw_mode_active = FT_FALSE;
            return (FT_ERR_SUCCESS);
        }
        return (error_code);
    }
    if (!GetConsoleMode(handle, &mode))
    {
        int32_t error_code = cmp_map_last_error_to_ft_error();

        if (error_code == FT_ERR_INVALID_HANDLE)
        {
            g_raw_mode_active = FT_FALSE;
            return (FT_ERR_SUCCESS);
        }
        return (error_code);
    }
    const DWORD raw_mode = mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT);
    if (!SetConsoleMode(handle, raw_mode))
    {
        int32_t error_code = cmp_map_last_error_to_ft_error();

        if (error_code == FT_ERR_INVALID_HANDLE)
        {
            g_raw_mode_active = FT_FALSE;
            return (FT_ERR_SUCCESS);
        }
        return (error_code);
    }
    g_orig_mode = mode;
    g_raw_mode_active = FT_TRUE;
    if (activated != ft_nullptr)
        *activated = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_readline_disable_raw_mode()
{
    HANDLE handle;

    if (g_raw_mode_active == FT_FALSE)
    {
        return (FT_ERR_SUCCESS);
    }
    handle = GetStdHandle(STD_INPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return (cmp_map_last_error_to_ft_error());
    }
    if (!SetConsoleMode(handle, g_orig_mode))
    {
        return (cmp_map_last_error_to_ft_error());
    }
    g_raw_mode_active = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_readline_terminal_width(int32_t *width_out)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    HANDLE handle;

    if (width_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *width_out = 0;
    handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE)
        return (cmp_map_last_error_to_ft_error());
    if (!GetConsoleScreenBufferInfo(handle, &info))
        return (cmp_map_last_error_to_ft_error());
    *width_out = info.srWindow.Right - info.srWindow.Left + 1;
    return (FT_ERR_SUCCESS);
}
#else
# include <termios.h>
# include <unistd.h>
# include <sys/ioctl.h>
# include <errno.h>
static termios g_orig_termios;
static ft_bool g_raw_mode_active;

static int32_t cmp_map_errno_to_ft_error()
{
    if (errno == 0)
        return (FT_ERR_TERMINATED);
    return (cmp_map_system_error_to_ft(errno));
}

int32_t cmp_readline_enable_raw_mode(ft_bool *activated)
{
    if (activated != ft_nullptr)
        *activated = FT_FALSE;
    if (g_raw_mode_active == FT_TRUE)
        return (FT_ERR_SUCCESS);
    struct termios original;
    if (isatty(STDIN_FILENO) == 0)
    {
        g_raw_mode_active = FT_FALSE;
        return (FT_ERR_SUCCESS);
    }
    if (tcgetattr(STDIN_FILENO, &original) == -1)
    {
        return (cmp_map_errno_to_ft_error());
    }
    struct termios raw = original;
    raw.c_lflag &= ~(ECHO | ICANON);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == -1)
    {
        return (cmp_map_errno_to_ft_error());
    }
    g_orig_termios = original;
    g_raw_mode_active = FT_TRUE;
    if (activated != ft_nullptr)
        *activated = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_readline_disable_raw_mode()
{
    if (g_raw_mode_active == FT_FALSE)
        return (FT_ERR_SUCCESS);
    if (tcsetattr(STDIN_FILENO, TCSANOW, &g_orig_termios) == -1)
    {
        return (cmp_map_errno_to_ft_error());
    }
    g_raw_mode_active = FT_FALSE;
    return (FT_ERR_SUCCESS);
}

int32_t cmp_readline_terminal_width(int32_t *width_out)
{
    struct winsize window_size;

    if (width_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *width_out = 0;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size) == -1)
        return (cmp_map_errno_to_ft_error());
    *width_out = static_cast<int32_t>(window_size.ws_col);
    return (FT_ERR_SUCCESS);
}
#endif
