#ifndef ENCODING_INTERNAL_HPP
# define ENCODING_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

# include "encoding.hpp"

// The output must not overlap the input and must include space for a terminator.
int32_t encoding_hex_encode_into(const uint8_t *input, ft_size_t input_size,
    ft_bool uppercase, char *output, ft_size_t output_capacity) noexcept;
int32_t encoding_base64url_encode_into(const uint8_t *input, ft_size_t input_size,
    ft_bool padded, char *output, ft_size_t output_capacity) noexcept;
int32_t encoding_base64url_decode_into(const char *input, ft_size_t input_size,
    uint8_t *output, ft_size_t output_capacity, ft_size_t *output_size) noexcept;

#endif
