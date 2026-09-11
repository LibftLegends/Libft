#version 450

layout(set = 0, binding = 0) uniform sampler2D diffuse_sampler;

layout(set = 1, binding = 0) uniform GlobalUbo
{
    mat4 view_proj;
    mat4 light_space_matrices[2]; // kMaxShadowCasters
    vec4 light_direction_or_position[4]; // kMaxLights
    vec4 light_color_intensity[4];
    vec4 light_count_ambient; // x = light count, y = ambient
    vec4 view_position;
    vec4 shadow_caster_count; // x = active shadow casters
    mat4 bone_matrices[16]; // kMaxBones — unused here, but must stay in the layout to match mesh.vert/renderer.hpp's GlobalUbo exactly
} global;
// sampler2DShadow: a hardware depth-COMPARE sampler (see
// Renderer::create_shadow_resources — compareEnable/compareOp on the
// VkSampler), so a single texture() call already returns a bilinearly
// filtered pass/fail result instead of a raw depth value. pcf() below
// layers a 3x3 manual tap on top of that for a properly soft shadow edge.
layout(set = 1, binding = 1) uniform sampler2DShadow shadow_maps[2]; // kMaxShadowCasters

layout(push_constant) uniform PushConstants
{
    mat4 model;
    vec4 tint;
    vec4 material_params; // x = roughness, y = metallic
} push;

layout(location = 0) in vec3 frag_world_pos;
layout(location = 1) in vec3 frag_normal;
layout(location = 2) in vec2 frag_uv;
layout(location = 3) in vec4 frag_light_space_pos_0;
layout(location = 4) in vec4 frag_light_space_pos_1;

layout(location = 0) out vec4 out_color;

const float PI = 3.14159265359;
// Must match Renderer::kShadowMapResolution.
const float SHADOW_MAP_RESOLUTION = 2048.0;

// 3x3 percentage-closer filtering: averages 9 bilinearly-filtered
// depth-compare taps around the fragment's shadow-map texel, which is what
// actually turns the shadow edge soft instead of a single hard sample.
float pcf(sampler2DShadow shadow_map, vec2 proj_uv, float ref_depth)
{
    float visibility = 0.0;
    vec2 texel_size = vec2(1.0 / SHADOW_MAP_RESOLUTION);
    for (int x = -1; x <= 1; x++)
    {
        for (int y = -1; y <= 1; y++)
        {
            vec2 offset = vec2(float(x), float(y)) * texel_size;
            visibility += texture(shadow_map, vec3(proj_uv + offset, ref_depth));
        }
    }
    return visibility / 9.0;
}

// Returns 1.0 = fully lit, 0.0 = fully shadowed, by `shadow_index`'s map.
// Only the first `shadow_caster_count` lights (kMaxShadowCasters at most)
// have a map at all — see Renderer::draw_frame.
//
// Falling outside a light's shadow frustum (directional or point) reads as
// "fully lit" here, not "fully shadowed": an earlier version tried making
// point lights default to shadowed outside their frustum, reasoning that a
// narrow downward-facing cone should never illuminate past its own edge.
// That was based on a misdiagnosis — comparing two adjacent rooms, turning
// on one room's light visibly brightened wall surfaces right next to the
// *open doorway* between them, which looked like light leaking through the
// solid dividing wall but was actually correct: light legitimately
// spilling through an open doorway onto the nearby surfaces of the next
// room, the way it would in reality. The "shadowed outside the frustum"
// change was reverted because it made surfaces genuinely lit by their own
// room's light — but sitting just past that light's necessarily-finite
// downward cone — incorrectly go dark instead, which was a worse artifact
// than the one it was meant to fix.
float compute_visibility(int shadow_index, vec4 light_space_pos, vec3 normal, vec3 light_dir)
{
    if (light_space_pos.w <= 0.0)
        return 1.0; // behind the light entirely: nothing to occlude with

    vec3 proj = light_space_pos.xyz / light_space_pos.w;
    proj.xy = proj.xy * 0.5 + 0.5;

    if (proj.z > 1.0)
        return 1.0; // past the far plane
    if (proj.x < 0.0 || proj.x > 1.0 || proj.y < 0.0 || proj.y > 1.0)
        return 1.0; // outside the frustum's side edges

    float bias = max(0.0025 * (1.0 - dot(normal, light_dir)), 0.0008);
    float ref_depth = proj.z - bias;

    // shadow_index selects a compile-time-constant array element in each
    // branch (0 or 1), so this needs no dynamic-indexing device feature.
    if (shadow_index == 0)
        return pcf(shadow_maps[0], proj.xy, ref_depth);
    return pcf(shadow_maps[1], proj.xy, ref_depth);
}

// Trowbridge-Reitz/GGX normal distribution: how concentrated the
// microfacets are around the half-vector. Smaller roughness => a tighter,
// brighter highlight; this is what actually makes a "smooth metal" look
// different from a "rough wood" beyond just their base color.
float distribution_ggx(float n_dot_h, float roughness)
{
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (n_dot_h * n_dot_h) * (alpha2 - 1.0) + 1.0;
    return alpha2 / max(PI * denom * denom, 1e-6);
}

// Smith's method, Schlick-GGX approximation of each direction's
// self-shadowing/masking by the microfacets.
float geometry_schlick_ggx(float n_dot_x, float roughness)
{
    float r = roughness + 1.0;
    float k = (r * r) / 8.0; // direct-lighting remapping (not the IBL one)
    return n_dot_x / max(n_dot_x * (1.0 - k) + k, 1e-6);
}

float geometry_smith(float n_dot_v, float n_dot_l, float roughness)
{
    return geometry_schlick_ggx(n_dot_v, roughness) * geometry_schlick_ggx(n_dot_l, roughness);
}

// Schlick's approximation of the Fresnel term: reflectance rises toward 1
// at grazing angles for every material, dielectric or metal.
vec3 fresnel_schlick(float cos_theta, vec3 f0)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void main()
{
    vec4 albedo_sample = texture(diffuse_sampler, frag_uv) * push.tint;
    vec3 albedo = albedo_sample.rgb;
    float roughness = clamp(push.material_params.x, 0.045, 1.0); // avoid a singular D at roughness=0
    float metallic = clamp(push.material_params.y, 0.0, 1.0);

    vec3 normal = normalize(frag_normal);
    vec3 view_dir = normalize(global.view_position.xyz - frag_world_pos);
    float n_dot_v = max(dot(normal, view_dir), 1e-4);

    // Dielectrics (wood, plastic, ...) reflect ~4% at normal incidence
    // regardless of color; metals tint their entire specular response by
    // their own albedo instead of having a separate diffuse term at all.
    // This one line is most of what makes "metallic" vs. "wood-like" an
    // actual shading difference rather than just a different texture.
    vec3 f0 = mix(vec3(0.04), albedo, metallic);

    // Ambient term: a diffuse part (zero for metals, which have no diffuse
    // response at all) plus a flat-color specular part using f0 as a stand-in
    // for reflecting the environment. There's no environment map/IBL in this
    // engine yet, so without this a rough metal would render as solid black
    // everywhere except inside a direct specular highlight — physically
    // "correct" for zero ambient light, but a very visible artifact. This
    // is the standard cheap substitute engines use before adding real IBL.
    vec3 result = (albedo * (1.0 - metallic) + f0) * global.light_count_ambient.y;

    int light_count = int(global.light_count_ambient.x);
    int shadow_caster_count = int(global.shadow_caster_count.x);
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

        float n_dot_l = max(dot(normal, light_dir), 0.0);
        if (n_dot_l <= 0.0)
            continue;

        vec3 half_vector = normalize(view_dir + light_dir);
        float n_dot_h = max(dot(normal, half_vector), 0.0);
        float v_dot_h = max(dot(view_dir, half_vector), 0.0);

        float distribution = distribution_ggx(n_dot_h, roughness);
        float geometry = geometry_smith(n_dot_v, n_dot_l, roughness);
        vec3 fresnel = fresnel_schlick(v_dot_h, f0);

        vec3 specular = (distribution * geometry * fresnel) / max(4.0 * n_dot_v * n_dot_l, 1e-4);

        // Energy conservation: whatever fraction of light isn't reflected
        // specularly (1 - fresnel) is available to the diffuse term, and
        // metals have no diffuse term at all.
        vec3 diffuse = (vec3(1.0) - fresnel) * (1.0 - metallic) * albedo / PI;

        float visibility = 1.0;
        if (i < shadow_caster_count)
        {
            vec4 light_space_pos = (i == 0) ? frag_light_space_pos_0 : frag_light_space_pos_1;
            visibility = compute_visibility(i, light_space_pos, normal, light_dir);
        }

        vec3 radiance = global.light_color_intensity[i].rgb * global.light_color_intensity[i].a
            * attenuation * visibility;

        result += (diffuse + specular) * radiance * n_dot_l;
    }

    out_color = vec4(result, albedo_sample.a);
}
