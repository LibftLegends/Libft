#include "../test_internal.hpp"
#include "../../Modules/HTML/html_parser.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/system_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include <new>
#include <string>

#include "../../Modules/Basic/limits.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

int test_html_create_node(void)
{
    html_node *node = html_create_node("div", "content");
    if (!node)
        return (0);
    int ok = node->tag && std::strcmp(node->tag, "div") == 0 &&
             node->text && std::strcmp(node->text, "content") == 0;
    html_free_nodes(node);
    return (ok);
}

int test_html_find_by_tag(void)
{
    html_node *root = html_create_node("div", ft_nullptr);
    html_node *child = html_create_node("span", ft_nullptr);
    html_add_child(root, child);
    html_node *found = html_find_by_tag(root, "span");
    int ok = (found == child);
    html_free_nodes(root);
    return (ok);
}

int test_html_write_to_string(void)
{
    html_node *node = html_create_node("div", "Hello");
    char *result = html_write_to_string(node);
    const char *expected = "<div>Hello</div>\n";
    int ok = result && std::strcmp(result, expected) == 0;
    if (result)
        cma_free(result);
    html_free_nodes(node);
    return (ok);
}

FT_TEST(test_html_write_to_string_allocation_failure_sets_errno)
{
    html_node *node = html_create_node("div", "Hello");
    FT_ASSERT(node != ft_nullptr);
    cma_set_alloc_limit(1);
    char *result = html_write_to_string(node);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(ft_nullptr, result);
    html_free_nodes(node);
    return (1);
}

FT_TEST(test_html_write_to_string_success_clears_errno)
{
    html_node *node = html_create_node("div", "Hello");
    FT_ASSERT(node != ft_nullptr);
    char *result = html_write_to_string(node);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(result, "<div>Hello</div>\n"));
    cma_free(result);
    html_free_nodes(node);
    return (1);
}

FT_TEST(test_html_write_to_string_escapes_attribute_values)
{
    html_node *node = html_create_node("div", ft_nullptr);
    FT_ASSERT(node != ft_nullptr);
    html_attr *attribute = html_create_attr("data", "A \"quote\" & <tag>");
    FT_ASSERT(attribute != ft_nullptr);
    html_add_attr(node, attribute);
    char *result = html_write_to_string(node);
    FT_ASSERT(result != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(result, "<div data=\"A \"quote\" & <tag>\"/>\n"));
    cma_free(result);
    html_free_nodes(node);
    return (1);
}

static std::string html_written_bytes;

static int64_t capture_short_html_write(int32_t, const void *bytes, ft_size_t length)
{
    if (length > 97)
        length = 97;
    html_written_bytes.append(static_cast<const char *>(bytes), length);
    return (static_cast<int64_t>(length));
}

static int64_t stall_html_write(int32_t, const void *, ft_size_t)
{
    return (0);
}

FT_TEST(test_html_file_writer_matches_string_and_retries_short_writes)
{
    const char *path = "test_html_buffered_writer.html";
    std::string long_text(20000, 'x');
    char tag[] = "div";
    char key[] = "data";
    char value[] = "A & B";
    html_attr attribute{key, value, ft_nullptr};
    html_node child{tag, long_text.data(), ft_nullptr, ft_nullptr, ft_nullptr};
    html_node root{tag, ft_nullptr, &attribute, &child, ft_nullptr};
    char *serialized = html_write_to_string(&root);
    FT_ASSERT(serialized != ft_nullptr);
    const std::string expected = "<div data=\"A & B\">\n  <div>" + long_text + "</div>\n</div>\n";
    const ft_bool string_matches = expected == serialized;
    cma_free(serialized);
    FT_ASSERT_EQ(FT_TRUE, string_matches);

    html_written_bytes.clear();
    su_set_write_syscall_hook(capture_short_html_write);
    const int32_t short_write = html_write_to_file(path, &root);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, short_write);
    FT_ASSERT_EQ(expected, html_written_bytes);
    html_written_bytes.clear();

    su_set_write_syscall_hook(stall_html_write);
    const int32_t stalled = html_write_to_file(path, &root);
    su_reset_write_syscall_hook();
    FT_ASSERT_EQ(FT_ERR_INTERNAL, stalled);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, html_write_to_file(path, &root));
    ft_string disk;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, disk));
    FT_ASSERT_EQ(expected, std::string(disk.c_str(), disk.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    return (1);
}

FT_TEST(test_html_find_by_selector_matches_attributes_without_allocating)
{
    html_node *root = html_create_node("div", ft_nullptr);
    FT_ASSERT(root != ft_nullptr);
    html_attr *id_attr = html_create_attr("id", "value");
    FT_ASSERT(id_attr != ft_nullptr);
    html_add_attr(root, id_attr);
    cma_set_alloc_limit(1);
    html_node *found = html_find_by_selector(root, "[id=\"value\"]");
    html_node *presence = html_find_by_selector(root, "[id]");
    html_node *partial_value = html_find_by_selector(root, "[id=val]");
    html_node *partial_key = html_find_by_selector(root, "[i=value]");
    html_node *longer_value = html_find_by_selector(root, "[id=value-longer]");
    html_node *malformed = html_find_by_selector(root, "[id=value");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(root, found);
    FT_ASSERT_EQ(root, presence);
    FT_ASSERT_EQ(ft_nullptr, partial_value);
    FT_ASSERT_EQ(ft_nullptr, partial_key);
    FT_ASSERT_EQ(ft_nullptr, longer_value);
    FT_ASSERT_EQ(ft_nullptr, malformed);
    html_free_nodes(root);
    return (1);
}

FT_TEST(test_html_find_by_selector_quoted_attribute_values)
{
    html_node *root = html_create_node("div", ft_nullptr);
    FT_ASSERT(root != ft_nullptr);
    html_node *child = html_create_node("p", ft_nullptr);
    FT_ASSERT(child != ft_nullptr);
    html_attr *attr = html_create_attr("data-id", "42");
    FT_ASSERT(attr != ft_nullptr);
    html_add_attr(child, attr);
    html_add_child(root, child);
    html_node *found_double = html_find_by_selector(root, "[data-id=\"42\"]");
    FT_ASSERT(found_double == child);
    html_node *found_single = html_find_by_selector(root, "[data-id='42']");
    FT_ASSERT(found_single == child);
    html_free_nodes(root);
    return (1);
}

FT_TEST(test_html_remove_helpers_validate_inputs)
{
    html_node *root = html_create_node("div", ft_nullptr);

    FT_ASSERT(root != ft_nullptr);
    html_remove_nodes_by_tag(ft_nullptr, "div");
    html_remove_nodes_by_tag(&root, ft_nullptr);
    html_remove_nodes_by_attr(ft_nullptr, "id", "value");
    html_remove_nodes_by_attr(&root, ft_nullptr, "value");
    html_remove_nodes_by_attr(&root, "id", ft_nullptr);
    html_remove_nodes_by_text(ft_nullptr, "text");
    html_remove_nodes_by_text(&root, ft_nullptr);
    html_free_nodes(root);
    return (1);
}

FT_TEST(test_html_remove_nodes_preserves_surviving_siblings)
{
    html_node *root = html_create_node("root", ft_nullptr);
    FT_ASSERT(root != ft_nullptr);
    const char *tags[] = {"drop", "keep", "keep", "drop", "last"};
    const char *texts[] = {ft_nullptr, "remove text", "remove attr", ft_nullptr, ft_nullptr};
    html_node **next_child = &root->children;
    ft_size_t index = 0;
    while (index < sizeof(tags) / sizeof(tags[0]))
    {
        html_node *child = html_create_node(tags[index], texts[index]);
        if (child == ft_nullptr)
        {
            html_free_nodes(root);
            FT_ASSERT(child != ft_nullptr);
        }
        *next_child = child;
        next_child = &child->next;
        ++index;
    }
    html_node *marked = root->children->next->next;
    marked->attributes = html_create_attr("id", "remove");
    marked->children = html_create_node("nested", ft_nullptr);
    if (marked->attributes == ft_nullptr || marked->children == ft_nullptr)
    {
        html_free_nodes(root);
        FT_ASSERT(FT_FALSE);
    }
    html_node *survivor = marked->next->next;
    html_remove_nodes_by_tag(&root, "drop");
    html_remove_nodes_by_text(&root, "remove text");
    html_remove_nodes_by_attr(&root, "id", "remove");
    const ft_bool preserved = root->children == survivor && survivor->next == ft_nullptr;
    html_free_nodes(root);
    FT_ASSERT_EQ(FT_TRUE, preserved);
    return (1);
}

FT_TEST(test_html_free_nodes_deep_tree)
{
    constexpr ft_size_t NODE_COUNT = 100000;
    html_node *root = html_create_node("root", ft_nullptr);
    FT_ASSERT(root != ft_nullptr);
    html_node *parent = root;
    ft_size_t index = 0;
    while (index < NODE_COUNT)
    {
        html_node *child = new(std::nothrow) html_node{};
        if (child == ft_nullptr)
        {
            html_free_nodes(root);
            FT_ASSERT(child != ft_nullptr);
        }
        parent->children = child;
        if (index % 1024 == 0)
        {
            child->next = new(std::nothrow) html_node{};
            if (child->next == ft_nullptr)
            {
                html_free_nodes(root);
                FT_ASSERT(FT_FALSE);
            }
        }
        parent = child;
        ++index;
    }
    html_free_nodes(root);
    return (1);
}

int test_html_find_by_attr(void)
{
    html_node *root = html_create_node("div", ft_nullptr);
    html_node *child = html_create_node("p", ft_nullptr);
    html_attr *attr = html_create_attr("id", "main");
    html_add_attr(child, attr);
    html_add_child(root, child);
    html_node *found = html_find_by_attr(root, "id", "main");
    int ok = (found == child);
    html_free_nodes(root);
    return (ok);
}

int test_html_find_by_selector(void)
{
    html_node *root = html_create_node("div", ft_nullptr);
    html_node *child_id = html_create_node("p", ft_nullptr);
    html_attr *id_attr = html_create_attr("id", "main");
    html_add_attr(child_id, id_attr);
    html_add_child(root, child_id);
    html_node *child_class = html_create_node("span", ft_nullptr);
    html_attr *class_attr = html_create_attr("class", "highlight");
    html_add_attr(child_class, class_attr);
    html_add_child(root, child_class);
    html_node *found_id = html_find_by_selector(root, "#main");
    html_node *found_class = html_find_by_selector(root, ".highlight");
    int ok = (found_id == child_id) && (found_class == child_class);
    html_free_nodes(root);
    return (ok);
}

int test_html_query_selector(void)
{
    html_node *root = html_create_node("div", ft_nullptr);
    html_node *child_tag = html_create_node("p", ft_nullptr);
    html_node *child_class = html_create_node("span", ft_nullptr);
    html_attr *class_attr = html_create_attr("class", "note");
    html_add_attr(child_class, class_attr);
    html_node *child_id = html_create_node("a", ft_nullptr);
    html_attr *id_attr = html_create_attr("id", "link");
    html_add_attr(child_id, id_attr);
    html_add_child(root, child_tag);
    html_add_child(root, child_class);
    html_add_child(root, child_id);
    html_node *found_tag = html_query_selector(root, "p");
    html_node *found_class = html_query_selector(root, ".note");
    html_node *found_id = html_query_selector(root, "#link");
    int is_ok = (found_tag == child_tag) && (found_class == child_class) && (found_id == child_id);
    html_free_nodes(root);
    return (is_ok);
}
