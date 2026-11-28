#include "../test_internal.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/Filesystem/filesystem.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"

static ft_bool file_test_digest_equals(const uint8_t *left, const uint8_t *right,
    ft_size_t length)
{
    if (std::memcmp(left, right, length) == 0)
        return (FT_TRUE);
    return (FT_FALSE);
}

static ft_bool file_test_flag_is_set(uint32_t flags, uint32_t flag_value)
{
    if ((flags & flag_value) != 0U)
        return (FT_TRUE);
    return (FT_FALSE);
}

FT_TEST(test_file_hash_sha256_depends_on_contents)
{
    const char *first_path;
    const char *second_path;
    uint8_t first_sha1_digest[20];
    uint8_t second_sha1_digest[20];
    uint8_t first_digest[32];
    uint8_t second_digest[32];
    uint8_t different_digest[32];

    first_path = "test_file_hash_first.txt";
    second_path = "test_file_hash_second.txt";
    (void)file_delete(first_path);
    (void)file_delete(second_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(first_path, "hello", 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(second_path, "hello", 5));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_hash_sha1(first_path, first_sha1_digest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_hash_sha1(second_path, second_sha1_digest));
    FT_ASSERT_EQ(FT_TRUE, file_test_digest_equals(first_sha1_digest,
            second_sha1_digest, sizeof(first_sha1_digest)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_hash_sha256(first_path, first_digest));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_hash_sha256(second_path, second_digest));
    FT_ASSERT_EQ(FT_TRUE, file_test_digest_equals(first_digest, second_digest,
            sizeof(first_digest)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(second_path, "hello!", 6));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_hash_sha256(second_path, different_digest));
    FT_ASSERT_EQ(FT_FALSE, file_test_digest_equals(first_digest,
            different_digest, sizeof(first_digest)));
    (void)file_delete(first_path);
    (void)file_delete(second_path);
    return (1);
}

FT_TEST(test_file_hash_streams_binary_files_without_allocating_the_payload)
{
    constexpr uint8_t SHA1_EXPECTED[20] = {
        0x65, 0xd1, 0x4e, 0xcd, 0x3a, 0xc4, 0x57, 0xc1, 0xf2, 0x49,
        0x9a, 0x1e, 0xfe, 0x42, 0xeb, 0xa0, 0x52, 0xe0, 0x22, 0x4b
    };
    constexpr uint8_t SHA256_EXPECTED[32] = {
        0x51, 0xe3, 0xca, 0x48, 0x22, 0x3e, 0x56, 0x30, 0xe6, 0xba, 0x69,
        0xbf, 0x82, 0xe4, 0x51, 0xde, 0x9c, 0xaf, 0xc9, 0x0d, 0x55, 0x56,
        0x67, 0x8f, 0xaa, 0x0e, 0xaa, 0xe0, 0xd4, 0xf6, 0x8f, 0xfe
    };
    const char *path = "test_file_hash_binary.bin";
    uint8_t payload[65536 + 56];
    ft_size_t index = 0;
    while (index < sizeof(payload))
    {
        payload[index] = static_cast<uint8_t>(index * 17 + 3);
        ++index;
    }
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path,
        reinterpret_cast<const char *>(payload), sizeof(payload)));
    uint8_t sha1[20];
    uint8_t sha256[32];
    cma_set_alloc_limit(1);
    const int32_t sha1_error = file_hash_sha1(path, sha1);
    const int32_t sha256_error = file_hash_sha256(path, sha256);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sha1_error);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sha256_error);
    FT_ASSERT_EQ(FT_TRUE, file_test_digest_equals(sha1, SHA1_EXPECTED, sizeof(sha1)));
    FT_ASSERT_EQ(FT_TRUE, file_test_digest_equals(sha256, SHA256_EXPECTED, sizeof(sha256)));
    return (1);
}

FT_TEST(test_file_metadata_diff_reports_type_size_and_missing_changes)
{
    const char *root_path;
    const char *first_file_path;
    const char *second_file_path;
    const char *directory_path;
    const char *missing_path;
    uint32_t difference_flags;

    root_path = "test_file_metadata_diff";
    first_file_path = "test_file_metadata_diff/first.txt";
    second_file_path = "test_file_metadata_diff/second.txt";
    directory_path = "test_file_metadata_diff/directory";
    missing_path = "test_file_metadata_diff/missing.txt";
    (void)file_delete_recursive(root_path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_create_directories(directory_path, 0700));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(first_file_path, "abc", 3));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(second_file_path, "abcd", 4));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_metadata_diff(first_file_path,
            first_file_path, &difference_flags));
    FT_ASSERT_EQ(FILE_METADATA_DIFF_NONE, difference_flags);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_metadata_diff(first_file_path,
            second_file_path, &difference_flags));
    FT_ASSERT_EQ(FT_TRUE, file_test_flag_is_set(difference_flags,
            FILE_METADATA_DIFF_SIZE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_metadata_diff(first_file_path,
            directory_path, &difference_flags));
    FT_ASSERT_EQ(FT_TRUE, file_test_flag_is_set(difference_flags,
            FILE_METADATA_DIFF_TYPE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_metadata_diff(first_file_path,
            missing_path, &difference_flags));
    FT_ASSERT_EQ(FT_TRUE, file_test_flag_is_set(difference_flags,
            FILE_METADATA_DIFF_MISSING));
    (void)file_delete_recursive(root_path);
    return (1);
}

FT_TEST(test_file_metadata_diff_reports_permission_change_without_size_change)
{
    const char *left = "test_file_metadata_permissions_left.txt";
    const char *right = "test_file_metadata_permissions_right.txt";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(left, "x", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(right, "x", 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_file_set_permissions(left, 6, 4, 4, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_file_set_permissions(right, 4, 4, 4, ft_nullptr));
    uint32_t differences = FILE_METADATA_DIFF_NONE;
    const int32_t result = file_metadata_diff(left, right, &differences);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_file_set_permissions(right, 6, 4, 4, ft_nullptr));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(left));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(right));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT_EQ(static_cast<uint32_t>(FILE_METADATA_DIFF_PERMISSIONS), differences);
    return (1);
}
