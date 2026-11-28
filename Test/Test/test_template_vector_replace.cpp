#include "../test_internal.hpp"
#include "../../Modules/Template/vector.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

FT_TEST(test_template_vector_replace_cleans_up_partially_copied_elements)
{
    ft_vector<ft_string> source;
    ft_vector<ft_string> target;
    ft_string short_text;
    ft_string long_text;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, short_text.initialize("keep"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_text.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, long_text.assign(1024U, 'a'));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(short_text));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(long_text));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.push_back(short_text));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(2U * sizeof(ft_string)));
    const int32_t result = target.replace_from(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    FT_ASSERT_EQ(1U, target.size());
    FT_ASSERT_EQ(4U, target[0].size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.replace_from(source));
    FT_ASSERT_EQ(2U, target.size());
    FT_ASSERT_EQ(1024U, target[1].size());
    return (1);
}

FT_TEST(test_template_vector_replace_trivial_reuses_existing_storage)
{
    ft_vector<int32_t> source;
    ft_vector<int32_t> target;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(7));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.push_back(8));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.push_back(1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.push_back(2));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, target.push_back(3));
    const int32_t *storage = target.begin();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(1U));
    const int32_t result = target.replace_from(source);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_set_alloc_limit(0U));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, result);
    FT_ASSERT(target.begin() == storage);
    FT_ASSERT_EQ(2U, target.size());
    FT_ASSERT_EQ(7, target[0]);
    FT_ASSERT_EQ(8, target[1]);
    return (1);
}
