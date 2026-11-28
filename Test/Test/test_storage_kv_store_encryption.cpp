#include "../test_internal.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Storage/kv_store.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#ifdef LIBFT_TEST_BUILD
FT_TEST(test_storage_kv_store_encryption_matches_legacy_ciphertext)
{
    const char *path = "test_storage_encryption_legacy.json";
    constexpr char EMPTY[] = "{\"kv_store\":{}}";
    constexpr char LEGACY[] =
        "iJ5a7OnYDhw3BfHnwCnN01f8ms6SOs4BZrUAT2KHu+RalTE/tzfm4qadV4shtkzbF4aRFg==";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, EMPTY, sizeof(EMPTY) - 1));
    kv_store store;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.initialize(path, "0123456789abcdef", FT_TRUE));
    ft_string plain;
    ft_string encoded;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plain.initialize("value crossing an AES block boundary"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, store.configure_encryption("short", FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, store.configure_encryption("other-16byte-key", FT_TRUE, ""));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.encrypt_value(plain, encoded));
    FT_ASSERT_EQ(0, ft_strcmp(encoded.c_str(), LEGACY));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, plain.clear());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, store.decrypt_value(encoded, plain));
    FT_ASSERT_EQ(0, ft_strcmp(plain.c_str(), "value crossing an AES block boundary"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, encoded.assign("AAAAAAAAAAAAAAAAAAAAAA==", 24));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, store.decrypt_value(encoded, plain));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}
#endif
