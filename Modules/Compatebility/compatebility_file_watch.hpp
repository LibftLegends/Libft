#ifndef COMPATEBILITY_FILE_WATCH_HPP
#define COMPATEBILITY_FILE_WATCH_HPP


#ifndef LIBFT_INTERNAL_HEADERS
# error "This is a libft internal header. Define LIBFT_INTERNAL_HEADERS only when building libft internals."
#endif
#include "../File/file_watch_event.hpp"

struct cmp_file_watch_context;

cmp_file_watch_context *cmp_file_watch_create(void);
void cmp_file_watch_destroy(cmp_file_watch_context *context);
int32_t cmp_file_watch_start(cmp_file_watch_context *context, const char *path);
void cmp_file_watch_request_stop(cmp_file_watch_context *context);
// The waiter must have finished before releasing its native resources.
void cmp_file_watch_stop(cmp_file_watch_context *context);
ft_bool cmp_file_watch_wait_event(cmp_file_watch_context *context,
    cmp_file_watch_event *event);

#endif
