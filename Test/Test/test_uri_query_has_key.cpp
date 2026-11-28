#include "../test_internal.hpp"
#include "../../Modules/URI/uri.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_uri_query_has_key_accepts_empty_and_encoded_values)
{
    const char *query_string;

    query_string = "empty=&name=Ada%20Lovelace";
    FT_ASSERT_EQ(FT_TRUE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "empty", 5));
    FT_ASSERT_EQ(FT_TRUE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "name", 4));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "missing", 7));
    return (1);
}

FT_TEST(test_uri_query_has_key_rejects_invalid_null_spans_and_prefixes)
{
    const char *query_string;

    query_string = "alpha=1&alphabet=2&name=Ada+Lovelace";
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "alph", 4));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "missing", 7));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(ft_nullptr, 1, "alpha", 5));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), ft_nullptr, 1));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key("bad=%ZZ", 7, "bad", 3));
    return (1);
}

FT_TEST(test_uri_query_has_key_matches_encoded_keys_and_duplicate_first_value)
{
    const char *query_string;

    query_string = "a%20b=first&a%20b=second";
    FT_ASSERT_EQ(FT_TRUE, uri_query_has_key(query_string,
            ft_strlen_size_t(query_string), "a b", 3));
    FT_ASSERT_EQ(FT_TRUE, uri_query_has_key("flag&&tail=value",
            ft_strlen_size_t("flag&&tail=value"), "flag", 4));
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key("", 0, "flag", 4));
    return (1);
}

FT_TEST(test_uri_query_lookup_requires_no_allocations_for_keys)
{
    const char *query = "unmatched=first&a%00b=%2f&last=value";
    const char key[] = {'a', '\0', 'b'};
    cma_set_alloc_limit(1U);
    const ft_bool exists = uri_query_has_key(query, ft_strlen_size_t(query),
        key, sizeof(key));
    cma_set_alloc_limit(3U);
    char *value = uri_query_get_value(query, ft_strlen_size_t(query), "last", 4U);
    cma_set_alloc_limit(0U);
    FT_ASSERT_EQ(FT_TRUE, exists);
    FT_ASSERT_EQ(ft_nullptr, value);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, uri_get_error());

    const char *duplicate = "a%ZZ=skip&name=%ZZ&name=valid";
    FT_ASSERT_EQ(FT_FALSE, uri_query_has_key(duplicate, ft_strlen_size_t(duplicate),
        "name", 4U));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, uri_get_error());
    FT_ASSERT_EQ(FT_TRUE, uri_query_has_key(ft_nullptr, 0U, ft_nullptr, 0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, uri_get_error());
    return (1);
}
