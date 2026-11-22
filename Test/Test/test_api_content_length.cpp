#include "../test_internal.hpp"
#include "../../Modules/API/api_internal.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_api_append_content_length_propagates_string_error)
{
    ft_string request;
    bool append_result;

    cma_set_alloc_limit(1);
    append_result = api_append_content_length_header(request, 42);
    FT_ASSERT_EQ(false, append_result);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, request.get_error());
    cma_set_alloc_limit(0);
    return (1);
}

FT_TEST(test_api_append_content_length_successful_append)
{
    ft_string request;
    bool append_result;

    append_result = api_append_content_length_header(request, 7);
    FT_ASSERT_EQ(true, append_result);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request.get_error());
    FT_ASSERT(request.find("Content-Length: 7") != ft_string::npos);
    return (1);
}

FT_TEST(test_api_append_content_length_keeps_prefix_and_full_width_digits)
{
    ft_string request;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, request.initialize("POST / HTTP/1.1"));
    FT_ASSERT_EQ(FT_TRUE, api_append_content_length_header(request, FT_SYSTEM_SIZE_MAX));
    FT_ASSERT_EQ(FT_TRUE, request == "POST / HTTP/1.1\r\nContent-Length: 18446744073709551615");
    FT_ASSERT_EQ(FT_TRUE, api_append_content_length_header(request, 0U));
    FT_ASSERT_EQ(FT_TRUE, request ==
        "POST / HTTP/1.1\r\nContent-Length: 18446744073709551615\r\nContent-Length: 0");
    return (1);
}
