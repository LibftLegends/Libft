/**
 * @file grammar.hpp
 * @brief Recursive-descent JSON grammar productions, building a JsonValue
 * DOM tree from a JsonLexer's token stream.
 *
 * Supports the full JSON grammar (objects, arrays, strings with standard
 * escapes, numbers, true/false/null) minus unicode `\uXXXX` escapes, which
 * this engine's scene files don't need.
 */
#pragma once

#include "../vre.hpp"
#include "lexer.hpp"
#include "value.hpp"

namespace vre
{
class JsonGrammar
{
  public:
	JsonGrammar();
	JsonGrammar(const JsonGrammar &other);
	JsonGrammar &operator=(const JsonGrammar &other);
	~JsonGrammar();

	/// Productions are read from `lexer`, which must outlive this JsonGrammar.
	explicit JsonGrammar(JsonLexer *lexer);

	/// Parses one JSON value (object/array/string/number/bool/null) at
	/// the lexer's current position.
	bool parse_value(JsonValue *out_value);

  private:
	bool parse_object(JsonValue *out_value);
	bool parse_array(JsonValue *out_value);
	bool parse_string_value(JsonValue *out_value);
	bool parse_number(JsonValue *out_value);
	bool parse_bool(JsonValue *out_value);
	bool parse_null(JsonValue *out_value);

	JsonLexer *_lexer;
		///< Non-owning: the caller's lexer outlives this parse.
};

} // namespace vre
