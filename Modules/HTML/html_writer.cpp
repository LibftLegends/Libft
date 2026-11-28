#include "html_writer_internal.hpp"
#include "../System_utils/system_utils.hpp"
#include <fcntl.h>

struct html_file_output
{
    int32_t descriptor;
    ft_size_t used;
    char buffer[16384];
};

static int32_t html_flush_file_output(html_file_output &output)
{
    if (output.used != 0 && su_write(output.descriptor, output.buffer, output.used) < 0)
        return (FT_ERR_INTERNAL);
    output.used = 0;
    return (FT_ERR_SUCCESS);
}

static int32_t html_append_file_output(html_file_output &output, std::string_view bytes)
{
    if (bytes.empty())
        return (FT_ERR_SUCCESS);
    if (bytes.size() > sizeof(output.buffer) - output.used
        && html_flush_file_output(output) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    if (bytes.size() > sizeof(output.buffer))
    {
        if (su_write(output.descriptor, bytes.data(), bytes.size()) < 0)
            return (FT_ERR_INTERNAL);
        return (FT_ERR_SUCCESS);
    }
    (void)ft_memcpy(output.buffer + output.used, bytes.data(), bytes.size());
    output.used += bytes.size();
    return (FT_ERR_SUCCESS);
}

int32_t html_write_to_file(const char *file_path, html_node *node_list)
{
    if (file_path == ft_nullptr)
        return (FT_ERR_INTERNAL);
    const int32_t descriptor = su_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (descriptor < 0)
        return (FT_ERR_INTERNAL);
    html_file_output output;
    output.descriptor = descriptor;
    output.used = 0;
    auto emit = [&output](std::string_view bytes)
    {
        return (html_append_file_output(output, bytes));
    };
    int32_t result = html_emit_nodes(node_list, 0, "(null)", emit);
    if (result == FT_ERR_SUCCESS)
        result = html_flush_file_output(output);
    if (su_close(descriptor) != FT_ERR_SUCCESS)
        result = FT_ERR_INTERNAL;
    return (result);
}
