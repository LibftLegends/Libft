# Basic

The `Basic` module provides C-style memory, string, character, numeric parsing, and low-level UTF-8 decode/encode helpers. Most functions operate on caller-owned buffers. Higher-level allocation-returning helpers such as hashing over `ft_string`, `ft_span_dup`, `ft_strmapi`, and UTF transform/grapheme helpers live in `Advanced`.

## Core Types

- `ft_size_t` - Project-wide unsigned size type.
- `ft_bool` - Project-wide 8-bit boolean type.
- `FT_FALSE` / `FT_TRUE` - Boolean constants used across the library.
- `ft_nullptr_t` / `ft_nullptr` - Project null-pointer literal stand-in assignable to pointer types.
- `FT_PRId64` / `FT_PRIu64` - Portable 64-bit integer format specifiers from `<cinttypes>`.
- `FT_INT64_FORMAT` / `FT_UINT64_FORMAT` - Basic-owned 64-bit integer length modifiers selected by platform for decimal formatting.
- `FT_INT64_DECIMAL_FORMAT` / `FT_UINT64_DECIMAL_FORMAT` - Ready-to-use decimal `printf`/`snprintf` format strings for 64-bit signed and unsigned integers.
- `FT_NATIVE_SIZE_TO_FT_SIZE_CAST` - Converts a platform-native `size_t` to
  `ft_size_t` where their underlying types differ, such as macOS ABI
  callbacks; it is an identity expression on platforms where they match.

## String Length

- `ft_strlen_raw(const char *string)` - Returns the length of a non-null C string as `ft_size_t`.
- `ft_strlen_size_t(const char *string)` - Returns a C string length as `ft_size_t`, or `0` for null.
- `ft_strlen(const char *string)` - Returns a C string length clamped to `FT_INT32_MAX`, or `0` for null.
- `ft_strnlen(const char *string, ft_size_t maximum_length)` - Counts characters up to a maximum bound.
- `ft_wstrlen(const wchar_t *string)` - Returns the length of a wide-character string.

## String Search and Compare

- `ft_strchr(const char *string, int32_t char_to_find)` - Finds the first matching character.
- `ft_strrchr(const char *string, int32_t char_to_find)` - Finds the last matching character.
- `ft_strstr(const char *haystack, const char *needle)` - Finds a substring.
- `ft_strnstr(const char *haystack, const char *needle, ft_size_t maximum_length)` - Finds a substring inside a bounded prefix.
- `ft_strcmp(const char *string1, const char *string2)` - Compares two C strings.
- `ft_strncmp(const char *string_1, const char *string_2, ft_size_t maximum_length)` - Compares bounded prefixes of two C strings.
- `ft_strcasecmp(const char *left, const char *right)` - Compares two C strings using ASCII case folding.
- `ft_strncasecmp(const char *left, const char *right, ft_size_t maximum_length)` - Compares bounded prefixes using ASCII case folding.
- `ft_str_starts_with(const char *string, const char *prefix)` - Reports whether a string starts with a given prefix.
- `ft_str_ends_with(const char *string, const char *suffix)` - Reports whether a string ends with a given suffix.
- `ft_str_contains(const char *haystack, const char *needle)` - Reports whether a string contains a given substring.
- `ft_locale_compare(const char *left, const char *right, const char *locale_name)` - Compares strings using locale-aware collation.

## String Copy and Append

- `ft_strlcpy(char *destination, const char *source, ft_size_t buffer_size)` - Copies a C string with truncation reporting.
- `ft_strlcat(char *destination, const char *source, ft_size_t buffer_size)` - Appends a C string with truncation reporting.
- `ft_strncpy(char *destination, const char *source, ft_size_t number_of_characters)` - Copies at most a fixed number of characters.
- `ft_strcpy_s(char *destination, ft_size_t destination_size, const char *source)` - Bounds-checked string copy returning an error code.
- `ft_strncpy_s(char *destination, ft_size_t destination_size, const char *source, ft_size_t maximum_copy_length)` - Bounds-checked limited string copy.
- `ft_strcat_s(char *destination, ft_size_t destination_size, const char *source)` - Bounds-checked string append.
- `ft_strncat_s(char *destination, ft_size_t destination_size, const char *source, ft_size_t maximum_append_length)` - Bounds-checked limited string append.
- `ft_to_lower(char *string)` - Converts ASCII letters in a mutable C string to lowercase.
- `ft_to_upper(char *string)` - Converts ASCII letters in a mutable C string to uppercase.
- `ft_striteri(char *string, void (*function)(uint32_t, char *))` - Applies an index-aware callback to each mutable character.
- `ft_strtok(char *string, const char *delimiters)` - Tokenizes a string using delimiter characters.

## Memory Helpers

- `ft_bzero(void *string, ft_size_t size)` - Sets a memory range to zero.
- `ft_memset(void *destination, int32_t value, ft_size_t number_of_bytes)` - Fills memory with a byte value.
- `ft_memcpy(void *destination, const void *source, ft_size_t size)` - Copies non-overlapping memory.
- `ft_memcpy_s(void *destination, ft_size_t destination_size, const void *source, ft_size_t number_of_bytes)` - Bounds-checked memory copy returning an error code.
- `ft_memmove(void *destination, const void *source, ft_size_t size)` - Copies memory safely when ranges overlap.
- `ft_memmove_s(void *destination, ft_size_t destination_size, const void *source, ft_size_t number_of_bytes)` - Bounds-checked overlap-safe memory copy.
- `ft_memchr(const void *pointer, int32_t character, ft_size_t size)` - Searches memory for a byte value.
- `ft_memrchr(const void *pointer, int32_t character, ft_size_t size)` - Searches memory for the last matching byte value.
- `ft_memcmp(const void *pointer1, const void *pointer2, ft_size_t size)` - Compares two memory ranges.
- `ft_constant_time_equal(const void *pointer1, const void *pointer2, ft_size_t size)` - Compares two memory ranges without early exit for timing-sensitive checks.
- `ft_memswap(void *left, void *right, ft_size_t size)` - Swaps two caller-owned memory ranges.
- `ft_memmem(const void *haystack, ft_size_t haystack_size, const void *needle, ft_size_t needle_size)` - Searches a bounded memory range for another bounded range.

## ASCII Classification

- `ft_isxdigit(int32_t character)` - Tests for an ASCII hexadecimal digit.
- `ft_ispunct(int32_t character)` - Tests for an ASCII punctuation character.
- `ft_isgraph(int32_t character)` - Tests for a visible ASCII character excluding space.
- `ft_iscntrl(int32_t character)` - Tests for an ASCII control character.
- `ft_isblank(int32_t character)` - Tests for ASCII space or horizontal tab.

## Checked Size and Alignment Helpers

- `ft_size_add_checked(ft_size_t left, ft_size_t right, ft_size_t *result_pointer)` - Adds sizes and reports overflow.
- `ft_size_multiply_checked(ft_size_t left, ft_size_t right, ft_size_t *result_pointer)` - Multiplies sizes and reports overflow.
- `ft_is_power_of_two(ft_size_t value)` - Tests whether a size is a non-zero power of two.
- `ft_align_up_checked(ft_size_t value, ft_size_t alignment, ft_size_t *result_pointer)` - Aligns a size upward using a power-of-two alignment and reports invalid input or overflow.

## Character Checks

- `ft_isdigit(int32_t character)` - Tests for an ASCII digit.
- `ft_isalpha(int32_t character)` - Tests for an ASCII letter.
- `ft_isalnum(int32_t character)` - Tests for an ASCII letter or digit.
- `ft_isascii(int32_t character)` - Tests whether a value falls within the 7-bit ASCII range.
- `ft_isprint(int32_t character)` - Tests for a printable ASCII character.
- `ft_islower(int32_t character)` - Tests for a lowercase ASCII letter.
- `ft_isupper(int32_t character)` - Tests for an uppercase ASCII letter.
- `ft_isspace(int32_t character)` - Tests for an ASCII whitespace character.

## ASCII and UTF-8 Boundary Checks

- `ft_utf8_is_leading_byte(int32_t byte_value)` - Reports whether a byte can start a valid UTF-8 sequence.
- `ft_utf8_is_trailing_byte(int32_t byte_value)` - Reports whether a byte is a UTF-8 continuation byte.

## Numeric Parsing

- `ft_atoi(const char *string)` - Parses a decimal string into `int32_t`.
- `ft_validate_int(const char *input)` - Validates that a string can be parsed as an integer.
- `ft_atol(const char *string)` - Parses a decimal string into `int64_t`.
- `ft_strtol(const char *input_string, char **end_pointer, int32_t numeric_base)` - Parses a signed integer with caller-selected base and end pointer reporting.
- `ft_strtoul(const char *input_string, char **end_pointer, int32_t numeric_base)` - Parses an unsigned integer with caller-selected base and end pointer reporting.
- `ft_parse_uint32(const char *string, char **end_pointer, uint32_t *value_pointer)` - Parses a decimal `uint32_t` with explicit overflow reporting.
- `ft_parse_int64(const char *string, char **end_pointer, int64_t *value_pointer)` - Parses a decimal `int64_t` with explicit overflow reporting.

## UTF Helpers

- `ft_utf8_to_utf16(const char *input, ft_size_t input_length, ft_size_t *output_length_pointer)` - Converts UTF-8 text to an allocated UTF-16 buffer.
- `ft_utf8_to_utf32(const char *input, ft_size_t input_length, ft_size_t *output_length_pointer)` - Converts UTF-8 text to an allocated UTF-32 buffer.
- `ft_utf8_next(const char *string, ft_size_t string_length, ft_size_t *index_pointer, uint32_t *code_point_pointer, ft_size_t *sequence_length_pointer)` - Decodes the next code point and advances the index.
- `ft_utf8_count(const char *string, ft_size_t *code_point_count_pointer)` - Counts UTF-8 code points in a null-terminated string.
- `ft_utf8_encode(uint32_t code_point, char *buffer, ft_size_t buffer_size, ft_size_t *encoded_length_pointer)` - Encodes one Unicode code point into UTF-8.
- `ft_utf8_validate(const char *string, ft_size_t length)` - Validates every UTF-8 sequence in a bounded buffer without allocation.

## String Trimming

- `ft_strtrim_left_in_place(char *string)` - Removes leading ASCII whitespace from a mutable string in place.
- `ft_strtrim_right_in_place(char *string)` - Removes trailing ASCII whitespace from a mutable string in place.
- `ft_strtrim_in_place(char *string)` - Removes leading and trailing ASCII whitespace from a mutable string in place.
