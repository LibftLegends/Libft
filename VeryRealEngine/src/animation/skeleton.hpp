/**
 * @file skeleton.hpp
 * @brief Bonus "Skeletal Animation" (Chapter VII): a from-scratch bone
 * hierarchy, keyframe animation clip, and the Animator that turns "elapsed
 * time" into the per-bone skinning matrices Renderer::draw_frame() uploads
 * for GPU linear-blend skinning (see mesh_data.hpp's MeshVertex doc comment
 * and mesh.vert's skin_matrix computation).
 *
 * Deliberately simple, matching this engine's actual demo content (a
 * hand-authored 2-3 bone rig, one looping clip): Euler-angle rotation
 * keyframes with linear interpolation, not quaternions/slerp — correct and
 * artifact-free for the small per-keyframe rotation deltas this project's
 * own animation actually uses, at the cost of not being a general-purpose
 * animation system (gimbal lock and non-shortest-path interpolation are
 * real limitations of Euler lerp for large or compound rotations — a
 * documented scope line, not an oversight).
 */
#pragma once

#include "../math/vre_math.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace vre
{

/// Sentinel meaning "no parent" (a root bone).
constexpr int32_t kNoParentBone = -1;

/**
 * @brief One bone's place in the hierarchy and its bind (rest) pose local
 * transform, relative to its parent.
 */
struct Bone
{
    std::string name;
    int32_t parent_index = kNoParentBone;
    vec3 bind_local_position;
    vec3 bind_local_rotation; ///< Euler radians (X, Y, Z, applied Z*Y*X — see mat4::compose).
};

/**
 * @brief A bone hierarchy plus its precomputed bind pose, shared by every
 * Animator that plays a clip on this skeleton.
 */
class Skeleton
{
    public:
        std::vector<Bone> bones;

        /**
         * @brief Computes and caches each bone's inverse bind-pose world
         * matrix — call once after `bones` is fully populated (see
         * skinned_mesh_loader.cpp), before any Animator uses this skeleton.
         */
        void compute_bind_pose();

        /// @return Bone `index`'s cached inverse bind-pose world matrix (see compute_bind_pose()).
        const mat4 &bind_pose_inverse(size_t index) const { return _bind_pose_inverse[index]; }

    private:
        std::vector<mat4> _bind_pose_inverse;
};

/// One bone's rotation at one point in time, in an AnimationTrack.
struct Keyframe
{
    float time = 0.0f;
    vec3 rotation; ///< Euler radians, local (replaces the bone's bind_local_rotation at this time).
};

/// One bone's full set of keyframes over an AnimationClip's duration. Keyframes must be sorted by time.
struct AnimationTrack
{
    uint32_t bone_index = 0;
    std::vector<Keyframe> keyframes;
};

/// A named, timed collection of per-bone keyframe tracks — one looping animation.
struct AnimationClip
{
    float duration = 1.0f; ///< Seconds; playback loops back to 0 after this.
    std::vector<AnimationTrack> tracks; ///< Bones with no track here just hold their bind pose.
};

/**
 * @brief Plays one AnimationClip on one Skeleton: advances a clock and
 * produces the resulting skinning matrices each frame.
 */
class Animator
{
    public:
        Animator(const Skeleton *skeleton, const AnimationClip *clip)
            : _skeleton(skeleton), _clip(clip) {}

        /// Advances playback time by `delta_seconds`, looping at the clip's duration.
        void update(float delta_seconds);

        /**
         * @brief Computes this frame's skinning matrix for every bone:
         * `animated_world(bone) * inverse(bind_world(bone))` — the standard
         * skinning-matrix formula, so a vertex bound at the bind pose ends
         * up exactly where the animated skeleton currently puts that bone,
         * not offset by the bind pose's own transform as well.
         * @param out_matrices Resized to `_skeleton->bones.size()` and filled in bone order.
         */
        void compute_bone_matrices(std::vector<mat4> *out_matrices) const;

    private:
        const Skeleton *_skeleton;
        const AnimationClip *_clip;
        float _time = 0.0f;
};

} // namespace vre
