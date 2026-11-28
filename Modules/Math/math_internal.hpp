#ifndef MATH_INTERNAL_HPP
# define MATH_INTERNAL_HPP

#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif

#include "roll.hpp"

int32_t     math_roll_check_number_next(char *string, int32_t index);
int32_t     math_roll_check_number_previous(char *string, int32_t index);
int32_t     math_roll_check_character(char character);
int32_t     math_check_string_number(char *string);

void        math_free_parse(char **parsed_strings);
int32_t     math_roll_convert_previous(char *string, int32_t *index, int32_t *error);
int32_t     math_roll_convert_next(char *string, int32_t index, int32_t *error);
int32_t     math_roll_itoa(int32_t result, int32_t *index, char *string);
int32_t     math_is_unary_sign(const char *expression, int32_t index);

int32_t     math_roll_validate(char *string);
int32_t     math_roll_parse_brackets(char *expression, int32_t &length);
int32_t     math_roll_execute_dice(char *expression, int32_t &length);
int32_t     math_roll_execute_md(char *expression, int32_t &length);
int32_t     math_roll_execute_pm(char *expression, int32_t &length);
int32_t     math_process_sign(char *expression, int32_t &length, int32_t &index);
int32_t     math_roll_replace_number(char *expression, int32_t &length,
                int32_t &index, int32_t right_index, int32_t value);

#endif
