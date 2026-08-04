#include "yaml.hpp"
#include <fcntl.h>
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../Template/pair.hpp"

static int32_t write_indent(ft_string &output, int32_t indent) noexcept
{
    int32_t indent_index = 0;
    int32_t append_error;

    while (indent_index < indent)
    {
        append_error = output.append(' ');
        if (append_error != FT_ERR_SUCCESS)
            return (append_error);
        indent_index++;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t write_node(const yaml_value *value, ft_string &output, int32_t indent) noexcept
{
    if (!value)
    {
        return (FT_ERR_SUCCESS);
    }
    yaml_type value_type;

    value_type = value->get_type();
    if (value_type == YAML_SCALAR)
    {
        int32_t indent_error;
        int32_t append_error;

        indent_error = write_indent(output, indent);
        if (indent_error != FT_ERR_SUCCESS)
            return (indent_error);
        const ft_string &scalar = value->get_scalar();
        append_error = output.append(scalar);
        if (append_error != FT_ERR_SUCCESS)
            return (append_error);
        append_error = output.append('\n');
        if (append_error != FT_ERR_SUCCESS)
            return (append_error);
        return (FT_ERR_SUCCESS);
    }
    if (value_type == YAML_LIST)
    {
        const ft_vector<yaml_value*> &list_ref = value->get_list();
        ft_size_t index = 0;
        ft_size_t list_count = list_ref.size();
        while (index < list_count)
        {
            const yaml_value *item = list_ref[index];
            ft_bool item_is_scalar = FT_FALSE;
            if (item)
            {
                yaml_type item_type = item->get_type();
                if (item_type == YAML_SCALAR)
                    item_is_scalar = FT_TRUE;
            }
            if (item_is_scalar)
            {
                int32_t indent_error;
                int32_t append_error;

                indent_error = write_indent(output, indent);
                if (indent_error != FT_ERR_SUCCESS)
                    return (indent_error);
                append_error = output.append("- ");
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                const ft_string &scalar = item->get_scalar();
                append_error = output.append(scalar);
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                append_error = output.append('\n');
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
            }
            else
            {
                int32_t indent_error;
                int32_t append_error;

                indent_error = write_indent(output, indent);
                if (indent_error != FT_ERR_SUCCESS)
                    return (indent_error);
                append_error = output.append("-\n");
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                int32_t node_error;

                node_error = write_node(item, output, indent + 2);
                if (node_error != FT_ERR_SUCCESS)
                    return (node_error);
            }
            index++;
        }
        return (FT_ERR_SUCCESS);
    }
    if (value_type == YAML_MAP)
    {
        const ft_map<ft_string, yaml_value*> &map_ref = value->get_map();
        const ft_vector<ft_string> &keys = value->get_map_keys();
        ft_size_t key_index = 0;
        ft_size_t key_count = keys.size();
        while (key_index < key_count)
        {
            const ft_string &key = keys[key_index];
            const yaml_value *child = map_ref.at(key);
            ft_bool child_is_scalar = FT_FALSE;
            if (child)
            {
                yaml_type child_type = child->get_type();
                if (child_type == YAML_SCALAR)
                    child_is_scalar = FT_TRUE;
            }
            if (child_is_scalar)
            {
                int32_t indent_error;
                int32_t append_error;

                indent_error = write_indent(output, indent);
                if (indent_error != FT_ERR_SUCCESS)
                    return (indent_error);
                append_error = output.append(key);
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                append_error = output.append(": ");
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                const ft_string &scalar = child->get_scalar();
                append_error = output.append(scalar);
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                append_error = output.append('\n');
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
            }
            else
            {
                int32_t indent_error;
                int32_t append_error;

                indent_error = write_indent(output, indent);
                if (indent_error != FT_ERR_SUCCESS)
                    return (indent_error);
                append_error = output.append(key);
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                append_error = output.append(":\n");
                if (append_error != FT_ERR_SUCCESS)
                    return (append_error);
                int32_t node_error;

                node_error = write_node(child, output, indent + 2);
                if (node_error != FT_ERR_SUCCESS)
                    return (node_error);
            }
            key_index++;
        }
        return (FT_ERR_SUCCESS);
    }
    return (FT_ERR_SUCCESS);
}

static void yaml_writer_delete_string(ft_string *string) noexcept
{
    if (string == ft_nullptr)
        return ;
    (void)string->destroy();
    delete string;
    return ;
}

ft_string *yaml_write_to_string(const yaml_value *value) noexcept
{
    ft_string *output;
    int32_t initialize_error;
    int32_t write_error;

    output = new (std::nothrow) ft_string();
    if (output == ft_nullptr)
        return (ft_nullptr);
    initialize_error = output->initialize();
    if (initialize_error != FT_ERR_SUCCESS)
    {
        delete output;
        return (ft_nullptr);
    }
    write_error = write_node(value, *output, 0);
    if (write_error != FT_ERR_SUCCESS)
    {
        yaml_writer_delete_string(output);
        return (ft_nullptr);
    }
    return (output);
}

int32_t yaml_write_to_file(const char *file_path, const yaml_value *value) noexcept
{
    ft_string *output;
    su_file *file;
    const char *data;
    ft_size_t expected_size;
    ft_size_t written;

    output = yaml_write_to_string(value);
    if (output == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    file = su_fopen(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file == ft_nullptr)
    {
        yaml_writer_delete_string(output);
        return (FT_ERR_IO);
    }
    data = output->c_str();
    expected_size = output->size();
    written = su_fwrite(data, 1, expected_size, file);
    if (written != expected_size)
    {
        (void)su_fclose(file);
        yaml_writer_delete_string(output);
        return (FT_ERR_IO);
    }
    if (su_fclose(file) != 0)
    {
        yaml_writer_delete_string(output);
        return (FT_ERR_IO);
    }
    yaml_writer_delete_string(output);
    return (FT_ERR_SUCCESS);
}

int32_t yaml_write_to_backend(ft_document_sink &sink, const yaml_value *value) noexcept
{
    ft_string *serialized;
    int32_t write_result;

    serialized = yaml_write_to_string(value);
    if (serialized == ft_nullptr)
        return (FT_ERR_INVALID_STATE);
    write_result = sink.write_all(serialized->c_str(), serialized->size());
    yaml_writer_delete_string(serialized);
    if (write_result != FT_ERR_SUCCESS)
        return (write_result);
    return (FT_ERR_SUCCESS);
}
