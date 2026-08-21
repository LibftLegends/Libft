#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"

struct logger_context_message_capture
{
    char message[2048];
};

static int32_t logger_context_capture_sink(const char *message, void *user_data)
{
    logger_context_message_capture *capture;

    if (message == ft_nullptr || user_data == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    capture = static_cast<logger_context_message_capture *>(user_data);
    ft_strlcpy(capture->message, message, sizeof(capture->message));
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_logger_context_push_field_adds_one_context_entry)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("request_id", "42"));
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER,
            ft_log_context_push_field(ft_nullptr, "42"));
    return (1);
}

FT_TEST(test_logger_context_push_field_accepts_empty_value_and_rejects_null_value)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("empty", ""));
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER,
            ft_log_context_push_field("missing", ft_nullptr));
    return (1);
}

FT_TEST(test_logger_context_push_field_is_visible_in_json_output)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];
    logger_context_message_capture capture;

    ft_log_close();
    ft_log_enable_async(FT_FALSE);
    ft_log_set_level(LOG_LEVEL_INFO);
    capture.message[0] = '\0';
    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
            ft_log_add_sink(logger_context_capture_sink, &capture));
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
            ft_log_context_push_field("request_id", "edge-42"));
    ft_log_info("context test");
    ft_log_remove_sink(logger_context_capture_sink, &capture);
    FT_ASSERT(ft_strstr(capture.message, "request_id=edge-42") != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_json_sink(capture.message, &write_fd));
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "request_id") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "edge-42") != ft_nullptr);
    ft_log_context_clear();
    ft_log_remove_sink(ft_json_sink, &write_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}
