#include "../../../Modules/Logger/logger_internal.hpp"
#include "../../../Modules/Compatebility/compatebility_internal.hpp"
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <fcntl.h>

#ifdef LIBFT_BENCHMARK_ROTATION_BASELINE
int32_t logger_prepare_rotation(s_file_sink *sink, ft_bool *rotate_for_size,
    ft_bool *rotate_for_age);
void logger_execute_rotation(s_file_sink *sink);

static int32_t benchmark_write(s_file_sink &sink, const char *message)
{
    ft_bool size_before = FT_FALSE;
    ft_bool age_before = FT_FALSE;
    if (logger_prepare_rotation(&sink, &size_before, &age_before) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    const int32_t error = ft_file_sink(message, &sink);
    if (error != FT_ERR_SUCCESS)
        return (error);
    ft_bool size_after = FT_FALSE;
    ft_bool age_after = FT_FALSE;
    if (logger_prepare_rotation(&sink, &size_after, &age_after) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_OPERATION);
    if (size_before || age_before || size_after || age_after)
        logger_execute_rotation(&sink);
    return (FT_ERR_SUCCESS);
}
#else
static int32_t benchmark_write(s_file_sink &sink, const char *message)
{
    return (logger_write_rotating_file(&sink, message));
}
#endif

static int32_t benchmark_rotation(const char *label, const char *path, uint32_t max_age)
{
    constexpr uint32_t ITERATIONS = 1024U;
    constexpr char MESSAGE[] = "logger file rotation metadata benchmark";
    s_file_sink sink;
    if (sink.path.initialize(path) != FT_ERR_SUCCESS)
        return (1);
    sink.file_descriptor = cmp_open(path, O_CREAT | O_WRONLY | O_APPEND | O_TRUNC, 0644);
    if (sink.file_descriptor < 0)
        return (1);
    if (file_sink_prepare_thread_safety(&sink) != FT_ERR_SUCCESS)
    {
        (void)cmp_close(sink.file_descriptor);
        return (1);
    }
    sink.max_size = UINT64_MAX;
    sink.max_age_seconds = max_age;
    uint32_t iteration = 0U;
    const auto start = std::chrono::steady_clock::now();
    while (iteration < ITERATIONS && benchmark_write(sink, MESSAGE) == FT_ERR_SUCCESS)
        iteration += 1U;
    const double elapsed = std::chrono::duration<double, std::nano>(
        std::chrono::steady_clock::now() - start).count();
    (void)cmp_close(sink.file_descriptor);
    file_sink_teardown_thread_safety(&sink);
    ft_size_t size = 0U;
    if (iteration != ITERATIONS
        || cmp_file_get_size(path, &size, ft_nullptr) != FT_ERR_SUCCESS
        || size != ITERATIONS * (sizeof(MESSAGE) - 1U))
        return (1);
    std::printf("%s: %.2f ns/op, checksum %" PRIu64 "\n",
        label, elapsed / ITERATIONS, size);
    return (0);
}

int main(int argument_count, char **arguments)
{
    if (argument_count != 2)
    {
        std::fprintf(stderr, "Usage: benchmark_logger_rotation OUTPUT_FILE\n");
        return (1);
    }
    return (benchmark_rotation("size-only file sink", arguments[1], 0U)
        || benchmark_rotation("size and age file sink", arguments[1], UINT32_MAX));
}
