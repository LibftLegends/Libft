/**
 * @file jsonlexer.hpp
 * @brief Character-level cursor over JSON source text: whitespace
 * skipping, single-character lookahead/consumption, and the token-level
 * reads (a quoted string, a number, a keyword literal) every
 * JsonGrammar production is built from.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class JsonLexer
{
  public:
	JsonLexer();
	JsonLexer(const JsonLexer &other);
	JsonLexer &operator=(const JsonLexer &other);
	~JsonLexer();

	explicit JsonLexer(const std::string &text);

	/// @return The next unconsumed character, or '\0' at end of input.
	char peek() const;
	/// Consumes and returns the next character, or '\0' at end of input.
	char advance();
	/// Advances past any run of whitespace.
	void skip_whitespace();
	/// Consumes `c` if it's next. @return false (consuming nothing) otherwise.
	bool expect(char c);

	/// Consumes `literal` (e.g. "true") if it's next. @return false
	/// (consuming nothing) otherwise.
	bool consume_literal(const char *literal);
	/**
		* @brief Consumes a `"..."`-delimited string, resolving the standard
		* backslash escapes (not `\uXXXX`, which this engine's scene files
		* don't need).
		* @param out_value Receives the unescaped string contents.
		* @return false on a missing opening quote, an unterminated string,
		* or an unsupported escape.
		*/
	bool consume_quoted_string(std::string *out_value);
	/**
		* @brief Consumes a JSON number token: `-?digits(.digits)?
		* ([eE][+-]?digits)?`.
		* @param out_token Receives the raw token text, ready for std::strtod.
		* @return false if nothing matching a number was consumed.
		*/
	bool consume_number_token(std::string *out_token);

  private:
	const std::string *_text;
		///< Non-owning: the caller's text outlives this parse.
	size_t _position;
};

} // namespace vre
