# URI

The `URI` module parses, normalizes, percent-encodes, and queries URI text. The caller releases returned strings and byte buffers with `cma_free`.

## Types

- `uri_components` - Parsed URI fields. Members are `scheme`, `userinfo`, `host`, `port`, `path`, `query`, and `fragment`; each field is an allocated C string or null when absent.

## Public API

- `uri_components_reset(uri_components *components)` - Resets all component pointers to null without freeing existing allocations.
- `uri_components_destroy(uri_components *components)` - Frees all allocated component strings and resets the structure.
- `uri_parse(const char *input, ft_size_t input_size, uri_components *components)` - Parses a URI span into individual components.
- `uri_normalize(const char *input, ft_size_t input_size)` - Allocates a normalized URI string.
- `uri_percent_encode_component(const uint8_t *input, ft_size_t input_size, ft_bool keep_slash)` - Percent-encodes bytes for a URI component, optionally preserving `/`.
- `uri_percent_decode_component(const char *input, ft_size_t input_size, ft_size_t *output_size)` - Decodes a percent-encoded URI component into an allocated byte buffer.
- `uri_query_get_value(const char *query, ft_size_t query_size, const char *key, ft_size_t key_size)` - Finds a query parameter by key and returns its decoded value.
- `uri_get_error()` - Returns the URI module's last error code.
- `uri_get_error_str()` - Returns readable text for the URI module's last error.
- `uri_query_has_key(...)` - Checks the first matching parameter and validates its value without allocating. A parameter can have an empty value.
