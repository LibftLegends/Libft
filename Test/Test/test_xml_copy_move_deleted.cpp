#include "../test_internal.hpp"
#include "../../Modules/XML/xml.hpp"
#include "../../Modules/Template/move.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <string>
#include <type_traits>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_xml_node_copy_move_deleted)
{
    bool copy_constructible;
    bool copy_assignable;
    bool move_constructible;
    bool move_assignable;

    copy_constructible = std::is_copy_constructible<xml_node>::value;
    copy_assignable = std::is_copy_assignable<xml_node>::value;
    move_constructible = std::is_move_constructible<xml_node>::value;
    move_assignable = std::is_move_assignable<xml_node>::value;
    FT_ASSERT(!copy_constructible);
    FT_ASSERT(!copy_assignable);
    FT_ASSERT(!move_constructible);
    FT_ASSERT(!move_assignable);
    return (1);
}

FT_TEST(test_xml_document_copy_move_deleted)
{
    xml_document source_document;
    xml_document *moved_document_pointer;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_document.load_from_string("<root><child>value</child></root>"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.get_error());
    FT_ASSERT_EQ(FT_TRUE, source_document.is_thread_safe());

    moved_document_pointer = new xml_document();
    FT_ASSERT(moved_document_pointer != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_document_pointer->move(source_document));
    FT_ASSERT_EQ(FT_TRUE, moved_document_pointer->is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_document_pointer->get_error());
    FT_ASSERT(moved_document_pointer->get_root() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_document_pointer->get_error());
    FT_ASSERT_EQ(0, ft_strcmp("root", moved_document_pointer->get_root()->name));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_document_pointer->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, moved_document_pointer->get_error());
    FT_ASSERT_EQ(0, ft_strcmp(moved_document_pointer->get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    delete moved_document_pointer;
    return (1);
}

FT_TEST(test_xml_document_move_into_initialized_destination_preserves_source_thread_safety)
{
    xml_document source_document;
    xml_document destination_document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        source_document.load_from_string("<moved attr=\"x\"/>"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.get_error());
    FT_ASSERT_EQ(FT_FALSE, source_document.is_thread_safe());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS,
        destination_document.load_from_string("<old/>"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.move(source_document));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.get_error());
    FT_ASSERT_EQ(FT_FALSE, destination_document.is_thread_safe());
    FT_ASSERT(destination_document.get_root() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp("moved", destination_document.get_root()->name));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination_document.get_error());
    FT_ASSERT_EQ(0, ft_strcmp(destination_document.get_error_str(),
        ft_strerror(FT_ERR_SUCCESS)));
    return (1);
}

FT_TEST(test_xml_document_copy_preserves_attributes_and_namespace_ownership)
{
    xml_document source;
    xml_document destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.load_from_string(
        "<p:root xmlns:p='urn:outer' xmlns='urn:default' data='value'>"
        "<p:child>text</p:child><p:override xmlns:p='urn:inner'><p:leaf/></p:override>"
        "<child/></p:root>"));
    const xml_node *source_root = source.get_root();
    FT_ASSERT(source_root != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_TRUE, destination.is_thread_safe());
    xml_node *root = destination.get_root();
    FT_ASSERT(root != ft_nullptr && root != source_root);
    FT_ASSERT(root->name != source_root->name && root->namespace_uri != source_root->namespace_uri);
    FT_ASSERT(root->mutex != ft_nullptr && root->mutex != source_root->mutex);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());

    FT_ASSERT_STR_EQ("p:root", root->name);
    FT_ASSERT_STR_EQ("urn:outer", root->namespace_uri);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), root->attributes.size());
    ft_bool attribute_matches = FT_FALSE;
    auto attribute = root->attributes.begin();
    const auto attribute_end = root->attributes.end();
    while (attribute != attribute_end)
    {
        if (ft_strcmp(attribute->first, "data") == 0)
            attribute_matches = ft_strcmp(attribute->second, "value") == 0;
        ++attribute;
    }
    FT_ASSERT_EQ(FT_TRUE, attribute_matches);
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), root->children.size());
    FT_ASSERT_EQ(root->namespace_uri, root->children[0]->namespace_uri);
    FT_ASSERT_STR_EQ("text", root->children[0]->text);
    FT_ASSERT_STR_EQ("urn:inner", root->children[1]->namespace_uri);
    FT_ASSERT_EQ(root->children[1]->namespace_uri, root->children[1]->children[0]->namespace_uri);
    FT_ASSERT_STR_EQ("urn:default", root->children[2]->namespace_uri);
    return (1);
}

FT_TEST(test_xml_document_copy_failure_destroys_destination_and_allows_retry)
{
    xml_document source;
    xml_document destination;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.load_from_string("<old/>"));
    const std::string payload(8192, 'x');
    const std::string input = "<root><first/><second>" + payload + "</second></root>";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.load_from_string(input.c_str()));
    const xml_node *original_root = source.get_root();
    cma_set_alloc_limit(2048);
    const int32_t status = destination.initialize(source);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, status);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, destination.get_error());
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    FT_ASSERT_EQ(ft_nullptr, destination._root);
    FT_ASSERT_EQ(original_root, source.get_root());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_STR_EQ(payload.c_str(), destination.get_root()->children[1]->text);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(ft_nullptr, destination.get_root());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize(source));
    FT_ASSERT_EQ(FT_CLASS_STATE_DESTROYED, destination._initialised_state);
    return (1);
}
