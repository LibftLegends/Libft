#ifndef FT_HTML_WRITER_INTERNAL_HPP
#define FT_HTML_WRITER_INTERNAL_HPP

#include "html_parser.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Basic/limits.hpp"
#include <string_view>

static std::string_view html_writer_view(const char *text)
{
    return (std::string_view(text, ft_strlen_size_t(text)));
}

template <typename Emit>
static int32_t html_emit_indent(Emit &emit, ft_size_t remaining)
{
    constexpr std::string_view spaces = "                                                                ";
    while (remaining != 0)
    {
        ft_size_t chunk_size = remaining;
        if (chunk_size > spaces.size())
            chunk_size = spaces.size();
        if (emit(std::string_view(spaces.data(), chunk_size)) != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        remaining -= chunk_size;
    }
    return (FT_ERR_SUCCESS);
}

template <typename Emit>
static int32_t html_emit_nodes(const html_node *node, ft_size_t indent,
    std::string_view null_tag, Emit &emit)
{
    while (node != ft_nullptr)
    {
        std::string_view tag = null_tag;
        if (node->tag != ft_nullptr)
            tag = html_writer_view(node->tag);
        if (html_emit_indent(emit, indent) != FT_ERR_SUCCESS || emit("<") != FT_ERR_SUCCESS
            || emit(tag) != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        const html_attr *attribute = node->attributes;
        while (attribute != ft_nullptr)
        {
            if (attribute->key != ft_nullptr && attribute->value != ft_nullptr
                && (emit(" ") != FT_ERR_SUCCESS || emit(html_writer_view(attribute->key)) != FT_ERR_SUCCESS
                    || emit("=\"") != FT_ERR_SUCCESS || emit(html_writer_view(attribute->value)) != FT_ERR_SUCCESS
                    || emit("\"") != FT_ERR_SUCCESS))
                return (FT_ERR_INTERNAL);
            attribute = attribute->next;
        }
        if (node->text == ft_nullptr && node->children == ft_nullptr)
        {
            if (emit("/>\n") != FT_ERR_SUCCESS)
                return (FT_ERR_INTERNAL);
            node = node->next;
            continue ;
        }
        if (emit(">") != FT_ERR_SUCCESS
            || (node->text != ft_nullptr && emit(html_writer_view(node->text)) != FT_ERR_SUCCESS))
            return (FT_ERR_INTERNAL);
        if (node->children != ft_nullptr)
        {
            if (indent > FT_SYSTEM_SIZE_MAX - 2 || emit("\n") != FT_ERR_SUCCESS
                || html_emit_nodes(node->children, indent + 2, null_tag, emit) != FT_ERR_SUCCESS
                || html_emit_indent(emit, indent) != FT_ERR_SUCCESS)
                return (FT_ERR_INTERNAL);
        }
        if (emit("</") != FT_ERR_SUCCESS || emit(tag) != FT_ERR_SUCCESS || emit(">\n") != FT_ERR_SUCCESS)
            return (FT_ERR_INTERNAL);
        node = node->next;
    }
    return (FT_ERR_SUCCESS);
}

#endif
