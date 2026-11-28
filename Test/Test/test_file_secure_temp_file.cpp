#include "../test_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

FT_TEST(test_file_secure_temp_file_creates_unique_existing_file)
{
    ft_string first_path;
    ft_string second_path;
    int32_t first_descriptor;
    int32_t second_descriptor;

    first_descriptor = -1;
    second_descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_secure_temp_file(ft_nullptr, "secure_test",
            &first_path, &first_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_secure_temp_file(ft_nullptr, "secure_test",
            &second_path, &second_descriptor));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR, file_get_type(first_path.c_str()));
    FT_ASSERT_EQ(FILE_TYPE_REGULAR, file_get_type(second_path.c_str()));
    FT_ASSERT_EQ(FT_FALSE, file_path_equal(first_path.c_str(), second_path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_close_descriptor(first_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_close_descriptor(second_descriptor));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(first_path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(second_path.c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, first_path.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, second_path.destroy());
    return (1);
}

FT_TEST(test_file_secure_temp_file_rejects_invalid_outputs)
{
    ft_string path;
    int32_t descriptor;

    descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, file_secure_temp_file(ft_nullptr,
            "secure_test", ft_nullptr, &descriptor));
    FT_ASSERT_EQ(FT_ERR_INVALID_POINTER, file_secure_temp_file(ft_nullptr,
            "secure_test", &path, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.destroy());
    return (1);
}

FT_TEST(test_file_secure_temp_file_preserves_missing_directory_error)
{
    ft_string missing_directory;
    ft_string path;
    int32_t descriptor;
    int32_t error_code;

    descriptor = -1;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_directory.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, filesystem_temp_path(
            "libft_missing_secure_directory", "dir", &missing_directory));
    (void)file_delete(missing_directory.c_str());
    error_code = file_secure_temp_file(missing_directory.c_str(), "secure_test",
            &path, &descriptor);
    FT_ASSERT_NE(FT_ERR_ALREADY_EXISTS, error_code);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, path.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_directory.destroy());
    return (1);
}
