/**
 * @file vre_math.hpp
 * @brief Minimal, dependency-free math types for VeryRealEngine.
 *
 * Deliberately not reusing FullLibft/Modules/Math: that module pulls in
 * CMA, PThread, RNG, Template and friends (see verdict.md), which would drag
 * the whole FullLibft tree into an engine that must stand on its own.
 */
#pragma once

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vre
{

/// A 3-component vector, used throughout the engine for positions, directions, and colors.
struct vec3
{
    float x;
    float y;
    float z;

    vec3() : x(0.0f), y(0.0f), z(0.0f) {}
    vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}

    vec3 operator+(const vec3 &o) const { return vec3(x + o.x, y + o.y, z + o.z); }
    vec3 operator-(const vec3 &o) const { return vec3(x - o.x, y - o.y, z - o.z); }
    vec3 operator*(float s) const { return vec3(x * s, y * s, z * s); }

    /// @return The dot product of `a` and `b`.
    static float dot(const vec3 &a, const vec3 &b)
    {
        return (a.x * b.x + a.y * b.y + a.z * b.z);
    }

    /// @return The cross product of `a` and `b`.
    static vec3 cross(const vec3 &a, const vec3 &b)
    {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }

    /// @return `v` scaled to unit length, or the zero vector if `v` is (numerically) zero-length.
    static vec3 normalize(const vec3 &v)
    {
        float length = std::sqrt(dot(v, v));
        if (length <= 0.0f)
            return vec3(0.0f, 0.0f, 0.0f);
        return vec3(v.x / length, v.y / length, v.z / length);
    }
};

/**
 * @brief Column-major 4x4 matrix, laid out the way Vulkan/GLSL expect it
 * (matches the memory layout of `mat4` in std140/push-constant blocks).
 */
struct mat4
{
    float m[16];

    /// @return The 4x4 identity matrix.
    static mat4 identity()
    {
        mat4 result;
        std::memset(result.m, 0, sizeof(result.m));
        result.m[0] = 1.0f;
        result.m[5] = 1.0f;
        result.m[10] = 1.0f;
        result.m[15] = 1.0f;
        return result;
    }

    /// @return The matrix product `a * b`.
    static mat4 multiply(const mat4 &a, const mat4 &b)
    {
        mat4 result;
        for (int col = 0; col < 4; col++)
        {
            for (int row = 0; row < 4; row++)
            {
                float sum = 0.0f;
                for (int k = 0; k < 4; k++)
                    sum += a.m[k * 4 + row] * b.m[col * 4 + k];
                result.m[col * 4 + row] = sum;
            }
        }
        return result;
    }

    /// @return A translation matrix by `t`.
    static mat4 translate(const vec3 &t)
    {
        mat4 result = identity();
        result.m[12] = t.x;
        result.m[13] = t.y;
        result.m[14] = t.z;
        return result;
    }

    /// @return A rotation matrix of `radians` around the Y axis.
    static mat4 rotate_y(float radians)
    {
        mat4 result = identity();
        float c = std::cos(radians);
        float s = std::sin(radians);
        result.m[0] = c;
        result.m[2] = -s;
        result.m[8] = s;
        result.m[10] = c;
        return result;
    }

    /// @return A rotation matrix of `radians` around the X axis.
    static mat4 rotate_x(float radians)
    {
        mat4 result = identity();
        float c = std::cos(radians);
        float s = std::sin(radians);
        result.m[5] = c;
        result.m[6] = s;
        result.m[9] = -s;
        result.m[10] = c;
        return result;
    }

    /// @return A rotation matrix of `radians` around the Z axis.
    static mat4 rotate_z(float radians)
    {
        mat4 result = identity();
        float c = std::cos(radians);
        float s = std::sin(radians);
        result.m[0] = c;
        result.m[1] = s;
        result.m[4] = -s;
        result.m[5] = c;
        return result;
    }

    /// @return A scale matrix by `s`.
    static mat4 scale(const vec3 &s)
    {
        mat4 result = identity();
        result.m[0] = s.x;
        result.m[5] = s.y;
        result.m[10] = s.z;
        return result;
    }

    /**
     * @brief Composes a local transform the way the scene graph's JSON
     * fields describe it: scale first, then rotate (Z * Y * X, i.e. roll
     * then yaw then pitch applied to the object), then translate.
     * @param position Translation component.
     * @param euler_radians Rotation, applied Z then Y then X, in radians.
     * @param scale_factor Per-axis scale, applied before rotation.
     * @return The composed transform matrix.
     */
    static mat4 compose(const vec3 &position, const vec3 &euler_radians, const vec3 &scale_factor)
    {
        mat4 rotation = multiply(rotate_z(euler_radians.z),
            multiply(rotate_y(euler_radians.y), rotate_x(euler_radians.x)));
        return multiply(translate(position), multiply(rotation, scale(scale_factor)));
    }

    /// @return A right-handed look-at view matrix, matching a Y-up world.
    /// @param eye Camera position.
    /// @param center Point the camera looks at.
    /// @param up World up vector.
    static mat4 look_at(const vec3 &eye, const vec3 &center, const vec3 &up)
    {
        vec3 f = vec3::normalize(center - eye);
        vec3 s = vec3::normalize(vec3::cross(f, up));
        vec3 u = vec3::cross(s, f);

        mat4 result = identity();
        result.m[0] = s.x;
        result.m[4] = s.y;
        result.m[8] = s.z;
        result.m[1] = u.x;
        result.m[5] = u.y;
        result.m[9] = u.z;
        result.m[2] = -f.x;
        result.m[6] = -f.y;
        result.m[10] = -f.z;
        result.m[12] = -vec3::dot(s, eye);
        result.m[13] = -vec3::dot(u, eye);
        result.m[14] = vec3::dot(f, eye);
        return result;
    }

    /**
     * @brief Right-handed perspective projection with Vulkan's [0,1] depth
     * range and flipped Y (Vulkan's NDC Y points down, unlike OpenGL's).
     * @param fov_y_radians Vertical field of view, in radians.
     * @param aspect Viewport width / height.
     * @param z_near Near clip distance.
     * @param z_far Far clip distance.
     * @return The projection matrix.
     */
    static mat4 perspective(float fov_y_radians, float aspect, float z_near, float z_far)
    {
        mat4 result;
        std::memset(result.m, 0, sizeof(result.m));
        float tan_half_fov = std::tan(fov_y_radians * 0.5f);

        result.m[0] = 1.0f / (aspect * tan_half_fov);
        result.m[5] = -1.0f / tan_half_fov;
        result.m[10] = z_far / (z_near - z_far);
        result.m[11] = -1.0f;
        result.m[14] = -(z_far * z_near) / (z_far - z_near);
        return result;
    }

    /**
     * @brief Orthographic projection, same Vulkan [0,1] depth range and
     * flipped Y as perspective() above.
     *
     * Used for the directional-light shadow map's projection, where a
     * light "camera" has no perspective falloff.
     * @return The projection matrix.
     */
    static mat4 orthographic(float left, float right, float bottom, float top,
        float z_near, float z_far)
    {
        mat4 result = identity();
        result.m[0] = 2.0f / (right - left);
        result.m[5] = -2.0f / (top - bottom);
        result.m[10] = 1.0f / (z_near - z_far);
        result.m[12] = -(right + left) / (right - left);
        result.m[13] = -(top + bottom) / (top - bottom);
        result.m[14] = z_near / (z_near - z_far);
        return result;
    }

    /**
     * @brief General 4x4 inverse via the cofactor/adjugate method (the
     * classic public-domain formula, e.g. as used in MESA's
     * gluInvertMatrix).
     *
     * Needed to reconstruct view-space position from depth for the SSAO
     * pass (Renderer's post-process subpass) — none of the specific
     * matrix constructors above need inverting, so this earns its keep as
     * a general fallback rather than something to special-case per shape.
     * @param m Matrix to invert.
     * @return The inverse of `m`, or the identity matrix if `m` is (numerically) singular.
     */
    static mat4 inverse(const mat4 &m)
    {
        const float *a = m.m;
        float inv[16];

        inv[0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] - a[9] * a[6] * a[15]
            + a[9] * a[7] * a[14] + a[13] * a[6] * a[11] - a[13] * a[7] * a[10];
        inv[4] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] + a[8] * a[6] * a[15]
            - a[8] * a[7] * a[14] - a[12] * a[6] * a[11] + a[12] * a[7] * a[10];
        inv[8] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] - a[8] * a[5] * a[15]
            + a[8] * a[7] * a[13] + a[12] * a[5] * a[11] - a[12] * a[7] * a[9];
        inv[12] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] + a[8] * a[5] * a[14]
            - a[8] * a[6] * a[13] - a[12] * a[5] * a[10] + a[12] * a[6] * a[9];

        inv[1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] + a[9] * a[2] * a[15]
            - a[9] * a[3] * a[14] - a[13] * a[2] * a[11] + a[13] * a[3] * a[10];
        inv[5] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] - a[8] * a[2] * a[15]
            + a[8] * a[3] * a[14] + a[12] * a[2] * a[11] - a[12] * a[3] * a[10];
        inv[9] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] + a[8] * a[1] * a[15]
            - a[8] * a[3] * a[13] - a[12] * a[1] * a[11] + a[12] * a[3] * a[9];
        inv[13] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] - a[8] * a[1] * a[14]
            + a[8] * a[2] * a[13] + a[12] * a[1] * a[10] - a[12] * a[2] * a[9];

        inv[2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] - a[5] * a[2] * a[15]
            + a[5] * a[3] * a[14] + a[13] * a[2] * a[7] - a[13] * a[3] * a[6];
        inv[6] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] + a[4] * a[2] * a[15]
            - a[4] * a[3] * a[14] - a[12] * a[2] * a[7] + a[12] * a[3] * a[6];
        inv[10] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] - a[4] * a[1] * a[15]
            + a[4] * a[3] * a[13] + a[12] * a[1] * a[7] - a[12] * a[3] * a[5];
        inv[14] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] + a[4] * a[1] * a[14]
            - a[4] * a[2] * a[13] - a[12] * a[1] * a[6] + a[12] * a[2] * a[5];

        inv[3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] + a[5] * a[2] * a[11]
            - a[5] * a[3] * a[10] - a[9] * a[2] * a[7] + a[9] * a[3] * a[6];
        inv[7] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] - a[4] * a[2] * a[11]
            + a[4] * a[3] * a[10] + a[8] * a[2] * a[7] - a[8] * a[3] * a[6];
        inv[11] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] + a[4] * a[1] * a[11]
            - a[4] * a[3] * a[9] - a[8] * a[1] * a[7] + a[8] * a[3] * a[5];
        inv[15] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] - a[4] * a[1] * a[10]
            + a[4] * a[2] * a[9] + a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

        float determinant = a[0] * inv[0] + a[1] * inv[4] + a[2] * inv[8] + a[3] * inv[12];
        if (std::fabs(determinant) < 1e-12f)
            return identity();

        float inverse_determinant = 1.0f / determinant;
        mat4 result;
        for (int i = 0; i < 16; i++)
            result.m[i] = inv[i] * inverse_determinant;
        return result;
    }

    // Transforms a point (implicit w=1) by an affine matrix — i.e. no
    // perspective divide, which every caller of this (culling, not
    // rendering) needs: model matrices are always affine (translate *
    // rotate * scale), never a projection.
    /// @return `p` transformed by affine matrix `m` (implicit w=1, no perspective divide).
    static vec3 transform_point(const mat4 &m, const vec3 &p)
    {
        return vec3(
            m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]);
    }
};

/**
 * @brief Axis-aligned bounding box, used for both frustum and occlusion
 * culling (renderer.cpp).
 *
 * One is computed per mesh at load time in object-local space
 * (Renderer::load_mesh_from_obj), then re-derived in world space per draw
 * call via transform() below, using each RenderItem's model matrix.
 */
struct AABB
{
    vec3 min;
    vec3 max;

    /**
     * @brief Re-derives a tight world-space AABB from a local-space one
     * and an arbitrary (possibly rotated) affine transform, by
     * transforming all 8 corners and taking their bounds — simpler and
     * easier to verify correct than the axis-projection shortcut (Arvo's
     * method), and cheap enough at this engine's object counts (tens, not
     * millions).
     * @param local Object-local bounding box.
     * @param model World-space model matrix to apply.
     * @return The tight world-space AABB enclosing the transformed box.
     */
    static AABB transform(const AABB &local, const mat4 &model)
    {
        const vec3 corners[8] = {
            vec3(local.min.x, local.min.y, local.min.z),
            vec3(local.max.x, local.min.y, local.min.z),
            vec3(local.min.x, local.max.y, local.min.z),
            vec3(local.max.x, local.max.y, local.min.z),
            vec3(local.min.x, local.min.y, local.max.z),
            vec3(local.max.x, local.min.y, local.max.z),
            vec3(local.min.x, local.max.y, local.max.z),
            vec3(local.max.x, local.max.y, local.max.z),
        };

        AABB result;
        result.min = result.max = mat4::transform_point(model, corners[0]);
        for (int i = 1; i < 8; i++)
        {
            vec3 world_corner = mat4::transform_point(model, corners[i]);
            result.min.x = std::min(result.min.x, world_corner.x);
            result.min.y = std::min(result.min.y, world_corner.y);
            result.min.z = std::min(result.min.z, world_corner.z);
            result.max.x = std::max(result.max.x, world_corner.x);
            result.max.y = std::max(result.max.y, world_corner.y);
            result.max.z = std::max(result.max.z, world_corner.z);
        }
        return result;
    }
};

// A camera's view frustum as 6 world-space planes, each stored as
// (a, b, c, d) with the "inside" half-space defined by a*x+b*y+c*z+d >= 0.
// Extracted directly from a combined view-projection matrix via the
// standard Gribb/Hartmann method — this works on any composed clip matrix
// without needing the projection's individual fov/aspect/near/far
// parameters, so it's agnostic to whether the caller used perspective() or
// orthographic() to build it.
/**
 * @brief A camera's view frustum as 6 world-space planes.
 *
 * Each stored as (a, b, c, d) with the "inside" half-space defined by
 * a*x+b*y+c*z+d >= 0. Extracted directly from a combined view-projection
 * matrix via the standard Gribb/Hartmann method — this works on any
 * composed clip matrix without needing the projection's individual
 * fov/aspect/near/far parameters, so it's agnostic to whether the caller
 * used perspective() or orthographic() to build it.
 */
struct Frustum
{
    float planes[6][4]; ///< Left, Right, Bottom, Top, Near, Far, in that order.

    /// @return The frustum described by clip matrix `view_projection`.
    static Frustum from_view_projection(const mat4 &view_projection)
    {
        Frustum frustum;
        const float *m = view_projection.m;
        // Column-major storage (m[col*4+row]): component c of row r is
        // m[r + c*4].
        auto elem = [&](int r, int c) { return m[r + c * 4]; };
        float row0[4] = {elem(0, 0), elem(0, 1), elem(0, 2), elem(0, 3)};
        float row1[4] = {elem(1, 0), elem(1, 1), elem(1, 2), elem(1, 3)};
        float row2[4] = {elem(2, 0), elem(2, 1), elem(2, 2), elem(2, 3)};
        float row3[4] = {elem(3, 0), elem(3, 1), elem(3, 2), elem(3, 3)};

        auto set_plane = [&](int index, const float row_a[4], const float row_b[4], float sign)
        {
            float a = row_a[0] + sign * row_b[0];
            float b = row_a[1] + sign * row_b[1];
            float c = row_a[2] + sign * row_b[2];
            float d = row_a[3] + sign * row_b[3];
            float length = std::sqrt(a * a + b * b + c * c);
            if (length > 1e-8f)
            {
                a /= length; b /= length; c /= length; d /= length;
            }
            frustum.planes[index][0] = a;
            frustum.planes[index][1] = b;
            frustum.planes[index][2] = c;
            frustum.planes[index][3] = d;
        };

        // Left/Right/Bottom/Top: standard row3 +/- row0/row1, unaffected
        // by Vulkan's [0,1] depth range (only the near plane below cares
        // about that).
        set_plane(0, row3, row0, 1.0f);  // Left   = row3 + row0
        set_plane(1, row3, row0, -1.0f); // Right  = row3 - row0
        set_plane(2, row3, row1, 1.0f);  // Bottom = row3 + row1
        set_plane(3, row3, row1, -1.0f); // Top    = row3 - row1
        // Near = row2 directly (Vulkan clip-space z in [0,1]: the "z >= 0"
        // half-space, unlike OpenGL's [-1,1] range which needs row3+row2).
        static const float zero_row[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        set_plane(4, row2, zero_row, 1.0f);
        set_plane(5, row3, row2, -1.0f); // Far = row3 - row2
        return frustum;
    }

    // Conservative test: false only if the box is entirely on the outside
    // of at least one plane. May return true for a handful of boxes just
    // outside the frustum near a corner (the standard AABB-vs-frustum
    // false-positive case) — never a false negative, which is the
    // direction that would actually be visibly wrong (popping).
    /**
     * @brief Conservative frustum/AABB intersection test.
     *
     * False only if the box is entirely on the outside of at least one
     * plane. May return true for a handful of boxes just outside the
     * frustum near a corner (the standard AABB-vs-frustum false-positive
     * case) — never a false negative, which is the direction that would
     * actually be visibly wrong (popping).
     * @param box World-space box to test.
     * @return true if `box` might be visible (is not conclusively outside the frustum).
     */
    bool intersects_aabb(const AABB &box) const
    {
        vec3 center = (box.min + box.max) * 0.5f;
        vec3 extent = (box.max - box.min) * 0.5f;
        for (const auto &plane : planes)
        {
            float distance = plane[0] * center.x + plane[1] * center.y
                + plane[2] * center.z + plane[3];
            float radius = extent.x * std::fabs(plane[0])
                + extent.y * std::fabs(plane[1]) + extent.z * std::fabs(plane[2]);
            if (distance + radius < 0.0f)
                return false;
        }
        return true;
    }
};

} // namespace vre
