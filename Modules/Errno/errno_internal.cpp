#include "errno_internal.hpp"
#include "errno.hpp"
#include "../Basic/basic.hpp"
#include "../Basic/class_nullptr.hpp"
#include <charconv>
#include <cstdio>

int32_t errno_write_stderr(const char *string) noexcept
{
    size_t write_length;
    size_t write_result;

    if (string == ft_nullptr)
        return (FT_ERR_INVALID_POINTER);
    write_length = ft_strlen_size_t(string);
    write_result = std::fwrite(string, 1, write_length, stderr);
    if (write_result != write_length)
        return (FT_ERR_SYSTEM);
    if (std::fflush(stderr) != 0)
        return (FT_ERR_SYSTEM);
    return (FT_ERR_SUCCESS);
}

int32_t errno_write_stderr_fragments(const char *const *fragments,
    ft_size_t fragment_count) noexcept
{
    char buffer[512];
    ft_size_t buffered_size = 0U;
    ft_size_t index = 0U;
    while (index < fragment_count)
    {
        const char *fragment = fragments[index++];
        const ft_size_t size = ft_strlen_size_t(fragment);
        if (size > sizeof(buffer) - buffered_size && buffered_size != 0U)
        {
            if (std::fwrite(buffer, 1U, buffered_size, stderr) != buffered_size)
                return (FT_ERR_SYSTEM);
            buffered_size = 0U;
        }
        if (size > sizeof(buffer))
        {
            if (std::fwrite(fragment, 1U, size, stderr) != size)
                return (FT_ERR_SYSTEM);
        }
        else if (size != 0U)
        {
            ft_memcpy(buffer + buffered_size, fragment, size);
            buffered_size += size;
        }
    }
    if (buffered_size != 0U
        && std::fwrite(buffer, 1U, buffered_size, stderr) != buffered_size)
        return (FT_ERR_SYSTEM);
    if (std::fflush(stderr) != 0)
        return (FT_ERR_SYSTEM);
    return (FT_ERR_SUCCESS);
}

void    errno_abort_lifecycle(uint8_t initialised_state, const char *method_name,
            const char *reason)
{
#ifdef LIBFT_TEST_BUILD
    static_cast<void>(initialised_state);
    static_cast<void>(method_name);
    static_cast<void>(reason);
#else
    if (method_name == ft_nullptr)
        method_name = "unknown";
    if (reason == ft_nullptr)
        reason = "unknown";
    char state[4];
    const auto converted = std::to_chars(state, state + sizeof(state) - 1U,
            static_cast<uint32_t>(initialised_state));
    *converted.ptr = '\0';
    const char *const fragments[] = {"lifecycle error: state=", state,
        " method=", method_name, " reason=", reason, "\n"};
    errno_write_stderr_fragments(fragments, sizeof(fragments) / sizeof(fragments[0]));
#endif
    su_abort();
    return ;
}

void    errno_abort_if_uninitialised(uint8_t initialised_state,
            const char *method_name)
{
    if (initialised_state != FT_CLASS_STATE_UNINITIALISED)
    {
        return ;
    }
    errno_abort_lifecycle(initialised_state, method_name,
        "called while object is uninitialised");
    return ;
}

void    errno_abort_if_uninitialised_or_destroyed(uint8_t initialised_state,
            const char *method_name)
{
    if (initialised_state == FT_CLASS_STATE_INITIALISED)
    {
        return ;
    }
    errno_abort_lifecycle(initialised_state, method_name,
        "called while object is uninitialised or destroyed");
    return ;
}
