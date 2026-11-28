#include "xml.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"

int32_t xml_serialize_to_string(xml_serialize_callback serialize_callback,
    void *user_data, ft_string &output) noexcept
{
    xml_document document;
    int32_t operation_error;

    if (serialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = serialize_callback(document, user_data);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    return (document.write_to_string(output));
}

int32_t xml_serialize_to_backend(xml_serialize_callback serialize_callback,
    void *user_data, ft_document_sink &sink) noexcept
{
    xml_document document;
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

int32_t xml_deserialize_from_string(const char *content,
    xml_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    xml_document document;
    int32_t operation_error;

    if (content == ft_nullptr || deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = document.load_from_string(content);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = deserialize_callback(document, user_data);
    return (operation_error);
}

int32_t xml_deserialize_from_backend(ft_document_source &source,
    xml_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    xml_document document;
    int32_t operation_error;

    if (deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = document.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = document.load_from_backend(source);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = deserialize_callback(document, user_data);
    return (operation_error);
}
