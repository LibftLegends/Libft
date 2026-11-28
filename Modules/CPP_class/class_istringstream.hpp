#ifndef FT_ISTRINGSTREAM_HPP
#define FT_ISTRINGSTREAM_HPP

#include "class_string.hpp"
#include "class_istream.hpp"
#include "class_stringbuf.hpp"

class ft_istringstream : public ft_istream
{
    friend ft_istringstream &operator>>(ft_istringstream &input, int32_t &value);
    friend ft_istringstream &operator>>(ft_istringstream &input, ft_string &value);

#ifdef LIBFT_TEST_BUILD
    public:
#else
    private:
#endif
        ft_stringbuf _buffer;

    public:
        ft_istringstream() noexcept;
        ft_istringstream(const ft_istringstream &) noexcept = delete;
        ft_istringstream(ft_istringstream &&) noexcept = delete;
        ~ft_istringstream() noexcept;

        ft_istringstream &operator=(const ft_istringstream &) = delete;
        ft_istringstream &operator=(ft_istringstream &&other) = delete;

        int32_t initialize(const ft_string &string) noexcept;
        int32_t destroy() noexcept;
        int32_t move(ft_istringstream &other) noexcept;

        ft_string *get_string() const;

    protected:
        ssize_t do_read(char *buffer, ft_size_t count);
};

ft_istringstream &operator>>(ft_istringstream &input, int32_t &value);
ft_istringstream &operator>>(ft_istringstream &input, ft_string &value);

#endif
