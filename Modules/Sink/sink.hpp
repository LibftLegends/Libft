#ifndef SINK_HPP
#define SINK_HPP

#include "../Basic/basic.hpp"
#include "../Errno/errno.hpp"

struct sink_record
{
    int32_t level;
    const char *message;
};

int32_t sink_record_message(int32_t level, const char *message) noexcept;
int32_t sink_record_message(int32_t level, const char *message, ft_size_t length) noexcept;
int32_t sink_get_record_count(ft_size_t *count_out) noexcept;
// The borrowed message pointer remains valid until sink_clear().
int32_t sink_get_record(ft_size_t index, sink_record *record_out) noexcept;
void sink_clear(void) noexcept;

#endif
