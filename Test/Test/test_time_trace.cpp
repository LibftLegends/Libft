#include "../test_internal.hpp"
#include "../../Modules/Time/time.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <fstream>
#include <iterator>
#include <string>
#include <cstdio>

#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_time_trace_writes_events)
{
    const char              *file_path;
    bool                    result;
    std::ifstream           input;
    std::string             contents;

    file_path = "time_trace_test.json";
    std::remove(file_path);
    result = time_trace_begin_session(file_path);
    FT_ASSERT_EQ(true, result);
    result = time_trace_begin_event("outer_event", "test_category");
    FT_ASSERT_EQ(true, result);
    time_sleep_ms(1);
    result = time_trace_end_event();
    FT_ASSERT_EQ(true, result);
    result = time_trace_instant_event("instant_event", "test_category");
    FT_ASSERT_EQ(true, result);
    result = time_trace_end_session();
    FT_ASSERT_EQ(true, result);
    input.open(file_path, std::ios::binary);
    FT_ASSERT_EQ(true, input.good());
    contents.assign((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    input.close();
    FT_ASSERT(contents.find("\"traceEvents\"") != std::string::npos);
    FT_ASSERT(contents.find("\"outer_event\"") != std::string::npos);
    FT_ASSERT(contents.find("\"instant_event\"") != std::string::npos);
    FT_ASSERT(contents.find("\"ph\":\"X\"") != std::string::npos);
    FT_ASSERT(contents.find("\"ph\":\"i\"") != std::string::npos);
    std::remove(file_path);
    return (1);
}

FT_TEST(test_time_trace_detects_unbalanced_events)
{
    const char  *file_path;
    bool        result;

    file_path = "time_trace_unbalanced.json";
    std::remove(file_path);
    result = time_trace_begin_session(file_path);
    FT_ASSERT_EQ(true, result);
    result = time_trace_begin_event("outer_event", "test_category");
    FT_ASSERT_EQ(true, result);
    result = time_trace_end_session();
    FT_ASSERT_EQ(false, result);
    result = time_trace_end_event();
    FT_ASSERT_EQ(true, result);
    result = time_trace_end_session();
    FT_ASSERT_EQ(true, result);
    std::remove(file_path);
    return (1);
}

FT_TEST(test_time_trace_escapes_control_bytes_in_consecutive_events)
{
    const char *path = "time_trace_escape_test.json";
    std::remove(path);
    FT_ASSERT(time_trace_begin_session(path));
    FT_ASSERT(time_trace_instant_event("quote\" newline\n", "backslash\\"));
    FT_ASSERT(time_trace_begin_event("form\fback\b", "low\x01"));
    FT_ASSERT(time_trace_end_event());
    FT_ASSERT(time_trace_end_session());
    std::ifstream input(path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>());
    input.close();
    std::remove(path);
    FT_ASSERT(contents.find("quote\\\" newline\\n") != std::string::npos);
    FT_ASSERT(contents.find("backslash\\\\") != std::string::npos);
    FT_ASSERT(contents.find("form\\u000cback\\u0008") != std::string::npos);
    FT_ASSERT(contents.find("low\\u0001") != std::string::npos);
    FT_ASSERT(contents.find(',') != std::string::npos);
    return (1);
}
