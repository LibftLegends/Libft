#include "json_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_big_number.hpp"
#include "../CMA/CMA.hpp"
#include "../Errno/errno.hpp"
#include <new>

json_item *json_create_item_owned(char *key, char *value)
{
    json_item *item = new (std::nothrow) json_item{};
    if (item == ft_nullptr)
        return (ft_nullptr);
    if (json_item_enable_thread_safety(item) != FT_ERR_SUCCESS)
    {
        delete item;
        return (ft_nullptr);
    }
    item->key = key;
    item->value = value;
    json_item_refresh_numeric_state(item);
    return (item);
}

static json_item *json_create_item_with_value(const char *key, char *value)
{
    if (value == ft_nullptr)
        return (ft_nullptr);
    char *owned_key = adv_strdup(key);
    if (owned_key == ft_nullptr)
    {
        cma_free(value);
        return (ft_nullptr);
    }
    json_item *item = json_create_item_owned(owned_key, value);
    if (item == ft_nullptr)
    {
        cma_free(owned_key);
        cma_free(value);
    }
    return (item);
}

json_item *json_create_item(const char *key, const char *value)
{
    return (json_create_item_with_value(key, adv_strdup(value)));
}

json_item *json_create_item(const char *key, const ft_bool value)
{
    if (value == FT_TRUE)
        return (json_create_item_with_value(key, adv_strdup("true")));
    return (json_create_item_with_value(key, adv_strdup("false")));
}

json_item *json_create_item(const char *key, const int32_t value)
{
    return (json_create_item_with_value(key, adv_itoa(value)));
}

json_item *json_create_item(const char *key, const ft_big_number &value)
{
    return (json_create_item(key, value.c_str()));
}
