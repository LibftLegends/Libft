#ifndef ERRNO_INTERNAL_HPP
# define ERRNO_INTERNAL_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "errno.hpp"
#include <cstdint>

void    su_abort(void);
int64_t su_write(int32_t file_descriptor, const void *buffer,
            ft_size_t count);

int32_t errno_write_stderr(const char *string) noexcept;
int32_t errno_write_stderr_fragments(const char *const *fragments,
            ft_size_t fragment_count) noexcept;

void    errno_abort_if_uninitialised_or_destroyed(uint8_t initialised_state,
            const char *method_name);
void    errno_abort_if_uninitialised(uint8_t initialised_state,
            const char *method_name);
void    errno_abort_lifecycle(uint8_t initialised_state, const char *method_name,
            const char *reason);

#endif
