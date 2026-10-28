#ifndef SYSTEM_UTILS_HPP
# define SYSTEM_UTILS_HPP

#include "../Basic/basic.hpp"
#include "../Basic/limits.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../Template/vector.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"

class pt_mutex;

typedef struct s_su_environment_snapshot
{
    ft_vector<ft_string> entries;
}   t_su_environment_snapshot;

char    *su_getenv(const char *name);
int32_t     su_setenv(const char *name, const char *value, int32_t overwrite);
int32_t     su_unsetenv(const char *name);
int32_t     su_putenv(char *string);
int32_t     su_environment_snapshot_capture(t_su_environment_snapshot *snapshot);
int32_t     su_environment_snapshot_restore(const t_su_environment_snapshot *snapshot);
void    su_environment_snapshot_dispose(t_su_environment_snapshot *snapshot);
int32_t     su_environment_sandbox_begin(t_su_environment_snapshot *snapshot);
int32_t     su_environment_sandbox_end(t_su_environment_snapshot *snapshot);
char    *su_get_home_directory(void);
int32_t     su_open(const char *path_name);
int32_t     su_open(const char *path_name, int32_t flags);
int32_t     su_open(const char *path_name, int32_t flags, mode_t mode);
/* Returns the exact mapped platform error and leaves the descriptor at -1. */
int32_t     su_open_with_error(const char *path_name, int32_t flags,
            mode_t mode, int32_t *file_descriptor_out,
            int32_t *error_code_out);
#ifdef LIBFT_TEST_BUILD
void        su_test_inject_next_open_error(int32_t system_error);
void        su_test_clear_open_error(void);
void        su_test_inject_write_interrupts(uint32_t count);
void        su_test_clear_write_interrupts(void);
ft_bool     su_test_take_write_interrupt(void);
#endif
int64_t su_read(int32_t file_descriptor, void *buffer, ft_size_t count);
int64_t su_write(int32_t file_descriptor, const void *buffer, ft_size_t count);
/* Retries EINTR and reports the terminal mapped error without losing it. */
int64_t su_write_with_error(int32_t file_descriptor, const void *buffer,
            ft_size_t count, int32_t *error_code_out);
off_t   su_lseek(int32_t file_descriptor, off_t offset, int32_t whence);
#ifdef LIBFT_TEST_BUILD
typedef int64_t (*t_su_write_syscall_hook)(int32_t file_descriptor,
            const void *buffer, ft_size_t count);
void    su_set_write_syscall_hook(t_su_write_syscall_hook hook);
void    su_reset_write_syscall_hook(void);
#endif
int32_t     su_close(int32_t file_descriptor);
uint32_t    su_get_cpu_count(void);
uint64_t su_get_total_memory(void);
void    su_abort(void);
void    su_exit(int32_t exit_code);
void    su_sigabrt(void);
void    su_sigfpe(void);
void    su_sigill(void);
void    su_sigint(void);
void    su_sigsegv(void);
void    su_sigterm(void);
void    su_assert(ft_bool condition, const char *message);

typedef void (*t_su_resource_tracer)(const char *reason);
int32_t     su_register_resource_tracer(t_su_resource_tracer tracer);
int32_t     su_unregister_resource_tracer(t_su_resource_tracer tracer);
void    su_clear_resource_tracers(void);
void    su_run_resource_tracers(const char *reason);
void    su_internal_set_abort_reason(const char *reason);
const char *su_internal_take_abort_reason(void);

int32_t     su_locale_compare(const char *left, const char *right, const char *locale_name,
            int32_t *result);
int32_t     su_locale_casefold(const char *input, const char *locale_name,
            ft_string &output);

typedef struct su_file
{
    pt_mutex    *mutex;
    ft_bool        closed;
    int32_t         _descriptor;
}   su_file;

#ifdef LIBFT_TEST_BUILD
void    su_force_file_stream_allocation_failure(ft_bool should_fail);
#endif
su_file *su_fopen(const char *path_name);
su_file *su_fopen(const char *path_name, int32_t flags);
su_file *su_fopen(const char *path_name, int32_t flags, mode_t mode);
int32_t     su_fclose(su_file *stream);
ft_size_t  su_fread(void *buffer, ft_size_t size, ft_size_t count, su_file *stream);
ft_size_t  su_fwrite(const void *buffer, ft_size_t size, ft_size_t count, su_file *stream);
int32_t     su_fseek(su_file *stream, int64_t offset, int32_t origin);
int64_t    su_ftell(su_file *stream);

int32_t     su_file_prepare_thread_safety(su_file *stream);
void    su_file_teardown_thread_safety(su_file *stream);
int32_t     su_file_lock(su_file *stream, ft_bool *lock_acquired);
void    su_file_unlock(su_file *stream, ft_bool lock_acquired);

int32_t     su_environment_enable_thread_safety(void);
void    su_environment_disable_thread_safety(void);

int32_t     su_copy_file(const char *source_path, const char *destination_path);
int32_t     su_copy_directory_recursive(const char *source_path,
            const char *destination_path);
int32_t     su_inspect_permissions(const char *path, mode_t *permissions_out);
int32_t     su_chmod(const char *path, int32_t owner_permissions,
            int32_t group_permissions, int32_t other_permissions);

typedef void (*t_su_service_signal_handler)(int32_t signal_number, void *user_context);
#ifdef LIBFT_TEST_BUILD
void    su_service_force_no_fork(ft_bool enable);
#endif
int32_t su_service_daemonize(const char *working_directory,
            const char *pid_file_path, ft_bool redirect_standard_streams);
int32_t su_service_install_signal_handlers(t_su_service_signal_handler handler,
            void *user_context);
void    su_service_clear_signal_handlers(void);

typedef int32_t (*t_su_health_check)(void *context, ft_string &detail);

typedef struct s_su_health_check_result
{
    ft_string   name;
    ft_bool        healthy;
    ft_string   detail;
    int32_t         error_code;
}   t_su_health_check_result;

int32_t     su_health_register_check(const char *name, t_su_health_check check,
            void *context);
int32_t     su_health_unregister_check(const char *name);
void    su_health_clear_checks(void);
int32_t     su_health_run_checks(t_su_health_check_result *results,
            ft_size_t capacity, ft_size_t *count);
int32_t     su_health_run_check(const char *name, t_su_health_check_result *result);

#endif
