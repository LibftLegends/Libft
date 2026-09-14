#include "bonereader.hpp"

namespace vre
{
SkinnedMeshBoneReader::SkinnedMeshBoneReader()
{
}

SkinnedMeshBoneReader::SkinnedMeshBoneReader(
	const SkinnedMeshBoneReader &)
{
}

SkinnedMeshBoneReader &SkinnedMeshBoneReader::operator=(
	const SkinnedMeshBoneReader &)
{
	return (*this);
}

SkinnedMeshBoneReader::~SkinnedMeshBoneReader()
{
}

bool SkinnedMeshBoneReader::read(const JsonValue &bones_field,
	Skeleton *out_skeleton)
{
	const JsonValue	*field;

	for (const JsonValue &bone_json : bones_field.array_elements())
	{
		Bone bone;
		field = bone_json.find("name");
		bone.set_name((field != nullptr) ? field->as_string() : "");
		field = bone_json.find("parent");
		bone.set_parent_index((field != nullptr)
			? static_cast<int32_t>(field->as_number(-1))
			: Bone::no_parent_index());
		field = bone_json.find("position");
		bone.set_bind_local_position((field != nullptr)
			? field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
			: vec3(0.0f, 0.0f, 0.0f));
		field = bone_json.find("rotation");
		bone.set_bind_local_rotation((field != nullptr)
			? field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
			: vec3(0.0f, 0.0f, 0.0f));
		out_skeleton->bones().push_back(bone);
	}
	out_skeleton->compute_bind_pose();
	return (true);
}

} // namespace vre
