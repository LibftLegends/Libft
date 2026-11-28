#include "../test_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../Modules/DUMB/dumb_io.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstdio>

FT_TEST(test_dumb_io_read_file_preserves_binary_and_empty_files)
{
    char path[128];
    std::snprintf(path, sizeof(path), "build_logs/dumb_io_binary_" FT_UINT64_DECIMAL_FORMAT ".bin",
        cmp_get_process_id());
    const char payload[] = {'\0', 'a', '\r', '\n'};
    char *buffer = ft_nullptr;
    ft_size_t size = 0;
    const int32_t write_status = ft_write_file(path, payload, sizeof(payload));
    const int32_t read_status = ft_read_file(path, &buffer, &size);
    const ft_bool matches = buffer != ft_nullptr && size == sizeof(payload)
        && ft_memcmp(buffer, payload, sizeof(payload)) == 0;
    cma_free(buffer);
    const int32_t empty_write_status = ft_write_file(path, "", 0);
    size = sizeof(payload);
    buffer = ft_nullptr;
    const int32_t empty_read_status = ft_read_file(path, &buffer, &size);
    (void)std::remove(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, write_status);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_status);
    FT_ASSERT(matches);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_write_status);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, empty_read_status);
    FT_ASSERT_EQ(0U, size);
    FT_ASSERT(buffer == ft_nullptr);
    return (1);
}
