#include "compatebility_internal.hpp"
#include "../Errno/errno.hpp"
#if defined(_WIN32) || defined(_WIN64)
int32_t cmp_syslog_open(const char *identifier)
{
    (void)identifier;
    return (FT_ERR_UNSUPPORTED_TYPE);
}

void cmp_syslog_write(const char *message)
{
    (void)message;
    return ;
}

void cmp_syslog_close(void)
{
    return ;
}
#else
#include "../Basic/basic.hpp"
#include <limits>
#include <new>
#include <syslog.h>

struct s_cmp_syslog_configuration
{
    char *identifier = ft_nullptr;
    ft_bool configured = FT_FALSE;

    ~s_cmp_syslog_configuration()
    {
        if (this->configured != FT_FALSE)
            cmp_syslog_close();
        return ;
    }
};

static s_cmp_syslog_configuration g_syslog_configuration;

int32_t cmp_syslog_open(const char *identifier)
{
    const char *previous_identifier = g_syslog_configuration.identifier;
    if (g_syslog_configuration.configured != FT_FALSE
        && (identifier == previous_identifier || (identifier != ft_nullptr
            && previous_identifier != ft_nullptr && ft_strcmp(identifier, previous_identifier) == 0)))
        return (FT_ERR_SUCCESS);

    char *owned_identifier = ft_nullptr;
    if (identifier != ft_nullptr)
    {
        const ft_size_t length = ft_strlen_size_t(identifier);
        if (length >= std::numeric_limits<std::size_t>::max())
            return (FT_ERR_OUT_OF_RANGE);
        owned_identifier = new (std::nothrow) char[static_cast<std::size_t>(length + 1U)];
        if (owned_identifier == ft_nullptr)
            return (FT_ERR_NO_MEMORY);
        ft_memcpy(owned_identifier, identifier, length + 1U);
    }
    else if (previous_identifier != ft_nullptr)
        closelog();
    openlog(owned_identifier, LOG_PID | LOG_CONS, LOG_USER);
    delete[] previous_identifier;
    g_syslog_configuration.identifier = owned_identifier;
    g_syslog_configuration.configured = FT_TRUE;
    return (FT_ERR_SUCCESS);
}

void cmp_syslog_write(const char *message)
{
    syslog(LOG_INFO, "%s", message);
    return ;
}

void cmp_syslog_close(void)
{
    closelog();
    delete[] g_syslog_configuration.identifier;
    g_syslog_configuration.identifier = ft_nullptr;
    g_syslog_configuration.configured = FT_FALSE;
    return ;
}
#endif
