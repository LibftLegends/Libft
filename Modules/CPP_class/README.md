# CPP_class

The `CPP_class` module contains lifecycle-managed C++ utility classes used across the library: strings, big numbers, files/streams, byte buffers, and bitsets.

## Strings

- `ft_string` - Owning mutable string with lifecycle, assignment, append, comparison, indexing, conversion to `const char *`, `c_str`, `data`, `size`, `empty`, `find`, `erase`, `push_back`, `back`, `resize_length`, `substr`, `from_error`, error getters, and optional thread safety. `assign(const ft_string &)` replaces the text of an initialized string, reusing its buffer when possible and preserving its text if growth fails.
- `ft_string_proxy` - Error-propagating proxy returned by string concatenation operators. It supports chained `operator+`, conversion to `ft_string *`, and `get_error`.
- Free string operators - `operator+` overloads combine `ft_string`, `ft_string_proxy`, C strings, and characters; comparison overloads compare C strings with `ft_string`.

## Big Numbers

- `ft_big_number` - Arbitrary-size integer-like value with lifecycle, assignment, arithmetic operators, comparison operators, `assign`, `assign_base`, append/reduce helpers, `clear`, `c_str`, `size`, `empty`, sign checks, base conversion, modular exponentiation, error accessors, and optional thread safety.
- `ft_big_number_proxy` - Error-propagating arithmetic proxy with arithmetic operators, conversion to `ft_big_number *`, and `get_error`.

## Files and Streams

- `ft_file` - Lifecycle wrapper around a file descriptor with `open`, `write`, `write_buffer`, `read`, `seek`, `printf`, `copy_to`, `copy_to_with_buffer`, `close`, descriptor access, error accessors, and optional thread safety.
- `ft_istream` - Abstract lifecycle input stream with `read`, error accessors, and optional thread safety.
- `ft_fd_istream` - `ft_istream` implementation that reads from a file descriptor.
- `ft_istringstream` - `ft_istream` implementation that reads from in-memory string data.
- `ft_ofstream` - Output stream wrapper for writing files.
- `ft_file_stream` - File stream abstraction for read/write style workflows.
- `ft_stringbuf` - String-backed stream buffer.

## Buffers, Bitsets, and Utility Types

- `DataBuffer` - Lifecycle serialization buffer backed by `ft_vector<uint8_t>`. It exposes `clear`, `size`, `data`, `tell`, `seek`, `good`, `bad`, stream-style `operator<<`/`operator>>`, error accessors, and optional thread safety.
- `data_buffer_proxy` - Error-propagating proxy for chained `DataBuffer` stream operators.
- `ft_bitset` - Lifecycle dynamic bitset with `set`, `reset`, `flip`, `test`, `size`, `clear`, error accessors, lock helpers, and optional thread safety.
All lifecycle classes in this module use explicit `initialize`/`destroy` flows and expose `move(...)` instead of returning lifecycle objects by value.
