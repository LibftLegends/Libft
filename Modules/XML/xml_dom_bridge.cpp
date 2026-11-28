#include "xml_dom_bridge.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <new>

static int32_t xml_dom_create_node(const xml_node &source, ft_dom_node *&target) noexcept;

static int32_t xml_dom_populate_node(const xml_node &source, ft_dom_node &target) noexcept
{
    const char *name = source.name;
    if (!name || name[0] == '\0')
        name = source.local_name;
    if (!name)
        name = "";
    int32_t status = target.set_name(name);
    if (status != FT_ERR_SUCCESS)
        return (status);
    target.set_type(FT_DOM_NODE_ELEMENT);
    if (source.namespace_prefix && source.namespace_prefix[0] != '\0')
        status = target.add_attribute("xml:prefix", source.namespace_prefix);
    if (status == FT_ERR_SUCCESS && source.namespace_uri && source.namespace_uri[0] != '\0')
        status = target.add_attribute("xml:ns", source.namespace_uri);
    if (status == FT_ERR_SUCCESS && source.text && source.text[0] != '\0')
        status = target.set_value(source.text);
    if (status != FT_ERR_SUCCESS)
        return (status);

    auto attribute = source.attributes.begin();
    status = source.attributes.get_error();
    if (status != FT_ERR_SUCCESS)
        return (status);
    const auto attribute_end = source.attributes.end();
    status = source.attributes.get_error();
    if (status != FT_ERR_SUCCESS)
        return (status);
    while (attribute != attribute_end)
    {
        const char *key = attribute->first;
        const char *value = attribute->second;
        if (!key)
            key = "";
        if (!value)
            value = "";
        status = target.add_attribute(key, value);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++attribute;
    }

    const ft_size_t child_count = source.children.size();
    ft_size_t index = 0;
    while (index < child_count)
    {
        const xml_node *child = source.children[index];
        status = source.children.get_error();
        if (status != FT_ERR_SUCCESS)
            return (status);
        if (child)
        {
            ft_dom_node *child_copy = ft_nullptr;
            status = xml_dom_create_node(*child, child_copy);
            if (status == FT_ERR_SUCCESS)
                status = target.add_child(child_copy);
            if (status != FT_ERR_SUCCESS)
            {
                delete child_copy;
                return (status);
            }
        }
        ++index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t xml_dom_create_node(const xml_node &source, ft_dom_node *&target) noexcept
{
    target = new (std::nothrow) ft_dom_node();
    if (!target)
        return (FT_ERR_NO_MEMORY);
    int32_t status = target->initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);
    ft_bool lock_acquired = FT_FALSE;
    status = xml_node_lock(&source, &lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = xml_dom_populate_node(source, *target);
    xml_node_unlock(&source, lock_acquired);
    return (status);
}

int32_t xml_document_to_dom(const xml_document &document, ft_dom_document &dom) noexcept
{
    ft_dom_node *new_root = ft_nullptr;
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = document.lock(&lock_acquired);
    if (status == FT_ERR_SUCCESS)
    {
        if (document._root)
            status = xml_dom_create_node(*document._root, new_root);
        else
            status = FT_ERR_INVALID_ARGUMENT;
        document.unlock(lock_acquired);
    }
    if (status == FT_ERR_SUCCESS)
        status = dom.replace_root(new_root);
    if (status != FT_ERR_SUCCESS)
    {
        delete new_root;
        return (FT_ERR_INTERNAL);
    }
    return (FT_ERR_SUCCESS);
}

int32_t xml_document_from_dom(const ft_dom_document &dom, xml_document &document) noexcept
{
    if (document.replace_from_dom(dom) != FT_ERR_SUCCESS)
        return (FT_ERR_INTERNAL);
    return (FT_ERR_SUCCESS);
}
