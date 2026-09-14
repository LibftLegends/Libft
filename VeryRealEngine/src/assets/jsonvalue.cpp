#include "jsonvalue.hpp"

namespace vre
{
JsonValue::JsonValue() : _type(JsonType::Null), _bool_value(false),
	_number_value(0.0)
{
}

JsonValue::JsonValue(const JsonValue &other) : _type(other._type),
	_bool_value(other._bool_value), _number_value(other._number_value),
	_string_value(other._string_value), _array_value(other._array_value),
	_object_value(other._object_value)
{
}

JsonValue &JsonValue::operator=(const JsonValue &other)
{
	if (this != &other)
	{
		_type = other._type;
		_bool_value = other._bool_value;
		_number_value = other._number_value;
		_string_value = other._string_value;
		_array_value = other._array_value;
		_object_value = other._object_value;
	}
	return (*this);
}

JsonValue::~JsonValue()
{
}

JsonType JsonValue::type() const
{
	return (_type);
}

void JsonValue::set_type(JsonType type)
{
	_type = type;
}

bool JsonValue::is_object() const
{
	return (_type == JsonType::Object);
}

bool JsonValue::is_array() const
{
	return (_type == JsonType::Array);
}

const JsonValue *JsonValue::find(const std::string &key) const
{
	if (_type != JsonType::Object)
		return (nullptr);
	auto it = _object_value.find(key);
	if (it == _object_value.end())
		return (nullptr);
	return (&it->second);
}

double JsonValue::as_number(double default_value) const
{
	return ((_type == JsonType::Number) ? _number_value : default_value);
}

std::string JsonValue::as_string(const std::string &default_value) const
{
	return ((_type == JsonType::String) ? _string_value : default_value);
}

bool JsonValue::as_bool(bool default_value) const
{
	return ((_type == JsonType::Boolean) ? _bool_value : default_value);
}

vec3 JsonValue::as_vec3(const vec3 &default_value) const
{
	if (_type != JsonType::Array || _array_value.size() != 3)
		return (default_value);
	return (vec3(static_cast<float>(_array_value[0].as_number()),
			static_cast<float>(_array_value[1].as_number()),
			static_cast<float>(_array_value[2].as_number())));
}

const std::vector<JsonValue> &JsonValue::array_elements() const
{
	return (_array_value);
}

const std::string &JsonValue::string_value() const
{
	return (_string_value);
}

void JsonValue::set_bool_value(bool value)
{
	_bool_value = value;
}

void JsonValue::set_number_value(double value)
{
	_number_value = value;
}

void JsonValue::set_string_value(const std::string &value)
{
	_string_value = value;
}

void JsonValue::push_array_element(const JsonValue &value)
{
	_array_value.push_back(value);
}

void JsonValue::set_object_member(const std::string &key,
	const JsonValue &value)
{
	_object_value[key] = value;
}

} // namespace vre
