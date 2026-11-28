#ifndef COMPATEBILITY_SYSTEM_TEST_HOOKS_HPP
#define COMPATEBILITY_SYSTEM_TEST_HOOKS_HPP

#include <cstdint>

void    cmp_set_force_cross_device_move(int force_cross_device_move);

void    cmp_set_force_cpu_count_success(unsigned int cpu_count);
void    cmp_set_force_cpu_count_failure(int errno_value);
void    cmp_clear_force_cpu_count_result(void);

void    cmp_set_force_total_memory_success(uint64_t memory_size);
void    cmp_set_force_total_memory_failure(int errno_value);
#if defined(_WIN32) || defined(_WIN64)
void    cmp_set_force_total_memory_windows_failure(unsigned long last_error);
void    cmp_force_rng_windows_acquire_failure(uint32_t last_error);
void    cmp_force_rng_windows_generate_failure(uint32_t last_error);
void    cmp_clear_force_rng_windows_failures(void);
#endif
void    cmp_clear_force_total_memory_result(void);

void    cmp_set_force_unsetenv_result(int result, int errno_value);
void    cmp_clear_force_unsetenv_result(void);
void    cmp_set_force_unsetenv_windows_errors(int last_error, int socket_error);

void    cmp_set_force_putenv_result(int result, int errno_value);
void    cmp_clear_force_putenv_result(void);
void    cmp_set_force_putenv_windows_error(int last_error);

#if !defined(_WIN32) && !defined(_WIN64)
void    cmp_force_rng_open_failure(int error_code);
void    cmp_force_rng_read_failure(int error_code);
void    cmp_force_rng_read_eof(void);
void    cmp_force_rng_close_failure(int error_code);
void    cmp_force_rng_short_reads(uint64_t maximum_read_size);
void    cmp_clear_force_rng_failures(void);
#endif

#endif
