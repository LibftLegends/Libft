#include "../test_internal.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cerrno>

FT_TEST(test_cmp_system_error_mapping_preserves_success_and_fallback)
{
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cmp_map_system_error_to_ft(0));
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, cmp_map_system_error_to_ft(EACCES));
    FT_ASSERT_EQ(29000 + ERRNO_OFFSET, cmp_map_system_error_to_ft(29000));
    return (1);
}

#if defined(_WIN32) || defined(_WIN64)
FT_TEST(test_cmp_windows_system_error_mapping_converts_each_range)
{
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, cmp_map_system_error_to_ft(ERROR_PATH_NOT_FOUND));
    FT_ASSERT_EQ(FT_ERR_INVALID_PATH, cmp_map_system_error_to_ft(ERROR_INVALID_NAME));
    FT_ASSERT_EQ(FT_ERR_FULL, cmp_map_system_error_to_ft(ERROR_NO_SYSTEM_RESOURCES));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, cmp_map_system_error_to_ft(WSAEWOULDBLOCK));
    FT_ASSERT_EQ(FT_ERR_IO, cmp_map_system_error_to_ft(WSAEDISCON));
    FT_ASSERT_EQ(FT_ERR_SOCKET_RESOLVE_AGAIN, cmp_map_system_error_to_ft(WSATRY_AGAIN));
    FT_ASSERT_EQ(FT_ERR_SOCKET_RESOLVE_NO_NAME, cmp_map_system_error_to_ft(WSANO_DATA));
    FT_ASSERT_EQ(FT_ERR_OUT_OF_RANGE, cmp_map_system_error_to_ft(FT_INT32_MAX));
    return (1);
}
#endif
