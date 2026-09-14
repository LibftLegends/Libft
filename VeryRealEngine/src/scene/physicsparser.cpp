#include "physicsparser.hpp"

namespace vre
{
ScenePhysicsParser::ScenePhysicsParser()
{
}

ScenePhysicsParser::~ScenePhysicsParser()
{
}

bool ScenePhysicsParser::parse(const JsonValue &object_json,
	const std::string &name, const vec3 &position,
	RigidBodyDesc *out_desc) const
{
	const JsonValue *physics_field = object_json.find("physics");
	if (physics_field == nullptr || !physics_field->is_object())
		return (false);

	out_desc->set_position(position);
	out_desc->set_debug_name(name);

	const JsonValue *static_field = physics_field->find("static");
	out_desc->set_static((static_field != nullptr)
		? static_field->as_bool(false) : false);

	const JsonValue *trigger_field = physics_field->find("trigger");
	out_desc->set_trigger((trigger_field != nullptr)
		? trigger_field->as_bool(false) : false);

	const JsonValue *mass_field = physics_field->find("mass");
	out_desc->set_mass(static_cast<float>(
		(mass_field != nullptr) ? mass_field->as_number(1.0) : 1.0));

	const JsonValue *restitution_field = physics_field->find("restitution");
	out_desc->set_restitution(static_cast<float>((restitution_field != nullptr)
		? restitution_field->as_number(0.3) : 0.3));

	const JsonValue *friction_field = physics_field->find("friction");
	out_desc->set_friction(static_cast<float>((friction_field != nullptr)
		? friction_field->as_number(0.5) : 0.5));

	const JsonValue *velocity_field = physics_field->find("initial_velocity");
	out_desc->set_velocity((velocity_field != nullptr)
		? velocity_field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
		: vec3(0.0f, 0.0f, 0.0f));

	std::string shape = physics_field->find("collider") != nullptr
		? physics_field->find("collider")->as_string("box") : "box";
	if (shape == "sphere")
	{
		out_desc->collider().set_type(Collider::Type::Sphere);
		const JsonValue *radius_field = physics_field->find("radius");
		out_desc->collider().set_radius(static_cast<float>(
			(radius_field != nullptr) ? radius_field->as_number(0.5) : 0.5));
	}
	else
	{
		out_desc->collider().set_type(Collider::Type::Box);
		const JsonValue *half_extents_field = physics_field->find("half_extents");
		out_desc->collider().set_half_extents((half_extents_field != nullptr)
			? half_extents_field->as_vec3(vec3(0.5f, 0.5f, 0.5f))
			: vec3(0.5f, 0.5f, 0.5f));
	}

	return (true);
}

} // namespace vre
