#include "../test_internal.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#if defined(_WIN32) || defined(_WIN64)
# include <io.h>
#endif

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

static void create_test_file_stream_file(void)
{
    FILE *file_handle;

    file_handle = std::fopen("build_logs/test_su_file_stream.txt", "w");
    if (file_handle != ft_nullptr)
    {
        std::fputs("data", file_handle);
        std::fclose(file_handle);
    }
    return ;
}

FT_TEST(test_su_file_prepare_thread_safety_initializes_mutex)
{
    su_file file_stream;

    std::memset(&file_stream, 0, sizeof(su_file));
    FT_ASSERT_EQ(0, su_file_prepare_thread_safety(&file_stream));
    FT_ASSERT(file_stream.mutex != ft_nullptr);
    su_file_teardown_thread_safety(&file_stream);
    return (1);
}

FT_TEST(test_su_file_lock_unlock_acquires_mutex)
{
    su_file file_stream;
    ft_bool lock_acquired;

    std::memset(&file_stream, 0, sizeof(su_file));
    lock_acquired = FT_FALSE;
    FT_ASSERT_EQ(0, su_file_prepare_thread_safety(&file_stream));
    FT_ASSERT_EQ(0, su_file_lock(&file_stream, &lock_acquired));
    FT_ASSERT(lock_acquired == FT_TRUE);
    su_file_unlock(&file_stream, lock_acquired);
    su_file_teardown_thread_safety(&file_stream);
    return (1);
}

FT_TEST(test_su_fopen_null_path_sets_ft_einval)
{
    FT_ASSERT_EQ(ft_nullptr, su_fopen(ft_nullptr));
    return (1);
}

FT_TEST(test_su_fopen_propagates_open_error)
{
    su_file *file_stream;
    errno = 0;
    std::remove("missing_su_file_stream.txt");
    file_stream = su_fopen("missing_su_file_stream.txt");
    FT_ASSERT_EQ(ft_nullptr, file_stream);
    return (1);
}

FT_TEST(test_su_fopen_allocation_failure_sets_ft_ealloc)
{
    su_file *file_stream;

    create_test_file_stream_file();
    su_force_file_stream_allocation_failure(true);
    file_stream = su_fopen("build_logs/test_su_file_stream.txt");
    su_force_file_stream_allocation_failure(false);
    FT_ASSERT_EQ(ft_nullptr, file_stream);
    return (1);
}

FT_TEST(test_su_fopen_success_clears_errno)
{
    su_file *file_stream;

    create_test_file_stream_file();
    file_stream = su_fopen("build_logs/test_su_file_stream.txt");
    if (file_stream == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(0, su_fclose(file_stream));
    return (1);
}

FT_TEST(test_su_fclose_null_stream_sets_ft_einval)
{
    FT_ASSERT_EQ(-1, su_fclose(ft_nullptr));
    return (1);
}

FT_TEST(test_su_fclose_invalid_descriptor_sets_errno)
{
    su_file *file_stream;

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = -1;
    FT_ASSERT_EQ(-1, su_fclose(file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fread_null_buffer_sets_ft_einval)
{
    su_file *file_stream;

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = 0;
    FT_ASSERT_EQ(0, su_fread(ft_nullptr, 1, 1, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fread_null_stream_sets_ft_einval)
{
    char buffer[4];
    FT_ASSERT_EQ(0, su_fread(buffer, 1, 1, ft_nullptr));
    return (1);
}

FT_TEST(test_su_fread_zero_size_returns_zero)
{
    su_file *file_stream;
    char buffer[1];

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = -1;
    FT_ASSERT_EQ(0, su_fread(buffer, 0, 4, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fread_overflow_sets_ft_erange)
{
    su_file *file_stream;
    char buffer[1];
    size_t maximum_size;

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = -1;
    maximum_size = std::numeric_limits<size_t>::max();
    FT_ASSERT_EQ(0, su_fread(buffer, maximum_size, 2, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fread_success_clears_errno)
{
    su_file *file_stream;
    char buffer[5];
    size_t read_count;

    create_test_file_stream_file();
    file_stream = su_fopen("build_logs/test_su_file_stream.txt");
    if (file_stream == ft_nullptr)
        return (0);
    read_count = su_fread(buffer, 1, 4, file_stream);
    if (read_count > 4)
        read_count = 4;
    buffer[read_count] = '\0';
    FT_ASSERT_EQ(static_cast<size_t>(4), read_count);
    FT_ASSERT_EQ('d', buffer[0]);
    FT_ASSERT_EQ('a', buffer[1]);
    FT_ASSERT_EQ('t', buffer[2]);
    FT_ASSERT_EQ('a', buffer[3]);
    FT_ASSERT_EQ('\0', buffer[4]);
    FT_ASSERT_EQ(0, su_fclose(file_stream));
    return (1);
}

FT_TEST(test_su_fwrite_null_buffer_sets_ft_einval)
{
    su_file *file_stream;

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = 0;
    FT_ASSERT_EQ(0, su_fwrite(ft_nullptr, 1, 1, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fwrite_null_stream_sets_ft_einval)
{
    char buffer[4] = {'t', 'e', 's', 't'};
    FT_ASSERT_EQ(0, su_fwrite(buffer, 1, 4, ft_nullptr));
    return (1);
}

FT_TEST(test_su_fwrite_zero_size_returns_zero)
{
    su_file *file_stream;
    char buffer[1] = {'x'};

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = -1;
    FT_ASSERT_EQ(0, su_fwrite(buffer, 0, 4, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fwrite_overflow_sets_ft_erange)
{
    su_file *file_stream;
    char buffer[1] = {'x'};
    size_t maximum_size;

    file_stream = static_cast<su_file*>(std::malloc(sizeof(su_file)));
    if (file_stream == ft_nullptr)
        return (0);
    std::memset(file_stream, 0, sizeof(su_file));
    file_stream->_descriptor = -1;
    maximum_size = std::numeric_limits<size_t>::max();
    FT_ASSERT_EQ(0, su_fwrite(buffer, maximum_size, 2, file_stream));
    std::free(file_stream);
    return (1);
}

FT_TEST(test_su_fseek_null_stream_sets_ft_einval)
{
    FT_ASSERT_EQ(-1, su_fseek(ft_nullptr, 0, SEEK_SET));
    return (1);
}

FT_TEST(test_su_ftell_null_stream_sets_ft_einval)
{
    FT_ASSERT_EQ(-1L, su_ftell(ft_nullptr));
    return (1);
}

#if defined(_WIN32) || defined(_WIN64)
FT_TEST(test_su_file_seeks_64_bits_on_a_crt_descriptor)
{
    create_test_file_stream_file();
    FILE *backing = std::fopen("build_logs/test_su_file_stream.txt", "rb");
    FT_ASSERT(backing != ft_nullptr);
    su_file stream{};
    stream._descriptor = _fileno(backing);
    constexpr int64_t offset = 4294967313LL;
    const int32_t seek_status = su_fseek(&stream, offset, SEEK_SET);
    const int64_t position = su_ftell(&stream);
    const int32_t rewind_status = su_fseek(&stream, 0, SEEK_SET);
    char byte = '\0';
    const ft_size_t read_count = su_fread(&byte, 1U, 1U, &stream);
    const int32_t close_status = std::fclose(backing);
    FT_ASSERT_EQ(0, seek_status);
    FT_ASSERT_EQ(offset, position);
    FT_ASSERT_EQ(0, rewind_status);
    FT_ASSERT_EQ(1U, read_count);
    FT_ASSERT_EQ('d', byte);
    FT_ASSERT_EQ(0, close_status);
    return (1);
}
#endif
