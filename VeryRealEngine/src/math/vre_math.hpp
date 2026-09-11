// Minimal, dependency-free math types for VeryRealEngine.
// Deliberately not reusing FullLibft/Modules/Math: that module pulls in
// CMA, PThread, RNG, Template and friends (see verdict.md), which would drag
// the whole FullLibft tree into an engine that must stand on its own.
#pragma once

#include <cmath>
#include <cstring>

namespace vre
{

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

    static float dot(const vec3 &a, const vec3 &b)
    {
        return (a.x * b.x + a.y * b.y + a.z * b.z);
    }

    static vec3 cross(const vec3 &a, const vec3 &b)
    {
        return vec3(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x);
    }

    static vec3 normalize(const vec3 &v)
    {
        float length = std::sqrt(dot(v, v));
        if (length <= 0.0f)
            return vec3(0.0f, 0.0f, 0.0f);
        return vec3(v.x / length, v.y / length, v.z / length);
    }
};

// Column-major 4x4 matrix, laid out the way Vulkan/GLSL expect it
// (matches the memory layout of `mat4` in std140/push-constant blocks).
struct mat4
{
    float m[16];

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

    static mat4 translate(const vec3 &t)
    {
        mat4 result = identity();
        result.m[12] = t.x;
        result.m[13] = t.y;
        result.m[14] = t.z;
        return result;
    }

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

    static mat4 scale(const vec3 &s)
    {
        mat4 result = identity();
        result.m[0] = s.x;
        result.m[5] = s.y;
        result.m[10] = s.z;
        return result;
    }

    // Composes a local transform the way the scene graph's JSON fields
    // describe it: scale first, then rotate (Z * Y * X, i.e. roll then
    // yaw then pitch applied to the object), then translate.
    static mat4 compose(const vec3 &position, const vec3 &euler_radians, const vec3 &scale_factor)
    {
        mat4 rotation = multiply(rotate_z(euler_radians.z),
            multiply(rotate_y(euler_radians.y), rotate_x(euler_radians.x)));
        return multiply(translate(position), multiply(rotation, scale(scale_factor)));
    }

    // Right-handed look-at, matching a Y-up world.
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

    // Right-handed perspective projection with Vulkan's [0,1] depth range
    // and flipped Y (Vulkan's NDC Y points down, unlike OpenGL's).
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

    // Orthographic projection, same Vulkan [0,1] depth range and flipped Y
    // as perspective() above. Used for the directional-light shadow map's
    // projection, where a light "camera" has no perspective falloff.
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

    // General 4x4 inverse via the cofactor/adjugate method (the classic
    // public-domain formula, e.g. as used in MESA's gluInvertMatrix).
    // Needed to reconstruct view-space position from depth for the SSAO
    // pass (Renderer's post-process subpass) — none of the specific
    // matrix constructors above need inverting, so this earns its keep as
    // a general fallback rather than something to special-case per shape.
    // Returns the identity matrix if `m` is (numerically) singular.
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
};

} // namespace vre
