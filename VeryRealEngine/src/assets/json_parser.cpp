#include "json_parser.hpp"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace vre
{

namespace
{

// A tiny recursive-descent parser over the whole text held in memory —
// scene files are small hand-authored documents, not a streaming-parser
// use case.
class Parser
{
    public:
        explicit Parser(const std::string &text) : _text(text), _position(0) {}

        bool parse(JsonValue *out_value)
        {
            skip_whitespace();
            if (!parse_value(out_value))
                return false;
            skip_whitespace();
            return true;
        }

    private:
        const std::string &_text;
        size_t _position;

        char peek() const { return (_position < _text.size()) ? _text[_position] : '\0'; }
        char advance() { return (_position < _text.size()) ? _text[_position++] : '\0'; }

        void skip_whitespace()
        {
            while (_position < _text.size() && std::isspace(static_cast<unsigned char>(peek())))
                _position++;
        }

        bool expect(char c)
        {
            if (peek() != c)
                return false;
            _position++;
            return true;
        }

        bool parse_value(JsonValue *out_value)
        {
            skip_whitespace();
            char c = peek();
            if (c == '{')
                return parse_object(out_value);
            if (c == '[')
                return parse_array(out_value);
            if (c == '"')
                return parse_string_value(out_value);
            if (c == 't' || c == 'f')
                return parse_bool(out_value);
            if (c == 'n')
                return parse_null(out_value);
            if (c == '-' || std::isdigit(static_cast<unsigned char>(c)))
                return parse_number(out_value);
            return false;
        }

        bool parse_object(JsonValue *out_value)
        {
            if (!expect('{'))
                return false;
            out_value->type = JsonType::Object;

            skip_whitespace();
            if (peek() == '}')
            {
                _position++;
                return true;
            }

            while (true)
            {
                skip_whitespace();
                JsonValue key_value;
                if (!parse_string_value(&key_value))
                    return false;
                skip_whitespace();
                if (!expect(':'))
                    return false;

                JsonValue member;
                if (!parse_value(&member))
                    return false;
                out_value->object_value[key_value.string_value] = member;

                skip_whitespace();
                if (expect(','))
                    continue;
                if (expect('}'))
                    return true;
                return false;
            }
        }

        bool parse_array(JsonValue *out_value)
        {
            if (!expect('['))
                return false;
            out_value->type = JsonType::Array;

            skip_whitespace();
            if (peek() == ']')
            {
                _position++;
                return true;
            }

            while (true)
            {
                JsonValue element;
                if (!parse_value(&element))
                    return false;
                out_value->array_value.push_back(element);

                skip_whitespace();
                if (expect(','))
                    continue;
                if (expect(']'))
                    return true;
                return false;
            }
        }

        bool parse_string_value(JsonValue *out_value)
        {
            std::string result;
            if (!parse_raw_string(&result))
                return false;
            out_value->type = JsonType::String;
            out_value->string_value = result;
            return true;
        }

        bool parse_raw_string(std::string *out_string)
        {
            if (!expect('"'))
                return false;
            out_string->clear();
            while (true)
            {
                if (_position >= _text.size())
                    return false;
                char c = advance();
                if (c == '"')
                    return true;
                if (c == '\\')
                {
                    char escaped = advance();
                    switch (escaped)
                    {
                        case '"': out_string->push_back('"'); break;
                        case '\\': out_string->push_back('\\'); break;
                        case '/': out_string->push_back('/'); break;
                        case 'n': out_string->push_back('\n'); break;
                        case 't': out_string->push_back('\t'); break;
                        case 'r': out_string->push_back('\r'); break;
                        case 'b': out_string->push_back('\b'); break;
                        case 'f': out_string->push_back('\f'); break;
                        default: return false; // \uXXXX and friends: not needed by scene files
                    }
                }
                else
                {
                    out_string->push_back(c);
                }
            }
        }

        bool parse_number(JsonValue *out_value)
        {
            size_t start = _position;
            if (peek() == '-')
                _position++;
            while (std::isdigit(static_cast<unsigned char>(peek())))
                _position++;
            if (peek() == '.')
            {
                _position++;
                while (std::isdigit(static_cast<unsigned char>(peek())))
                    _position++;
            }
            if (peek() == 'e' || peek() == 'E')
            {
                _position++;
                if (peek() == '+' || peek() == '-')
                    _position++;
                while (std::isdigit(static_cast<unsigned char>(peek())))
                    _position++;
            }
            if (_position == start)
                return false;

            out_value->type = JsonType::Number;
            out_value->number_value = std::strtod(_text.substr(start, _position - start).c_str(), nullptr);
            return true;
        }

        bool parse_bool(JsonValue *out_value)
        {
            if (_text.compare(_position, 4, "true") == 0)
            {
                _position += 4;
                out_value->type = JsonType::Bool;
                out_value->bool_value = true;
                return true;
            }
            if (_text.compare(_position, 5, "false") == 0)
            {
                _position += 5;
                out_value->type = JsonType::Bool;
                out_value->bool_value = false;
                return true;
            }
            return false;
        }

        bool parse_null(JsonValue *out_value)
        {
            if (_text.compare(_position, 4, "null") == 0)
            {
                _position += 4;
                out_value->type = JsonType::Null;
                return true;
            }
            return false;
        }
};

} // namespace

bool parse_json(const std::string &text, JsonValue *out_value)
{
    Parser parser(text);
    return parser.parse(out_value);
}

bool load_json_file(const char *path, JsonValue *out_value)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::fprintf(stderr, "json_parser: failed to open \"%s\"\n", path);
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (!parse_json(buffer.str(), out_value))
    {
        std::fprintf(stderr, "json_parser: failed to parse \"%s\"\n", path);
        return false;
    }
    return true;
}

} // namespace vre
