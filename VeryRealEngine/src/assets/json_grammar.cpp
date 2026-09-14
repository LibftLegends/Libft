/**
 * @file json_grammar.cpp
 * @brief JsonParser::TextParser's per-grammar-production methods, split out
 * of json_parser.cpp purely to keep each file under this project's
 * 250-line cap — both files define methods of the same nested
 * JsonParser::TextParser class declared in json_parser.hpp.
 */
#include "json_parser.hpp"

namespace vre
{

bool JsonParser::TextParser::parse_object(JsonValue *out_value)
{
    if (!expect('{'))
        return (false);
    out_value->set_type(JsonType::Object);

    skip_whitespace();
    if (peek() == '}')
    {
        advance();
        return (true);
    }

    while (true)
    {
        skip_whitespace();
        JsonValue key_value;
        if (!parse_string_value(&key_value))
            return (false);
        skip_whitespace();
        if (!expect(':'))
            return (false);

        JsonValue member;
        if (!parse_value(&member))
            return (false);
        out_value->set_object_member(key_value.string_value(), member);

        skip_whitespace();
        if (expect(','))
            continue;
        if (expect('}'))
            return (true);
        return (false);
    }
}

bool JsonParser::TextParser::parse_array(JsonValue *out_value)
{
    if (!expect('['))
        return (false);
    out_value->set_type(JsonType::Array);

    skip_whitespace();
    if (peek() == ']')
    {
        advance();
        return (true);
    }

    while (true)
    {
        JsonValue element;
        if (!parse_value(&element))
            return (false);
        out_value->push_array_element(element);

        skip_whitespace();
        if (expect(','))
            continue;
        if (expect(']'))
            return (true);
        return (false);
    }
}

bool JsonParser::TextParser::parse_string_value(JsonValue *out_value)
{
    std::string result;
    if (!parse_raw_string(&result))
        return (false);
    out_value->set_type(JsonType::String);
    out_value->set_string_value(result);
    return (true);
}

bool JsonParser::TextParser::parse_raw_string(std::string *out_string)
{
    if (!expect('"'))
        return (false);
    out_string->clear();
    while (true)
    {
        if (_position >= _text->size())
            return (false);
        char c = advance();
        if (c == '"')
            return (true);
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
                default: return (false); // \uXXXX and friends: not needed by scene files
            }
        }
        else
        {
            out_string->push_back(c);
        }
    }
}

bool JsonParser::TextParser::parse_number(JsonValue *out_value)
{
    size_t start = _position;
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

    out_value->set_type(JsonType::Number);
    out_value->set_number_value(std::strtod(_text->substr(start, _position - start).c_str(), nullptr));
    return (true);
}

bool JsonParser::TextParser::parse_bool(JsonValue *out_value)
{
    if (_text->compare(_position, 4, "true") == 0)
    {
        _position += 4;
        out_value->set_type(JsonType::Bool);
        out_value->set_bool_value(true);
        return (true);
    }
    if (_text->compare(_position, 5, "false") == 0)
    {
        _position += 5;
        out_value->set_type(JsonType::Bool);
        out_value->set_bool_value(false);
        return (true);
    }
    return (false);
}

bool JsonParser::TextParser::parse_null(JsonValue *out_value)
{
    if (_text->compare(_position, 4, "null") == 0)
    {
        _position += 4;
        out_value->set_type(JsonType::Null);
        return (true);
    }
    return (false);
}

} // namespace vre
