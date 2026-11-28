#include "../test_internal.hpp"
#include <unistd.h>
#include "../../Modules/CPP_class/class_stringbuf.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <atomic>
#include <chrono>
#include <thread>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_ft_stringbuf_read_basic)
{
    ft_string source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("hello"));
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    char storage[8];
    ssize_t bytes_read;

    bytes_read = buffer.read(storage, 5);
    storage[static_cast<std::size_t>(bytes_read)] = '\0';
    FT_ASSERT_EQ(5, bytes_read);
    FT_ASSERT_EQ(0, ft_strcmp(storage, "hello"));
    FT_ASSERT_EQ(true, buffer.is_valid());
    return (1);
}

FT_TEST(test_ft_stringbuf_read_null_buffer_sets_error)
{
    ft_string source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("data"));
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    ssize_t bytes_read;

    bytes_read = buffer.read(ft_nullptr, 3);
    FT_ASSERT_EQ(static_cast<ssize_t>(-1), bytes_read);
    FT_ASSERT_EQ(true, buffer.is_valid());
    return (1);
}

FT_TEST(test_ft_stringbuf_str_returns_remaining)
{
    ft_string source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("abcdef"));
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    char storage[4];
    std::size_t bytes_read;
    ft_string remaining;

    bytes_read = buffer.read(storage, 3);
    storage[bytes_read] = '\0';
    FT_ASSERT_EQ(static_cast<std::size_t>(3), bytes_read);
    FT_ASSERT_EQ(0, ft_strcmp(storage, "abc"));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, remaining.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_string(remaining));
    FT_ASSERT_EQ(0, ft_strcmp(remaining.c_str(), "def"));
    FT_ASSERT_EQ(true, buffer.is_valid());
    return (1);
}

FT_TEST(test_ft_stringbuf_read_past_end_returns_zero)
{
    ft_string source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize("xy"));
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    char storage[4];
    std::size_t bytes_read;

    bytes_read = buffer.read(storage, 2);
    storage[bytes_read] = '\0';
    FT_ASSERT_EQ(static_cast<std::size_t>(2), bytes_read);
    FT_ASSERT_EQ(0, ft_strcmp(storage, "xy"));

    bytes_read = buffer.read(storage, 2);
    FT_ASSERT_EQ(static_cast<std::size_t>(0), bytes_read);
    FT_ASSERT_EQ(true, buffer.is_valid());
    return (1);
}

FT_TEST(test_ft_stringbuf_bulk_read_clamps_to_remaining_binary_bytes)
{
    const char bytes[] = { 'a', '\0', 'b', '\0', 'c' };
    ft_string source;
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.assign(bytes, sizeof(bytes)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    char output[6] = {};
    output[5] = '!';
    FT_ASSERT_EQ(2, buffer.read(output, 2));
    FT_ASSERT_EQ(3, buffer.read(output + 2, FT_SYSTEM_SIZE_MAX));
    FT_ASSERT_EQ(0, ft_memcmp(bytes, output, sizeof(bytes)));
    FT_ASSERT_EQ('!', output[5]);
    FT_ASSERT_EQ(0, buffer.read(output, sizeof(output)));
    return (1);
}

FT_TEST(test_ft_stringbuf_concurrent_reads_are_serialized)
{
    const char *seed_data;
    std::atomic<bool> start_flag;
    std::atomic<bool> worker_done;
    ft_string worker_output;
    ft_string main_output;
    std::thread worker_thread;

    seed_data = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    ft_string source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize(seed_data));
    ft_stringbuf buffer;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.initialize(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, worker_output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, main_output.initialize());
    start_flag.store(false);
    worker_done.store(false);
    worker_thread = std::thread([&buffer, &start_flag, &worker_done, &worker_output]() {
        char single_character[2];
        ssize_t bytes_read;

        while (!start_flag.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        while (true)
        {
            bytes_read = buffer.read(single_character, 1);
            if (bytes_read <= 0)
                break ;
            single_character[1] = '\0';
            worker_output.append(single_character, 1);
        }
        worker_done.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    start_flag.store(true);

    char single_character[2];
    ssize_t bytes_read;

    while (true)
    {
        bytes_read = buffer.read(single_character, 1);
        if (bytes_read <= 0)
            break ;
        single_character[1] = '\0';
        main_output.append(single_character, 1);
    }

    while (!worker_done.load())
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    worker_thread.join();

    FT_ASSERT_EQ(source.size(), worker_output.size() + main_output.size());

    ft_string remaining;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, remaining.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, buffer.get_string(remaining));
    FT_ASSERT_EQ(0u, remaining.size());

    bool seen[128];
    unsigned int seen_index;

    seen_index = 0;
    while (seen_index < 128)
    {
        seen[seen_index] = false;
        seen_index++;
    }

    const char *worker_chars;
    unsigned int worker_index;

    worker_chars = worker_output.c_str();
    worker_index = 0;
    while (worker_index < worker_output.size())
    {
        unsigned char value;

        value = static_cast<unsigned char>(worker_chars[worker_index]);
        FT_ASSERT_EQ(false, seen[value]);
        seen[value] = true;
        worker_index++;
    }

    const char *main_chars;
    unsigned int main_index;

    main_chars = main_output.c_str();
    main_index = 0;
    while (main_index < main_output.size())
    {
        unsigned char value;

        value = static_cast<unsigned char>(main_chars[main_index]);
        FT_ASSERT_EQ(false, seen[value]);
        seen[value] = true;
        main_index++;
    }

    const char *source_chars;
    unsigned int source_index;

    source_chars = source.c_str();
    source_index = 0;
    while (source_index < source.size())
    {
        unsigned char value;

        value = static_cast<unsigned char>(source_chars[source_index]);
        FT_ASSERT_EQ(true, seen[value]);
        source_index++;
    }

    return (1);
}
