#version 450

// Generates one big triangle covering the whole screen from gl_VertexIndex
// alone — no vertex buffer needed, the standard trick for a fullscreen
// post-process pass.
layout(location = 0) out vec2 frag_uv;

void main()
{
    vec2 positions[3] = vec2[](vec2(-1.0, -1.0), vec2(3.0, -1.0), vec2(-1.0, 3.0));
    vec2 position = positions[gl_VertexIndex];
    gl_Position = vec4(position, 0.0, 1.0);
    frag_uv = position * 0.5 + 0.5;
}
