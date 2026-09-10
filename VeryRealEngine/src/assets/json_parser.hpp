#pragma once

#include "json_value.hpp"

namespace vre
{

// Parses `text` as JSON into *out_value. Supports the full JSON grammar
// (objects, arrays, strings with standard escapes, numbers, true/false/null)
// minus unicode \uXXXX escapes, which the engine's scene files don't need.
bool parse_json(const std::string &text, JsonValue *out_value);

// Convenience: reads the file at `path` and parses it.
bool load_json_file(const char *path, JsonValue *out_value);

} // namespace vre
