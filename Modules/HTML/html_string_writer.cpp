#include "html_writer_internal.hpp"
#include "../CMA/CMA.hpp"

struct html_string_builder
{
    char *buffer;
    ft_size_t length;
    ft_size_t capacity;
};

static int32_t html_append_string_output(html_string_builder &builder, std::string_view bytes)
{
    if (bytes.empty())
        return (FT_ERR_SUCCESS);
    if (bytes.size() > FT_SYSTEM_SIZE_MAX - builder.length - 1)
        return (FT_ERR_INTERNAL);
    const ft_size_t required = builder.length + bytes.size() + 1;
    if (required > builder.capacity)
    {
        ft_size_t capacity = builder.capacity;
        while (capacity < required)
        {
            if (capacity > FT_SYSTEM_SIZE_MAX / 2)
            {
                capacity = required;
                break ;
            }
            capacity *= 2;
        }
        auto *buffer = static_cast<char *>(cma_realloc(builder.buffer, capacity));
        if (buffer == ft_nullptr)
            return (FT_ERR_INTERNAL);
        builder.buffer = buffer;
        builder.capacity = capacity;
    }
    (void)ft_memcpy(builder.buffer + builder.length, bytes.data(), bytes.size());
    builder.length += bytes.size();
    return (FT_ERR_SUCCESS);
}

char *html_write_to_string(html_node *node_list)
{
    constexpr ft_size_t INITIAL_CAPACITY = 64;
    html_string_builder builder{static_cast<char *>(cma_malloc(INITIAL_CAPACITY)), 0, INITIAL_CAPACITY};
    if (builder.buffer == ft_nullptr)
        return (ft_nullptr);
    auto emit = [&builder](std::string_view bytes)
    {
        return (html_append_string_output(builder, bytes));
    };
    if (html_emit_nodes(node_list, 0, "", emit) != FT_ERR_SUCCESS)
    {
        cma_free(builder.buffer);
        return (ft_nullptr);
    }
    builder.buffer[builder.length] = '\0';
    return (builder.buffer);
}
