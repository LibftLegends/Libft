#include "../test_internal.hpp"
#include "../../Modules/HTML/html_document.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/HTML/html_parser.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_html_document_manual_thread_safety_lifecycle)
{
    html_document document;
    int initialize_error;

    initialize_error = document.initialize();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, initialize_error);
    FT_ASSERT_EQ(false, document.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.enable_thread_safety());
    FT_ASSERT_EQ(true, document.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.disable_thread_safety());
    FT_ASSERT_EQ(false, document.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_html_document_create_node_without_thread_safety)
{
    html_document document;
    html_node *node;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(false, document.is_thread_safe());
    node = document.create_node("div", "text");
    FT_ASSERT(node != ft_nullptr);
    html_free_nodes(node);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_html_document_enable_thread_safety_allocation_failure)
{
    html_document document;
    int thread_safety_error;
    html_node *node;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    cma_set_alloc_limit(1);
    thread_safety_error = document.enable_thread_safety();
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, thread_safety_error);
    FT_ASSERT_EQ(false, document.is_thread_safe());
    node = document.create_node("div", "text");
    FT_ASSERT(node != ft_nullptr);
    html_free_nodes(node);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_html_document_copies_deep_tree_and_resumes_siblings)
{
    constexpr ft_size_t TREE_DEPTH = 4096;
    html_document source;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    html_node *root = source.create_node("r", ft_nullptr);
    FT_ASSERT(root != ft_nullptr);
    source.append_node(root);
    html_node *node = root;
    ft_size_t depth = 0;
    while (depth < TREE_DEPTH)
    {
        if (depth < 32)
        {
            node->next = source.create_node("s", "sibling");
            FT_ASSERT(node->next != ft_nullptr);
            node->next->attributes = source.create_attr("k", "v");
            FT_ASSERT(node->next->attributes != ft_nullptr);
        }
        node->children = source.create_node("c", ft_nullptr);
        FT_ASSERT(node->children != ft_nullptr);
        node = node->children;
        ++depth;
    }

    html_document copy;
    ft_size_t allocation_count_before = 0;
    ft_size_t allocation_count_after = 0;
    ft_size_t free_count_before = 0;
    ft_size_t free_count_after = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocation_count_before, &free_count_before));
    cma_set_alloc_limit(sizeof(html_node) + sizeof(html_attr));
    const int32_t failed_copy = copy.initialize(source);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocation_count_after, &free_count_after));
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, failed_copy);
    FT_ASSERT(allocation_count_after - allocation_count_before >= 8);
    FT_ASSERT_EQ(allocation_count_after - allocation_count_before, free_count_after - free_count_before);
    FT_ASSERT_EQ(ft_nullptr, copy.get_root());
    FT_ASSERT_EQ(root, source.get_root());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, copy.initialize(source));
    html_node *cloned_node = copy.get_root();
    node = root;
    depth = 0;
    while (node)
    {
        FT_ASSERT(cloned_node != ft_nullptr);
        FT_ASSERT(node != cloned_node);
        FT_ASSERT(node->tag != cloned_node->tag);
        FT_ASSERT_STR_EQ(node->tag, cloned_node->tag);
        if (depth < 32)
        {
            FT_ASSERT(cloned_node->next != ft_nullptr);
            FT_ASSERT(cloned_node->next != node->next);
            FT_ASSERT_STR_EQ("sibling", cloned_node->next->text);
            FT_ASSERT(cloned_node->next->attributes != ft_nullptr);
            FT_ASSERT_STR_EQ("k", cloned_node->next->attributes->key);
            FT_ASSERT_STR_EQ("v", cloned_node->next->attributes->value);
            FT_ASSERT_EQ(ft_nullptr, cloned_node->next->next);
        }
        node = node->children;
        cloned_node = cloned_node->children;
        ++depth;
    }
    FT_ASSERT_EQ(TREE_DEPTH + 1, depth);
    FT_ASSERT_EQ(ft_nullptr, cloned_node);
    return (1);
}

FT_TEST(test_html_document_move_releases_previous_tree)
{
    html_document source;
    html_document destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    html_node *new_root = source.create_node("new", ft_nullptr);
    FT_ASSERT(new_root != ft_nullptr);
    source.append_node(new_root);
    ft_size_t allocations_before = 0;
    ft_size_t allocations_after = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_before, ft_nullptr));
    html_node *old_root = destination.create_node("old", "text");
    FT_ASSERT(old_root != ft_nullptr);
    destination.append_node(old_root);
    ft_size_t free_count_before = 0;
    ft_size_t free_count_after = 0;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(&allocations_after, &free_count_before));
    FT_ASSERT(allocations_after > allocations_before);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.move(source));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, cma_get_stats(ft_nullptr, &free_count_after));
    FT_ASSERT_EQ(allocations_after - allocations_before, free_count_after - free_count_before);
    FT_ASSERT_EQ(new_root, destination.get_root());
    FT_ASSERT_EQ(ft_nullptr, source.get_root());
    return (1);
}
