#include "../test_internal.hpp"
#include "../../Modules/Parser/document_backend.hpp"
#include "../../Modules/Errno/errno.hpp"
#include "../../Modules/CMA/CMA.hpp"
#include "../../Modules/File/file_utils.hpp"
#include "../../Modules/System_utils/test_system_utils_runner.hpp"
#include <string>

#include "../../Modules/Basic/class_nullptr.hpp"
#include "../../Modules/PThread/mutex.hpp"
#include "../../Modules/PThread/recursive_mutex.hpp"
#ifndef LIBFT_TEST_BUILD
#endif

FT_TEST(test_parser_memory_document_source_read_all_success)
{
    ft_memory_document_source source;
    ft_string output;
    static const char input_data[] = "memory-data";

    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    source.set_data(input_data, sizeof(input_data) - 1);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.read_all(output));
    FT_ASSERT_EQ(0, ft_strcmp(output.c_str(), input_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.destroy());
    return (1);
}

FT_TEST(test_parser_memory_document_sink_write_all_success)
{
    ft_memory_document_sink sink;
    ft_string storage;
    static const char input_data[] = "stored-data";

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.initialize());
    sink.set_storage(&storage);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.write_all(input_data, sizeof(input_data) - 1));
    FT_ASSERT_EQ(0, ft_strcmp(storage.c_str(), input_data));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.get_error());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.destroy());
    return (1);
}

FT_TEST(test_parser_document_backends_share_lifecycle_and_lock_through_base_references)
{
    ft_memory_document_source source;
    ft_memory_document_sink sink;
    ft_document_source &input = source;
    ft_document_sink &output = sink;
    ft_string storage;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.enable_thread_safety());
    FT_ASSERT_EQ(FT_TRUE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_TRUE, output.is_thread_safe());
    ft_bool locked = FT_FALSE;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.lock(&locked));
    FT_ASSERT_EQ(FT_TRUE, locked);
    source.set_data("shared", sizeof("shared") - 1);
    source.unlock(locked);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.read_all(storage));
    sink.set_storage(&storage);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.write_all("value", sizeof("value") - 1));
    FT_ASSERT_STR_EQ("value", storage.c_str());

    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.destroy());
    FT_ASSERT_EQ(FT_FALSE, source.is_thread_safe());
    FT_ASSERT_EQ(FT_FALSE, sink.is_thread_safe());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), source.get_length());
    FT_ASSERT_EQ(ft_nullptr, sink.get_storage());
    return (1);
}

FT_TEST(test_parser_file_document_backend_adopts_binary_data_and_preserves_output_on_failure)
{
    const char *path = "test-parser-document-binary.tmp";
    std::string payload(4096, 'x');
    payload[3] = '\0';
    payload[4] = '\r';
    payload[5] = '\n';
    ft_file_document_source source;
    ft_file_document_sink sink;
    ft_document_source &input = source;
    ft_document_sink &destination = sink;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.enable_thread_safety());
    FT_ASSERT_STR_EQ("", source.get_path());
    FT_ASSERT_STR_EQ("", sink.get_path());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, output.initialize("previous"));
    source.set_path(path);
    sink.set_path(path);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.write_all(payload.c_str(), payload.size()));

    cma_set_alloc_limit(128);
    const int32_t allocation_error = source.read_all(output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_NO_MEMORY, allocation_error);
    FT_ASSERT_STR_EQ("previous", output.c_str());
    cma_set_alloc_limit(payload.size() + 1);
    const int32_t read_error = source.read_all(output);
    cma_set_alloc_limit(0);
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_error);
    FT_ASSERT_EQ(payload, std::string(output.c_str(), output.size()));

    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.write_all(ft_nullptr, 0));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.read_all(output));
    FT_ASSERT_EQ(static_cast<ft_size_t>(0), output.size());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, file_delete(path));
    FT_ASSERT_EQ(FT_ERR_IO, source.read_all(output));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_STR_EQ("", source.get_path());
    FT_ASSERT_STR_EQ("", sink.get_path());
    return (1);
}

FT_TEST(test_parser_memory_backends_preserve_binary_storage_on_failure)
{
    ft_memory_document_source source;
    ft_memory_document_sink sink;
    ft_string storage;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, storage.initialize("previous"));
    const char binary[] = {'a', '\0', 'b'};
    source.set_data(binary, sizeof(binary));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.read_all(storage));
    sink.set_storage(&storage);
    const std::string expected(binary, sizeof(binary));
    FT_ASSERT_EQ(expected, std::string(storage.c_str(), storage.size()));
    source.set_data(ft_nullptr, 1);
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, source.read_all(storage));
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sink.write_all(ft_nullptr, 1));
    FT_ASSERT_EQ(expected, std::string(storage.c_str(), storage.size()));
    return (1);
}

#if NETWORKING_HAS_OPENSSL
FT_TEST(test_parser_http_backend_configuration_preserves_previous_endpoint_on_failure)
{
    ft_http_document_source source;
    ft_http_document_sink sink;
    ft_document_source &input = source;
    ft_document_sink &destination = sink;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.initialize());
    FT_ASSERT_STR_EQ("", source.get_host());
    FT_ASSERT_EQ(ft_nullptr, sink.get_port());
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, source.read_all(output));
    FT_ASSERT_EQ(FT_ERR_INVALID_STATE, sink.write_all(ft_nullptr, 0));
    source.configure("example.test", "/source", FT_TRUE, "8443");
    sink.configure("example.test", "/sink", FT_TRUE, "8443");
    const std::string long_path(1024, 'x');
    cma_set_alloc_limit(128);
    source.configure("changed.test", long_path.c_str(), FT_FALSE, ft_nullptr);
    sink.configure("changed.test", long_path.c_str(), FT_FALSE, ft_nullptr);
    cma_set_alloc_limit(0);
    FT_ASSERT_STR_EQ("example.test", source.get_host());
    FT_ASSERT_STR_EQ("/source", source.get_path());
    FT_ASSERT_STR_EQ("/sink", sink.get_path());
    FT_ASSERT_STR_EQ("8443", sink.get_port());
    FT_ASSERT_EQ(FT_TRUE, source.is_ssl_enabled());
    FT_ASSERT_EQ(FT_TRUE, sink.is_ssl_enabled());
    FT_ASSERT_EQ(FT_ERR_INVALID_ARGUMENT, sink.write_all(ft_nullptr, 1));
    FT_ASSERT_EQ(FT_ERR_SUCCESS, input.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, destination.destroy());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_STR_EQ("", source.get_host());
    FT_ASSERT_STR_EQ("", sink.get_path());
    return (1);
}
#endif

#if NETWORKING_HAS_OPENSSL && defined(LIBFT_PARSER_HTTP_TRANSPORT_TEST)
static ft_http_document_source *source_to_reconfigure;
static const char *response_storage;
static const char http_test_body[] = {'a', '\0', 'b'};

int32_t http_get_body(const char *host, const char *path, ft_string &response,
    ft_bool use_ssl, const char *port)
{
    if (ft_strcmp(host, "example.test") != 0 || ft_strcmp(port, "8443") != 0 || !use_ssl)
        return (FT_ERR_INVALID_STATE);
    if (ft_strcmp(path, "/failure") == 0)
    {
        (void)response.assign("partial", sizeof("partial") - 1);
        return (FT_ERR_IO);
    }
    if (source_to_reconfigure)
        source_to_reconfigure->configure("changed.test", "/changed", FT_FALSE, ft_nullptr);
    if (ft_strcmp(host, "example.test") != 0 || ft_strcmp(path, "/source") != 0
        || ft_strcmp(port, "8443") != 0)
        return (FT_ERR_INVALID_STATE);
    const int32_t status = response.assign(http_test_body, sizeof(http_test_body));
    response_storage = response.c_str();
    return (status);
}

int32_t http_post_body(const char *host, const char *path, const char *body, ft_size_t body_length,
    ft_bool use_ssl, const char *port)
{
    if (ft_strcmp(host, "example.test") != 0 || ft_strcmp(path, "/sink") != 0
        || use_ssl || port != ft_nullptr || body_length != sizeof(http_test_body)
        || ft_memcmp(body, http_test_body, sizeof(http_test_body)) != 0)
        return (FT_ERR_INVALID_STATE);
    return (FT_ERR_SUCCESS);
}

FT_TEST(test_parser_http_backend_transfers_binary_payloads_through_transport_boundary)
{
    ft_http_document_source source;
    ft_http_document_sink sink;
    ft_string output;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.initialize());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, source.enable_thread_safety());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.enable_thread_safety());
    source.configure("example.test", "/source", FT_TRUE, "8443");
    sink.configure("example.test", "/sink", FT_FALSE, ft_nullptr);
    source_to_reconfigure = &source;
    const int32_t read_status = source.read_all(output);
    source_to_reconfigure = ft_nullptr;
    FT_ASSERT_EQ(FT_ERR_SUCCESS, read_status);
    FT_ASSERT_EQ(response_storage, output.c_str());
    FT_ASSERT_EQ(static_cast<ft_size_t>(sizeof(http_test_body)), output.size());
    FT_ASSERT_EQ(0, ft_memcmp(output.c_str(), http_test_body, output.size()));
    FT_ASSERT_STR_EQ("changed.test", source.get_host());
    FT_ASSERT_EQ(FT_ERR_SUCCESS, sink.write_all(output.c_str(), output.size()));

    source.configure("example.test", "/failure", FT_TRUE, "8443");
    FT_ASSERT_EQ(FT_ERR_IO, source.read_all(output));
    FT_ASSERT_EQ(response_storage, output.c_str());
    FT_ASSERT_EQ(0, ft_memcmp(output.c_str(), http_test_body, output.size()));
    return (1);
}
#endif
