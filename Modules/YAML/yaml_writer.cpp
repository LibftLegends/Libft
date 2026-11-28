#include "yaml.hpp"
#include <fcntl.h>
#include <new>
#include "../System_utils/system_utils.hpp"
#include "../Errno/errno.hpp"

static int32_t write_indent(ft_string &output, ft_size_t indent) noexcept
{
    constexpr char SPACES[] = "                                                                ";
    constexpr ft_size_t BLOCK_SIZE = sizeof(SPACES) - 1;
    while (indent >= BLOCK_SIZE)
    {
        const int32_t status = output.append(SPACES, BLOCK_SIZE);
        if (status != FT_ERR_SUCCESS)
            return (status);
        indent -= BLOCK_SIZE;
    }
    if (indent)
        return (output.append(SPACES, indent));
    return (FT_ERR_SUCCESS);
}

static int32_t write_node(const yaml_value *value, ft_string &output, ft_size_t indent) noexcept;

static int32_t write_scalar(const yaml_value &value, ft_string &output) noexcept
{
    const int32_t status = output.append(value.get_scalar());
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (output.append('\n'));
}

static int32_t write_entry(const yaml_value *value, const ft_string *key,
    ft_string &output, ft_size_t indent) noexcept
{
    int32_t status = write_indent(output, indent);
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (key)
    {
        status = output.append(*key);
        if (status == FT_ERR_SUCCESS)
            status = output.append(':');
    }
    else
        status = output.append('-');
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (value && value->get_type() == YAML_SCALAR)
    {
        status = output.append(' ');
        if (status != FT_ERR_SUCCESS)
            return (status);
        return (write_scalar(*value, output));
    }
    status = output.append('\n');
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (write_node(value, output, indent + 2));
}

static int32_t write_map(const yaml_value &value, ft_string &output, ft_size_t indent) noexcept
{
    const auto &entries = value.get_map();
    const auto &keys = value.get_map_keys();
    const ft_size_t count = keys.size();
    ft_size_t index = 0;
    const int32_t visit_status = entries.visit_entries([&](const Pair<ft_string, yaml_value *> &entry) noexcept -> int32_t
    {
        if (index >= count || !(entry.key == keys[index]))
            return (FT_ERR_SUCCESS);
        ++index;
        return (write_entry(entry.value, &entry.key, output, indent));
    });
    if (visit_status != FT_ERR_SUCCESS)
        return (visit_status);

    // Repeated or out-of-order keys still follow the public key list.
    while (index < count)
    {
        const ft_string &key = keys[index];
        const int32_t status = write_entry(entries.at(key), &key, output, indent);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t write_node(const yaml_value *value, ft_string &output, ft_size_t indent) noexcept
{
    if (!value)
        return (FT_ERR_SUCCESS);
    const yaml_type type = value->get_type();
    if (type == YAML_SCALAR)
    {
        const int32_t status = write_indent(output, indent);
        if (status != FT_ERR_SUCCESS)
            return (status);
        return (write_scalar(*value, output));
    }
    if (type == YAML_MAP)
        return (write_map(*value, output, indent));
    if (type == YAML_LIST)
    {
        const auto &items = value->get_list();
        const ft_size_t count = items.size();
        ft_size_t index = 0;
        while (index < count)
        {
            const int32_t status = write_entry(items[index], nullptr, output, indent);
            if (status != FT_ERR_SUCCESS)
                return (status);
            ++index;
        }
    }
    return (FT_ERR_SUCCESS);
}

static int32_t serialize(const yaml_value *value, ft_string &output) noexcept
{
    const int32_t status = output.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);
    return (write_node(value, output, 0));
}

ft_string *yaml_write_to_string(const yaml_value *value) noexcept
{
    ft_string *output = new (std::nothrow) ft_string();
    if (output && serialize(value, *output) != FT_ERR_SUCCESS)
    {
        delete output;
        return (nullptr);
    }
    return (output);
}

int32_t yaml_write_to_string(const yaml_value *value, ft_string &output) noexcept
{
    ft_string serialized;
    if (serialize(value, serialized) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    return (output.move(serialized));
}

int32_t yaml_write_to_file(const char *file_path, const yaml_value *value) noexcept
{
    ft_string output;
    if (serialize(value, output) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    su_file *file = su_fopen(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (!file)
        return (FT_ERR_IO);
    const ft_size_t size = output.size();
    const ft_size_t written = su_fwrite(output.c_str(), 1, size, file);
    if (su_fclose(file) != 0 || written != size)
        return (FT_ERR_IO);
    return (FT_ERR_SUCCESS);
}

int32_t yaml_write_to_backend(ft_document_sink &sink, const yaml_value *value) noexcept
{
    ft_string output;
    if (serialize(value, output) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    return (sink.write_all(output.c_str(), output.size()));
}
