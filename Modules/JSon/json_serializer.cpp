#include "json_internal.hpp"
#include "document.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

int32_t json_serialize_to_string(json_serialize_callback serialize_callback,
    void *user_data, ft_string &output) noexcept
{
    json_document document;
    char *serialized_content;
    ft_size_t serialized_length;
    int32_t operation_error;

    if (serialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = serialize_callback(document, user_data);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = json_document_serialize_buffer(document,
        serialized_content, serialized_length);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = ft_string_take_owned_buffer(output, serialized_content, serialized_length);
    if (operation_error != FT_ERR_SUCCESS)
        cma_free(serialized_content);
    return (operation_error);
}

int32_t json_serialize_to_backend(json_serialize_callback serialize_callback,
    void *user_data, ft_document_sink &sink) noexcept
{
    json_document document;
    int32_t operation_error;

    if (serialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = serialize_callback(document, user_data);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = document.write_to_backend(sink);
    return (operation_error);
}

int32_t json_deserialize_from_string(const char *content,
    json_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    json_document document;
    int32_t operation_error;

    if (content == ft_nullptr || deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = document.read_from_string(content);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = deserialize_callback(document, user_data);
    return (operation_error);
}

int32_t json_deserialize_from_backend(ft_document_source &source,
    json_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    json_document document;
    int32_t operation_error;

    if (deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = document.read_from_backend(source);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = deserialize_callback(document, user_data);
    return (operation_error);
}
