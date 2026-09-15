#include "grammar.hpp"

namespace vre
{
JsonGrammar::JsonGrammar() : _lexer(nullptr)
{
}

JsonGrammar::JsonGrammar(const JsonGrammar &other) : _lexer(other._lexer)
{
}

JsonGrammar &JsonGrammar::operator=(const JsonGrammar &other)
{
	if (this != &other)
		_lexer = other._lexer;
	return (*this);
}

JsonGrammar::~JsonGrammar()
{
}

JsonGrammar::JsonGrammar(JsonLexer *lexer) : _lexer(lexer)
{
}

bool JsonGrammar::parse_value(JsonValue *out_value)
{
	char	c;

	_lexer->skip_whitespace();
	c = _lexer->peek();
	if (c == '{')
		return (parse_object(out_value));
	if (c == '[')
		return (parse_array(out_value));
	if (c == '"')
		return (parse_string_value(out_value));
	if (c == 't' || c == 'f')
		return (parse_bool(out_value));
	if (c == 'n')
		return (parse_null(out_value));
	if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
		return (parse_number(out_value));
	return (false);
}

bool JsonGrammar::parse_object(JsonValue *out_value)
{
	if (!_lexer->expect('{'))
		return (false);
	out_value->set_type(JsonType::Object);
	_lexer->skip_whitespace();
	if (_lexer->peek() == '}')
	{
		_lexer->advance();
		return (true);
	}
	while (true)
	{
		JsonValue	key_value;
		JsonValue	member;

		_lexer->skip_whitespace();
		if (!parse_string_value(&key_value))
			return (false);
		_lexer->skip_whitespace();
		if (!_lexer->expect(':'))
			return (false);
		if (!parse_value(&member))
			return (false);
		out_value->set_object_member(key_value.string_value(), member);
		_lexer->skip_whitespace();
		if (_lexer->expect(','))
			continue ;
		if (_lexer->expect('}'))
			return (true);
		return (false);
	}
}

bool JsonGrammar::parse_array(JsonValue *out_value)
{
	if (!_lexer->expect('['))
		return (false);
	out_value->set_type(JsonType::Array);
	_lexer->skip_whitespace();
	if (_lexer->peek() == ']')
	{
		_lexer->advance();
		return (true);
	}
	while (true)
	{
		JsonValue	element;

		if (!parse_value(&element))
			return (false);
		out_value->push_array_element(element);
		_lexer->skip_whitespace();
		if (_lexer->expect(','))
			continue ;
		if (_lexer->expect(']'))
			return (true);
		return (false);
	}
}

bool JsonGrammar::parse_string_value(JsonValue *out_value)
{
	std::string	result;

	if (!_lexer->consume_quoted_string(&result))
		return (false);
	out_value->set_type(JsonType::String);
	out_value->set_string_value(result);
	return (true);
}

bool JsonGrammar::parse_number(JsonValue *out_value)
{
	std::string	token;

	if (!_lexer->consume_number_token(&token))
		return (false);
	out_value->set_type(JsonType::Number);
	out_value->set_number_value(std::strtod(token.c_str(), nullptr));
	return (true);
}

bool JsonGrammar::parse_bool(JsonValue *out_value)
{
	if (_lexer->consume_literal("true"))
	{
		out_value->set_type(JsonType::Boolean);
		out_value->set_bool_value(true);
		return (true);
	}
	if (_lexer->consume_literal("false"))
	{
		out_value->set_type(JsonType::Boolean);
		out_value->set_bool_value(false);
		return (true);
	}
	return (false);
}

bool JsonGrammar::parse_null(JsonValue *out_value)
{
	if (!_lexer->consume_literal("null"))
		return (false);
	out_value->set_type(JsonType::Null);
	return (true);
}

} // namespace vre
