#version 450

layout(set = 0, binding = 0) uniform sampler2D diffuse_sampler;

layout(set = 1, binding = 0) uniform GlobalUbo
{
    mat4 view_proj;
    mat4 light_space_matrix;
    vec4 light_direction_or_position[4]; // kMaxLights
    vec4 light_color_intensity[4];
    vec4 light_count_ambient; // x = count, y = ambient
    vec4 view_position;
} global;
layout(set = 1, binding = 1) uniform sampler2D shadow_map;

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 tint;
} push;

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in vec4 frag_light_space_pos;

layout(location = 0) out vec4 out_color;

// Only light index 0 (the shadow caster — guaranteed directional, see
// Renderer::draw_frame) occludes; other lights shade without shadowing.
// That's a deliberate scope line for this step, not a limit of the
// shadow-mapping mechanism, which would extend to more shadow-casting
// lights by adding more shadow maps.
float compute_shadow(vec3 normal, vec3 light_dir)
{
    vec3 proj = frag_light_space_pos.xyz / frag_light_space_pos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0 || proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 0.0; // outside the light's frustum: nothing to occlude with

    float closest_depth = texture(shadow_map, proj.xy).r;
    float current_depth = proj.z;
    // Slope-scaled bias against shadow acne, on top of the shadow pass's
    // own depth-bias rasterization state.
    float bias = max(0.0025 * (1.0 - dot(normal, light_dir)), 0.0008);
    return (current_depth - bias > closest_depth) ? 1.0 : 0.0;
}

void main()
{
    vec4 albedo = texture(diffuse_sampler, frag_uv) * push.tint;
    vec3 normal = normalize(frag_normal);

    vec3 result = albedo.rgb * global.light_count_ambient.y;

    int light_count = int(global.light_count_ambient.x);
    for (int i = 0; i < light_count; i++)
    {
        vec4 dir_or_pos = global.light_direction_or_position[i];
        vec3 light_dir;
        float attenuation = 1.0;

        if (dir_or_pos.w < 0.5)
        {
            // Directional: direction the light travels, so the direction
            // *toward* the light is the negation.
            light_dir = normalize(-dir_or_pos.xyz);
        }
        else
        {
            vec3 to_light = dir_or_pos.xyz - frag_world_pos;
            float dist = length(to_light);
            light_dir = to_light / max(dist, 0.0001);
            attenuation = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
        }

        float ndotl = max(dot(normal, light_dir), 0.0);
        float shadow = (i == 0) ? compute_shadow(normal, light_dir) : 0.0;

        vec3 light_color = global.light_color_intensity[i].rgb;
        float intensity = global.light_color_intensity[i].a;

        result += albedo.rgb * light_color * intensity * ndotl * attenuation * (1.0 - shadow);
    }

    out_color = vec4(result, albedo.a);
}
