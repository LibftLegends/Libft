#include "../test_internal.hpp"
#include "../../Modules/URI/uri.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_uri_normalize_lowercases_and_removes_dot_segments)
{
    char *normalized_uri;
    const char *uri_input;

    uri_input = "HTTP://Example.COM/a/./b/../c/%7euser";
    normalized_uri = uri_normalize(uri_input, ft_strlen_size_t(uri_input));
    FT_ASSERT(normalized_uri != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(normalized_uri,
        "http://example.com/a/c/~user"));
    cma_free(normalized_uri);
    return (1);
}

FT_TEST(test_uri_normalize_reuses_path_storage)
{
    const char *path = "/foo/bar/..";
    cma_set_alloc_limit(12U);
    char *normalized = uri_normalize(path, ft_strlen_size_t(path));
    cma_set_alloc_limit(0U);
    FT_ASSERT(normalized != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp("/foo", normalized));
    cma_free(normalized);
    return (1);
}
