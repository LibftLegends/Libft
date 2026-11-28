#include "printf_internal.hpp"
#include "../Basic/class_nullptr.hpp"
#include "../Errno/errno.hpp"
#include <atomic>
#include <pthread.h>
#include <climits>

struct s_pf_custom_specifier_entry
{
    std::atomic<ft_bool> registered{FT_FALSE};
    t_pf_custom_formatter handler = ft_nullptr;
    void *context = ft_nullptr;
};

static pthread_mutex_t g_pf_custom_specifiers_mutex = PTHREAD_MUTEX_INITIALIZER;
static s_pf_custom_specifier_entry g_pf_custom_specifiers[UCHAR_MAX + 1];

int32_t pf_enable_thread_safety(void)
{
    return (FT_ERR_SUCCESS);
}

int32_t pf_disable_thread_safety(void)
{
    return (FT_ERR_SUCCESS);
}

ft_bool pf_has_custom_specifier(char specifier)
{
    const auto &entry = g_pf_custom_specifiers[static_cast<uint8_t>(specifier)];
    return (entry.registered.load(std::memory_order_acquire));
}

int32_t pf_register_custom_specifier(char specifier, t_pf_custom_formatter handler, void *context)
{
    if (handler == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (pthread_mutex_lock(&g_pf_custom_specifiers_mutex) != 0)
        return (FT_ERR_INTERNAL);
    auto &entry = g_pf_custom_specifiers[static_cast<uint8_t>(specifier)];
    int32_t error = FT_ERR_ALREADY_EXISTS;
    if (entry.handler == ft_nullptr)
    {
        entry.context = context;
        entry.handler = handler;
        entry.registered.store(FT_TRUE, std::memory_order_release);
        error = FT_ERR_SUCCESS;
    }
    (void)pthread_mutex_unlock(&g_pf_custom_specifiers_mutex);
    return (error);
}

int32_t pf_unregister_custom_specifier(char specifier)
{
    if (pthread_mutex_lock(&g_pf_custom_specifiers_mutex) != 0)
        return (FT_ERR_INTERNAL);
    auto &entry = g_pf_custom_specifiers[static_cast<uint8_t>(specifier)];
    entry.handler = ft_nullptr;
    entry.context = ft_nullptr;
    entry.registered.store(FT_FALSE, std::memory_order_release);
    (void)pthread_mutex_unlock(&g_pf_custom_specifiers_mutex);
    return (FT_ERR_SUCCESS);
}

int32_t pf_try_format_custom_specifier(char specifier, va_list *arguments,
    ft_string &output, ft_bool *handled)
{
    if (handled == ft_nullptr)
        return (FT_ERR_INVALID_ARGUMENT);
    *handled = FT_FALSE;
    if (arguments == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    if (pf_has_custom_specifier(specifier) == FT_FALSE)
        return (FT_ERR_SUCCESS);

    if (pthread_mutex_lock(&g_pf_custom_specifiers_mutex) != 0)
        return (FT_ERR_INTERNAL);
    const auto &entry = g_pf_custom_specifiers[static_cast<uint8_t>(specifier)];
    const t_pf_custom_formatter handler = entry.handler;
    void *context = entry.context;
    (void)pthread_mutex_unlock(&g_pf_custom_specifiers_mutex);
    if (handler == ft_nullptr)
        return (FT_ERR_SUCCESS);

    const int32_t error = output.initialize();
    if (error != FT_ERR_SUCCESS)
        return (error);
    *handled = FT_TRUE;
    return (handler(arguments, output, context));
}
