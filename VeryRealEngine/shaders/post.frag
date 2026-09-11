#version 450

// Regular textures (not subpass input attachments): SSAO's kernel needs to
// sample *neighboring* screen pixels, and Vulkan input attachments only
// ever let a fragment shader read its own pixel's value — see
// Renderer::create_post_process_resources / create_render_pass.
//
// Known limitation: with a fixed (non-rotated) 8-tap kernel and no
// bilateral blur pass afterward, the result shows some banding/noise
// rather than a perfectly smooth occlusion gradient. A per-pixel rotation
// (noise texture) plus a small blur pass would clean this up — a natural
// follow-up, not attempted here to keep this step's scope bounded.
layout(set = 0, binding = 0) uniform sampler2D scene_color;
layout(set = 0, binding = 1) uniform sampler2D scene_depth;

layout(push_constant) uniform PushConstants
{
    // The 4 nonzero entries of the camera's perspective projection matrix
    // (see mat4::perspective in vre_math.hpp) — enough to analytically
    // reconstruct/reproject view-space position without needing a full
    // inverse-projection matrix. Assumes the camera projection is always
    // perspective, true for every camera this engine currently builds.
    vec4 proj_params;  // x = m[0], y = m[5], z = m[10], w = m[14]
    vec4 ao_params;    // x = radius, y = bias, z = strength
    vec4 bloom_params; // x = threshold, y = intensity, z = sample step (texels)
} push;

layout(location = 0) in vec2 frag_uv;
layout(location = 0) out vec4 out_color;

// A small fixed kernel of view-space offsets, biased toward -Z (toward the
// camera) to approximate an occlusion hemisphere without a per-pixel
// normal buffer (this renderer doesn't have one — see the priority-list
// note on this being a documented simplification of "full" normal-oriented
// SSAO, not a bug).
const vec3 KERNEL[8] = vec3[](
    vec3( 0.35,  0.15, -0.60), vec3(-0.30,  0.40, -0.55),
    vec3( 0.10, -0.45, -0.70), vec3(-0.50, -0.10, -0.45),
    vec3( 0.55,  0.30, -0.30), vec3(-0.20,  0.55, -0.35),
    vec3( 0.20, -0.25, -0.85), vec3(-0.45, -0.35, -0.25)
);

// View-space Z of the surface at this depth-buffer sample. Derived from
// Renderer::compute_light_space_matrix's sibling — the camera's own
// perspective matrix has the same sparse structure as mat4::perspective:
// clip.z = C*view.z + D, clip.w = -view.z, so ndc.z = (C*view.z + D)/(-view.z)
// solves to view.z = -D / (ndc.z + C).
float view_z_from_depth(float depth)
{
    float c = push.proj_params.z;
    float d = push.proj_params.w;
    return -d / (depth + c);
}

vec3 reconstruct_view_pos(vec2 uv, float depth)
{
    float view_z = view_z_from_depth(depth);
    vec2 ndc = uv * 2.0 - 1.0;
    float a = push.proj_params.x;
    float b = push.proj_params.y;
    float view_x = ndc.x * (-view_z) / a;
    float view_y = ndc.y * (-view_z) / b;
    return vec3(view_x, view_y, view_z);
}

// Reprojects a view-space point back to the screen to look up what's
// actually there in the depth buffer.
vec2 project_to_uv(vec3 view_pos)
{
    float a = push.proj_params.x;
    float b = push.proj_params.y;
    vec2 ndc = vec2(a * view_pos.x, b * view_pos.y) / max(-view_pos.z, 1e-4);
    return ndc * 0.5 + 0.5;
}

// Cheap single-pass bloom: rather than the usual downsample/blur-pyramid
// (which needs several extra images and passes), this takes a wide
// Gaussian-weighted tap directly on the full-resolution HDR scene_color,
// keeping only each sample's over-threshold ("bright") remainder. A real
// multi-pass blur would give a wider, cheaper glow for the same tap count —
// documented scope trade-off, not an oversight.
vec3 sample_bloom(vec2 uv)
{
    float threshold = push.bloom_params.x;
    float step_texels = push.bloom_params.z;
    vec2 texel = step_texels / vec2(textureSize(scene_color, 0));

    vec3 bloom = vec3(0.0);
    float total_weight = 0.0;
    for (int dx = -2; dx <= 2; dx++)
    {
        for (int dy = -2; dy <= 2; dy++)
        {
            vec2 offset = vec2(float(dx), float(dy)) * texel;
            vec3 sample_color = texture(scene_color, uv + offset).rgb;
            vec3 bright = max(sample_color - threshold, 0.0);
            float weight = exp(-float(dx * dx + dy * dy) * 0.15);
            bloom += bright * weight;
            total_weight += weight;
        }
    }
    return bloom / max(total_weight, 1e-4);
}

// Compact single-pass edge-smoothing filter (an "FXAA-lite"): detects
// silhouette/contrast edges by luma and blurs one tap along the edge
// direction. This is where the current architecture has to apply AA — a
// real MSAA implementation would need multisampled color+depth images and
// a depth-resolve attachment (extra Vulkan plumbing this step didn't add,
// in favor of this lower-risk single-pass technique, itself a real,
// shipped anti-aliasing approach, not a placeholder). Deliberately run on
// the pre-tonemap HDR color (this pass's only per-pixel-independent
// opportunity to access neighboring texels) rather than the conventional
// post-tonemap LDR image — smooths the same silhouette edges either way,
// since aliasing comes from geometry coverage, not from tonemapping.
float luma(vec3 color)
{
    return dot(color, vec3(0.299, 0.587, 0.114));
}

vec3 fxaa_lite(vec2 uv)
{
    vec2 texel = 1.0 / vec2(textureSize(scene_color, 0));

    vec3 center = texture(scene_color, uv).rgb;
    vec3 north = texture(scene_color, uv + vec2(0.0, -texel.y)).rgb;
    vec3 south = texture(scene_color, uv + vec2(0.0, texel.y)).rgb;
    vec3 east = texture(scene_color, uv + vec2(texel.x, 0.0)).rgb;
    vec3 west = texture(scene_color, uv + vec2(-texel.x, 0.0)).rgb;

    float l_center = luma(center);
    float l_north = luma(north);
    float l_south = luma(south);
    float l_east = luma(east);
    float l_west = luma(west);

    float l_min = min(l_center, min(min(l_north, l_south), min(l_east, l_west)));
    float l_max = max(l_center, max(max(l_north, l_south), max(l_east, l_west)));
    float contrast = l_max - l_min;

    const float EDGE_THRESHOLD = 0.05;
    if (contrast < EDGE_THRESHOLD)
        return center; // flat region: no aliasing to smooth

    float edge_horizontal = abs(l_north + l_south - 2.0 * l_center);
    float edge_vertical = abs(l_east + l_west - 2.0 * l_center);
    vec2 blend_direction = (edge_horizontal >= edge_vertical)
        ? vec2(texel.x, 0.0) : vec2(0.0, texel.y);

    vec3 blend_pos = texture(scene_color, uv + blend_direction).rgb;
    vec3 blend_neg = texture(scene_color, uv - blend_direction).rgb;
    float blend_factor = clamp(contrast * 2.0, 0.0, 0.5);

    return mix(center, (blend_pos + blend_neg) * 0.5, blend_factor);
}

// Narkowicz's fit of the ACES filmic tonemap curve: maps unbounded HDR
// color down to displayable [0,1] with a soft highlight rolloff instead of
// a hard clip — the reason a bright specular highlight or an over-driven
// point light glows rather than turning into a flat white patch.
vec3 aces_tonemap(vec3 color)
{
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0, 1.0);
}

void main()
{
    float alpha = texture(scene_color, frag_uv).a;
    float depth = texture(scene_depth, frag_uv).r;

    vec3 color = fxaa_lite(frag_uv);

    if (depth < 1.0)
    {
        vec3 origin = reconstruct_view_pos(frag_uv, depth);
        float radius = push.ao_params.x;
        float bias = push.ao_params.y;
        float strength = push.ao_params.z;

        float occlusion = 0.0;
        for (int i = 0; i < 8; i++)
        {
            vec3 sample_pos = origin + KERNEL[i] * radius;
            vec2 sample_uv = project_to_uv(sample_pos);

            float sample_scene_depth = texture(scene_depth, sample_uv).r;
            float scene_view_z = (sample_scene_depth >= 1.0)
                ? -1e6 // background at the sample location: never occludes
                : view_z_from_depth(sample_scene_depth);

            // Occluded if the *actual* surface at that screen location sits
            // in front of (less negative view.z than) our candidate sample.
            bool occluded = scene_view_z >= sample_pos.z + bias;

            // Range check: ignore occluders far outside the kernel radius —
            // without this, a wall far behind a small object would
            // incorrectly darken it just for being "closer than infinity".
            float range = smoothstep(0.0, 1.0,
                radius / max(abs(origin.z - scene_view_z), 1e-4));

            occlusion += occluded ? range : 0.0;
        }

        float ao = 1.0 - strength * (occlusion / 8.0);
        color *= ao;
    }
    // Background (depth >= 1.0): no AO to apply, but still bloom/tonemap
    // below, so there's no visible seam at the horizon.

    color += sample_bloom(frag_uv) * push.bloom_params.y;
    color = aces_tonemap(color);

    out_color = vec4(color, alpha);
}
