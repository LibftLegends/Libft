#include "skinnedmeshvertexreader.hpp"

namespace vre
{
SkinnedMeshVertexReader::SkinnedMeshVertexReader()
{
}

SkinnedMeshVertexReader::SkinnedMeshVertexReader(
	const SkinnedMeshVertexReader &)
{
}

SkinnedMeshVertexReader &SkinnedMeshVertexReader::operator=(
	const SkinnedMeshVertexReader &)
{
	return (*this);
}

SkinnedMeshVertexReader::~SkinnedMeshVertexReader()
{
}

bool SkinnedMeshVertexReader::read_vec4_floats(const JsonValue *value,
	float out[4], float default_value)
{
	for (int i = 0; i < 4; i++)
		out[i] = default_value;
	if (value == nullptr || !value->is_array())
		return (false);
	for (size_t i = 0; i < value->array_elements().size() && i < 4; i++)
		out[i] = static_cast<float>(value->array_elements()[i].as_number());
	return (true);
}

MeshVertex SkinnedMeshVertexReader::read(const JsonValue &vertex_json)
{
	MeshVertex			vertex;
	const JsonValue		*field;
	vec3				position;
	vec3				normal;
	float				bone_indices[4];
	float				bone_weights[4];
	bool				has_weights;

	field = vertex_json.find("position");
	position = (field != nullptr) ? field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
		: vec3(0.0f, 0.0f, 0.0f);
	field = vertex_json.find("normal");
	normal = (field != nullptr) ? field->as_vec3(vec3(0.0f, 1.0f, 0.0f))
		: vec3(0.0f, 1.0f, 0.0f);
	vertex.set_position(0, position.x());
	vertex.set_position(1, position.y());
	vertex.set_position(2, position.z());
	vertex.set_normal(0, normal.x());
	vertex.set_normal(1, normal.y());
	vertex.set_normal(2, normal.z());

	field = vertex_json.find("uv");
	if (field != nullptr && field->is_array()
		&& field->array_elements().size() == 2)
	{
		vertex.set_uv(0,
			static_cast<float>(field->array_elements()[0].as_number()));
		vertex.set_uv(1,
			static_cast<float>(field->array_elements()[1].as_number()));
	}

	read_vec4_floats(vertex_json.find("bone_indices"), bone_indices, 0.0f);
	has_weights = read_vec4_floats(vertex_json.find("bone_weights"),
			bone_weights, 0.0f);
	if (!has_weights)
		bone_weights[0] = 1.0f; // default: fully bound to bone 0 (see above)
	for (int i = 0; i < 4; i++)
		vertex.set_bone_index(i, bone_indices[i] + 1.0f); // +1: see above
	for (int i = 0; i < 4; i++)
		vertex.set_bone_weight(i, bone_weights[i]);
	return (vertex);
}

} // namespace vre
