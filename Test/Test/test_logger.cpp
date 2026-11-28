#include "../test_internal.hpp"
#include "../../Modules/Logger/logger.hpp"
#include "../../Modules/Logger/logger_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include <unistd.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_logger_color_toggle)
{
    ft_log_set_color(false);
    FT_ASSERT_EQ(false, ft_log_get_color());
    ft_log_set_color(true);
    FT_ASSERT_EQ(true, ft_log_get_color());
    return (1);
}

FT_TEST(test_logger_quoted_spans_preserve_plain_and_json_control_escapes)
{
    ft_string output;
    const char *input = "ordinary span\t\"quoted\"\\path\n\x01\xC3\xA9";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_append_quoted_value(output, input, FT_FALSE));
    FT_ASSERT_EQ(0, ft_strcmp(output.c_str(),
        "\"ordinary span\\x09\\\"quoted\\\"\\\\path\\x0A\\x01\xC3\xA9\""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_append_quoted_value(output, input, FT_TRUE));
    FT_ASSERT_EQ(0, ft_strcmp(output.c_str(),
        "\"ordinary span\\t\\\"quoted\\\"\\\\path\\n\\u0001\xC3\xA9\""));
    return (1);
}

FT_TEST(test_logger_filtered_structured_messages_do_not_allocate)
{
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t free_count = 0U;
    const t_log_level previous = g_level.load(std::memory_order_relaxed);
    ft_log_set_level(LOG_LEVEL_ERROR);
    const int32_t before_error = cma_get_stats(&before, &free_count);
    s_log_field field;
    field.key = "scope";
    field.value = "a field with enough text to require heap-backed formatting";
    ft_log_debug_structured("filtered message", &field, 1U);
    const int32_t after_error = cma_get_stats(&after, &free_count);
    ft_log_set_level(previous);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, before_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, after_error);
    FT_ASSERT_EQ(before, after);
    return (1);
}

FT_TEST(test_logger_structured_payload_survives_sync_and_async_json_output)
{
    char path[1024];
    int32_t descriptor = test_create_temp_file_from_template(path, sizeof(path),
        "logger_payloadXXXXXX");
    FT_ASSERT(descriptor >= 0);
    const std::string message = std::string(4096U, 'm') + " message-end";
    const std::string field_value = std::string(2048U, 'x') + " field-end";
    s_log_field field;
    field.key = "detail";
    field.value = field_value.c_str();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(ft_json_sink, &descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("detail", "[pending]"));
    ft_log_info("%s", "{\"literal\":1}");
    ft_log_context_clear();
    ft_log_info_structured(message.c_str(), &field, 1U);
    ft_log_enable_async(FT_TRUE);
    ft_log_info("%s", "[truncated");
    ft_log_info_structured(message.c_str(), &field, 1U);
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(ft_json_sink, &descriptor);
    FT_ASSERT_EQ(0, lseek(descriptor, 0, SEEK_SET));
    std::string output;
    char buffer[2048];
    ssize_t bytes_read = read(descriptor, buffer, sizeof(buffer));
    while (bytes_read > 0)
    {
        output.append(buffer, static_cast<ft_size_t>(bytes_read));
        bytes_read = read(descriptor, buffer, sizeof(buffer));
    }
    close(descriptor);
    std::remove(path);
    FT_ASSERT_EQ(0, bytes_read);
    const std::string expected = "\"message\":{\"message\":\"" + message
        + "\",\"fields\":{\"detail\":\"" + field_value + "\"}}}\n";
    const auto first = output.find(expected);
    FT_ASSERT(first != std::string::npos);
    FT_ASSERT(output.find(expected, first + expected.size()) != std::string::npos);
    FT_ASSERT(output.find("\"message\":\"{\\\"literal\\\":1}\",\"detail\":\"[pending]\"") != std::string::npos);
    FT_ASSERT(output.find("\"message\":\"[truncated\"") != std::string::npos);
    return (1);
}

FT_TEST(test_logger_redaction_skips_allocation_without_a_match)
{
    ft_vector<s_redaction_rule> rules;
    s_redaction_rule rule;
    ft_string text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rules.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rule.pattern.initialize("aa"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rule.replacement.initialize("X"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, rules.push_back(rule));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text.initialize("ordinary message"));
    ft_size_t before = 0U;
    ft_size_t after = 0U;
    ft_size_t free_count = 0U;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&before, &free_count));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_apply_redactions(text, rules));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&after, &free_count));
    FT_ASSERT_EQ(before, after);
    FT_ASSERT_EQ(0, ft_strcmp("ordinary message", text.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text.assign("aaaaa", 5U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, logger_apply_redactions(text, rules));
    FT_ASSERT_EQ(0, ft_strcmp("XXa", text.c_str()));
    return (1);
}

FT_TEST(test_logger_json_sink)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[256];

    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, ft_json_sink(ft_nullptr, &write_fd));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    ft_log_info("json check");
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(buffer[0] == '{');
    FT_ASSERT(ft_strstr(buffer, "\"level\":\"INFO\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"severity\":20") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"message\":\"json check\"") != ft_nullptr);
    ft_log_remove_sink(ft_json_sink, &write_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_structured_logging)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];
    s_log_field fields[3];

    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    fields[0].key = "user";
    fields[0].value = "alice";
    fields[1].key = "ip";
    fields[1].value = "127.0.0.1";
    fields[2].key = "attempts";
    fields[2].value = ft_nullptr;
    ft_log_info_structured("login", fields, 3);
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "\"message\":{\"message\":\"login\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"user\":\"alice\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"ip\":\"127.0.0.1\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"attempts\":null") != ft_nullptr);
    ft_log_remove_sink(ft_json_sink, &write_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_context_prefixes_plain_logs)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];
    s_log_field context_fields[3];

    ft_log_set_color(false);
    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    context_fields[0].key = "request_id";
    context_fields[0].value = "abc123";
    context_fields[1].key = "customer";
    context_fields[1].value = "premium";
    context_fields[2].key = "trace";
    context_fields[2].value = ft_nullptr;
    {
        ft_log_context_guard guard;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize(context_fields, 3));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.get_error());
        ft_log_info("processing order");
    }
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "\"request_id\":\"abc123\"") != ft_nullptr ||
        ft_strstr(buffer, "request_id=abc123") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"customer\":\"premium\"") != ft_nullptr ||
        ft_strstr(buffer, "customer=premium") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"trace\":true") != ft_nullptr ||
        ft_strstr(buffer, "trace]") != ft_nullptr);
    ft_log_remove_sink(ft_json_sink, &write_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_context_enriches_structured_logs)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];
    s_log_field base_fields[1];
    s_log_field context_fields[2];

    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    base_fields[0].key = "operation";
    base_fields[0].value = "sync";
    context_fields[0].key = "request_id";
    context_fields[0].value = "ctx-42";
    context_fields[1].key = "attempt";
    context_fields[1].value = ft_nullptr;
    {
        ft_log_context_guard guard;

        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize());
        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.initialize(context_fields, 2));
        FT_ASSERT_EQ(FT_ERR_SUCCESS, guard.get_error());
        ft_log_info_structured("sync", base_fields, 1);
    }
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "\"operation\":\"sync\"") != ft_nullptr ||
        ft_strstr(buffer, "\\\"operation\\\":\\\"sync\\\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"request_id\":\"ctx-42\"") != ft_nullptr ||
        ft_strstr(buffer, "\\\"request_id\\\":\\\"ctx-42\\\"") != ft_nullptr);
    FT_ASSERT(ft_strstr(buffer, "\"attempt\":true") != ft_nullptr ||
        ft_strstr(buffer, "\\\"attempt\\\":null") != ft_nullptr);
    ft_log_remove_sink(ft_json_sink, &write_fd);
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_redaction_masks_plain_logs)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];

    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    ft_log_clear_redactions();
    FT_ASSERT_EQ(0, ft_log_add_redaction("token=abcd"));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    ft_log_info("issuing %s", "token=abcd");
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "\"message\":\"issuing [REDACTED]\"") != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, ft_strstr(buffer, "token=abcd"));
    ft_log_remove_sink(ft_json_sink, &write_fd);
    ft_log_clear_redactions();
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_redaction_masks_structured_fields)
{
    int pipe_fds[2];
    int write_fd;
    ssize_t read_count;
    char buffer[512];
    s_log_field fields[2];

    FT_ASSERT_EQ(0, pipe(pipe_fds));
    write_fd = pipe_fds[1];
    ft_log_clear_redactions();
    FT_ASSERT_EQ(0, ft_log_add_redaction_with_replacement("secret-token", "***"));
    FT_ASSERT_EQ(0, ft_log_add_sink(ft_json_sink, &write_fd));
    fields[0].key = "session";
    fields[0].value = "secret-token";
    fields[1].key = "status";
    fields[1].value = "ok";
    ft_log_info_structured("login", fields, 2);
    read_count = read(pipe_fds[0], buffer, sizeof(buffer) - 1);
    FT_ASSERT(read_count > 0);
    buffer[read_count] = '\0';
    FT_ASSERT(ft_strstr(buffer, "***") != ft_nullptr);
    FT_ASSERT_EQ(ft_nullptr, ft_strstr(buffer, "secret-token"));
    ft_log_remove_sink(ft_json_sink, &write_fd);
    ft_log_clear_redactions();
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return (1);
}

FT_TEST(test_logger_preserves_redactions_when_updating_the_snapshot_fails)
{
    const std::string pattern(1024U, 'a');
    ft_log_clear_redactions();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_redaction_with_replacement(pattern.c_str(), "retained"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(256U));
    const int32_t add_error = ft_log_add_redaction_with_replacement("other", "changed");
    const int32_t reset_error = cma_set_alloc_limit(0U);

    ft_string text;
    ft_string context;
    const std::string combined = pattern + " other";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, text.initialize(combined.c_str()));
    const int32_t apply_error = logger_prepare_redacted_context(text, context);
    ft_log_clear_redactions();
    FT_ASSERT_NEQ(FT_ERR_SUCCESS, add_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, reset_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, apply_error);
    FT_ASSERT_EQ(0, ft_strcmp("retained other", text.c_str()));
    return (1);
}

FT_TEST(test_logger_escapes_structured_redactions_before_encoding)
{
    int32_t descriptors[2];
    FT_ASSERT_EQ(0, pipe(descriptors));
    ft_log_clear_redactions();
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_redaction_with_replacement("secret\n", "quote\"\\\t"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_redaction_with_replacement("private-key", "masked key"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_redaction_with_replacement("reserved", "message"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("private-key", "secret\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("reserved", "metadata"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_add_sink(ft_json_sink, &descriptors[1]));
    s_log_field field;
    field.key = "field";
    field.value = "secret\n";
    ft_log_enable_async(FT_TRUE);
    ft_log_info_structured("secret\n", &field, 1U);
    ft_log_enable_async(FT_FALSE);
    ft_log_remove_sink(ft_json_sink, &descriptors[1]);
    ft_log_context_clear();
    ft_log_clear_redactions();
    close(descriptors[1]);
    std::string output;
    char buffer[512];
    ssize_t count = read(descriptors[0], buffer, sizeof(buffer));
    while (count > 0)
    {
        output.append(buffer, static_cast<ft_size_t>(count));
        count = read(descriptors[0], buffer, sizeof(buffer));
    }
    close(descriptors[0]);
    FT_ASSERT_EQ(0, count);
    const std::string escaped = R"(quote\"\\\t)";
    const std::string expected = "\"message\":{\"message\":\"" + escaped
        + "\",\"fields\":{\"field\":\"" + escaped
        + "\",\"masked key\":\"" + escaped + "\",\"message\":\"metadata\"}}"
        + ",\"masked key\":\"" + escaped + "\"}\n";
    FT_ASSERT(output.find(expected) != std::string::npos);
    FT_ASSERT(output.find("secret") == std::string::npos);
    return (1);
}

FT_TEST(test_logger_redacts_message_and_context_with_the_same_configuration)
{
    ft_log_clear_redactions();
    ft_log_context_clear();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_log_context_push_field("context", "snapshot-secret"));
    std::atomic<ft_bool> writer_ok{FT_TRUE};
    std::thread writer([&writer_ok]()
    {
        uint32_t iteration = 0U;
        while (iteration < 300U)
        {
            ft_log_clear_redactions();
            if (ft_log_add_redaction_with_replacement("snapshot-secret", "intermediate") != FT_ERR_SUCCESS
                || ft_log_add_redaction_with_replacement("intermediate", "masked") != FT_ERR_SUCCESS)
                writer_ok.store(FT_FALSE, std::memory_order_relaxed);
            iteration += 1U;
        }
        return ;
    });

    uint32_t iteration = 0U;
    ft_bool consistent = FT_TRUE;
    while (iteration < 500U && consistent != FT_FALSE)
    {
        ft_string message;
        ft_string context;
        if (message.initialize("snapshot-secret") != FT_ERR_SUCCESS
            || logger_prepare_redacted_context(message, context) != FT_ERR_SUCCESS
            || ft_strstr(context.c_str(), message.c_str()) == ft_nullptr)
            consistent = FT_FALSE;
        iteration += 1U;
    }
    writer.join();
    ft_log_context_clear();
    ft_log_clear_redactions();
    FT_ASSERT_EQ(FT_TRUE, writer_ok.load(std::memory_order_relaxed));
    FT_ASSERT_EQ(FT_TRUE, consistent);
    return (1);
}
