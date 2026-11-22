#include "api_internal.hpp"
#include "../Basic/basic.hpp"
#include <charconv>
#include <limits>

ft_bool api_append_content_length_header(ft_string &request, ft_size_t content_length)
{
    constexpr char HEADER[] = "\r\nContent-Length: ";
    constexpr ft_size_t HEADER_LENGTH = sizeof(HEADER) - 1U;
    constexpr ft_size_t DIGIT_CAPACITY = std::numeric_limits<ft_size_t>::digits10 + 1U;
    if (!request.is_initialised() && request.initialize() != FT_ERR_SUCCESS)
        return (FT_FALSE);
    char record[HEADER_LENGTH + DIGIT_CAPACITY];
    ft_memcpy_fixed<HEADER_LENGTH>(record, HEADER);
    const auto result = std::to_chars(record + HEADER_LENGTH, record + sizeof(record), content_length);
    return (request.append(record, static_cast<ft_size_t>(result.ptr - record)) == FT_ERR_SUCCESS);
}
