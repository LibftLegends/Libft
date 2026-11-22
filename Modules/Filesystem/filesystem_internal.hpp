#ifndef FILESYSTEM_INTERNAL_HPP
# define FILESYSTEM_INTERNAL_HPP

# include "filesystem.hpp"

ft_string *filesystem_normalize_portable_slashes(ft_string *value) noexcept;
ft_string *filesystem_normalize_safe_relative_path(const char *path);

#endif
