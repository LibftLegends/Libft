#include "skeleton.hpp"

#include <algorithm>
#include <cmath>

namespace vre
{

void Skeleton::compute_bind_pose()
{
    std::vector<mat4> bind_world(bones.size());
    for (size_t i = 0; i < bones.size(); i++)
    {
        mat4 local = mat4::compose(bones[i].bind_local_position, bones[i].bind_local_rotation,
            vec3(1.0f, 1.0f, 1.0f)); // rigid only — see this file's header comment
        bind_world[i] = (bones[i].parent_index == kNoParentBone)
            ? local
            : mat4::multiply(bind_world[bones[i].parent_index], local);
    }

    _bind_pose_inverse.resize(bones.size());
    for (size_t i = 0; i < bones.size(); i++)
        _bind_pose_inverse[i] = mat4::inverse(bind_world[i]);
}

namespace
{

/// Linearly interpolates a track's rotation at `time`, clamping at both ends
/// rather than extrapolating past the first/last keyframe.
vec3 sample_track(const AnimationTrack &track, float time)
{
    const std::vector<Keyframe> &keys = track.keyframes;
    if (keys.empty())
        return vec3(0.0f, 0.0f, 0.0f);
    if (time <= keys.front().time)
        return keys.front().rotation;
    if (time >= keys.back().time)
        return keys.back().rotation;

    for (size_t i = 0; i + 1 < keys.size(); i++)
    {
        if (time < keys[i].time || time > keys[i + 1].time)
            continue;
        float span = keys[i + 1].time - keys[i].time;
        float t = (span > 0.0f) ? (time - keys[i].time) / span : 0.0f;
        return keys[i].rotation + (keys[i + 1].rotation - keys[i].rotation) * t;
    }
    return keys.back().rotation; // unreachable given the bounds checks above; safe fallback
}

} // namespace

void Animator::update(float delta_seconds)
{
    _time += delta_seconds;
    if (_clip->duration > 0.0f)
        _time = std::fmod(_time, _clip->duration);
    if (_time < 0.0f) // fmod can return negative for a negative dividend; not expected here, but cheap to guard
        _time += _clip->duration;
}

void Animator::compute_bone_matrices(std::vector<mat4> *out_matrices) const
{
    const std::vector<Bone> &bones = _skeleton->bones;
    out_matrices->resize(bones.size());

    std::vector<mat4> animated_world(bones.size());
    for (size_t i = 0; i < bones.size(); i++)
    {
        vec3 rotation = bones[i].bind_local_rotation;
        for (const AnimationTrack &track : _clip->tracks)
        {
            if (track.bone_index != i)
                continue;
            rotation = sample_track(track, _time);
            break; // at most one track per bone in a well-formed clip
        }

        mat4 local = mat4::compose(bones[i].bind_local_position, rotation,
            vec3(1.0f, 1.0f, 1.0f));
        animated_world[i] = (bones[i].parent_index == kNoParentBone)
            ? local
            : mat4::multiply(animated_world[bones[i].parent_index], local);
    }

    for (size_t i = 0; i < bones.size(); i++)
    {
        (*out_matrices)[i] = mat4::multiply(animated_world[i],
            _skeleton->bind_pose_inverse(i));
    }
}

} // namespace vre
