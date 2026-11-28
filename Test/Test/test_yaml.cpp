#include "../test_internal.hpp"
#include "../../Modules/YAML/yaml.hpp"
#include "../../Modules/CPP_class/class_string.hpp"
#include "../../Modules/Basic/basic.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include <cerrno>
#include <memory>
#include <new>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/pair.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_yaml_reader_slices_keep_embedded_nuls_and_clamp_ranges)
{
    const char bytes[] = {'a', '\0', 'b', '\n', '\n', 'c', '\r', '\n'};
    ft_string input;
    ft_string prefix;
    ft_vector<ft_string> lines;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.assign(bytes, sizeof(bytes)));
    FT_ASSERT_EQ(1U, yaml_find_char(input, '\0'));
    FT_ASSERT_EQ(3U, yaml_find_char(input, '\n'));
    FT_ASSERT_EQ(FT_SYSTEM_SIZE_MAX, yaml_find_char(input, 'z'));

    const std::unique_ptr<ft_string> slice(yaml_substr(input, 1, FT_SYSTEM_SIZE_MAX));
    const std::unique_ptr<ft_string> empty(yaml_substr(input, FT_SYSTEM_SIZE_MAX, 1));
    const std::unique_ptr<ft_string> at_end(yaml_substr_from(input, sizeof(bytes)));
    FT_ASSERT(slice.get() != ft_nullptr);
    FT_ASSERT_EQ(sizeof(bytes) - 1, slice->size());
    FT_ASSERT_EQ(0, ft_memcmp(bytes + 1, slice->c_str(), slice->size()));
    FT_ASSERT(empty.get() != ft_nullptr);
    FT_ASSERT_EQ(0U, empty->size());
    FT_ASSERT(at_end.get() != ft_nullptr);
    FT_ASSERT_EQ(0U, at_end->size());
    FT_ASSERT(yaml_substr_from(input, sizeof(bytes) + 1) == ft_nullptr);

    FT_ASSERT_EQ(FT_ERR_SUCCESS, prefix.initialize("prefix"));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, lines.push_back(prefix));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, yaml_split_lines(input, lines));
    FT_ASSERT_EQ(4U, lines.size());
    FT_ASSERT_EQ(0, ft_strcmp(lines[0].c_str(), "prefix"));
    FT_ASSERT_EQ(3U, lines[1].size());
    FT_ASSERT_EQ(0, ft_memcmp(lines[1].c_str(), bytes, 3));
    FT_ASSERT_EQ(0U, lines[2].size());
    FT_ASSERT_EQ(0, ft_strcmp(lines[3].c_str(), "c\r"));
    return (1);
}

FT_TEST(test_yaml_reader_trims_without_allocating)
{
    const char bytes[] = {' ', '\t', 'a', '\0', 'b', '\r', ' '};
    ft_string input;
    ft_string whitespace;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.assign(bytes, sizeof(bytes)));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, whitespace.initialize(" \t\r\n"));

    cma_set_alloc_limit(1);
    yaml_trim(input);
    yaml_trim(whitespace);
    cma_set_alloc_limit(0);

    FT_ASSERT_EQ(3U, input.size());
    FT_ASSERT_EQ(0, ft_memcmp(bytes + 2, input.c_str(), input.size()));
    FT_ASSERT_EQ(0U, whitespace.size());
    FT_ASSERT_EQ('\0', whitespace.c_str()[0]);
    return (1);
}

FT_TEST(test_yaml_round_trip)
{
    yaml_value root;
    std::unique_ptr<yaml_value> name_value_guard;
    std::unique_ptr<yaml_value> role_value_guard;
    ft_string *yaml_string;
    ft_string *round_trip_string;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, root.initialize());
    root.set_type(YAML_MAP);
    name_value_guard.reset(new (std::nothrow) yaml_value());
    FT_ASSERT(name_value_guard.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name_value_guard->initialize());
    ft_string name_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, name_key.initialize("name"));
    ft_string alice_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, alice_value.initialize("Alice"));
    name_value_guard->set_scalar(alice_value);
    root.add_map_item(name_key, name_value_guard.get());
    name_value_guard.release();

    role_value_guard.reset(new (std::nothrow) yaml_value());
    FT_ASSERT(role_value_guard.get() != ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, role_value_guard->initialize());
    ft_string role_key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, role_key.initialize("role"));
    ft_string role_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, role_value.initialize("ranger"));
    role_value_guard->set_scalar(role_value);
    root.add_map_item(role_key, role_value_guard.get());
    role_value_guard.release();

    yaml_string = yaml_write_to_string(&root);
    FT_ASSERT(yaml_string != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, yaml_string->is_initialised());

    round_trip_string = yaml_write_to_string(&root);
    FT_ASSERT(round_trip_string != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, round_trip_string->is_initialised());
    FT_ASSERT_EQ(0, ft_strcmp(yaml_string->c_str(), round_trip_string->c_str()));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, yaml_string->destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, round_trip_string->destroy());
    delete yaml_string;
    delete round_trip_string;
    return (1);
}

FT_TEST(test_yaml_write_to_file_reports_write_failure)
{
    yaml_value scalar;

    if (scalar.initialize() != FT_ERR_SUCCESS)
        return (0);
    ft_string content_value;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, content_value.initialize("content"));
    scalar.set_scalar(content_value);
    FT_ASSERT_EQ(FT_ERR_IO, yaml_write_to_file("/dev/full", &scalar));
    return (1);
}

FT_TEST(test_yaml_value_thread_safety_guard)
{
    yaml_value value;

    if (value.initialize() != FT_ERR_SUCCESS)
        return (0);
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, value.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, value.disable_thread_safety());
    FT_ASSERT_EQ(FT_FALSE, value.is_thread_safe());
    return (1);
}

FT_TEST(test_yaml_writers_preserve_order_and_previous_output_on_failure)
{
    ft_string input;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize("name: value\nitems:\n  - one\n  - two\n"));
    const std::unique_ptr<yaml_value> root(yaml_read_from_string(input));
    FT_ASSERT(root.get() != ft_nullptr);
    ft_string key;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, key.initialize("duplicate"));
    root->add_map_item(key, ft_nullptr);
    root->add_map_item(key, ft_nullptr);
    constexpr char EXPECTED[] = "name: value\nitems:\n  - one\n  - two\nduplicate:\nduplicate:\n";

    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("previous"));
    ft_memory_document_sink sink;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    sink.set_storage(&output);
    cma_set_alloc_limit(1);
    const int32_t string_error = yaml_write_to_string(root.get(), output);
    const int32_t backend_error = yaml_write_to_backend(sink, root.get());
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, string_error);
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, backend_error);
    FT_ASSERT_STR_EQ("previous", output.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, yaml_write_to_string(root.get(), output));
    FT_ASSERT_STR_EQ(EXPECTED, output.c_str());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, yaml_write_to_backend(sink, root.get()));
    FT_ASSERT_STR_EQ(EXPECTED, output.c_str());
    return (1);
}
