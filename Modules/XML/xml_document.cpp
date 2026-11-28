#include "xml.hpp"
#include "../Parser/dom.hpp"
#include "../Advanced/advanced.hpp"
#include "../Compatebility/compatebility_internal.hpp"
#include "../CMA/CMA.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include "../Basic/basic.hpp"
#include "../PThread/recursive_mutex.hpp"
#include "../PThread/pthread_internal.hpp"
#include "../PThread/pthread.hpp"
#include "../Errno/errno_internal.hpp"
#include <cstdio>
#include <cerrno>
#include <new>
#include "../Basic/limits.hpp"
#include "../PThread/mutex.hpp"
#include "../Template/unordered_map.hpp"
#include "../Template/vector.hpp"

struct xml_namespace_entry
{
    char *prefix;
    char *uri;
    const xml_namespace_entry *parent;
    xml_namespace_entry *next_local;
};

thread_local int32_t xml_document::_last_error = FT_ERR_SUCCESS;

int32_t xml_document::set_error(int32_t error_code) noexcept
{
    xml_document::_last_error = error_code;
    return (error_code);
}

static const char *skip_whitespace(const char *string)
{
    while (string && (*string == ' ' || *string == '\n' || *string == '\t' || *string == '\r'))
        string++;
    return (string);
}

static int32_t  is_xml_whitespace(char character)
{
    if (character == ' ')
        return (1);
    if (character == '\n')
        return (1);
    if (character == '\t')
        return (1);
    if (character == '\r')
        return (1);
    return (FT_ERR_SUCCESS);
}

static const char *xml_skip_whitespace_range(const char *cursor, const char *end)
{
    while (cursor && cursor < end && is_xml_whitespace(*cursor))
        cursor++;
    return (cursor);
}

static int32_t xml_add_namespace_binding(xml_node *node, const char *prefix,
        ft_size_t prefix_length, const char *uri,
        const xml_namespace_entry **active_scope)
{
    xml_namespace_entry *entry;

    entry = new (std::nothrow) xml_namespace_entry();
    if (!entry)
    {
        return (FT_ERR_INTERNAL);
    }
    entry->prefix = ft_nullptr;
    if (prefix && prefix_length > 0)
    {
        entry->prefix = ft_span_dup(prefix, prefix_length);
        if (!entry->prefix)
        {
            delete entry;
            return (FT_ERR_INTERNAL);
        }
    }
    entry->uri = ft_span_dup(uri, ft_strlen_size_t(uri));
    if (!entry->uri)
    {
        if (entry->prefix)
            cma_free(entry->prefix);
        delete entry;
        return (FT_ERR_INTERNAL);
    }
    entry->parent = *active_scope;
    entry->next_local = node->namespace_bindings;
    node->namespace_bindings = entry;
    *active_scope = entry;
    return (FT_ERR_SUCCESS);
}

static const char *xml_resolve_namespace_uri(
        const xml_namespace_entry *scope_head, const char *prefix)
{
    const xml_namespace_entry *current;

    current = scope_head;
    while (current)
    {
        if (!prefix || *prefix == '\0')
        {
            if (!current->prefix || current->prefix[0] == '\0')
                return (current->uri);
        }
        else if (current->prefix && ft_strcmp(current->prefix, prefix) == 0)
            return (current->uri);
        current = current->parent;
    }
    return (ft_nullptr);
}

static int32_t xml_clone_bindings(const xml_namespace_entry *source, xml_node &target,
        const xml_namespace_entry *parent_scope) noexcept
{
    xml_namespace_entry *previous = ft_nullptr;
    while (source)
    {
        xml_namespace_entry *entry = new (std::nothrow) xml_namespace_entry{};
        if (entry == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        if (previous)
        {
            previous->next_local = entry;
            previous->parent = entry;
        }
        else
            target.namespace_bindings = entry;
        if (source->prefix)
        {
            entry->prefix = adv_strdup(source->prefix);
            if (!entry->prefix)
                return (FT_ERR_NO_MEMORY);
        }
        entry->uri = adv_strdup(source->uri);
        if (!entry->uri)
            return (FT_ERR_NO_MEMORY);
        previous = entry;
        source = source->next_local;
    }
    if (previous)
        previous->parent = parent_scope;
    return (FT_ERR_SUCCESS);
}

static int32_t xml_clone_attributes(const xml_node &source, xml_node &target) noexcept
{
    auto attribute = source.attributes.begin();
    const auto end = source.attributes.end();
    while (attribute != end)
    {
        char *key = adv_strdup(attribute->first);
        if (attribute->first && !key)
            return (FT_ERR_NO_MEMORY);
        char *value = adv_strdup(attribute->second);
        int32_t status = FT_ERR_NO_MEMORY;
        if (value || !attribute->second)
        {
            target.attributes.insert(key, value);
            status = target.attributes.get_error();
        }
        if (status != FT_ERR_SUCCESS)
        {
            cma_free(key);
            cma_free(value);
            return (status);
        }
        ++attribute;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t xml_clone_node(const xml_node &source, xml_node *&target,
        const xml_namespace_entry *source_scope,
        const xml_namespace_entry *target_scope) noexcept;

static int32_t xml_clone_node_content(const xml_node &source, xml_node &target,
        const xml_namespace_entry *source_scope,
        const xml_namespace_entry *target_scope) noexcept
{
    target.name = adv_strdup(source.name);
    target.namespace_prefix = adv_strdup(source.namespace_prefix);
    target.local_name = adv_strdup(source.local_name);
    target.text = adv_strdup(source.text);
    if ((source.name && !target.name) || (source.namespace_prefix && !target.namespace_prefix)
        || (source.local_name && !target.local_name) || (source.text && !target.text))
        return (FT_ERR_NO_MEMORY);

    int32_t status = xml_clone_bindings(source.namespace_bindings, target, target_scope);
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (source.namespace_bindings)
    {
        source_scope = source.namespace_bindings;
        target_scope = target.namespace_bindings;
    }
    target.namespace_uri = source.namespace_uri;
    if (source.namespace_uri)
    {
        // A parsed namespace URI borrows storage from this node or an ancestor.
        const xml_namespace_entry *source_binding = source_scope;
        const xml_namespace_entry *target_binding = target_scope;
        while (source_binding && target_binding)
        {
            if (source_binding->uri == source.namespace_uri)
            {
                target.namespace_uri = target_binding->uri;
                break ;
            }
            source_binding = source_binding->parent;
            target_binding = target_binding->parent;
        }
    }

    if (source.attributes.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        status = xml_clone_attributes(source, target);
        if (status != FT_ERR_SUCCESS)
            return (status);
    }
    if (source.children.is_initialised() != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    const ft_size_t child_count = source.children.size();
    if (child_count > target.children.capacity())
    {
        target.children.reserve(child_count);
        status = target.children.get_error();
        if (status != FT_ERR_SUCCESS)
            return (status);
    }
    ft_size_t child_index = 0;
    while (child_index < child_count)
    {
        const xml_node *source_child = source.children[child_index];
        status = target.children.push_back(ft_nullptr);
        if (status == FT_ERR_SUCCESS && source_child)
            status = xml_clone_node(*source_child, target.children[child_index], source_scope, target_scope);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++child_index;
    }
    return (FT_ERR_SUCCESS);
}

static int32_t xml_clone_node(const xml_node &source, xml_node *&target,
        const xml_namespace_entry *source_scope,
        const xml_namespace_entry *target_scope) noexcept
{
    target = new (std::nothrow) xml_node();
    if (!target)
        return (FT_ERR_NO_MEMORY);
    int32_t status = target->children.initialize();
    if (status == FT_ERR_SUCCESS)
        status = target->attributes.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);

    ft_bool lock_acquired = FT_FALSE;
    status = xml_node_lock(&source, &lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = xml_clone_node_content(source, *target, source_scope, target_scope);
    xml_node_unlock(&source, lock_acquired);
    if (status == FT_ERR_SUCCESS && lock_acquired)
        status = xml_node_prepare_thread_safety(target);
    return (status);
}

static int32_t xml_parse_attributes(xml_node *node, const char *start,
        const char *end, const xml_namespace_entry *scope_head,
        const xml_namespace_entry **out_scope, int32_t *error_code_out)
{
    const char                *cursor;
    const xml_namespace_entry *active_scope;

    cursor = start;
    active_scope = scope_head;
    if (error_code_out)
        *error_code_out = FT_ERR_SUCCESS;
    while (cursor && cursor < end)
    {
        cursor = xml_skip_whitespace_range(cursor, end);
        if (!cursor || cursor >= end)
            break ;
        if (*cursor == '/')
        {
            cursor++;
            continue ;
        }
        const char *name_start;
        const char *name_end;
        char       *attribute_name;
        ft_bool        has_value;
        char       *attribute_value;

        name_start = cursor;
        while (cursor < end && !is_xml_whitespace(*cursor)
            && *cursor != '=' && *cursor != '/' && *cursor != '>')
            cursor++;
        name_end = cursor;
        if (name_end <= name_start)
        {
            if (error_code_out)
                *error_code_out = FT_ERR_INVALID_ARGUMENT;
            return (FT_ERR_INTERNAL);
        }
        attribute_name = ft_span_dup(name_start,
                static_cast<ft_size_t>(name_end - name_start));
        if (!attribute_name)
        {
            if (error_code_out)
                *error_code_out = FT_ERR_NO_MEMORY;
            return (FT_ERR_INTERNAL);
        }
        cursor = xml_skip_whitespace_range(cursor, end);
        has_value = FT_FALSE;
        attribute_value = ft_nullptr;
        if (cursor && cursor < end && *cursor == '=')
        {
            char            quote_character;
            const char      *value_start;
            const char      *value_end;

            cursor++;
            cursor = xml_skip_whitespace_range(cursor, end);
            if (!cursor || cursor >= end)
            {
                cma_free(attribute_name);
                if (error_code_out)
                    *error_code_out = FT_ERR_INVALID_ARGUMENT;
                return (FT_ERR_INTERNAL);
            }
            quote_character = '\0';
            if (*cursor == '"' || *cursor == '\'')
            {
                quote_character = *cursor;
                cursor++;
            }
            value_start = cursor;
            if (quote_character != '\0')
            {
                cursor = static_cast<const char *>(ft_memchr(cursor, quote_character,
                    static_cast<ft_size_t>(end - cursor)));
                if (cursor == ft_nullptr)
                {
                    cma_free(attribute_name);
                    if (error_code_out)
                        *error_code_out = FT_ERR_INVALID_ARGUMENT;
                    return (FT_ERR_INTERNAL);
                }
            }
            else
            {
                while (cursor < end && !is_xml_whitespace(*cursor)
                    && *cursor != '/' && *cursor != '>')
                    ++cursor;
            }
            value_end = cursor;
            attribute_value = ft_span_dup(value_start,
                    static_cast<ft_size_t>(value_end - value_start));
            if (!attribute_value)
            {
                cma_free(attribute_name);
                if (error_code_out)
                    *error_code_out = FT_ERR_NO_MEMORY;
                return (FT_ERR_INTERNAL);
            }
            has_value = FT_TRUE;
            if (quote_character != '\0' && cursor < end && *cursor == quote_character)
                cursor++;
        }
        ft_bool        is_namespace;
        const char *namespace_prefix_pointer;
        ft_size_t      namespace_prefix_length;

        is_namespace = FT_FALSE;
        namespace_prefix_pointer = ft_nullptr;
        namespace_prefix_length = 0;
        if (attribute_name[0] == 'x' && attribute_name[1] == 'm'
            && attribute_name[2] == 'l' && attribute_name[3] == 'n'
            && attribute_name[4] == 's')
        {
            if (attribute_name[5] == '\0')
                is_namespace = FT_TRUE;
            else if (attribute_name[5] == ':' && attribute_name[6] != '\0')
            {
                is_namespace = FT_TRUE;
                namespace_prefix_pointer = attribute_name + 6;
                namespace_prefix_length = static_cast<ft_size_t>(name_end - name_start) - 6;
            }
            else if (attribute_name[5] == ':' && attribute_name[6] == '\0')
            {
                cma_free(attribute_name);
                if (attribute_value)
                    cma_free(attribute_value);
                if (error_code_out)
                    *error_code_out = FT_ERR_INVALID_ARGUMENT;
                return (FT_ERR_INTERNAL);
            }
        }
        if (is_namespace)
        {
            if (!has_value || !attribute_value)
            {
                cma_free(attribute_name);
                if (attribute_value)
                    cma_free(attribute_value);
                if (error_code_out)
                    *error_code_out = FT_ERR_INVALID_ARGUMENT;
                return (FT_ERR_INTERNAL);
            }
            if (xml_add_namespace_binding(node, namespace_prefix_pointer,
                    namespace_prefix_length, attribute_value, &active_scope) != 0)
            {
                cma_free(attribute_name);
                cma_free(attribute_value);
                if (error_code_out)
                    *error_code_out = FT_ERR_NO_MEMORY;
                return (FT_ERR_INTERNAL);
            }
        }
        node->attributes.insert(attribute_name, attribute_value);
        int32_t attribute_error = node->attributes.get_error();
        if (attribute_error != FT_ERR_SUCCESS)
        {
            if (attribute_value)
                cma_free(attribute_value);
            cma_free(attribute_name);
            if (error_code_out)
                *error_code_out = attribute_error;
            return (FT_ERR_INTERNAL);
        }
    }
    if (out_scope)
        *out_scope = active_scope;
    if (error_code_out)
        *error_code_out = FT_ERR_SUCCESS;
    return (FT_ERR_SUCCESS);
}

xml_node::xml_node() noexcept
    : mutex(ft_nullptr), name(ft_nullptr), namespace_prefix(ft_nullptr),
    local_name(ft_nullptr), namespace_uri(ft_nullptr), namespace_bindings(ft_nullptr), text(ft_nullptr),
    children(), attributes()
{
    return ;
}

xml_node::~xml_node() noexcept
{
    if (this->children.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        ft_size_t child_index = 0;
        ft_size_t child_size = this->children.size();

        while (child_index < child_size)
        {
            xml_node *child = this->children[child_index];
            if (child)
                delete child;
            child_index++;
        }
    }
    if (this->attributes.is_initialised() == FT_CLASS_STATE_INITIALISED)
    {
        ft_unordered_map<char*, char*>::iterator it = this->attributes.begin();
        ft_unordered_map<char*, char*>::iterator end = this->attributes.end();

        while (it != end)
        {
            if (it->first)
                cma_free(it->first);
            if (it->second)
                cma_free(it->second);
            ++it;
        }
    }
    if (this->namespace_prefix)
        cma_free(this->namespace_prefix);
    if (this->local_name)
        cma_free(this->local_name);
    xml_namespace_entry *binding = this->namespace_bindings;
    while (binding)
    {
        xml_namespace_entry *next_binding;

        next_binding = binding->next_local;
        if (binding->prefix)
            cma_free(binding->prefix);
        if (binding->uri)
            cma_free(binding->uri);
        delete binding;
        binding = next_binding;
    }
    if (this->name)
        cma_free(this->name);
    if (this->text)
        cma_free(this->text);
    xml_node_teardown_thread_safety(this);
    return ;
}

static int32_t xml_create_named_node(const char *name, ft_size_t name_length, xml_node *&node) noexcept
{
    node = new (std::nothrow) xml_node();
    if (!node)
        return (FT_ERR_NO_MEMORY);
    int32_t status = node->children.initialize();
    if (status == FT_ERR_SUCCESS)
        status = node->attributes.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);

    node->name = ft_span_dup(name, name_length);
    if (!node->name)
        return (FT_ERR_NO_MEMORY);
    const char *colon = static_cast<const char *>(ft_memchr(name, ':', name_length));
    if (!colon)
        node->local_name = ft_span_dup(name, name_length);
    else
    {
        const ft_size_t prefix_length = static_cast<ft_size_t>(colon - name);
        if (prefix_length == 0 || prefix_length + 1 == name_length)
            return (FT_ERR_INVALID_ARGUMENT);
        node->namespace_prefix = ft_span_dup(name, prefix_length);
        if (!node->namespace_prefix)
            return (FT_ERR_NO_MEMORY);
        node->local_name = ft_span_dup(colon + 1, name_length - prefix_length - 1);
    }
    if (!node->local_name)
        return (FT_ERR_NO_MEMORY);
    return (FT_ERR_SUCCESS);
}

static const char *parse_node(const char *string, xml_node **out_node,
        const xml_namespace_entry *scope_head, int32_t *error_code_out)
{
    if (error_code_out)
        *error_code_out = FT_ERR_SUCCESS;
    string = skip_whitespace(string);
    if (!string || *string != '<')
    {
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    string++;
    const char *name_start = string;
    while (*string && *string != '>' && *string != ' ' && *string != '/')
        string++;
    ft_size_t name_length = static_cast<ft_size_t>(string - name_start);
    xml_node *node = ft_nullptr;
    const int32_t create_status = xml_create_named_node(name_start, name_length, node);
    if (create_status != FT_ERR_SUCCESS)
    {
        delete node;
        if (error_code_out)
            *error_code_out = create_status;
        return (ft_nullptr);
    }
    int32_t prepare_result = xml_node_prepare_thread_safety(node);
    if (prepare_result != 0)
    {
        delete node;
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_STATE;
        return (ft_nullptr);
    }
    while (*string && *string != '>')
    {
        if (*string == '"' || *string == '\'')
        {
            const char *quote_end = ft_strchr(string + 1, *string);
            if (quote_end == ft_nullptr)
                break ;
            string = quote_end;
        }
        ++string;
    }
    if (*string != '>')
    {
        delete node;
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    const char *tag_end = string;
    int32_t self_closing = 0;
    const char *attribute_start = name_start + name_length;
    const char *attribute_end = tag_end;
    if (attribute_end > attribute_start)
    {
        const char *trim_cursor = attribute_end;

        while (trim_cursor > attribute_start && is_xml_whitespace(trim_cursor[-1]))
            trim_cursor--;
        if (trim_cursor > attribute_start && trim_cursor[-1] == '/')
        {
            self_closing = 1;
            trim_cursor--;
            while (trim_cursor > attribute_start && is_xml_whitespace(trim_cursor[-1]))
                trim_cursor--;
        }
        attribute_end = trim_cursor;
    }
    const xml_namespace_entry *active_scope = scope_head;
    if (attribute_end > attribute_start)
    {
        int32_t attribute_error = FT_ERR_SUCCESS;

        if (xml_parse_attributes(node, attribute_start, attribute_end,
                scope_head, &active_scope, &attribute_error) != 0)
        {
            delete node;
            if (error_code_out)
            {
                int32_t reported_error;

                if (attribute_error != FT_ERR_SUCCESS)
                    reported_error = attribute_error;
                else
                    reported_error = FT_ERR_INVALID_ARGUMENT;
                *error_code_out = reported_error;
            }
            return (ft_nullptr);
        }
    }
    if (node->namespace_prefix)
    {
        node->namespace_uri = xml_resolve_namespace_uri(active_scope,
                node->namespace_prefix);
        if (!node->namespace_uri)
        {
            delete node;
            if (error_code_out)
                *error_code_out = FT_ERR_INVALID_ARGUMENT;
            return (ft_nullptr);
        }
    }
    else
        node->namespace_uri = xml_resolve_namespace_uri(active_scope, ft_nullptr);
    string++;
    if (self_closing)
    {
        *out_node = node;
        if (error_code_out)
            *error_code_out = FT_ERR_SUCCESS;
        return (string);
    }
    const char *text_start = string;
    if (*string == '<' && string[1] == '/')
        ;
    else if (*string == '<')
    {
        while (1)
        {
            xml_node *child = ft_nullptr;
            int32_t child_error = FT_ERR_SUCCESS;

            string = parse_node(string, &child, active_scope, &child_error);
            if (!string)
            {
                delete node;
                if (error_code_out)
                {
                    int32_t reported_error;

                    if (child_error != FT_ERR_SUCCESS)
                        reported_error = child_error;
                    else
                        reported_error = FT_ERR_INVALID_ARGUMENT;
                    *error_code_out = reported_error;
                }
                return (ft_nullptr);
            }
            const int32_t children_error_code = node->children.push_back(child);
            if (children_error_code != FT_ERR_SUCCESS)
            {
                delete child;
                delete node;
                if (error_code_out)
                    *error_code_out = children_error_code;
                return (ft_nullptr);
            }
            string = skip_whitespace(string);
            if (*string == '<' && string[1] == '/')
                break ;
        }
    }
    else
    {
        string = ft_strchr(string, '<');
        while (string && string[1] != '/')
            string = ft_strchr(string + 1, '<');
        if (string == ft_nullptr)
        {
            delete node;
            if (error_code_out)
                *error_code_out = FT_ERR_INVALID_ARGUMENT;
            return (ft_nullptr);
        }
        ft_size_t text_length = static_cast<ft_size_t>(string - text_start);
        node->text = ft_span_dup(text_start, text_length);
        if (!node->text && text_length > 0)
        {
            delete node;
            if (error_code_out)
                *error_code_out = FT_ERR_NO_MEMORY;
            return (ft_nullptr);
        }
    }
    if (*string != '<' || string[1] != '/')
    {
        delete node;
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    string += 2;
    string = skip_whitespace(string);
    const char *closing_name_start = string;
    while (*string && *string != '>' && !is_xml_whitespace(*string))
        string++;
    const ft_size_t closing_name_length = static_cast<ft_size_t>(string - closing_name_start);
    if (closing_name_length != name_length || ft_memcmp(closing_name_start, name_start, name_length) != 0)
    {
        delete node;
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    while (*string && *string != '>')
    {
        if (!is_xml_whitespace(*string))
        {
            delete node;
            if (error_code_out)
                *error_code_out = FT_ERR_INVALID_ARGUMENT;
            return (ft_nullptr);
        }
        string++;
    }
    if (*string != '>')
    {
        delete node;
        if (error_code_out)
            *error_code_out = FT_ERR_INVALID_ARGUMENT;
        return (ft_nullptr);
    }
    string++;
    *out_node = node;
    if (error_code_out)
        *error_code_out = FT_ERR_SUCCESS;
    return (string);
}


static ft_bool xml_dom_name_is_valid(const char *name, ft_size_t length) noexcept
{
    if (!name || length == 0)
        return (FT_FALSE);
    ft_size_t index = 0;
    while (index < length)
    {
        const char character = name[index];
        if (character == '\0' || is_xml_whitespace(character) || character == '<'
            || character == '>' || character == '/' || character == '='
            || character == '"' || character == '\'')
            return (FT_FALSE);
        ++index;
    }
    return (FT_TRUE);
}

static int32_t xml_import_dom_attribute(xml_node &node, const ft_string &key,
        const ft_string &value, const xml_namespace_entry **scope) noexcept
{
    const char *key_data = key.c_str();
    const ft_size_t key_size = key.size();
    const char *value_data = value.c_str();
    const ft_size_t value_size = value.size();
    if (!xml_dom_name_is_valid(key_data, key_size) || ft_memchr(value_data, '\0', value_size))
        return (FT_ERR_INVALID_ARGUMENT);

    if (key_size >= 5 && ft_memcmp(key_data, "xmlns", 5) == 0)
    {
        if (key_size == 5 || key_data[5] == ':')
        {
            if (key_size == 6)
                return (FT_ERR_INVALID_ARGUMENT);
            const char *prefix = ft_nullptr;
            ft_size_t prefix_size = 0;
            if (key_size > 5)
            {
                prefix = key_data + 6;
                prefix_size = key_size - 6;
            }
            if (xml_add_namespace_binding(&node, prefix, prefix_size, value_data, scope) != FT_ERR_SUCCESS)
                return (FT_ERR_NO_MEMORY);
        }
    }

    char *owned_key = ft_span_dup(key_data, key_size);
    char *owned_value = ft_span_dup(value_data, value_size);
    int32_t status = FT_ERR_NO_MEMORY;
    if (owned_key && owned_value)
    {
        node.attributes.insert(owned_key, owned_value);
        status = node.attributes.get_error();
    }
    if (status != FT_ERR_SUCCESS)
    {
        cma_free(owned_key);
        cma_free(owned_value);
    }
    return (status);
}

static int32_t xml_import_dom_node(const ft_dom_node &source, xml_node *&target,
        const xml_namespace_entry *scope) noexcept;

static int32_t xml_import_dom_node_locked(const ft_dom_node &source, xml_node *&target,
        const xml_namespace_entry *scope) noexcept
{
    const ft_string &name = source.get_name();
    if (!xml_dom_name_is_valid(name.c_str(), name.size()))
        return (FT_ERR_INVALID_ARGUMENT);
    int32_t status = xml_create_named_node(name.c_str(), name.size(), target);
    if (status != FT_ERR_SUCCESS)
        return (status);

    const auto &keys = source.get_attribute_keys();
    const auto &values = source.get_attribute_values();
    const ft_size_t attribute_count = keys.size();
    if (values.size() != attribute_count)
        return (FT_ERR_INVALID_STATE);
    ft_size_t index = 0;
    while (index < attribute_count)
    {
        status = xml_import_dom_attribute(*target, keys[index], values[index], &scope);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++index;
    }
    target->namespace_uri = xml_resolve_namespace_uri(scope, target->namespace_prefix);
    if (target->namespace_prefix && !target->namespace_uri)
        return (FT_ERR_INVALID_ARGUMENT);

    const ft_string &value = source.get_value();
    const ft_size_t value_length = value.size();
    if (value_length != 0)
    {
        const char *value_data = value.c_str();
        if (ft_memchr(value_data, '\0', value_length))
            return (FT_ERR_INVALID_ARGUMENT);
        target->text = ft_span_dup(value_data, value_length);
        if (!target->text)
            return (FT_ERR_NO_MEMORY);
    }

    const auto &children = source.get_children();
    const ft_size_t child_count = children.size();
    if (child_count > target->children.capacity())
    {
        target->children.reserve(child_count);
        status = target->children.get_error();
        if (status != FT_ERR_SUCCESS)
            return (status);
    }
    index = 0;
    while (index < child_count)
    {
        const ft_dom_node *child = children[index];
        if (!child)
            return (FT_ERR_INVALID_ARGUMENT);
        status = target->children.push_back(ft_nullptr);
        if (status == FT_ERR_SUCCESS)
            status = xml_import_dom_node(*child, target->children[index], scope);
        if (status != FT_ERR_SUCCESS)
            return (status);
        ++index;
    }
    return (xml_node_prepare_thread_safety(target));
}

static int32_t xml_import_dom_node(const ft_dom_node &source, xml_node *&target,
        const xml_namespace_entry *scope) noexcept
{
    ft_bool lock_acquired = FT_FALSE;
    const int32_t lock_error = source.lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
        return (lock_error);
    const int32_t status = xml_import_dom_node_locked(source, target, scope);
    source.unlock(lock_acquired);
    return (status);
}

int32_t xml_document::replace_from_dom(const ft_dom_document &dom) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::replace_from_dom");
    xml_node *new_root = ft_nullptr;
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = dom.lock(&lock_acquired);
    if (status == FT_ERR_SUCCESS)
    {
        const ft_dom_node *source = dom.get_root();
        if (source)
            status = xml_import_dom_node(*source, new_root, ft_nullptr);
        else
            status = FT_ERR_INVALID_ARGUMENT;
        dom.unlock(lock_acquired);
    }
    if (status == FT_ERR_SUCCESS)
    {
        status = this->lock(&lock_acquired);
        if (status == FT_ERR_SUCCESS)
        {
            xml_node *previous_root = this->_root;
            this->_root = new_root;
            new_root = previous_root;
            this->unlock(lock_acquired);
        }
    }
    delete new_root;
    this->record_operation_error(status);
    return (status);
}

xml_document::xml_document() noexcept
    : _root(ft_nullptr), _mutex(ft_nullptr),
    _initialised_state(FT_CLASS_STATE_UNINITIALISED)
{
    return ;
}

xml_document::~xml_document() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        (void)this->destroy();
    return ;
}

int32_t xml_document::initialize() noexcept
{
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
        errno_abort_lifecycle(this->_initialised_state, "xml_document::initialize", "called while object is already initialised");
    this->_root = ft_nullptr;
    this->_mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    this->record_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t xml_document::initialize(const xml_document &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "xml_document::initialize(copy)", "source is uninitialised");
    const int32_t source_error = other.get_error();
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->record_operation_error(source_error);
        return (FT_ERR_SUCCESS);
    }

    xml_node *root_copy = ft_nullptr;
    ft_bool lock_acquired = FT_FALSE;
    int32_t status = other.lock(&lock_acquired);
    if (status == FT_ERR_SUCCESS)
    {
        if (other._root)
            status = xml_clone_node(*other._root, root_copy, ft_nullptr, ft_nullptr);
        other.unlock(lock_acquired);
    }
    if (status == FT_ERR_SUCCESS)
    {
        status = this->initialize();
        if (status == FT_ERR_SUCCESS && lock_acquired)
            status = this->enable_thread_safety();
    }
    if (status != FT_ERR_SUCCESS)
    {
        delete root_copy;
        (void)this->destroy();
        this->record_operation_error(status);
        return (status);
    }
    this->_root = root_copy;
    this->record_operation_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t xml_document::initialize(xml_document &&other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "xml_document::initialize(move)", "source is uninitialised");
    return (this->move(other));
}

int32_t xml_document::move(xml_document &other) noexcept
{
    if (this == &other)
        return (FT_ERR_SUCCESS);
    if (other._initialised_state == FT_CLASS_STATE_UNINITIALISED)
        errno_abort_lifecycle(other._initialised_state, "xml_document::move", "source is uninitialised");
    const int32_t source_error = other.get_error();
    if (this->_initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        const int32_t destroy_error = this->destroy();
        if (destroy_error != FT_ERR_SUCCESS)
            return (destroy_error);
    }
    if (other._initialised_state == FT_CLASS_STATE_DESTROYED)
    {
        this->_root = ft_nullptr;
        this->_mutex = ft_nullptr;
        this->_initialised_state = FT_CLASS_STATE_DESTROYED;
        this->record_operation_error(source_error);
        return (FT_ERR_SUCCESS);
    }
    this->_root = other._root;
    this->_mutex = other._mutex;
    other._root = ft_nullptr;
    other._mutex = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_INITIALISED;
    other._initialised_state = FT_CLASS_STATE_DESTROYED;
    this->record_operation_error(source_error);
    return (FT_ERR_SUCCESS);
}

int32_t xml_document::destroy() noexcept
{
    if (this->_initialised_state != FT_CLASS_STATE_INITIALISED)
        return (FT_ERR_SUCCESS);
    const int32_t disable_error = this->disable_thread_safety();
    delete this->_root;
    this->_root = ft_nullptr;
    this->_initialised_state = FT_CLASS_STATE_DESTROYED;
    this->record_operation_error(disable_error);
    return (disable_error);
}

void xml_document::record_operation_error(int32_t error_code) const noexcept
{
    (void)xml_document::set_error(error_code);
    return ;
}

int32_t xml_document::load_from_string(const char *xml) noexcept
{
    int32_t lock_error;
    int32_t parse_error;
    ft_bool lock_acquired;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::load_from_string");
    lock_acquired = FT_FALSE;
    lock_error = this->lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->record_operation_error(lock_error);
        return (lock_error);
    }
    if (!xml)
    {
        this->unlock(lock_acquired);
        this->record_operation_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    if (this->_root)
    {
        delete this->_root;
        this->_root = ft_nullptr;
    }
    xml_node *node = ft_nullptr;
    parse_error = FT_ERR_SUCCESS;
    const char *end = parse_node(xml, &node, ft_nullptr, &parse_error);
    if (!end)
    {
        int32_t error_code;

        error_code = parse_error;
        if (error_code == FT_ERR_SUCCESS)
            error_code = FT_ERR_INVALID_ARGUMENT;
        this->unlock(lock_acquired);
        this->record_operation_error(error_code);
        return (error_code);
    }
    const char *remaining = skip_whitespace(end);
    if (remaining && *remaining != '\0')
    {
        delete node;
        this->unlock(lock_acquired);
        this->record_operation_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    this->_root = node;
    this->unlock(lock_acquired);
    this->record_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

static int32_t xml_file_error() noexcept
{
    if (errno != 0)
        return (cmp_map_system_error_to_ft(errno));
    return (FT_ERR_INVALID_ARGUMENT);
}

static int32_t xml_read_stream_size(FILE *file, ft_size_t &size) noexcept
{
    if (cmp_file_stream_size(file, &size) == FT_ERR_SUCCESS)
        return (FT_ERR_SUCCESS);

    // Preserve support for seekable streams that cannot report metadata.
    errno = 0;
    if (std::fseek(file, 0, SEEK_END) != 0)
        return (xml_file_error());
    const int64_t stream_length = std::ftell(file);
    if (stream_length <= 0)
        return (FT_ERR_INVALID_ARGUMENT);
    errno = 0;
    if (std::fseek(file, 0, SEEK_SET) != 0)
        return (xml_file_error());
    size = static_cast<ft_size_t>(stream_length);
    return (FT_ERR_SUCCESS);
}

static int32_t read_file_content(const char *file_path, char **out_content)
{
    if (file_path == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    errno = 0;
    FILE *file = std::fopen(file_path, "rb");
    if (!file)
        return (xml_file_error());

    ft_size_t size = 0;
    int32_t status = xml_read_stream_size(file, size);
    if (status == FT_ERR_SUCCESS && (size == 0 || size >= SIZE_MAX))
        status = FT_ERR_INVALID_ARGUMENT;
    char *buffer = ft_nullptr;
    if (status == FT_ERR_SUCCESS)
    {
        buffer = static_cast<char *>(cma_malloc(size + 1));
        if (buffer == ft_nullptr)
            status = FT_ERR_NO_MEMORY;
    }
    if (status == FT_ERR_SUCCESS)
    {
        errno = 0;
        const size_t native_size = FT_UINT64_TO_NATIVE_SIZE_CAST(size);
        if (std::fread(buffer, 1, native_size, file) != native_size)
            status = xml_file_error();
    }
    std::fclose(file);
    if (status != FT_ERR_SUCCESS)
    {
        cma_free(buffer);
        return (status);
    }
    buffer[size] = '\0';
    *out_content = buffer;
    return (FT_ERR_SUCCESS);
}

int32_t xml_document::load_from_file(const char *file_path) noexcept
{
    char *content;
    int32_t   result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::load_from_file");
    content = ft_nullptr;
    result = read_file_content(file_path, &content);
    if (result != FT_ERR_SUCCESS)
    {
        this->record_operation_error(result);
        return (result);
    }
    result = this->load_from_string(content);
    cma_free(content);
    if (result == FT_ERR_SUCCESS)
        this->record_operation_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t xml_document::load_from_backend(ft_document_source &source) noexcept
{
    ft_string content;
    int32_t read_result;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::load_from_backend");
    read_result = content.initialize();
    if (read_result != FT_ERR_SUCCESS)
    {
        this->record_operation_error(read_result);
        return (read_result);
    }
    read_result = source.read_all(content);
    if (read_result != FT_ERR_SUCCESS)
    {
        this->record_operation_error(read_result);
        return (read_result);
    }
    return (this->load_from_string(content.c_str()));
}

static int32_t append_node_qualified_name(const xml_node *node, ft_string &buffer) noexcept
{
    const char *prefix = node->namespace_prefix;
    const char *local_name = node->local_name;
    int32_t status = FT_ERR_SUCCESS;
    if (prefix && prefix[0] != '\0')
    {
        status = buffer.append(prefix);
        if (status == FT_ERR_SUCCESS)
            status = buffer.append(':');
    }
    if (status != FT_ERR_SUCCESS)
        return (status);
    if (!local_name || local_name[0] == '\0')
        local_name = node->name;
    if (local_name)
        return (buffer.append(local_name));
    return (FT_ERR_SUCCESS);
}

static int32_t write_node(const xml_node *node, ft_string &buffer) noexcept
{
    if (!node)
        return (FT_ERR_INVALID_ARGUMENT);
    ft_bool lock_acquired = FT_FALSE;
    if (xml_node_lock(node, &lock_acquired) != FT_ERR_SUCCESS)
        return (FT_ERR_INVALID_STATE);
    int32_t status = buffer.append('<');
    if (status == FT_ERR_SUCCESS)
        status = append_node_qualified_name(node, buffer);
    if (status == FT_ERR_SUCCESS)
        status = buffer.append('>');
    if (status == FT_ERR_SUCCESS && node->text)
        status = buffer.append(node->text);
    ft_size_t index = 0;
    const ft_size_t child_count = node->children.size();
    while (status == FT_ERR_SUCCESS && index < child_count)
    {
        status = write_node(node->children[index], buffer);
        ++index;
    }
    if (status == FT_ERR_SUCCESS)
        status = buffer.append("</", 2);
    if (status == FT_ERR_SUCCESS)
        status = append_node_qualified_name(node, buffer);
    if (status == FT_ERR_SUCCESS)
        status = buffer.append('>');
    xml_node_unlock(node, lock_acquired);
    return (status);
}

int32_t xml_document::serialize(ft_string &buffer) const noexcept
{
    int32_t status = buffer.initialize();
    if (status != FT_ERR_SUCCESS)
        return (status);
    ft_bool lock_acquired = FT_FALSE;
    status = this->lock(&lock_acquired);
    if (status != FT_ERR_SUCCESS)
        return (status);
    status = write_node(this->_root, buffer);
    this->unlock(lock_acquired);
    if (status == FT_ERR_SUCCESS)
        status = buffer.append('\n');
    return (status);
}

char *xml_document::write_to_string() const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::write_to_string");
    ft_string content;
    const int32_t status = this->serialize(content);
    if (status != FT_ERR_SUCCESS)
    {
        this->record_operation_error(status);
        return (ft_nullptr);
    }
    const ft_size_t length = content.size() + 1;
    char *result = static_cast<char *>(cma_malloc(length));
    if (result == ft_nullptr)
    {
        this->record_operation_error(FT_ERR_NO_MEMORY);
        return (ft_nullptr);
    }
    ft_memcpy(result, content.c_str(), length);
    this->record_operation_error(FT_ERR_SUCCESS);
    return (result);
}

int32_t xml_document::write_to_string(ft_string &output) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::write_to_string");
    ft_string content;
    int32_t status = this->serialize(content);
    if (status == FT_ERR_SUCCESS)
    {
        if (output.is_initialised() == FT_FALSE)
            status = output.move(content);
        else
            status = output.assign(content.c_str(), content.size());
    }
    this->record_operation_error(status);
    return (status);
}

int32_t xml_document::write_to_file(const char *file_path) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::write_to_file");
    ft_string content;
    int32_t status = this->serialize(content);
    if (status != FT_ERR_SUCCESS)
    {
        this->record_operation_error(status);
        return (status);
    }
    if (file_path == ft_nullptr)
    {
        this->record_operation_error(FT_ERR_INVALID_ARGUMENT);
        return (FT_ERR_INVALID_ARGUMENT);
    }
    errno = 0;
    FILE *file = std::fopen(file_path, "wb");
    if (!file)
    {
        status = FT_ERR_INVALID_ARGUMENT;
        if (errno != 0)
            status = cmp_map_system_error_to_ft(errno);
        this->record_operation_error(status);
        return (status);
    }
    const size_t length = FT_UINT64_TO_NATIVE_SIZE_CAST(content.size());
    errno = 0;
    if (std::fwrite(content.c_str(), 1, length, file) != length)
    {
        status = FT_ERR_INVALID_ARGUMENT;
        if (errno != 0)
            status = cmp_map_system_error_to_ft(errno);
        std::fclose(file);
    }
    else
    {
        errno = 0;
        if (std::fclose(file) != 0)
        {
            status = FT_ERR_INVALID_ARGUMENT;
            if (errno != 0)
                status = cmp_map_system_error_to_ft(errno);
        }
    }
    this->record_operation_error(status);
    return (status);
}

int32_t xml_document::write_to_backend(ft_document_sink &sink) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::write_to_backend");
    ft_string content;
    int32_t status = this->serialize(content);
    if (status == FT_ERR_SUCCESS)
        status = sink.write_all(content.c_str(), content.size());
    this->record_operation_error(status);
    return (status);
}

xml_node *xml_document::get_root() const noexcept
{
    ft_bool lock_acquired;
    int32_t lock_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::get_root");
    lock_acquired = FT_FALSE;
    lock_error = this->lock(&lock_acquired);
    if (lock_error != FT_ERR_SUCCESS)
    {
        this->record_operation_error(lock_error);
        return (ft_nullptr);
    }
    xml_node *root = this->_root;
    this->unlock(lock_acquired);
    this->record_operation_error(FT_ERR_SUCCESS);
    return (root);
}

void xml_document::set_manual_error(int32_t error_code) noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::set_manual_error");
    this->record_operation_error(error_code);
    return ;
}

int32_t xml_document::get_error() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "xml_document::get_error");
    return (xml_document::_last_error);
}

const char *xml_document::get_error_str() const noexcept
{
    errno_abort_if_uninitialised(this->_initialised_state, "xml_document::get_error_str");
    int32_t error_code = this->get_error();
    const char *message = ft_strerror(error_code);

    if (message == ft_nullptr)
        message = "unknown error";
    return (message);
}

int32_t xml_document::enable_thread_safety() noexcept
{
    pt_recursive_mutex *mutex_pointer;
    int32_t mutex_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::enable_thread_safety");
    if (this->_mutex)
    {
        this->record_operation_error(FT_ERR_SUCCESS);
        return (FT_ERR_SUCCESS);
    }
    mutex_pointer = new (std::nothrow) pt_recursive_mutex();
    if (!mutex_pointer)
    {
        this->record_operation_error(FT_ERR_NO_MEMORY);
        return (FT_ERR_NO_MEMORY);
    }
    mutex_error = mutex_pointer->initialize();
    if (mutex_error != FT_ERR_SUCCESS)
    {
        delete mutex_pointer;
        this->record_operation_error(mutex_error);
        return (mutex_error);
    }
    this->_mutex = mutex_pointer;
    this->record_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

int32_t xml_document::disable_thread_safety() noexcept
{
    int32_t destroy_error;

    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::disable_thread_safety");
    if (!this->_mutex)
        return (FT_ERR_SUCCESS);
    destroy_error = this->_mutex->destroy();
    delete this->_mutex;
    this->_mutex = ft_nullptr;
    this->record_operation_error(destroy_error);
    return (destroy_error);
}

ft_bool xml_document::is_thread_safe() const noexcept
{
    return (this->_mutex != ft_nullptr);
}

int32_t xml_document::lock(ft_bool *lock_acquired) const noexcept
{
    ft_bool has_mutex;
    int32_t mutex_result;
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::lock");
    if (lock_acquired)
        *lock_acquired = FT_FALSE;
    has_mutex = (this->_mutex != ft_nullptr);
    mutex_result = pt_recursive_mutex_lock_if_not_null(this->_mutex);
    int32_t reported_error;

    reported_error = mutex_result;
    if (reported_error != FT_ERR_SUCCESS)
    {
        this->record_operation_error(reported_error);
        return (reported_error);
    }
    if (lock_acquired && has_mutex)
        *lock_acquired = FT_TRUE;
    this->record_operation_error(FT_ERR_SUCCESS);
    return (FT_ERR_SUCCESS);
}

void xml_document::unlock(ft_bool lock_acquired) const noexcept
{
    errno_abort_if_uninitialised_or_destroyed(this->_initialised_state, "xml_document::unlock");
    if (!lock_acquired)
        return ;
    (void)pt_recursive_mutex_unlock_if_not_null(this->_mutex);
    this->record_operation_error(FT_ERR_SUCCESS);
    return ;
}
