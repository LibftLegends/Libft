#include "../test_internal.hpp"
#include "../../Modules/Parser/dom.hpp"
#include "../../Modules/JSon/document.hpp"
#include "../../Modules/JSon/json_dom_bridge.hpp"
#include "../../Modules/JSon/json.hpp"
#include "../../Modules/CPP_class/class_big_number.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/XML/xml.hpp"
#include "../../Modules/XML/xml_dom_bridge.hpp"
#include "../../Modules/YAML/yaml.hpp"
#include "../../Modules/YAML/yaml_dom_bridge.hpp"
#include "../../Modules/CMA/CMA.hpp"

#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

#include <new>
#include <string>
#include <memory>

static void yaml_value_deleter(yaml_value *value)
{
    if (value != ft_nullptr)
        yaml_free(value);
    return ;
}

static void initialize_simple_dom_document(ft_dom_document &document)
{
    ft_dom_node *root_node;
    ft_dom_node *child_node;

    if (document.initialize() != FT_ERR_SUCCESS)
        return ;
    root_node = new(std::nothrow) ft_dom_node();
    if (!root_node)
        return ;
    if (root_node->initialize() != FT_ERR_SUCCESS)
    {
        delete root_node;
        return ;
    }
    root_node->set_type(FT_DOM_NODE_OBJECT);
    root_node->set_name("root");
    child_node = new(std::nothrow) ft_dom_node();
    if (!child_node)
    {
        delete root_node;
        return ;
    }
    if (child_node->initialize() != FT_ERR_SUCCESS)
    {
        delete child_node;
        delete root_node;
        return ;
    }
    child_node->set_type(FT_DOM_NODE_ELEMENT);
    child_node->set_name("child");
    root_node->add_child(child_node);
    document.set_root(root_node);
    return ;
}

FT_TEST(test_dom_find_path_locates_nested_nodes)
{
    ft_dom_document document;
    ft_dom_node *root_node;
    const ft_dom_node *found_node;
    ft_string child_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_path.initialize("child"));

    initialize_simple_dom_document(document);
    root_node = document.get_root();
    FT_ASSERT(root_node != ft_nullptr);
    found_node = ft_nullptr;
    FT_ASSERT_EQ(0, ft_dom_find_path(root_node, child_path, &found_node));
    FT_ASSERT(found_node != ft_nullptr);
    FT_ASSERT(std::string(found_node->get_name().c_str()) == "child");
    return (1);
}

FT_TEST(test_dom_find_path_reports_missing_segments)
{
    ft_dom_document document;
    ft_dom_node *root_node;
    const ft_dom_node *found_node;
    ft_string missing_path;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_path.initialize("missing"));

    initialize_simple_dom_document(document);
    root_node = document.get_root();
    FT_ASSERT(root_node != ft_nullptr);
    found_node = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, ft_dom_find_path(root_node, missing_path, &found_node));
    FT_ASSERT(found_node == ft_nullptr);
    return (1);
}

FT_TEST(test_dom_schema_reports_missing_required_nodes)
{
    ft_dom_document document;
    ft_dom_schema schema;
    ft_dom_validation_report report;

    initialize_simple_dom_document(document);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, schema.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, report.initialize());
    ft_string child_rule;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_rule.initialize("child"));
    ft_string missing_rule;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_rule.initialize("missing"));
    FT_ASSERT_EQ(0, schema.add_rule(child_rule, FT_DOM_NODE_ELEMENT, true));
    FT_ASSERT_EQ(0, schema.add_rule(missing_rule, FT_DOM_NODE_ELEMENT, true));
    FT_ASSERT_EQ(0, schema.validate(document, report));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_rule.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_rule.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, schema.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, report.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_root()->get_name().get_error());
    FT_ASSERT_EQ(false, report.valid());
    const ft_vector<ft_dom_validation_error> &errors = report.errors();
    FT_ASSERT_EQ(static_cast<size_t>(1), errors.size());
    const ft_dom_validation_error &error_entry = errors[0];
    FT_ASSERT(std::string(error_entry.path.c_str()) == "missing");
    FT_ASSERT(std::string(error_entry.message.c_str()) == "Required node missing");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, missing_rule.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_rule.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, report.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, schema.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_dom_schema_reports_type_mismatches)
{
    ft_dom_document document;
    ft_dom_schema schema;
    ft_dom_validation_report report;
    ft_dom_node *root_node;
    const ft_vector<ft_dom_node*> *children_pointer;
    ft_dom_node *child_node;

    initialize_simple_dom_document(document);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, schema.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, report.initialize());
    root_node = document.get_root();
    FT_ASSERT(root_node != ft_nullptr);
    children_pointer = &root_node->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(1), children_pointer->size());
    child_node = (*children_pointer)[0];
    FT_ASSERT(child_node != ft_nullptr);
    child_node->set_type(FT_DOM_NODE_VALUE);
    ft_string child_object_rule;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_object_rule.initialize("child"));
    FT_ASSERT_EQ(0, schema.add_rule(child_object_rule, FT_DOM_NODE_OBJECT, true));
    FT_ASSERT_EQ(0, schema.validate(document, report));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_object_rule.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, schema.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, report.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_node->get_name().get_error());
    FT_ASSERT_EQ(false, report.valid());
    const ft_vector<ft_dom_validation_error> &errors = report.errors();
    FT_ASSERT_EQ(static_cast<size_t>(1), errors.size());
    const ft_dom_validation_error &error_entry = errors[0];
    FT_ASSERT(std::string(error_entry.path.c_str()) == "child");
    FT_ASSERT(std::string(error_entry.message.c_str()) == "Node type mismatch");
    return (1);
}

FT_TEST(test_json_dom_bridge_round_trip)
{
    json_document source_document;
    json_group *settings_group;
    json_item *name_item;
    json_item *enabled_item;
    json_item *big_number_item;
    ft_big_number big_number_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, big_number_value.initialize());
    settings_group = source_document.create_group("settings");
    FT_ASSERT(settings_group != ft_nullptr);
    name_item = source_document.create_item("name", "example");
    FT_ASSERT(name_item != ft_nullptr);
    enabled_item = source_document.create_item("enabled", "true");
    FT_ASSERT(enabled_item != ft_nullptr);
    big_number_value.assign("12345678901234567890");
    big_number_item = source_document.create_item("count", big_number_value);
    FT_ASSERT(big_number_item != ft_nullptr);
    source_document.append_group(settings_group);
    source_document.add_item(settings_group, name_item);
    source_document.add_item(settings_group, enabled_item);
    source_document.add_item(settings_group, big_number_item);

    ft_dom_document dom_document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dom_document.initialize());
    FT_ASSERT_EQ(0, json_document_to_dom(source_document, dom_document));
    ft_dom_node *dom_root = dom_document.get_root();
    FT_ASSERT(dom_root != ft_nullptr);
    FT_ASSERT(std::string(dom_root->get_name().c_str()) == "json");
    const ft_vector<ft_dom_node*> &group_nodes = dom_root->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(1), group_nodes.size());
    ft_dom_node *group_node = group_nodes[0];
    FT_ASSERT(group_node != ft_nullptr);
    FT_ASSERT(std::string(group_node->get_name().c_str()) == "settings");
    const ft_vector<ft_dom_node*> &item_nodes = group_node->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(3), item_nodes.size());

    size_t index;
    size_t count;

    index = 0;
    count = item_nodes.size();
    bool found_big_number;

    found_big_number = false;
    while (index < count)
    {
        ft_dom_node *item_node = item_nodes[index];
        FT_ASSERT(item_node != ft_nullptr);
        if (std::string(item_node->get_name().c_str()) == "count")
        {
            ft_string attribute_key;
            FT_ASSERT_EQ(FT_ERR_SUCCESS, attribute_key.initialize("json:type"));
            ft_string *attribute_value = item_node->get_attribute(attribute_key);
            FT_ASSERT(attribute_value != ft_nullptr);
            FT_ASSERT(std::string(attribute_value->c_str()) == "big_number");
            FT_ASSERT(std::string(attribute_value->c_str()) == "big_number");
            delete attribute_value;
            found_big_number = true;
        }
        index += 1;
    }
    FT_ASSERT(found_big_number);

    json_document round_trip_document;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_document.initialize());
    FT_ASSERT_EQ(0, json_document_from_dom(dom_document, round_trip_document));
    json_group *round_trip_group = round_trip_document.find_group("settings");
    FT_ASSERT(round_trip_group != ft_nullptr);
    json_item *round_trip_name = round_trip_document.find_item(round_trip_group, "name");
    FT_ASSERT(round_trip_name != ft_nullptr);
    FT_ASSERT(std::string(round_trip_name->value) == "example");
    json_item *round_trip_count = round_trip_document.find_item(round_trip_group, "count");
    FT_ASSERT(round_trip_count != ft_nullptr);
    FT_ASSERT(round_trip_count->is_big_number);
    FT_ASSERT(round_trip_count->big_number != ft_nullptr);
    ft_string *round_trip_count_string = round_trip_count->big_number->to_string_base(10);
    FT_ASSERT(round_trip_count_string != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_count_string->get_error());
    FT_ASSERT(std::string(round_trip_count_string->c_str()) == "12345678901234567890");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_count_string->destroy());
    delete round_trip_count_string;
    return (1);
}

FT_TEST(test_json_dom_bridge_rejects_non_object_root)
{
    ft_dom_document dom_document;
    ft_dom_node *root_node;
    json_document document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, dom_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.read_from_string("{\"old\":{\"item\":\"kept\"}}"));
    root_node = new(std::nothrow) ft_dom_node();
    if (!root_node)
        return (0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, root_node->initialize());
    root_node->set_type(FT_DOM_NODE_VALUE);
    if (root_node->set_name("root") != 0)
    {
        delete root_node;
        return (0);
    }
    if (root_node->set_value("content") != 0)
    {
        delete root_node;
        return (0);
    }
    dom_document.set_root(root_node);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, json_document_from_dom(dom_document, document));
    FT_ASSERT_EQ(0, ft_strcmp(document.get_value_by_pointer("/old/item"), "kept"));
    return (1);
}

FT_TEST(test_json_dom_bridge_keeps_negative_numbers_and_failed_conversion_destinations)
{
    json_document source;
    json_document destination;
    ft_dom_document dom;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dom.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.read_from_string(
        "{\"sample\":{\"first\":\"value\",\"number\":-18446744073709551617},\"empty\":{}}"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.read_from_string("{\"old\":{\"item\":\"kept\"}}"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_document_to_dom(source, dom));
    const ft_dom_node *previous_root = dom.get_root();
    FT_ASSERT(previous_root != ft_nullptr);
    const ft_dom_node *first_group = previous_root->get_children()[0];
    const ft_dom_node *number = first_group->get_children()[1];
    FT_ASSERT_EQ(0, ft_strcmp(number->get_value().c_str(), "-18446744073709551617"));
    cma_set_alloc_limit(1U);
    const int32_t export_error = json_document_to_dom(source, dom);
    const int32_t import_error = json_document_from_dom(dom, destination);
    cma_set_alloc_limit(0U);
    FT_ASSERT(export_error != FT_ERR_SUCCESS);
    FT_ASSERT(import_error != FT_ERR_SUCCESS);
    FT_ASSERT_EQ(previous_root, dom.get_root());
    FT_ASSERT_EQ(0, ft_strcmp(destination.get_value_by_pointer("/old/item"), "kept"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, json_document_from_dom(dom, destination));
    const json_group *group = destination.get_groups();
    FT_ASSERT(group != ft_nullptr && group->next != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(group->items->key, "first"));
    FT_ASSERT_EQ(0, ft_strcmp(group->next->name, "empty"));
    FT_ASSERT(group->next->items == ft_nullptr);
    const json_item *copied = destination.find_item_by_pointer("/sample/number");
    FT_ASSERT(copied != ft_nullptr && copied->is_big_number);
    FT_ASSERT_EQ(FT_TRUE, copied->big_number->is_negative());
    FT_ASSERT_EQ(0, ft_strcmp(copied->value, "-18446744073709551617"));
    return (1);
}

FT_TEST(test_xml_dom_bridge_round_trip)
{
    xml_document source_document;
    ft_dom_document dom_document;
    xml_document round_trip_document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dom_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source_document.load_from_string("<root attr=\"value\"><child>text</child></root>"));
    FT_ASSERT_EQ(0, xml_document_to_dom(source_document, dom_document));
    ft_dom_node *dom_root = dom_document.get_root();
    FT_ASSERT(dom_root != ft_nullptr);
    FT_ASSERT(std::string(dom_root->get_name().c_str()) == "root");
    ft_string attribute_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, attribute_key.initialize("attr"));
    ft_string *attribute_value = dom_root->get_attribute(attribute_key);
    FT_ASSERT(attribute_value != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, attribute_value->get_error());
    FT_ASSERT(std::string(attribute_value->c_str()) == "value");
    delete attribute_value;
    const ft_vector<ft_dom_node*> &child_nodes = dom_root->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(1), child_nodes.size());
    ft_dom_node *child_node = child_nodes[0];
    FT_ASSERT(child_node != ft_nullptr);
    FT_ASSERT(std::string(child_node->get_name().c_str()) == "child");
    ft_string child_value;
    child_value = child_node->get_value();
    FT_ASSERT_EQ(FT_ERR_SUCCESS, child_value.get_error());
    FT_ASSERT(std::string(child_value.c_str()) == "text");

    FT_ASSERT_EQ(0, xml_document_from_dom(dom_document, round_trip_document));
    char *serialized = round_trip_document.write_to_string();
    FT_ASSERT(serialized != ft_nullptr);
    bool matches_expected;

    matches_expected = std::string(serialized).find("<child>text</child>") != std::string::npos;
    cma_free(serialized);
    FT_ASSERT(matches_expected);
    return (1);
}

FT_TEST(test_yaml_dom_bridge_round_trip)
{
    std::unique_ptr<yaml_value, void (*)(yaml_value *)> root_guard(ft_nullptr, &yaml_value_deleter);
    std::unique_ptr<yaml_value> items_guard;
    std::unique_ptr<yaml_value> first_value_guard;
    std::unique_ptr<yaml_value> second_value_guard;
    yaml_value *root_pointer;
    yaml_value *items_pointer;
    ft_dom_document dom_document;
    ft_dom_node *dom_root;
    ft_dom_node *items_node;
    ft_dom_node *first_item_node;
    std::unique_ptr<yaml_value, void (*)(yaml_value *)> round_trip_guard(ft_nullptr, &yaml_value_deleter);
    yaml_value *round_trip_pointer;
    yaml_value *round_trip_items;
    yaml_value *round_trip_first_value;
    yaml_value *round_trip_second_value;
    ft_string items_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, dom_document.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, items_key.initialize("items"));

    root_guard.reset(new (std::nothrow) yaml_value());
    if (!root_guard.get())
        return (0);
    root_pointer = root_guard.get();
    if (root_pointer->initialize() != FT_ERR_SUCCESS)
        return (0);
    root_pointer->set_type(YAML_MAP);
    items_guard.reset(new (std::nothrow) yaml_value());
    if (!items_guard.get())
        return (0);
    items_pointer = items_guard.get();
    if (items_pointer->initialize() != FT_ERR_SUCCESS)
        return (0);
    items_pointer->set_type(YAML_LIST);
    first_value_guard.reset(new (std::nothrow) yaml_value());
    if (!first_value_guard.get())
        return (0);
    if (first_value_guard->initialize() != FT_ERR_SUCCESS)
        return (0);
    ft_string scalar_one;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scalar_one.initialize("one"));
    first_value_guard->set_scalar(scalar_one);
    items_pointer->add_list_item(first_value_guard.get());
    first_value_guard.release();
    second_value_guard.reset(new (std::nothrow) yaml_value());
    if (!second_value_guard.get())
        return (0);
    if (second_value_guard->initialize() != FT_ERR_SUCCESS)
        return (0);
    ft_string scalar_two;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, scalar_two.initialize("two"));
    second_value_guard->set_scalar(scalar_two);
    items_pointer->add_list_item(second_value_guard.get());
    second_value_guard.release();
    root_pointer->add_map_item(items_key, items_pointer);
    items_guard.release();
    FT_ASSERT_EQ(0, yaml_value_to_dom(root_pointer, dom_document));
    dom_root = dom_document.get_root();
    FT_ASSERT(dom_root != ft_nullptr);
    FT_ASSERT_EQ(FT_DOM_NODE_OBJECT, dom_root->get_type());
    const ft_vector<ft_dom_node*> &root_children = dom_root->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(1), root_children.size());
    items_node = root_children[0];
    FT_ASSERT(items_node != ft_nullptr);
    FT_ASSERT(std::string(items_node->get_name().c_str()) == "items");
    FT_ASSERT_EQ(FT_DOM_NODE_ARRAY, items_node->get_type());
    const ft_vector<ft_dom_node*> &item_children = items_node->get_children();
    FT_ASSERT_EQ(static_cast<size_t>(2), item_children.size());
    first_item_node = item_children[0];
    FT_ASSERT(first_item_node != ft_nullptr);
    FT_ASSERT(std::string(first_item_node->get_value().c_str()) == "one");
    round_trip_guard.reset(yaml_value_from_dom(dom_document));
    FT_ASSERT(round_trip_guard.get() != ft_nullptr);
    round_trip_pointer = round_trip_guard.get();
    FT_ASSERT_EQ(YAML_MAP, round_trip_pointer->get_type());
    const ft_map<ft_string, yaml_value*> &round_trip_map = round_trip_pointer->get_map();
    round_trip_items = round_trip_map.at(items_key);
    FT_ASSERT(round_trip_items != ft_nullptr);
    FT_ASSERT_EQ(YAML_LIST, round_trip_items->get_type());
    const ft_vector<yaml_value*> &round_trip_list = round_trip_items->get_list();
    FT_ASSERT_EQ(static_cast<size_t>(2), round_trip_list.size());
    round_trip_first_value = round_trip_list[0];
    FT_ASSERT(round_trip_first_value != ft_nullptr);
    FT_ASSERT(std::string(round_trip_first_value->get_scalar().c_str()) == "one");
    round_trip_second_value = round_trip_list[1];
    FT_ASSERT(round_trip_second_value != ft_nullptr);
    FT_ASSERT(std::string(round_trip_second_value->get_scalar().c_str()) == "two");
    return (1);
}
