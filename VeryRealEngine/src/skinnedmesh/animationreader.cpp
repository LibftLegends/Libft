#include "animationreader.hpp"

namespace vre
{
SkinnedMeshAnimationReader::SkinnedMeshAnimationReader()
{
}

SkinnedMeshAnimationReader::SkinnedMeshAnimationReader(
	const SkinnedMeshAnimationReader &)
{
}

SkinnedMeshAnimationReader &SkinnedMeshAnimationReader::operator=(
	const SkinnedMeshAnimationReader &)
{
	return (*this);
}

SkinnedMeshAnimationReader::~SkinnedMeshAnimationReader()
{
}

bool SkinnedMeshAnimationReader::read(const JsonValue &root,
	AnimationClip *out_clip)
{
	const JsonValue	*animation_field;
	const JsonValue	*tracks_field;
	const JsonValue	*field;

	animation_field = root.find("animation");
	if (animation_field == nullptr || !animation_field->is_object())
		return (false);
	field = animation_field->find("duration");
	out_clip->set_duration(static_cast<float>(
			(field != nullptr) ? field->as_number(1.0) : 1.0));
	tracks_field = animation_field->find("tracks");
	if (tracks_field == nullptr || !tracks_field->is_array())
		return (true);
	for (const JsonValue &track_json : tracks_field->array_elements())
	{
		AnimationTrack track;

		field = track_json.find("bone");
		track.set_bone_index(static_cast<uint32_t>(
				(field != nullptr) ? field->as_number(0) : 0));
		const JsonValue *keyframes_field = track_json.find("keyframes");
		if (keyframes_field != nullptr && keyframes_field->is_array())
		{
			for (const JsonValue &keyframe_json
				: keyframes_field->array_elements())
			{
				Keyframe keyframe;

				field = keyframe_json.find("time");
				keyframe.set_time(static_cast<float>(
						(field != nullptr) ? field->as_number(0.0) : 0.0));
				field = keyframe_json.find("rotation");
				keyframe.set_rotation((field != nullptr)
					? field->as_vec3(vec3(0.0f, 0.0f, 0.0f))
					: vec3(0.0f, 0.0f, 0.0f));
				track.keyframes().push_back(keyframe);
			}
		}
		out_clip->tracks().push_back(track);
	}
	return (true);
}

} // namespace vre
