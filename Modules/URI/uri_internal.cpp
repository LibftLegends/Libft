#include "uri_internal.hpp"
#include "../Basic/basic.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/limits.hpp"
#include "uri.hpp"

static thread_local int32_t g_uri_error = FT_ERR_SUCCESS;

int32_t uri_set_error(int32_t error_code) noexcept
{
    g_uri_error = error_code;
    return (error_code);
}

int32_t uri_get_error(void) noexcept
{
    return (g_uri_error);
}

const char *uri_get_error_str(void) noexcept
{
    return (ft_strerror(g_uri_error));
}

char *uri_duplicate_span(const char *input, ft_size_t start,
    ft_size_t end) noexcept
{
    char *output;
    ft_size_t size;

    if (end < start)
        return (ft_nullptr);
    size = end - start;
    if (size >= FT_SYSTEM_SIZE_MAX)
        return (ft_nullptr);
    output = static_cast<char *>(cma_malloc(size + 1));
    if (output == ft_nullptr)
        return (ft_nullptr);
    if (size != 0U)
        ft_memcpy(output, input + start, size);
    output[size] = '\0';
    return (output);
}

int32_t uri_assign_component(char **destination, const char *input,
    ft_size_t start, ft_size_t end) noexcept
{
    *destination = uri_duplicate_span(input, start, end);
    if (*destination == ft_nullptr)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

void uri_lower_string(char *string) noexcept
{
    ft_size_t index;

    if (string == ft_nullptr)
        return ;
    index = 0;
    while (string[index] != '\0')
    {
        string[index] = uri_lower_character(string[index]);
        index++;
    }
    return ;
}

void uri_delete_components(uri_components *components) noexcept
{
    if (components == ft_nullptr)
        return ;
    uri_components_destroy(components);
    return ;
}
