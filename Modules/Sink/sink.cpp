#include "sink.hpp"
#include "../PThread/pt_buffer.hpp"
#include <pthread.h>
#include <cstdlib>
#include <limits>
#include <new>

static pthread_mutex_t g_sink_mutex = PTHREAD_MUTEX_INITIALIZER;
static pt_buffer<sink_record *> g_sink_records{};

int32_t sink_record_message(int32_t level, const char *message, ft_size_t length) noexcept
{
    if (message == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    constexpr ft_size_t MAX_SIZE = std::numeric_limits<std::size_t>::max();
    if (length >= MAX_SIZE - sizeof(sink_record))
        return (FT_ERR_OUT_OF_RANGE);

    void *memory = std::malloc(FT_UINT64_TO_NATIVE_SIZE_CAST(sizeof(sink_record) + length + 1U));
    if (memory == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    char *storage = static_cast<char *>(memory) + sizeof(sink_record);
    if (length != 0U)
        (void)ft_memcpy(storage, message, length);
    storage[length] = '\0';
    auto *record = new (memory) sink_record{level, storage};

    if (pthread_mutex_lock(&g_sink_mutex) != 0)
    {
        std::free(record);
        return (FT_ERR_INTERNAL);
    }
    const int32_t error = pt_buffer_push(g_sink_records, record);
    (void)pthread_mutex_unlock(&g_sink_mutex);
    if (error != FT_ERR_SUCCESS)
        std::free(record);
    return (error);
}

int32_t sink_record_message(int32_t level, const char *message) noexcept
{
    if (message == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    return (sink_record_message(level, message, ft_strlen_size_t(message)));
}

int32_t sink_get_record_count(ft_size_t *count_out) noexcept
{
    if (count_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (pthread_mutex_lock(&g_sink_mutex) != 0)
        return (FT_ERR_INTERNAL);
    *count_out = g_sink_records.size;
    (void)pthread_mutex_unlock(&g_sink_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t sink_get_record(ft_size_t index, sink_record *record_out) noexcept
{
    if (record_out == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    if (pthread_mutex_lock(&g_sink_mutex) != 0)
        return (FT_ERR_INTERNAL);
    if (index >= g_sink_records.size)
    {
        (void)pthread_mutex_unlock(&g_sink_mutex);
        return (FT_ERR_OUT_OF_RANGE);
    }
    *record_out = *g_sink_records.data[index];
    (void)pthread_mutex_unlock(&g_sink_mutex);
    return (FT_ERR_SUCCESS);
}

void sink_clear() noexcept
{
    if (pthread_mutex_lock(&g_sink_mutex) != 0)
        return ;
    pt_buffer<sink_record *> detached = g_sink_records;
    pt_buffer_init(g_sink_records);
    (void)pthread_mutex_unlock(&g_sink_mutex);

    ft_size_t index = 0U;
    while (index < detached.size)
    {
        std::free(detached.data[index]);
        index += 1U;
    }
    pt_buffer_destroy(detached);
    return ;
}
