#include "../test_internal.hpp"
#include "../../Modules/XML/xml.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <cstring>
#include <string>

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_xml_document_initialize_and_thread_guard)
{
    xml_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_FALSE, document.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.disable_thread_safety());
    FT_ASSERT_EQ(false, document.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, document.is_thread_safe());
    return (1);
}

FT_TEST(test_xml_document_initialize_after_destroy_succeeds)
{
    xml_document document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    return (1);
}

FT_TEST(test_xml_document_write_to_string_empty_document_fails)
{
    xml_document document;
    char *result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    result = document.write_to_string();
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_xml_document_write_to_string_allocation_failure)
{
    xml_document document;
    char *result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.load_from_string("<root/>"));
    cma_set_alloc_limit(1);
    result = document.write_to_string();
    cma_set_alloc_limit(0);
    FT_ASSERT(result == ft_nullptr);
    return (1);
}

FT_TEST(test_xml_document_write_to_file_failures)
{
    xml_document document;
    int result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.load_from_string("<root/>"));
    result = document.write_to_file("Test");
    FT_ASSERT(result != FT_ERR_SUCCESS);
    result = document.write_to_file("/dev/full");
    FT_ASSERT(result != FT_ERR_SUCCESS);
    return (1);
}

FT_TEST(test_xml_document_load_from_string_invalid_inputs)
{
    xml_document document;
    int result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    result = document.load_from_string("<root>");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    result = document.load_from_string("<root><child></rood>");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    result = document.load_from_string("<root/>extra");
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, result);
    FT_ASSERT(document.get_root() == ft_nullptr);
    return (1);
}

FT_TEST(test_xml_document_load_from_string_allocation_failure)
{
    xml_document document;
    int result;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    cma_set_alloc_limit(1);
    result = document.load_from_string("<root><child/></root>");
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, result);
    return (1);
}

FT_TEST(test_xml_document_load_from_string_success_and_roundtrip)
{
    xml_document document;
    const char *xml_input;
    xml_node *root_node;
    char *written_string;
    bool attribute_found;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    xml_input = "<root attr=\"a>b\">payload</root>";
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.load_from_string(xml_input));
    root_node = document.get_root();
    FT_ASSERT(root_node != ft_nullptr);
    FT_ASSERT(root_node->text != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(root_node->text, "payload"));
    ft_unordered_map<char*, char*>::iterator attribute_iterator = root_node->attributes.begin();
    ft_unordered_map<char*, char*>::iterator attribute_end = root_node->attributes.end();
    attribute_found = false;
    while (attribute_iterator != attribute_end)
    {
        if (attribute_iterator->first
            && std::strcmp(attribute_iterator->first, "attr") == 0)
        {
            attribute_found = true;
            FT_ASSERT(attribute_iterator->second != ft_nullptr);
            FT_ASSERT_EQ(0, std::strcmp(attribute_iterator->second, "a>b"));
            break ;
        }
        ++attribute_iterator;
    }
    FT_ASSERT(attribute_found);
    written_string = document.write_to_string();
    FT_ASSERT(written_string != ft_nullptr);
    FT_ASSERT_EQ(0, std::strcmp(written_string, "<root>payload</root>\n"));
    cma_free(written_string);
    return (1);
}

FT_TEST(test_xml_document_writers_preserve_bytes_and_fail_atomically_on_buffer_growth)
{
    const char *path = "test-xml-large-writer.xml";
    std::string payload(20000, 'x');
    const std::string input = "<p:root xmlns:p=\"urn:test\"><p:child>" + payload
        + "</p:child><p:child>tail</p:child></p:root>";
    const std::string expected = "<p:root><p:child>" + payload
        + "</p:child><p:child>tail</p:child></p:root>\n";
    xml_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.load_from_string(input.c_str()));

    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("previous"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, "previous", sizeof("previous") - 1));
    ft_memory_document_sink sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    sink.set_storage(&output);
    cma_set_alloc_limit(128);
    char *raw_failure = document.write_to_string();
    const int32_t raw_error = document.get_error();
    const int32_t output_error = document.write_to_string(output);
    const int32_t backend_error = document.write_to_backend(sink);
    const int32_t file_error = document.write_to_file(path);
    cma_set_alloc_limit(0);
    if (raw_failure)
    {
        cma_free(raw_failure);
        FT_ASSERT(raw_failure == ft_nullptr);
    }
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, raw_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, output_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, backend_error);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, file_error);
    FT_ASSERT_STR_EQ("previous", output.c_str());
    ft_string disk;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, disk));
    FT_ASSERT_STR_EQ("previous", disk.c_str());

    char *raw = document.write_to_string();
    FT_ASSERT(raw != ft_nullptr);
    const ft_bool raw_matches = expected == raw;
    cma_free(raw);
    FT_ASSERT_EQ(FT_TRUE, raw_matches);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_backend(sink));
    FT_ASSERT_EQ(expected, std::string(output.c_str(), output.size()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_file(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_read_all(path, disk));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(expected, std::string(disk.c_str(), disk.size()));

    ft_string direct;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(direct));
    FT_ASSERT_EQ(expected, std::string(direct.c_str(), direct.size()));
    return (1);
}

FT_TEST(test_xml_document_loads_long_file_spans_and_rejects_invalid_input)
{
    const char *path = "test-xml-load-spans.xml";
    const std::string attribute = std::string(4096, 'a') + ">\"end";
    const std::string text = std::string(8192, 't') + "&lt;/tag&gt;";
    const std::string input = "<p:root xmlns:p='urn:test' data='" + attribute
        + "'><p:child>" + text + "</p:child></p:root>";
    xml_document document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, input.c_str(), input.size()));
    const int32_t status = document.load_from_file(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, status);
    xml_node *root = document.get_root();
    FT_ASSERT(root != ft_nullptr);
    FT_ASSERT_STR_EQ("p:root", root->name);
    FT_ASSERT_STR_EQ("root", root->local_name);
    FT_ASSERT_STR_EQ("urn:test", root->namespace_uri);
    FT_ASSERT_EQ(static_cast<ft_size_t>(1), root->children.size());
    FT_ASSERT_STR_EQ("p:child", root->children[0]->name);
    FT_ASSERT_STR_EQ(text.c_str(), root->children[0]->text);
    ft_bool attribute_matches = FT_FALSE;
    auto attribute_iterator = root->attributes.begin();
    const auto attribute_end = root->attributes.end();
    while (attribute_iterator != attribute_end)
    {
        if (std::strcmp(attribute_iterator->first, "data") == 0)
            attribute_matches = attribute == attribute_iterator->second;
        ++attribute_iterator;
    }
    FT_ASSERT_EQ(FT_TRUE, attribute_matches);

    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.load_from_file(ft_nullptr));
    FT_ASSERT_EQ(root, document.get_root());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(path, "", 0));
    const int32_t empty_status = document.load_from_file(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, empty_status);
    FT_ASSERT_EQ(root, document.get_root());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.load_from_string("<root>truncated<"));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, document.load_from_string("<p:root xmlns:p='urn:test'></p:roux>"));
    return (1);
}

FT_TEST(test_xml_node_thread_safety_root_locking)
{
    xml_document document;
    xml_node *root_node;
    ft_bool lock_acquired;
    int lock_status;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.load_from_string("<root><child/></root>"));
    root_node = document.get_root();
    FT_ASSERT(root_node != ft_nullptr);
    FT_ASSERT(root_node->mutex != ft_nullptr);
    lock_acquired = FT_FALSE;
    lock_status = xml_node_lock(root_node, &lock_acquired);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lock_status);
    FT_ASSERT_EQ(FT_TRUE, lock_acquired);
    xml_node_unlock(root_node, lock_acquired);
    return (1);
}

FT_TEST(test_xml_node_thread_safety_rejects_null_nodes)
{
    ft_bool lock_acquired;
    int lock_status;

    lock_acquired = FT_TRUE;
    lock_status = xml_node_lock(ft_nullptr, &lock_acquired);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, lock_status);
    FT_ASSERT_EQ(FT_FALSE, lock_acquired);
    xml_node_unlock(ft_nullptr, FT_TRUE);
    return (1);
}
