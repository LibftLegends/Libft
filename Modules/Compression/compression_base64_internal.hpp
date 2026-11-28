#ifndef COMPRESSION_BASE64_INTERNAL_HPP
# define COMPRESSION_BASE64_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "../Basic/basic.hpp"

ft_bool compression_base64_encode_into(const uint8_t *input, ft_size_t input_size,
    char *output, ft_size_t output_capacity) noexcept;

#endif
