// Minimal JSON DOM. Paired with json_parser.hpp/.cpp.
//
// Written from scratch — same "no non-system, no FullLibft" reasoning as
// obj_loader/tga_loader (see verdict.md's dependency audit): FullLibft's
// JSon module drags in Advanced/Parser/CPP_class/System_utils/Template,
// which drags in the rest of the tree, for a feature this small.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace vre
{

enum class JsonType
{
    Null,
    Bool,
    Number,
    String,
    Array,
    Object,
};

class JsonValue
{
    public:
        JsonType type = JsonType::Null;
        bool bool_value = false;
        double number_value = 0.0;
        std::string string_value;
        std::vector<JsonValue> array_value;
        std::map<std::string, JsonValue> object_value;

        bool is_object() const { return type == JsonType::Object; }
        bool is_array() const { return type == JsonType::Array; }

        // Returns nullptr if this isn't an object or the key is absent —
        // callers treat a missing field as "use the default".
        const JsonValue *find(const std::string &key) const
        {
            if (type != JsonType::Object)
                return nullptr;
            auto it = object_value.find(key);
            if (it == object_value.end())
                return nullptr;
            return &it->second;
        }

        double as_number(double default_value = 0.0) const
        {
            return (type == JsonType::Number) ? number_value : default_value;
        }

        std::string as_string(const std::string &default_value = "") const
        {
            return (type == JsonType::String) ? string_value : default_value;
        }

        bool as_bool(bool default_value = false) const
        {
            return (type == JsonType::Bool) ? bool_value : default_value;
        }
};

} // namespace vre
