#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include <thread>

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

FT_TEST(test_logger_context_growth_retains_owned_values_and_rolls_back)
{
    ft_log_context_clear();
    char original[] = "owned";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("original", original));
    original[0] = 'X';
    s_log_field fields[24];
    ft_size_t index = 0U;
    while (index < 24U)
    {
        fields[index].key = "extra";
        fields[index].value = "field";
        index += 1U;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push(fields, 24U));
    s_log_field rejected[2];
    rejected[0].key = "rollback";
    rejected[0].value = "discard";
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_log_context_push(rejected, 2U));
    ft_string flat;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, flat.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_context_format_flat(flat));
    FT_ASSERT(ft_strstr(flat.c_str(), "original=owned extra=field") != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, ft_strstr(flat.c_str(), "rollback"));
    ft_log_context_pop(24U);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_context_format_flat(flat));
    FT_ASSERT_EQ(0, ft_strcmp(flat.c_str(), "original=owned"));
    ft_log_context_clear();
    return (1);
}

FT_TEST(test_logger_structured_context_is_thread_local)
{
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("caller", "first"));
    int32_t result = FT_ERR_INTERNAL;
    std::thread worker([&result]()
    {
        ft_string output;
        if (output.initialize() != FT_ERR_SUCCESS
            || ft_log_context_push_field("worker", "second") != FT_ERR_SUCCESS)
            return ;
        ft_bool first = FT_TRUE;
        if (logger_context_append_json(output, first) == FT_ERR_SUCCESS
            && ft_strcmp(output.c_str(), "\"worker\":\"second\"") == 0)
            result = FT_ERR_SUCCESS;
        ft_log_context_clear();
        return ;
    });
    worker.join();
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    ft_bool first = FT_TRUE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_context_append_json(output, first));
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(0, ft_strcmp(output.c_str(), "\"caller\":\"first\""));
    return (1);
}

static ft_bool logger_context_matches(const char *expected)
{
    ft_string output;
    return (output.initialize() == FT_ERR_SUCCESS
        && logger_context_format_flat(output) == FT_ERR_SUCCESS
        && ft_strcmp(output.c_str(), expected) == 0);
}

FT_TEST(test_logger_context_guard_copy_shares_only_its_group)
{
    ft_log_context_clear();
    s_log_field field;
    field.key = "guard";
    field.value = "original";
    ft_log_context_guard source;
    ft_log_context_guard copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(&field, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("outside", "later"));
    FT_ASSERT(logger_context_matches("guard=original outside=later"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT(logger_context_matches("guard=original outside=later"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.destroy());
    FT_ASSERT(logger_context_matches("outside=later"));
    ft_log_context_clear();
    return (1);
}

FT_TEST(test_logger_context_guard_release_preserves_fields_after_all_copies_are_destroyed)
{
    ft_log_context_clear();
    s_log_field field;
    field.key = "released";
    field.value = "kept";
    ft_log_context_guard source;
    ft_log_context_guard copy;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(&field, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    copy.release();
    FT_ASSERT_EQ(FT_FALSE, copy.is_active());
    FT_ASSERT_EQ(FT_FALSE, source.is_active());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT(logger_context_matches("released=kept"));
    ft_log_context_pop(1U);
    FT_ASSERT(logger_context_matches(""));
    return (1);
}

FT_TEST(test_logger_context_guard_move_and_reinitialize_preserve_independent_fields)
{
    ft_log_context_clear();
    s_log_field field;
    field.key = "guard";
    field.value = "before";
    ft_log_context_guard source;
    ft_log_context_guard destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(&field, 1U));
    FT_ASSERT_EQ(static_cast<uint32_t>(FT_ERR_SUCCESS), destination.move(source));
    FT_ASSERT_EQ(FT_FALSE, source.is_active());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("keep", "later"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, destination.initialize(ft_nullptr, 1U));
    FT_ASSERT(logger_context_matches("guard=before keep=later"));
    field.value = "after";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(&field, 1U));
    FT_ASSERT(logger_context_matches("keep=later guard=after"));
    ft_log_context_pop(1U);
    FT_ASSERT_EQ(FT_FALSE, destination.is_active());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT(logger_context_matches("keep=later"));
    ft_log_context_clear();
    return (1);
}

FT_TEST(test_logger_context_guard_destroyed_on_another_thread_retires_origin_fields)
{
    ft_log_context_clear();
    s_log_field field;
    field.key = "guard";
    field.value = "on origin";
    ft_log_context_guard guard;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize(&field, 1U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("keep", "origin"));
    ft_bool worker_kept_context = FT_FALSE;
    std::thread worker([&guard, &worker_kept_context]()
    {
        if (ft_log_context_push_field("keep", "worker") != FT_ERR_SUCCESS)
            return ;
        if (guard.destroy() == FT_ERR_SUCCESS)
            worker_kept_context = logger_context_matches("keep=worker");
        ft_log_context_clear();
        return ;
    });
    worker.join();
    FT_ASSERT_EQ(FT_TRUE, worker_kept_context);
    FT_ASSERT(logger_context_matches("keep=origin"));
    ft_log_context_clear();
    return (1);
}

FT_TEST(test_logger_context_guard_can_outlive_its_origin_thread)
{
    ft_log_context_clear();
    ft_log_context_guard surviving;
    int32_t status = FT_ERR_INTERNAL;
    std::thread worker([&surviving, &status]()
    {
        thread_local ft_log_context_guard local;
        thread_local ft_log_context_guard abandoned;
        s_log_field field;
        field.key = "thread";
        field.value = "ending";
        if (local.initialize() != FT_ERR_SUCCESS
            || abandoned.initialize() != FT_ERR_SUCCESS
            || local.initialize(&field, 1U) != FT_ERR_SUCCESS
            || abandoned.initialize(&field, 1U) != FT_ERR_SUCCESS)
            return ;
        status = surviving.initialize(local);
        return ;
    });
    worker.join();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    FT_ASSERT_EQ(FT_FALSE, surviving.is_active());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, surviving.destroy());
    FT_ASSERT(logger_context_matches(""));
    return (1);
}
