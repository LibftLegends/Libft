#include "yaml.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

int32_t yaml_serialize_to_string(yaml_serialize_callback serialize_callback,
    void *user_data, ft_string &output) noexcept
{
    yaml_value root;
    int32_t operation_error;

    if (serialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = root.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = serialize_callback(root, user_data);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    return (yaml_write_to_string(&root, output));
}

int32_t yaml_serialize_to_backend(yaml_serialize_callback serialize_callback,
    void *user_data, ft_document_sink &sink) noexcept
{
    yaml_value root;
    int32_t operation_error;

    if (serialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = root.initialize();
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = serialize_callback(root, user_data);
    if (operation_error != FT_ERR_SUCCESS)
        return (operation_error);
    operation_error = yaml_write_to_backend(sink, &root);
    return (operation_error);
}

int32_t yaml_deserialize_from_string(const ft_string &content,
    yaml_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    yaml_value *root;
    int32_t operation_error;

    if (content.is_initialised() == FT_FALSE || deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    root = yaml_read_from_string(content);
    if (root == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = deserialize_callback(*root, user_data);
    yaml_free(root);
    return (operation_error);
}

int32_t yaml_deserialize_from_backend(ft_document_source &source,
    yaml_deserialize_callback deserialize_callback, void *user_data) noexcept
{
    yaml_value *root;
    int32_t operation_error;

    if (deserialize_callback == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    root = yaml_read_from_backend(source);
    if (root == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    operation_error = deserialize_callback(*root, user_data);
    yaml_free(root);
    return (operation_error);
}
