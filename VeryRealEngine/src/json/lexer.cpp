#include "lexer.hpp"

namespace vre
{
JsonLexer::JsonLexer() : _text(nullptr), _position(0)
{
}

JsonLexer::JsonLexer(const JsonLexer &other) : _text(other._text),
	_position(other._position)
{
}

JsonLexer &JsonLexer::operator=(const JsonLexer &other)
{
	if (this != &other)
	{
		_text = other._text;
		_position = other._position;
	}
	return (*this);
}

JsonLexer::~JsonLexer()
{
}

JsonLexer::JsonLexer(const std::string &text) : _text(&text), _position(0)
{
}

char JsonLexer::peek() const
{
	return ((_position < _text->size()) ? (*_text)[_position] : '\0');
}

char JsonLexer::advance()
{
	return ((_position < _text->size()) ? (*_text)[_position++] : '\0');
}

void JsonLexer::skip_whitespace()
{
	while (_position < _text->size()
		&& std::isspace(static_cast<unsigned char>(peek())))
		_position++;
}

bool JsonLexer::expect(char c)
{
	if (peek() != c)
		return (false);
	_position++;
	return (true);
}

bool JsonLexer::consume_literal(const char *literal)
{
	size_t	length;

	length = std::strlen(literal);
	if (_text->compare(_position, length, literal) != 0)
		return (false);
	_position += length;
	return (true);
}

bool JsonLexer::consume_quoted_string(std::string *out_value)
{
	char	c;
	char	escaped;

	if (!expect('"'))
		return (false);
	out_value->clear();
	while (true)
	{
		if (_position >= _text->size())
			return (false);
		c = advance();
		if (c == '"')
			return (true);
		if (c != '\\')
		{
			out_value->push_back(c);
			continue ;
		}
		escaped = advance();
		switch (escaped)
		{
			case '"': out_value->push_back('"'); break ;
			case '\\': out_value->push_back('\\'); break ;
			case '/': out_value->push_back('/'); break ;
			case 'n': out_value->push_back('\n'); break ;
			case 't': out_value->push_back('\t'); break ;
			case 'r': out_value->push_back('\r'); break ;
			case 'b': out_value->push_back('\b'); break ;
			case 'f': out_value->push_back('\f'); break ;
			default: return (false);
				// \uXXXX and friends: not needed by scene files
		}
	}
}

bool JsonLexer::consume_number_token(std::string *out_token)
{
	size_t	start;

	start = _position;
	if (peek() == '-')
		advance();
	while (std::isdigit(static_cast<unsigned char>(peek())))
		advance();
	if (peek() == '.')
	{
		advance();
		while (std::isdigit(static_cast<unsigned char>(peek())))
			advance();
	}
	if (peek() == 'e' || peek() == 'E')
	{
		advance();
		if (peek() == '+' || peek() == '-')
			advance();
		while (std::isdigit(static_cast<unsigned char>(peek())))
			advance();
	}
	if (_position == start)
		return (false);
	*out_token = _text->substr(start, _position - start);
	return (true);
}

} // namespace vre
