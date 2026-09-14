/**
 * @file json_parser.hpp
 * @brief Recursive-descent parser producing a JsonValue DOM tree.
 *
 * Supports the full JSON grammar (objects, arrays, strings with standard
 * escapes, numbers, true/false/null) minus unicode `\uXXXX` escapes, which
 * this engine's scene files don't need.
 */
#pragma once

#include "../vre.hpp"
#include "json_value.hpp"

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

  private:
	// Pure implementation detail (the actual recursive-descent state
	// machine) — kept nested rather than a top-level class, since
	// nothing outside JsonParser::parse() ever needs it directly.
	class TextParser
	{
		public:
		TextParser();
		TextParser(const TextParser &other);
		TextParser &operator=(const TextParser &other);
		~TextParser();

		explicit TextParser(const std::string &text);

		bool parse(JsonValue *out_value);

		private:
		char peek() const;
		char advance();
		void skip_whitespace();
		bool expect(char c);
		bool parse_value(JsonValue *out_value);
		bool parse_object(JsonValue *out_value);
		bool parse_array(JsonValue *out_value);
		bool parse_string_value(JsonValue *out_value);
		bool parse_raw_string(std::string *out_string);
		bool parse_number(JsonValue *out_value);
		bool parse_bool(JsonValue *out_value);
		bool parse_null(JsonValue *out_value);

		const std::string *_text;
			///< Non-owning: the caller's text outlives this parse.
		size_t _position;
	};
};

} // namespace vre
