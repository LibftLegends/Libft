#ifndef PRINTF_ENGINE_HPP
#define PRINTF_ENGINE_HPP

#include <cstddef>
#include <cstdarg>
#include <cstdint>
#include "../Errno/errno.hpp"

struct pf_engine_format_spec;

typedef int32_t (*t_pf_engine_write_callback)(const char *data_pointer, ft_size_t data_length, void *context, ft_size_t *written_count);
typedef int32_t (*t_pf_engine_flush_callback)(void *context);

int32_t pf_engine_format_with_snprintf(const char *format,
    t_pf_engine_write_callback writer, void *context, ft_size_t *written_count, ...);

// Keep the format text unchanged until formatting and its callbacks complete.
int32_t pf_engine_format(const char *format, va_list argument_list, t_pf_engine_write_callback writer, void *context, ft_size_t *written_count);
// Flush before formatter side effects. The caller flushes any final pending bytes.
int32_t pf_engine_format_with_flush(const char *format, va_list argument_list,
    t_pf_engine_write_callback writer, t_pf_engine_flush_callback flush,
    void *context, ft_size_t *written_count);

#endif
