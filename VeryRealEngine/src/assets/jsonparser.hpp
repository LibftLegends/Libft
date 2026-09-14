/**
 * @file jsonparser.hpp
 * @brief Public entry point for parsing JSON: wires a JsonLexer and a
 * JsonGrammar together over some source text, or a file's contents.
 */
#pragma once

#include "../vre.hpp"
#include "jsongrammar.hpp"
#include "jsonlexer.hpp"
#include "jsonvalue.hpp"

namespace vre
{
class JsonParser
{
  public:
	JsonParser();
	JsonParser(const JsonParser &other);
	JsonParser &operator=(const JsonParser &other);
	~JsonParser();

	/**
		* @brief Parses `text` as JSON into *out_value.
		* @param text JSON source text.
		* @param out_value Receives the parsed DOM tree.
		* @return true on success.
		*/
	static bool parse(const std::string &text, JsonValue *out_value);

	/**
		* @brief Convenience: reads the file at `path` and parses it.
		* @param path Filesystem path to a JSON file.
		* @param out_value Receives the parsed DOM tree.
		* @return true on success.
		*/
	static bool load_file(const char *path, JsonValue *out_value);
};

} // namespace vre
