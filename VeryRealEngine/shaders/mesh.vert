#version 450

// Must mirror Renderer::GlobalUbo (renderer.hpp) field-for-field — every
// field there is already vec4/mat4-sized, so std140 layout matches the C++
// struct's layout with no extra padding to account for.
layout(set = 1, binding = 0) uniform GlobalUbo
{
    mat4 view_proj;
    mat4 light_space_matrices[2]; // kMaxShadowCasters
    vec4 light_direction_or_position[4]; // kMaxLights
    vec4 light_color_intensity[4];
    vec4 light_count_ambient; // x = light count, y = ambient
    vec4 view_position;
    vec4 shadow_caster_count; // x = active shadow casters
} global;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 tint;
    vec4 material_params;
} push;

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 frag_world_pos;
layout(location = 1) out vec3 frag_normal;
layout(location = 2) out vec2 frag_uv;
layout(location = 3) out vec4 frag_light_space_pos_0;
layout(location = 4) out vec4 frag_light_space_pos_1;

void main()
{
    vec4 world_pos = push.model * vec4(in_position, 1.0);
    gl_Position = global.view_proj * world_pos;

    frag_world_pos = world_pos.xyz;
    // Uses the model matrix's upper-left 3x3 directly rather than a proper
    // inverse-transpose normal matrix, so non-uniform scale would skew
    // normals. Every object in the demo scene uses uniform scale, so this
    // is an acceptable simplification for step 5, not a general-purpose
    // normal transform.
    frag_normal = mat3(push.model) * in_normal;
    frag_uv = in_uv;
    frag_light_space_pos_0 = global.light_space_matrices[0] * world_pos;
    frag_light_space_pos_1 = global.light_space_matrices[1] * world_pos;
}
