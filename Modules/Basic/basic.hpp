#ifndef LIBFT_HPP
# define LIBFT_HPP

#include <cstdint>
#include <type_traits>

#ifndef FT_TYPES_HPP
# define FT_TYPES_HPP

typedef uint64_t ft_size_t;
typedef uint8_t ft_bool;

#ifndef FT_FALSE
# define FT_FALSE static_cast<ft_bool>(0U)
#endif

#ifndef FT_TRUE
# define FT_TRUE static_cast<ft_bool>(1U)
#endif

#endif

#include "limits.hpp"
#include "class_nullptr.hpp"

#ifdef _WIN32
# define FT_THREAD_ID_FROM_PTHREAD(value) \
    static_cast<pt_thread_id_type>(value)
# define FT_ZLIB_ULONG_CAST(value) \
    static_cast<uLong>(value)
# define FT_NETWORKING_SOCKLEN_CAST(value) \
    static_cast<socklen_t>(value)
# define FT_TIMEOUT_LONG_CAST(value) \
    static_cast<long>(value)
# define FT_SOCKET_DESCRIPTOR_CAST(value) \
    static_cast<socket_file_descriptor_type>(value)
# define FT_FILE_OFFSET_TO_INT64_CAST(value) \
    static_cast<int64_t>(value)
#else
# define FT_THREAD_ID_FROM_PTHREAD(value) (value)
# define FT_ZLIB_ULONG_CAST(value) (value)
# define FT_NETWORKING_SOCKLEN_CAST(value) (value)
# define FT_TIMEOUT_LONG_CAST(value) (value)
# define FT_SOCKET_DESCRIPTOR_CAST(value) (value)
# define FT_FILE_OFFSET_TO_INT64_CAST(value) (value)
#endif

template <typename TargetType, typename SourceType>
constexpr TargetType ft_platform_cast(SourceType value)
{
    if constexpr (std::is_same<TargetType, SourceType>::value)
        return (value);
    else
        return (static_cast<TargetType>(value));
}

#ifdef __APPLE__
# define FT_NATIVE_SIZE_TO_FT_SIZE_CAST(value) \
    static_cast<ft_size_t>(value)
#else
# define FT_NATIVE_SIZE_TO_FT_SIZE_CAST(value) (value)
#endif

class ft_string;
static constexpr ft_size_t ft_strlen_raw(const char *string)
{
    const char *string_pointer = string;

    while (*string_pointer)
        ++string_pointer;
    return (static_cast<ft_size_t>(string_pointer - string));
}

constexpr ft_size_t ft_strlen_size_t(const char *string)
{
    if (!string)
        return (0);
    return (ft_strlen_raw(string));
}

constexpr int32_t ft_strlen(const char *string)
{
    if (!string)
        return (0);
    ft_size_t length = ft_strlen_raw(string);
    if (length > static_cast<ft_size_t>(FT_INT32_MAX))
        return (FT_INT32_MAX);
    return (static_cast<int32_t>(length));
}

char            *ft_strchr(const char *string, int32_t char_to_find);
int32_t         ft_atoi(const char *string);
int32_t         ft_validate_int(const char *input);
void            ft_bzero(void *string, ft_size_t size);
void            *ft_memchr(const void *pointer, int32_t character, ft_size_t size);
void            *ft_memrchr(const void *pointer, int32_t character, ft_size_t size);
void            *ft_memcpy(void *destination, const void *source, ft_size_t size);
int32_t         ft_memcpy_s(void *destination, ft_size_t destination_size,
                    const void *source, ft_size_t number_of_bytes);
void            *ft_memmove(void *destination, const void *source, ft_size_t size);
int32_t         ft_memmove_s(void *destination, ft_size_t destination_size,
                    const void *source, ft_size_t number_of_bytes);
ft_size_t          ft_strlcat(char *destination, const char *source, ft_size_t buffer_size);
ft_size_t          ft_strlcpy(char *destination, const char *source, ft_size_t buffer_size);
ft_size_t          ft_strnlen(const char *string, ft_size_t maximum_length);
char            *ft_strrchr(const char *string, int32_t char_to_find);
char            *ft_strnstr(const char *haystack, const char *needle,
                    ft_size_t maximum_length);
char            *ft_strstr(const char *haystack, const char *needle);
int32_t         ft_strncmp(const char *string_1, const char *string_2,
                    ft_size_t maximum_length);
int32_t         ft_memcmp(const void *pointer1, const void *pointer2, ft_size_t size);
ft_bool         ft_constant_time_equal(const void *pointer1, const void *pointer2, ft_size_t size) noexcept;
int32_t         ft_strcasecmp(const char *left, const char *right);
int32_t         ft_strncasecmp(const char *left, const char *right, ft_size_t maximum_length);
ft_bool         ft_str_starts_with(const char *string, const char *prefix);
ft_bool         ft_str_ends_with(const char *string, const char *suffix);
ft_bool         ft_str_contains(const char *haystack, const char *needle);
int32_t         ft_isdigit(int32_t character);
int32_t         ft_isalpha(int32_t character);
int32_t         ft_isalnum(int32_t character);
int32_t         ft_isascii(int32_t character);
int32_t         ft_isprint(int32_t character);
int32_t         ft_islower(int32_t character);
int32_t         ft_isupper(int32_t character);
int64_t         ft_atol(const char *string);
int64_t         ft_strtol(const char *input_string, char **end_pointer,
                    int32_t numeric_base);
uint64_t        ft_strtoul(const char *input_string,
                    char **end_pointer, int32_t numeric_base);
int32_t         ft_strcmp(const char *string1,
                    const char *string2);
void            ft_to_lower(char *string);
void            ft_to_upper(char *string);
char            *ft_strncpy(char *destination,
                    const char *source, ft_size_t number_of_characters);
int32_t         ft_strcpy_s(char *destination, ft_size_t destination_size,
                    const char *source);
int32_t         ft_strncpy_s(char *destination, ft_size_t destination_size,
                    const char *source, ft_size_t maximum_copy_length);
int32_t         ft_strcat_s(char *destination, ft_size_t destination_size,
                    const char *source);
int32_t         ft_strncat_s(char *destination, ft_size_t destination_size,
                    const char *source, ft_size_t maximum_append_length);
char            *ft_strtok(char *string, const char *delimiters);
int32_t         ft_locale_compare(const char *left, const char *right,
                    const char *locale_name);
void            *ft_memset(void *destination, int32_t value, ft_size_t number_of_bytes);
int32_t         ft_isspace(int32_t character);
int32_t         ft_isxdigit(int32_t character);
int32_t         ft_ispunct(int32_t character);
int32_t         ft_isgraph(int32_t character);
int32_t         ft_iscntrl(int32_t character);
int32_t         ft_isblank(int32_t character);
void            ft_striteri(char *string, void (*function)(uint32_t, char *));
ft_size_t          ft_wstrlen(const wchar_t *string);
int32_t         ft_utf8_is_leading_byte(int32_t byte_value);
int32_t         ft_utf8_is_trailing_byte(int32_t byte_value);
char            *ft_strtrim_left_in_place(char *string);
char            *ft_strtrim_right_in_place(char *string);
char            *ft_strtrim_in_place(char *string);
char16_t        *ft_utf8_to_utf16(const char *input, ft_size_t input_length,
                    ft_size_t *output_length_pointer);
char32_t        *ft_utf8_to_utf32(const char *input, ft_size_t input_length,
                    ft_size_t *output_length_pointer);
int32_t         ft_size_add_checked(ft_size_t left, ft_size_t right,
                    ft_size_t *result_pointer);
int32_t         ft_size_multiply_checked(ft_size_t left, ft_size_t right,
                    ft_size_t *result_pointer);
ft_bool         ft_is_power_of_two(ft_size_t value);
int32_t         ft_align_up_checked(ft_size_t value, ft_size_t alignment,
                    ft_size_t *result_pointer);
void            *ft_memswap(void *left, void *right, ft_size_t size);
void            *ft_memmem(const void *haystack, ft_size_t haystack_size,
                    const void *needle, ft_size_t needle_size);
int32_t         ft_parse_uint32(const char *string, char **end_pointer,
                    uint32_t *value_pointer);
int32_t         ft_parse_int64(const char *string, char **end_pointer,
                    int64_t *value_pointer);
int32_t         ft_utf8_validate(const char *string, ft_size_t length);

#endif
