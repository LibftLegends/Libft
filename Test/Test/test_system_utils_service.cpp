#include "../test_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <process.h>
#else
# include <unistd.h>
#endif

static void remove_file_if_exists(const char *path)
{
    if (!path)
        return ;
    std::remove(path);
    return ;
}

FT_TEST(test_su_service_daemonize_creates_pid_file)
{
    char    pid_path[128];
    FILE    *pid_file;
    char    buffer[64];
    char    *line_result;
    long    file_pid;

#if defined(_WIN32) || defined(_WIN64)
    std::snprintf(pid_path, sizeof(pid_path), "service_pid_%lu.pid",
        static_cast<unsigned long>(_getpid()));
#else
    std::snprintf(pid_path, sizeof(pid_path), "service_pid_%ld.pid",
        static_cast<long>(getpid()));
#endif
    remove_file_if_exists(pid_path);
    su_service_force_no_fork(true);
    FT_ASSERT_EQ(0, su_service_daemonize(".", pid_path, false));
    su_service_force_no_fork(false);
    pid_file = std::fopen(pid_path, "r");
    FT_ASSERT(pid_file != ft_nullptr);
    std::memset(buffer, 0, sizeof(buffer));
    line_result = std::fgets(buffer, sizeof(buffer), pid_file);
    std::fclose(pid_file);
    FT_ASSERT(line_result != ft_nullptr);
    file_pid = std::strtol(buffer, ft_nullptr, 10);
#if defined(_WIN32) || defined(_WIN64)
    FT_ASSERT_EQ(static_cast<long>(_getpid()), file_pid);
#else
    FT_ASSERT_EQ(static_cast<long>(getpid()), file_pid);
#endif
    remove_file_if_exists(pid_path);
    return (1);
}

FT_TEST(test_su_service_install_signal_handlers_rejects_null)
{
    FT_ASSERT_EQ(-1, su_service_install_signal_handlers(ft_nullptr, ft_nullptr));
    return (1);
}

static volatile sig_atomic_t g_su_service_signal_value = 0;

static void test_su_service_signal_handler(int signal_number, void *context)
{
    (void)context;
    g_su_service_signal_value = signal_number;
    return ;
}

FT_TEST(test_su_service_install_signal_handlers_dispatches)
{
    g_su_service_signal_value = 0;
    FT_ASSERT_EQ(0, su_service_install_signal_handlers(test_su_service_signal_handler,
        ft_nullptr));
    std::raise(SIGTERM);
    FT_ASSERT_EQ(SIGTERM, g_su_service_signal_value);
    su_service_clear_signal_handlers();
    return (1);
}

FT_TEST(test_su_service_clear_restores_previous_handler)
{
    su_service_clear_signal_handlers();
    const auto original = std::signal(SIGTERM, SIG_IGN);
    FT_ASSERT(original != SIG_ERR);
    const int32_t install_result = su_service_install_signal_handlers(
        test_su_service_signal_handler, ft_nullptr);
    su_service_clear_signal_handlers();
    const auto restored = std::signal(SIGTERM, original);
    FT_ASSERT_EQ(0, install_result);
    FT_ASSERT(restored == SIG_IGN);
    return (1);
}
