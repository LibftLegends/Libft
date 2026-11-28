#ifndef PRINTF_INTERNAL_HPP
# define PRINTF_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "printf.hpp"
#include <cstdarg>
#include <unistd.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <limits.h>
#include <stddef.h>
#include <cstdio>

ft_size_t ft_strlen_printf(const char *string);
void ft_putchar_fd(const char character, int32_t file_descriptor, ft_size_t *count);
void ft_putstr_fd(const char *string, int32_t file_descriptor, ft_size_t *count);
void ft_putnbr_fd(int64_t number, int32_t file_descriptor, ft_size_t *count);
void ft_putunsigned_fd(uint64_t number, int32_t file_descriptor, ft_size_t *count);
void ft_puthex_fd(uint64_t number, int32_t file_descriptor, ft_bool uppercase, ft_size_t *count);
void ft_putoctal_fd(uint64_t number, int32_t file_descriptor, ft_size_t *count);
void ft_putptr_fd(void *pointer, int32_t file_descriptor, ft_size_t *count);

void ft_putfloat_fd(double number, int32_t file_descriptor, ft_size_t *count, int32_t precision);
void ft_putscientific_fd(double number, ft_bool uppercase, int32_t file_descriptor, ft_size_t *count, int32_t precision);
void ft_putgeneral_fd(double number, ft_bool uppercase, int32_t file_descriptor, ft_size_t *count, int32_t precision);
int32_t pf_printf_fd_v(int32_t file_descriptor, const char *format, va_list argument_list);
ft_bool pf_has_custom_specifier(char specifier);
int32_t pf_try_format_custom_specifier(char specifier, va_list *argument_list, ft_string &output, ft_bool *handled);
void pf_write_ft_string_fd(const ft_string &output, int32_t file_descriptor, ft_size_t *count);
void pf_write_ft_string_stream(const ft_string &output, FILE *stream, ft_size_t *count);
int32_t pf_flush_stream(FILE *stream);

#endif
