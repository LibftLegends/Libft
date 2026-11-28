#include "system_utils.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../CPP_class/class_string.hpp"
#include "../Errno/errno.hpp"
#include <locale>
#include <string>
#include <stdexcept>
#include <typeinfo>
#include <new>
static int32_t assign_locale(const char *locale_name, std::locale &locale_object)
{
    try
    {
        if (locale_name == ft_nullptr || *locale_name == '\0')
        {
            try
            {
                locale_object = std::locale("");
            }
            catch (const std::runtime_error &)
            {
                locale_object = std::locale::classic();
            }
        }
        else if (locale_name[0] == 'C' && locale_name[1] == '\0')
            locale_object = std::locale::classic();
        else
        {
            static thread_local std::string cached_name;
            static thread_local std::locale cached_locale = std::locale::classic();
            if (cached_name != locale_name)
            {
                const std::locale resolved(locale_name);
                cached_name.assign(locale_name);
                cached_locale = resolved;
            }
            locale_object = cached_locale;
        }
    }
    catch (const std::bad_alloc &)
    {
        return (-1);
    }
    catch (const std::runtime_error &)
    {
        return (-1);
    }
    return (0);
}

int32_t su_locale_compare(const char *left, const char *right, const char *locale_name, int32_t *result)
{
    if (left == ft_nullptr || right == ft_nullptr || result == ft_nullptr)
        return (-1);
    std::locale locale_object = std::locale::classic();
    if (assign_locale(locale_name, locale_object) != 0)
        return (-1);
    try
    {
        const auto &collate = std::use_facet<std::collate<char> >(locale_object);
        *result = collate.compare(left, left + ft_strlen_size_t(left),
            right, right + ft_strlen_size_t(right));
    }
    catch (const std::bad_cast &)
    {
        return (-1);
    }
    catch (const std::bad_alloc &)
    {
        return (-1);
    }
    return (0);
}

int32_t su_locale_casefold(const char *input, const char *locale_name, ft_string &output)
{
    if (input == ft_nullptr)
        return (-1);
    std::locale locale_object = std::locale::classic();
    if (assign_locale(locale_name, locale_object) != 0)
        return (-1);
    try
    {
        const auto &ctype = std::use_facet<std::ctype<char> >(locale_object);
        const ft_size_t length = ft_strlen_size_t(input);
        char stack_buffer[128];
        if (length <= sizeof(stack_buffer))
        {
            (void)ft_memcpy(stack_buffer, input, length);
            ctype.tolower(stack_buffer, stack_buffer + length);
            if (output.assign(stack_buffer, length) != FT_ERR_SUCCESS)
                return (-1);
            return (0);
        }
        std::string transformed(input, input + length);
        ctype.tolower(transformed.data(), transformed.data() + transformed.size());
        if (output.assign(transformed.data(), length) != FT_ERR_SUCCESS)
            return (-1);
    }
    catch (const std::bad_cast &)
    {
        return (-1);
    }
    catch (const std::bad_alloc &)
    {
        return (-1);
    }
    return (0);
}
