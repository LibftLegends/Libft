#include "jsonparser.hpp"

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
	JsonLexer	lexer(text);
	JsonGrammar	grammar(&lexer);

	lexer.skip_whitespace();
	if (!grammar.parse_value(out_value))
		return (false);
	lexer.skip_whitespace();
	return (true);
}

bool JsonParser::load_file(const char *path, JsonValue *out_value)
{
	std::ostringstream	buffer;

	std::ifstream file(path);
	if (!file.is_open())
	{
		std::fprintf(stderr, "jsonparser: failed to open \"%s\"\n", path);
		return (false);
	}
	buffer << file.rdbuf();
	if (!parse(buffer.str(), out_value))
	{
		std::fprintf(stderr, "jsonparser: failed to parse \"%s\"\n", path);
		return (false);
	}
	return (true);
}

} // namespace vre
