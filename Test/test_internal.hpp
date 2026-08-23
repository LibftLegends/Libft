#ifndef TEST_INTERNAL_HPP
#define TEST_INTERNAL_HPP

#ifndef LIBFT_TEST_BUILD
# ifndef FT_EFFICIENCY_BUILD
#  define LIBFT_TEST_BUILD
# endif
#endif

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#if !defined(_WIN32) && !defined(_WIN64)
# include <sys/resource.h>
# include <sys/types.h>
# include <sys/wait.h>
# include <pthread.h>
#endif
#include <unistd.h>

#include "../Modules/File/file_utils.hpp"
#include "../Modules/Compatebility/compatebility_internal.hpp"

#if defined(_WIN32) || defined(_WIN64)
# include <io.h>
# include <direct.h>

# ifndef STDIN_FILENO
#  define STDIN_FILENO 0
# endif
# ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
# endif
# ifndef STDERR_FILENO
#  define STDERR_FILENO 2
# endif

# ifndef SIGIOT
#  define SIGIOT SIGABRT
# endif

# ifndef SA_SIGINFO
#  define SA_SIGINFO 0
# endif
# ifndef SA_ONSTACK
#  define SA_ONSTACK 0
# endif
# ifndef SA_RESETHAND
#  define SA_RESETHAND 0
# endif

typedef jmp_buf sigjmp_buf;
typedef int sigset_t;

# define sigsetjmp(environment, savesigs) setjmp(environment)
# define siglongjmp(environment, value) longjmp(environment, value)

struct sigaction
{
    void (*sa_handler)(int);
    sigset_t sa_mask;
    int sa_flags;
};

static int sigemptyset(sigset_t *signal_set) noexcept
{
    if (signal_set != nullptr)
        *signal_set = 0;
    return (0);
}

static int __attribute__((unused)) sigaddset(sigset_t *signal_set, int signal_number) noexcept
{
    (void)signal_number;
    if (signal_set != nullptr)
        *signal_set = 0;
    return (0);
}

#if defined(__GNUC__)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif
static int sigaction(int signal_number, const struct sigaction *new_action,
    struct sigaction *old_action) noexcept
{
    void (*previous_handler)(int);

    if (new_action != nullptr)
        previous_handler = std::signal(signal_number, new_action->sa_handler);
    else
        previous_handler = std::signal(signal_number, SIG_DFL);
    if (previous_handler == SIG_ERR)
        return (-1);
    if (old_action != nullptr)
    {
        old_action->sa_handler = previous_handler;
        old_action->sa_mask = 0;
        old_action->sa_flags = 0;
    }
    return (0);
}
#if defined(__GNUC__)
# pragma GCC diagnostic pop
#endif

# define pipe(pipe_descriptors) _pipe(pipe_descriptors, 4096, _O_BINARY)
# define mkdir(directory_path, permissions) _mkdir(directory_path)

static inline char *mkdtemp(char *directory_template) noexcept
{
    size_t path_index;
    size_t template_length;

    if (directory_template == nullptr)
        return (nullptr);
    if (directory_template[0] == '/' || directory_template[0] == '\\')
    {
        std::memmove(directory_template,
            directory_template + 1,
            std::strlen(directory_template));
    }
    path_index = 0;
    while (directory_template[path_index] != '\0')
    {
        if (directory_template[path_index] == '/'
            || directory_template[path_index] == '\\')
        {
            directory_template[path_index] = '\0';
            if (directory_template[0] != '\0')
                (void)_mkdir(directory_template);
            directory_template[path_index] = '\\';
        }
        path_index += 1;
    }
    template_length = std::strlen(directory_template) + 1;
    if (_mktemp_s(directory_template, template_length) != 0)
        return (nullptr);
    if (_mkdir(directory_template) != 0)
        return (nullptr);
    return (directory_template);
}

#endif

static inline int32_t test_create_temp_file_from_template(
    const char *template_path, ft_string *path_out,
    int32_t *file_descriptor_out) noexcept
{
    ft_string directory_path;
    ft_string prefix;
    const char *base_name;
    const char *last_separator;
    ft_size_t base_length;
    int32_t error_code;

    if (path_out == ft_nullptr || file_descriptor_out == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    error_code = directory_path.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (error_code);
    error_code = prefix.initialize();
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)directory_path.destroy();
        return (error_code);
    }
    if (template_path == ft_nullptr || template_path[0] == '\0')
    {
        error_code = prefix.assign("libft_test", 10);
    }
    else
    {
        last_separator = std::strrchr(template_path, '/');
#if defined(_WIN32) || defined(_WIN64)
        {
            const char *alternate_separator;

            alternate_separator = std::strrchr(template_path, '\\');
            if (alternate_separator != ft_nullptr
                && (last_separator == ft_nullptr
                    || alternate_separator > last_separator))
                last_separator = alternate_separator;
        }
#endif
        if (last_separator != ft_nullptr)
        {
            if (last_separator > template_path)
            {
                error_code = directory_path.assign(template_path,
                        static_cast<ft_size_t>(last_separator - template_path));
                if (error_code != FT_ERR_SUCCESS)
                {
                    (void)prefix.destroy();
                    (void)directory_path.destroy();
                    return (error_code);
                }
            }
            base_name = last_separator + 1;
        }
        else
        {
            base_name = template_path;
        }
        base_length = std::strlen(base_name);
        while (base_length > 0 && base_name[base_length - 1] == 'X')
            --base_length;
        if (base_length == 0)
            error_code = prefix.assign("libft_test", 10);
        else
            error_code = prefix.assign(base_name, base_length);
    }
    if (error_code == FT_ERR_SUCCESS)
    {
        error_code = file_secure_temp_file(
                directory_path.size() > 0 ? directory_path.c_str() : ft_nullptr,
                prefix.c_str(), path_out, file_descriptor_out);
    }
    (void)prefix.destroy();
    (void)directory_path.destroy();
    return (error_code);
}

static inline int test_create_temp_file_from_template(
    char *path_buffer, size_t path_buffer_size,
    const char *template_path) noexcept
{
    ft_string path_out;
    int32_t file_descriptor;
    int32_t error_code;

    if (path_buffer == ft_nullptr || path_buffer_size == 0)
        return (-1);
    error_code = path_out.initialize();
    if (error_code != FT_ERR_SUCCESS)
        return (-1);
    error_code = test_create_temp_file_from_template(template_path, &path_out,
            &file_descriptor);
    if (error_code != FT_ERR_SUCCESS)
    {
        (void)path_out.destroy();
        return (-1);
    }
    if (path_out.size() + 1 > path_buffer_size)
    {
        (void)file_delete(path_out.c_str());
        (void)file_close_descriptor(file_descriptor);
        (void)path_out.destroy();
        return (-1);
    }
#if defined(_WIN32) || defined(_WIN64)
    {
        HANDLE duplicated_handle;
        HANDLE original_handle;
        int32_t file_descriptor_result;

        original_handle = cmp_retrieve_handle(file_descriptor);
        if (original_handle == INVALID_HANDLE_VALUE)
        {
            (void)file_delete(path_out.c_str());
            (void)file_close_descriptor(file_descriptor);
            (void)path_out.destroy();
            return (-1);
        }
        duplicated_handle = INVALID_HANDLE_VALUE;
        if (DuplicateHandle(GetCurrentProcess(), original_handle,
                GetCurrentProcess(), &duplicated_handle, 0, FALSE,
                DUPLICATE_SAME_ACCESS) == 0)
        {
            (void)file_delete(path_out.c_str());
            (void)file_close_descriptor(file_descriptor);
            (void)path_out.destroy();
            return (-1);
        }
        file_descriptor_result = _open_osfhandle(
                reinterpret_cast<intptr_t>(duplicated_handle),
                _O_RDWR | _O_BINARY);
        if (file_descriptor_result < 0)
        {
            if (CloseHandle(duplicated_handle) == 0)
                return (-1);
            (void)file_delete(path_out.c_str());
            (void)file_close_descriptor(file_descriptor);
            (void)path_out.destroy();
            return (-1);
        }
        (void)file_close_descriptor(file_descriptor);
        file_descriptor = file_descriptor_result;
    }
#endif
    ft_memcpy(path_buffer, path_out.c_str(), path_out.size() + 1);
    (void)path_out.destroy();
    return (file_descriptor);
}

static thread_local sigjmp_buf g_test_abort_signal_jump_buffer __attribute__((unused));
static volatile sig_atomic_t g_test_abort_signal_caught __attribute__((unused)) = 0;

static void __attribute__((unused)) test_abort_signal_handler(int signal_value)
{
    g_test_abort_signal_caught = signal_value;
    siglongjmp(g_test_abort_signal_jump_buffer, 1);
}

static int32_t __attribute__((unused)) test_capture_abort_output_begin(
    int32_t &saved_stderr,
    int32_t &pipe_read_descriptor,
    int32_t &pipe_write_descriptor)
{
    int32_t pipe_descriptors[2];

    saved_stderr = -1;
    pipe_read_descriptor = -1;
    pipe_write_descriptor = -1;
    if (pipe(pipe_descriptors) != 0)
        return (0);
    pipe_read_descriptor = pipe_descriptors[0];
    pipe_write_descriptor = pipe_descriptors[1];
    saved_stderr = dup(STDERR_FILENO);
    if (saved_stderr < 0)
    {
        if (close(pipe_read_descriptor) < 0)
            return (0);
        if (close(pipe_write_descriptor) < 0)
            return (0);
        return (0);
    }
    if (dup2(pipe_write_descriptor, STDERR_FILENO) < 0)
    {
        if (close(saved_stderr) < 0)
            return (0);
        if (close(pipe_read_descriptor) < 0)
            return (0);
        if (close(pipe_write_descriptor) < 0)
            return (0);
        return (0);
    }
    return (1);
}

static int32_t __attribute__((unused)) test_capture_abort_output_end(
    int32_t saved_stderr,
    int32_t pipe_read_descriptor,
    int32_t pipe_write_descriptor,
    char *output_buffer,
    size_t output_buffer_size)
{
    ssize_t read_result;
    size_t write_index;

    if (output_buffer != nullptr && output_buffer_size > 0)
        output_buffer[0] = '\0';
    if (std::fflush(stdout) != 0)
        return (0);
    if (std::fflush(stderr) != 0)
        return (0);
    if (saved_stderr >= 0)
    {
        if (dup2(saved_stderr, STDERR_FILENO) < 0)
            return (0);
        if (close(saved_stderr) < 0)
            return (0);
    }
    if (pipe_write_descriptor >= 0)
    {
        if (close(pipe_write_descriptor) < 0)
            return (0);
    }
    write_index = 0;
    if (output_buffer != nullptr && output_buffer_size > 1 && pipe_read_descriptor >= 0)
    {
        while (write_index + 1 < output_buffer_size)
        {
            read_result = read(pipe_read_descriptor,
                    output_buffer + write_index,
                    static_cast<unsigned int>(output_buffer_size - write_index - 1));
            if (read_result < 0)
                return (0);
            if (read_result == 0)
                break ;
            write_index += static_cast<size_t>(read_result);
        }
        output_buffer[write_index] = '\0';
    }
    if (pipe_read_descriptor >= 0)
    {
        if (close(pipe_read_descriptor) < 0)
            return (0);
    }
    return (1);
}

#if !defined(_WIN32) && !defined(_WIN64)
static void test_disable_child_core_dumps(void)
{
    struct rlimit core_limit;

    core_limit.rlim_cur = 0;
    core_limit.rlim_max = 0;
    (void)setrlimit(RLIMIT_CORE, &core_limit);
    return ;
}

static void test_unblock_abort_signals(void)
{
    sigset_t unblocked_signals;

    sigemptyset(&unblocked_signals);
    sigaddset(&unblocked_signals, SIGABRT);
    if (SIGIOT != SIGABRT)
        sigaddset(&unblocked_signals, SIGIOT);
    (void)pthread_sigmask(SIG_UNBLOCK, &unblocked_signals, nullptr);
    return ;
}

static int __attribute__((unused)) test_expect_sigabrt_process(
    void (*operation)(void))
{
    pid_t child_process_id;
    pid_t waited_process_id;
    int child_status;
    int null_descriptor;

    child_process_id = fork();
    if (child_process_id < 0)
        return (0);
    if (child_process_id == 0)
    {
        (void)signal(SIGABRT, SIG_DFL);
        if (SIGIOT != SIGABRT)
            (void)signal(SIGIOT, SIG_DFL);
        test_disable_child_core_dumps();
        test_unblock_abort_signals();
        null_descriptor = open("/dev/null", O_WRONLY);
        if (null_descriptor >= 0)
        {
            (void)dup2(null_descriptor, STDERR_FILENO);
            (void)close(null_descriptor);
        }
        operation();
        _exit(0);
    }
    waited_process_id = waitpid(child_process_id, &child_status, 0);
    if (waited_process_id != child_process_id
        || WIFSIGNALED(child_status) == 0)
        return (0);
    if (WTERMSIG(child_status) == SIGABRT)
        return (1);
    if (SIGIOT != SIGABRT && WTERMSIG(child_status) == SIGIOT)
        return (1);
    return (0);
}
#endif

static int __attribute__((unused)) test_expect_sigabrt_signal(void (*operation)(void))
{
#if !defined(_WIN32) && !defined(_WIN64)
    return (test_expect_sigabrt_process(operation));
#else
    struct sigaction old_action_abort;
    struct sigaction new_action_abort;
    struct sigaction old_action_iot;
    struct sigaction new_action_iot;
    int jump_result;
    int iot_handler_installed;
    char output_buffer[8192];
    int32_t saved_stderr;
    int32_t pipe_read_descriptor;
    int32_t pipe_write_descriptor;

    std::memset(&old_action_abort, 0, sizeof(old_action_abort));
    std::memset(&new_action_abort, 0, sizeof(new_action_abort));
    std::memset(&old_action_iot, 0, sizeof(old_action_iot));
    std::memset(&new_action_iot, 0, sizeof(new_action_iot));
    new_action_abort.sa_handler = &test_abort_signal_handler;
    new_action_iot.sa_handler = &test_abort_signal_handler;
    sigemptyset(&new_action_abort.sa_mask);
    sigemptyset(&new_action_iot.sa_mask);
    iot_handler_installed = 0;
    if (sigaction(SIGABRT, &new_action_abort, &old_action_abort) != 0)
        return (0);
    if (SIGIOT != SIGABRT
        && sigaction(SIGIOT, &new_action_iot, &old_action_iot) != 0)
    {
        if (sigaction(SIGABRT, &old_action_abort, nullptr) != 0)
            return (0);
        return (0);
    }
    if (SIGIOT != SIGABRT)
        iot_handler_installed = 1;
    if (test_capture_abort_output_begin(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor) == 0)
        return (0);
    g_test_abort_signal_caught = 0;
    jump_result = sigsetjmp(g_test_abort_signal_jump_buffer, 1);
    if (jump_result == 0)
        operation();
    if (test_capture_abort_output_end(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor,
            output_buffer, sizeof(output_buffer)) == 0)
        return (0);
    if (sigaction(SIGABRT, &old_action_abort, nullptr) != 0)
        return (0);
    if (iot_handler_installed != 0)
    {
        if (sigaction(SIGIOT, &old_action_iot, nullptr) != 0)
            return (0);
    }
    if (g_test_abort_signal_caught == SIGABRT)
        return (1);
    if (iot_handler_installed != 0 && g_test_abort_signal_caught == SIGIOT)
        return (1);
    return (0);
#endif
}

#if !defined(_WIN32) && !defined(_WIN64)
template <typename TypeName>
static int32_t test_expect_sigabrt_process_uninitialised(
    void (*operation)(TypeName &))
{
    pid_t child_process_id;
    pid_t waited_process_id;
    int child_status;
    alignas(TypeName) unsigned char object_storage[sizeof(TypeName)];
    TypeName *object_pointer;

    child_process_id = fork();
    if (child_process_id < 0)
        return (0);
    if (child_process_id == 0)
    {
        (void)signal(SIGABRT, SIG_DFL);
        if (SIGIOT != SIGABRT)
            (void)signal(SIGIOT, SIG_DFL);
        test_disable_child_core_dumps();
        test_unblock_abort_signals();
        std::memset(object_storage, 0, sizeof(object_storage));
        object_pointer = reinterpret_cast<TypeName *>(object_storage);
        operation(*object_pointer);
        _exit(0);
    }
    waited_process_id = waitpid(child_process_id, &child_status, 0);
    if (waited_process_id != child_process_id
        || WIFSIGNALED(child_status) == 0)
        return (0);
    if (WTERMSIG(child_status) == SIGABRT)
        return (1);
    if (SIGIOT != SIGABRT && WTERMSIG(child_status) == SIGIOT)
        return (1);
    return (0);
}
#endif

template <typename TypeName>
static int __attribute__((unused)) test_expect_sigabrt_signal_uninitialised(void (*operation)(TypeName &))
{
#if !defined(_WIN32) && !defined(_WIN64)
    return (test_expect_sigabrt_process_uninitialised<TypeName>(operation));
#else
    struct sigaction old_action_abort;
    struct sigaction new_action_abort;
    struct sigaction old_action_iot;
    struct sigaction new_action_iot;
    int jump_result;
    int iot_handler_installed;
    alignas(TypeName) unsigned char object_storage[sizeof(TypeName)];
    TypeName *object_pointer;
    char output_buffer[8192];
    int32_t saved_stderr;
    int32_t pipe_read_descriptor;
    int32_t pipe_write_descriptor;

    std::memset(&old_action_abort, 0, sizeof(old_action_abort));
    std::memset(&new_action_abort, 0, sizeof(new_action_abort));
    std::memset(&old_action_iot, 0, sizeof(old_action_iot));
    std::memset(&new_action_iot, 0, sizeof(new_action_iot));
    new_action_abort.sa_handler = &test_abort_signal_handler;
    new_action_iot.sa_handler = &test_abort_signal_handler;
    sigemptyset(&new_action_abort.sa_mask);
    sigemptyset(&new_action_iot.sa_mask);
    iot_handler_installed = 0;
    if (sigaction(SIGABRT, &new_action_abort, &old_action_abort) != 0)
        return (0);
    if (SIGIOT != SIGABRT
        && sigaction(SIGIOT, &new_action_iot, &old_action_iot) != 0)
    {
        if (sigaction(SIGABRT, &old_action_abort, nullptr) != 0)
            return (0);
        return (0);
    }
    if (SIGIOT != SIGABRT)
        iot_handler_installed = 1;
    if (test_capture_abort_output_begin(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor) == 0)
        return (0);
    g_test_abort_signal_caught = 0;
    jump_result = sigsetjmp(g_test_abort_signal_jump_buffer, 1);
    if (jump_result == 0)
    {
        std::memset(object_storage, 0, sizeof(object_storage));
        object_pointer = reinterpret_cast<TypeName *>(object_storage);
        operation(*object_pointer);
    }
    if (test_capture_abort_output_end(saved_stderr,
            pipe_read_descriptor, pipe_write_descriptor,
            output_buffer, sizeof(output_buffer)) == 0)
        return (0);
    if (sigaction(SIGABRT, &old_action_abort, nullptr) != 0)
        return (0);
    if (iot_handler_installed != 0)
    {
        if (sigaction(SIGIOT, &old_action_iot, nullptr) != 0)
            return (0);
    }
    if (g_test_abort_signal_caught == SIGABRT)
        return (1);
    if (iot_handler_installed != 0 && g_test_abort_signal_caught == SIGIOT)
        return (1);
    return (0);
#endif
}

#endif
