#include "../../../Modules/Compatebility/compatebility_internal.hpp"
#include "../../../Modules/Errno/errno.hpp"

#if defined(_WIN32) || defined(_WIN64)
# include <fcntl.h>
# include <io.h>

int32_t main()
{
    const int32_t first_free = _open("NUL", _O_RDONLY);
    if (first_free < 3 || _close(first_free) != 0)
        return (1);

    cmp_initialize_standard_file_descriptors();
    const HANDLE output = cmp_retrieve_handle(1);
    const HANDLE error = cmp_retrieve_handle(2);
    if (output == INVALID_HANDLE_VALUE || error == INVALID_HANDLE_VALUE
        || output != reinterpret_cast<HANDLE>(_get_osfhandle(1))
        || error != reinterpret_cast<HANDLE>(_get_osfhandle(2)))
        return (1);

    cmp_initialize_standard_file_descriptors();
    if (cmp_retrieve_handle(1) != output || cmp_retrieve_handle(2) != error)
        return (1);

    const int32_t next_free = _open("NUL", _O_RDONLY);
    if (next_free < 0)
        return (1);
    if (_close(next_free) != 0 || next_free != first_free)
        return (1);

    const char message[] = "standard descriptors initialized\n";
    int64_t written = 0;
    if (cmp_write(1, message, sizeof(message) - 1U, &written) != FT_ERR_SUCCESS
        || written != static_cast<int64_t>(sizeof(message) - 1U))
        return (1);
    return (0);
}
#endif
