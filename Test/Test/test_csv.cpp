#include "../test_internal.hpp"

#include <cmath>
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/CSV/csv.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/Parser/document_backend.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/Basic/limits.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#include "../../Modules/Template/vector.hpp"

FT_TEST(test_csv_split_line_handles_quotes_and_empty_fields)
{
    ft_vector<ft_string> fields;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, csv_split_line(
        "name,\"Hello, \"\"world\"\"\",,tail", fields, ','));
    FT_ASSERT_EQ(static_cast<ft_size_t>(4), fields.size());
    FT_ASSERT_EQ(FT_TRUE, fields[0] == "name");
    FT_ASSERT_EQ(FT_TRUE, fields[1] == "Hello, \"world\"");
    FT_ASSERT_EQ(FT_TRUE, fields[2] == "");
    FT_ASSERT_EQ(FT_TRUE, fields[3] == "tail");
    return (1);
}

FT_TEST(test_csv_split_line_rejects_unclosed_quote)
{
    ft_vector<ft_string> fields;

    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, csv_split_line(
        "name,\"broken", fields, ','));
    return (1);
}

FT_TEST(test_csv_escape_field_quotes_when_needed)
{
    char *escaped_field;

    escaped_field = csv_escape_field("value,with,comma", ',');
    FT_ASSERT(escaped_field != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(escaped_field, "\"value,with,comma\""));
    cma_free(escaped_field);
    escaped_field = csv_escape_field("plain", ',');
    FT_ASSERT(escaped_field != ft_nullptr);
    FT_ASSERT_EQ(0, ft_strcmp(escaped_field, "plain"));
    cma_free(escaped_field);
    return (1);
}

FT_TEST(test_csv_document_round_trip_string)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name,quote\nAlice,\"Hello, \"\"world\"\"\"\nBob,42", ',', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), document.column_count(0));
    FT_ASSERT(document.get_header(0) != ft_nullptr);
    FT_ASSERT(document.get_field(1, 0) != ft_nullptr);
    FT_ASSERT(document.get_field(1, 1) != ft_nullptr);
    FT_ASSERT(document.get_field(2, 1) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_header(0) == "name");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 0) == "Alice");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 1) == "Hello, \"world\"");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(2, 1) == "42");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "name,quote\nAlice,\"Hello, \"\"world\"\"\"\nBob,42");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_reads_from_backend)
{
    ft_csv_document document;
    ft_memory_document_source source;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    source.set_data("id,score\n1,10\n2,20", 18);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(source, ',', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT(document.get_field(1, 0) != ft_nullptr);
    FT_ASSERT(document.get_field(2, 1) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 0) == "1");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(2, 1) == "20");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_writes_to_backend)
{
    ft_csv_document document;
    ft_memory_document_sink sink;
    ft_string storage;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name,score\nAlice,10\nBob,20", ',', FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    sink.set_storage(&storage);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_backend(sink));
    FT_ASSERT_EQ(FT_TRUE, storage == "name,score\nAlice,10\nBob,20");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_handles_empty_input)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize("", ',', FT_FALSE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), document.row_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_reads_from_file)
{
    ft_csv_document document;
    ft_string output;
    const char *file_path;
    int32_t delete_error_code;

    file_path = "test_csv_document_reads_from_file.csv";
    delete_error_code = file_delete(file_path);
    FT_ASSERT(delete_error_code == FT_ERR_SUCCESS
        || delete_error_code == FT_ERR_IO);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(
        file_path, "left;right\nA;B", 14));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize_from_file(
        file_path, ';', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(2), document.row_count());
    FT_ASSERT(document.get_field(1, 1) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 1) == "B");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "left;right\nA;B");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    delete_error_code = file_delete(file_path);
    FT_ASSERT(delete_error_code == FT_ERR_SUCCESS
        || delete_error_code == FT_ERR_IO);
    return (1);
}

FT_TEST(test_csv_document_handles_empty_file)
{
    ft_csv_document document;
    ft_string output;
    const char *file_path;
    int32_t delete_error_code;

    file_path = "test_csv_document_handles_empty_file.csv";
    delete_error_code = file_delete(file_path);
    FT_ASSERT(delete_error_code == FT_ERR_SUCCESS
        || delete_error_code == FT_ERR_IO);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_write_all(file_path, ft_nullptr, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize_from_file(file_path, ',', FT_FALSE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), document.row_count());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    delete_error_code = file_delete(file_path);
    FT_ASSERT(delete_error_code == FT_ERR_SUCCESS
        || delete_error_code == FT_ERR_IO);
    return (1);
}

FT_TEST(test_csv_document_rejects_missing_file)
{
    ft_csv_document document;
    const char *file_path;
    int32_t delete_error_code;

    file_path = "test_csv_document_rejects_missing_file.csv";
    delete_error_code = file_delete(file_path);
    FT_ASSERT(delete_error_code == FT_ERR_SUCCESS
        || delete_error_code == FT_ERR_IO);
    FT_ASSERT_EQ(FT_ERR_FILE_OPEN_FAILED, document.initialize_from_file(
        file_path, ',', FT_FALSE));
    return (1);
}

FT_TEST(test_csv_document_rejects_malformed_quotes)
{
    ft_csv_document document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, document.initialize(
        "name,quote\nAlice,\"broken\"x", ',', FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_rejects_unclosed_quote)
{
    ft_csv_document document;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize());
    FT_ASSERT_EQ(FT_ERR_INVALID_OPERATION, document.initialize(
        "name,quote\nAlice,\"broken", ',', FT_TRUE));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_handles_embedded_newlines)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name,notes\nAlice,\"first line\nsecond line\"\nBob,done", ',', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT(document.get_field(1, 1) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 1) == "first line\nsecond line");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "name,notes\nAlice,\"first line\nsecond line\"\nBob,done");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_handles_crlf_line_endings)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name,score\r\nAlice,10\r\nBob,20\r\n", ',', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT(document.get_field(1, 0) != ft_nullptr);
    FT_ASSERT(document.get_field(2, 1) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 0) == "Alice");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(2, 1) == "20");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "name,score\nAlice,10\nBob,20");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_handles_custom_delimiter)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name;score;note\nAlice;10;ready\nBob;20;queued", ';', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.column_count(0));
    FT_ASSERT(document.get_field(1, 2) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(1, 2) == "ready");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "name;score;note\nAlice;10;ready\nBob;20;queued");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_handles_tab_delimiter)
{
    ft_csv_document document;
    ft_string output;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name\tscore\tnote\nAlice\t10\tready\nBob\t20\tqueued", '\t', FT_TRUE));
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.row_count());
    FT_ASSERT_EQ(static_cast<ft_size_t>(3), document.column_count(0));
    FT_ASSERT(document.get_field(2, 2) != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field(2, 2) == "queued");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.write_to_string(output));
    FT_ASSERT_EQ(FT_TRUE, output == "name\tscore\tnote\nAlice\t10\tready\nBob\t20\tqueued");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}

FT_TEST(test_csv_document_supports_header_lookup_typed_access_and_row_iteration)
{
    ft_csv_document document;
    const ft_string *row_pointer;
    ft_size_t column_count;
    int64_t score_value;
    uint64_t count_value;
    double ratio_value;
    ft_bool enabled_value;

    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.initialize(
        "name,score,count,ratio,enabled\nAlice,42,9000,3.5,true\nBob,-7,12,2.25,off", ',', FT_TRUE));
    FT_ASSERT(document.get_field_by_name(1, "score") != ft_nullptr);
    FT_ASSERT_EQ(FT_TRUE, *document.get_field_by_name(1, "name") == "Alice");
    FT_ASSERT_EQ(FT_TRUE, *document.get_field_by_name(2, "name") == "Bob");
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_int64(1, "score", &score_value));
    FT_ASSERT_EQ(static_cast<int64_t>(42), score_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_uint64(1, "count", &count_value));
    FT_ASSERT_EQ(static_cast<uint64_t>(9000), count_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_double(1, "ratio", &ratio_value));
    FT_ASSERT(std::fabs(ratio_value - 3.5) < 0.000001);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_bool(1, "enabled", &enabled_value));
    FT_ASSERT_EQ(FT_TRUE, enabled_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_int64(2, 1, &score_value));
    FT_ASSERT_EQ(static_cast<int64_t>(-7), score_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_uint64(2, 2, &count_value));
    FT_ASSERT_EQ(static_cast<uint64_t>(12), count_value);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_double(2, 2, &ratio_value));
    FT_ASSERT(std::fabs(ratio_value - 12.0) < 0.000001);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_double(2, 3, &ratio_value));
    FT_ASSERT(std::fabs(ratio_value - 2.25) < 0.000001);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.get_bool(2, "enabled", &enabled_value));
    FT_ASSERT_EQ(FT_FALSE, enabled_value);
    row_pointer = document.get_row(2, &column_count);
    FT_ASSERT(row_pointer != ft_nullptr);
    FT_ASSERT_EQ(static_cast<ft_size_t>(5), column_count);
    FT_ASSERT_EQ(FT_TRUE, row_pointer[0] == "Bob");
    FT_ASSERT_EQ(FT_TRUE, row_pointer[1] == "-7");
    FT_ASSERT_EQ(FT_TRUE, row_pointer[2] == "12");
    FT_ASSERT_EQ(FT_TRUE, row_pointer[3] == "2.25");
    FT_ASSERT_EQ(FT_TRUE, row_pointer[4] == "off");
    FT_ASSERT(document.get_field_by_name(1, "missing") == ft_nullptr);
    FT_ASSERT_EQ(FT_ERR_NOT_FOUND, document.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, document.destroy());
    return (1);
}
