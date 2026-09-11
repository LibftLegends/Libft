/**
 * @file json_parser.hpp
 * @brief Recursive-descent parser producing a JsonValue DOM tree.
 */
#pragma once

#include "json_value.hpp"

namespace vre
{

/**
 * @brief Parses `text` as JSON into *out_value.
 *
 * Supports the full JSON grammar (objects, arrays, strings with standard
 * escapes, numbers, true/false/null) minus unicode `\\uXXXX` escapes, which
 * the engine's scene files don't need.
 * @param text JSON source text.
 * @param out_value Receives the parsed DOM tree.
 * @return true on success.
 */
bool parse_json(const std::string &text, JsonValue *out_value);

/**
 * @brief Convenience: reads the file at `path` and parses it.
 * @param path Filesystem path to a JSON file.
 * @param out_value Receives the parsed DOM tree.
 * @return true on success.
 */
bool load_json_file(const char *path, JsonValue *out_value);

} // namespace vre
