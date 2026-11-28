#ifndef GET_NEXT_LINE_H
# define GET_NEXT_LINE_H

#include "../Basic/basic.hpp"
#include "../Template/vector.hpp"
#include <cstddef>

char    *ft_strjoin_gnl(char *string_one, char *string_two);
char    *get_next_line(int32_t file_descriptor, ft_size_t buffer_size);
#ifdef LIBFT_TEST_BUILD
void    gnl_set_leftover_alloc_hook(void *(*hook)(ft_size_t size));
void    gnl_reset_leftover_alloc_hook(void);
#endif
void    gnl_reset_all_streams(void);
int32_t     gnl_clear_stream(int32_t file_descriptor);
int32_t     ft_read_file_lines(int32_t file_descriptor, ft_vector<ft_string> &lines, ft_size_t buffer_size);
int32_t     ft_open_and_read_file(const char *path, ft_vector<ft_string> &lines, ft_size_t buffer_size);

#ifdef LIBFT_TEST_BUILD
void    gnl_destroy_streams_for_tests(void);
#endif

#endif
