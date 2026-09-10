#version 450

// Depth-only shadow pass: no fragment shader stage at all (see
// Renderer::create_shadow_pipeline) — only positions need to reach the
// rasterizer to populate the shadow map's depth buffer.
layout(push_constant) uniform ShadowPushConstants
{
    mat4 light_mvp;
} push;

layout(location = 0) in vec3 in_position;

void main()
{
    gl_Position = push.light_mvp * vec4(in_position, 1.0);
}
