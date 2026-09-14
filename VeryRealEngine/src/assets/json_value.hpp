/**
 * @file json_value.hpp
 * @brief Minimal JSON DOM. Paired with json_parser.hpp/.cpp.
 *
 * Written from scratch — same "no non-system, no FullLibft" reasoning as
 * obj_loader/tga_loader (see verdict.md's dependency audit): FullLibft's
 * JSon module drags in Advanced/Parser/CPP_class/System_utils/Template,
 * which drags in the rest of the tree, for a feature this small.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{

/** Which JSON value kind a JsonValue currently holds. */
enum class JsonType
{
	Null,
	Bool,
	Number,
	String,
	Array,
	Object,
};

/** A single JSON value: a tagged union of null/bool/number/string/array/object,
	forming the parsed DOM tree. */
class JsonValue
{
  public:
	JsonValue();
	JsonValue(const JsonValue &other);
	JsonValue &operator=(const JsonValue &other);
	~JsonValue();

	/** @return Which kind of value this is. */
	JsonType type() const;
	void set_type(JsonType type);

	/** @return true if this value is a JSON object. */
	bool is_object() const;
	/** @return true if this value is a JSON array. */
	bool is_array() const;

	/**
		* @brief Looks up a key in this object.
		*
		* Returns nullptr if this isn't an object or the key is absent —
		* callers treat a missing field as "use the default".
		* @param key Object key to look up.
		* @return Pointer to the value, or nullptr if not found.
		*/
	const JsonValue *find(const std::string &key) const;

	/** @return This value as a number, or `default_value` if it isn't one. */
	double as_number(double default_value = 0.0) const;
	/** @return This value as a string, or `default_value` if it isn't one. */
	std::string as_string(const std::string &default_value = "") const;
	/** @return This value as a bool, or `default_value` if it isn't one. */
	bool as_bool(bool default_value = false) const;
	/**
		* @brief Reads this value as a 3-element JSON array, e.g. `[1.0, 2.0,
			3.0]`.
		* @return The parsed vec3,
			or `default_value` if this isn't a 3-element array.
		*/
	vec3 as_vec3(const vec3 &default_value) const;

	/** @return This value's array elements (meaningful only when is_array() is true). */
	const std::vector<JsonValue> &array_elements() const;

	/** @return This value's raw string payload (meaningful only when type() is JsonType::String). */
	const std::string &string_value() const;

	void set_bool_value(bool value);
	void set_number_value(double value);
	void set_string_value(const std::string &value);
	/** @brief Appends `value` to this array value (type() must already be JsonType::Array). */
	void push_array_element(const JsonValue &value);
	/** @brief Inserts or replaces `key` in this object value (type() must already be JsonType::Object). */
	void set_object_member(const std::string &key, const JsonValue &value);

  private:
	JsonType _type;
	bool _bool_value;
	double _number_value;
	std::string _string_value;
	std::vector<JsonValue> _array_value;
	std::map<std::string, JsonValue> _object_value;
};

} // namespace vre
