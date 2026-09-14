#include "scenelightparser.hpp"

namespace vre
{
SceneLightParser::SceneLightParser()
{
}

SceneLightParser::~SceneLightParser()
{
}

void SceneLightParser::parse(const JsonValue &root,
	std::vector<Light> *out_lights, float *out_ambient) const
{
	const JsonValue *ambient_field = root.find("ambient");
	if (ambient_field != nullptr)
		*out_ambient = static_cast<float>(ambient_field->as_number(0.12));

	const JsonValue *lights_field = root.find("lights");
	if (lights_field == nullptr || !lights_field->is_array())
		return ;

	for (const JsonValue &light_json : lights_field->array_elements())
	{
		Light light;
		std::string type_name = light_json.find("type") != nullptr
			? light_json.find("type")->as_string("directional")
			: "directional";
		light.set_type((type_name == "point")
			? Light::Type::Point : Light::Type::Directional);

		const char *position_field_name = (light.type() == Light::Type::Point)
			? "position" : "direction";
		const JsonValue *position_field = light_json.find(position_field_name);
		light.set_direction_or_position((position_field != nullptr)
			? position_field->as_vec3(vec3(0.0f, -1.0f, 0.0f))
			: vec3(0.0f, -1.0f, 0.0f));

		const JsonValue *color_field = light_json.find("color");
		light.set_color((color_field != nullptr)
			? color_field->as_vec3(vec3(1.0f, 1.0f, 1.0f))
			: vec3(1.0f, 1.0f, 1.0f));

		const JsonValue *intensity_field = light_json.find("intensity");
		light.set_intensity(static_cast<float>(
			(intensity_field != nullptr)
			? intensity_field->as_number(1.0) : 1.0));

		out_lights->push_back(light);
	}
}

} // namespace vre
