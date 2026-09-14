#include "json_parser.hpp"

namespace vre
{

JsonParser::JsonParser()
{
}

JsonParser::JsonParser(const JsonParser &)
{
}

JsonParser &JsonParser::operator=(const JsonParser &)
{
	return (*this);
}

JsonParser::~JsonParser()
{
}

bool JsonParser::parse(const std::string &text, JsonValue *out_value)
{
	TextParser parser(text);
	return (parser.parse(out_value));
}

bool JsonParser::load_file(const char *path, JsonValue *out_value)
{
	std::ifstream file(path);
	if (!file.is_open())
	{
		std::fprintf(stderr, "json_parser: failed to open \"%s\"\n", path);
		return (false);
	}
	std::ostringstream buffer;
	buffer << file.rdbuf();
	if (!parse(buffer.str(), out_value))
	{
		std::fprintf(stderr, "json_parser: failed to parse \"%s\"\n", path);
		return (false);
	}
	return (true);
}

JsonParser::TextParser::TextParser() : _text(nullptr), _position(0)
{
}

JsonParser::TextParser::TextParser(const TextParser &other) : _text(other._text),
	_position(other._position)
{
}

JsonParser::TextParser &JsonParser::TextParser::operator=(const TextParser &other)
{
	if (this != &other)
	{
		_text = other._text;
		_position = other._position;
	}
	return (*this);
}

JsonParser::TextParser::~TextParser()
{
}

JsonParser::TextParser::TextParser(const std::string &text) : _text(&text),
	_position(0)
{
}

bool JsonParser::TextParser::parse(JsonValue *out_value)
{
	skip_whitespace();
	if (!parse_value(out_value))
		return (false);
	skip_whitespace();
	return (true);
}

char JsonParser::TextParser::peek() const
{
	return ((_position < _text->size()) ? (*_text)[_position] : '\0');
}

char JsonParser::TextParser::advance()
{
	return ((_position < _text->size()) ? (*_text)[_position++] : '\0');
}

void JsonParser::TextParser::skip_whitespace()
{
	while (_position < _text->size()
		&& std::isspace(static_cast<unsigned char>(peek())))
		_position++;
}

bool JsonParser::TextParser::expect(char c)
{
	if (peek() != c)
		return (false);
	_position++;
	return (true);
}

bool JsonParser::TextParser::parse_value(JsonValue *out_value)
{
	char	c;

	skip_whitespace();
	c = peek();
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

} // namespace vre
