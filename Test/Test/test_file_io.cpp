#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/File/open_dir.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>
#include <cstdio>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#if defined(_WIN32) || defined(_WIN64)
# include <io.h>
#else
# include <unistd.h>
#endif

static void create_test_file(void)
{
    FILE *file = ft_fopen("build_logs/test_file_io.txt", "w");
    if (file != ft_nullptr)
    {
        std::fputs("Line1\nLine2\n", file);
        ft_fclose(file);
    }
    return ;
}

static void force_file_descriptor_failure(FILE *file_handle)
{
    int file_descriptor;

    if (file_handle == ft_nullptr)
        return ;
#if defined(_WIN32) || defined(_WIN64)
    file_descriptor = _fileno(file_handle);
    if (file_descriptor >= 0)
        _close(file_descriptor);
#else
    file_descriptor = fileno(file_handle);
    if (file_descriptor >= 0)
        close(file_descriptor);
#endif
    return ;
}

FT_TEST(test_file_fopen_valid)
{
    FILE *file;

    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    FT_ASSERT(file != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file));
    return (1);
}

FT_TEST(test_file_fopen_invalid)
{
    FT_ASSERT_EQ(ft_nullptr, ft_fopen("missing_file.txt", "r"));
    return (1);
}

FT_TEST(test_file_fopen_invalid_mode_sets_errno)
{
    FILE *file;

    create_test_file();
    errno = 0;
    file = ft_fopen("build_logs/test_file_io.txt", "invalid");
    FT_ASSERT_EQ(ft_nullptr, file);
    return (1);
}

FT_TEST(test_file_fopen_null)
{
    FT_ASSERT_EQ(ft_nullptr, ft_fopen(ft_nullptr, "r"));
    FT_ASSERT_EQ(ft_nullptr, ft_fopen("build_logs/test_file_io.txt", ft_nullptr));
    return (1);
}

FT_TEST(test_file_fclose_null)
{
    FT_ASSERT_EQ(EOF, ft_fclose(ft_nullptr));
    return (1);
}

FT_TEST(test_file_fclose_failure_sets_errno)
{
    FILE *file;

    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    if (file == ft_nullptr)
        return (0);
    force_file_descriptor_failure(file);
    FT_ASSERT_EQ(EOF, ft_fclose(file));
    return (1);
}

FT_TEST(test_file_fgets_basic)
{
    char buffer[16];
    FILE *file;

    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    if (file == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(buffer, ft_fgets(buffer, 16, file));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Line1\n"));
    FT_ASSERT_EQ(buffer, ft_fgets(buffer, 16, file));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Line2\n"));
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(buffer, 16, file));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file));
    return (1);
}

FT_TEST(test_file_fgets_edge_cases)
{
    char buffer[8];
    FILE *file;

    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    if (file == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(buffer, ft_fgets(buffer, 5, file));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Line"));
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(ft_nullptr, 5, file));
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(buffer, 0, file));
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(buffer, 5, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file));
    return (1);
}

FT_TEST(test_file_fgets_negative_size_sets_errno)
{
    char buffer[16];
    FILE *file;

    buffer[0] = 'X';
    buffer[1] = '\0';
    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    if (file == ft_nullptr)
        return (0);
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(buffer, -1, file));
    FT_ASSERT_EQ('X', buffer[0]);
    FT_ASSERT_EQ('\0', buffer[1]);
    FT_ASSERT_EQ(buffer, ft_fgets(buffer, sizeof(buffer), file));
    FT_ASSERT_EQ(0, ft_strcmp(buffer, "Line1\n"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, ft_fclose(file));
    return (1);
}

FT_TEST(test_file_fgets_stream_error_sets_errno)
{
    char buffer[8];
    FILE *file;

    create_test_file();
    file = ft_fopen("build_logs/test_file_io.txt", "r");
    if (file == ft_nullptr)
        return (0);
    force_file_descriptor_failure(file);
    FT_ASSERT_EQ(ft_nullptr, ft_fgets(buffer, sizeof(buffer), file));
    return (1);
}

FT_TEST(test_file_readdir_returns_entries)
{
    file_dir *directory_stream;
    file_dirent *directory_entry;

    directory_stream = file_opendir(".");
    FT_ASSERT(directory_stream != ft_nullptr);
    directory_entry = file_readdir(directory_stream);
    FT_ASSERT(directory_entry != ft_nullptr);
    FT_ASSERT_EQ(0, file_closedir(directory_stream));
    return (1);
}
