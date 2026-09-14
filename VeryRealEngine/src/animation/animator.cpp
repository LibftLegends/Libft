#include "animator.hpp"

namespace vre
{
Animator::Animator() : _skeleton(nullptr), _clip(nullptr), _time(0.0f)
{
}

Animator::Animator(const Animator &other) : _skeleton(other._skeleton),
	_clip(other._clip), _time(other._time)
{
}

Animator &Animator::operator=(const Animator &other)
{
	if (this != &other)
	{
		_skeleton = other._skeleton;
		_clip = other._clip;
		_time = other._time;
	}
	return (*this);
}

Animator::~Animator()
{
}

Animator::Animator(const Skeleton *skeleton,
	const AnimationClip *clip) : _skeleton(skeleton), _clip(clip), _time(0.0f)
{
}

void Animator::update(float delta_seconds)
{
	_time += delta_seconds;
	if (_clip->duration() > 0.0f)
		_time = std::fmod(_time, _clip->duration());
	// fmod can return negative for a negative dividend; not expected
	// here, but cheap to guard
	if (_time < 0.0f)
		_time += _clip->duration();
}

vec3 Animator::sample_track(const AnimationTrack &track, float time)
{
	float	span;
	float	t;

	const std::vector<Keyframe> &keys = track.keyframes();
	if (keys.empty())
		return (vec3(0.0f, 0.0f, 0.0f));
	if (time <= keys.front().time())
		return (keys.front().rotation());
	if (time >= keys.back().time())
		return (keys.back().rotation());
	for (size_t i = 0; i + 1 < keys.size(); i++)
	{
		if (time < keys[i].time() || time > keys[i + 1].time())
			continue ;
		span = keys[i + 1].time() - keys[i].time();
		t = (span > 0.0f) ? (time - keys[i].time()) / span : 0.0f;
		return (keys[i].rotation() + (keys[i + 1].rotation()
				- keys[i].rotation()) * t);
	}
	return (keys.back().rotation());
		// unreachable given the bounds checks above; safe fallback
}

void Animator::compute_bone_matrices(std::vector<mat4> *out_matrices) const
{
	const std::vector<Bone> &bones = _skeleton->bones();
	out_matrices->resize(bones.size());

	std::vector<mat4> animated_world(bones.size());
	for (size_t i = 0; i < bones.size(); i++)
	{
		vec3 rotation = bones[i].bind_local_rotation();
		for (const AnimationTrack &track : _clip->tracks())
		{
			if (track.bone_index() != i)
				continue ;
			rotation = sample_track(track, _time);
			break ; // at most one track per bone in a well-formed clip
		}

		mat4 local = mat4::compose(bones[i].bind_local_position(), rotation,
				vec3(1.0f, 1.0f, 1.0f));
		if (bones[i].parent_index() == Bone::no_parent_index())
			animated_world[i] = local;
		else
			animated_world[i] = mat4::multiply(
					animated_world[bones[i].parent_index()], local);
	}

	for (size_t i = 0; i < bones.size(); i++)
	{
		(*out_matrices)[i] = mat4::multiply(animated_world[i],
				_skeleton->bind_pose_inverse(i));
	}
}

} // namespace vre
