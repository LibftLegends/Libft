/**
 * @file renderer.hpp
 * @brief Vulkan renderer: instance/device setup, swapchain, a depth-tested
 * textured+lit pipeline with shadow mapping, and a handle-based
 * mesh/material/texture system.
 *
 * Step 1 (../../verdict.md roadmap) proved swapchain + pipeline + depth
 * buffer. Step 2 added OBJ-loaded meshes and materials/textures. Step 5
 * added multiple light sources and shadow mapping: one depth-only render
 * pass per shadow-casting light, sampled back (with PCF filtering) in the
 * main pass's fragment shader.
 */
#pragma once

#include "../platform/window.hpp"
#include "../math/vre_math.hpp"
#include "../assets/mesh_data.hpp"
#include "../assets/tga_loader.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <map>
#include <string>
#include <cstdint>

namespace vre
{

/// Opaque handle identifying a GPU-resident texture inside a Renderer.
using TextureHandle = size_t;
/// Opaque handle identifying a GPU-resident material inside a Renderer.
using MaterialHandle = size_t;
/// Opaque handle identifying a GPU-resident mesh inside a Renderer.
using MeshHandle = size_t;

/**
 * @brief One item to draw this frame: a loaded mesh placed in the world by
 * a model matrix.
 *
 * The renderer combines this with the camera's view/projection (passed
 * separately to draw_frame()) to build each draw call's MVP.
 */
struct RenderItem
{
    MeshHandle mesh; ///< Which uploaded mesh to draw (see Renderer::load_mesh_from_obj()).
    mat4 model;      ///< World-space model matrix placing the mesh.

    /**
     * Stable identity used only for occlusion culling, so a query result
     * from a couple of frames ago can be matched back to "the same object"
     * even though RenderItem itself is rebuilt from scratch every frame
     * (Scene::collect_render_items uses each node's index; particles and
     * anything else without a meaningful stable identity leave this at the
     * default, which opts them out of occlusion culling — always drawn if
     * frustum-visible, same as before this feature existed).
     */
    uint32_t occlusion_id = UINT32_MAX;
};

/// Distinguishes how Light::direction_or_position should be interpreted.
enum class LightType
{
    Directional, ///< Parallel rays (e.g. sunlight); direction_or_position is a unit direction.
    Point,       ///< Radiates from a point; direction_or_position is a world-space position.
};

/// A single scene light: directional or point, with color/intensity and,
/// for the first Renderer::kMaxShadowCasters of them in scene order, a
/// real-time shadow map.
struct Light
{
    LightType type = LightType::Directional;
    /**
     * Directional: unit direction the light travels (e.g. (0,-1,0) = straight down).
     * Point: world-space position.
     */
    vec3 direction_or_position;
    vec3 color{1.0f, 1.0f, 1.0f}; ///< Linear RGB color, multiplied by intensity.
    float intensity = 1.0f;      ///< Scalar multiplier applied to color.
};

/**
 * @brief The engine's Vulkan renderer: owns the instance/device/swapchain,
 * every render pass and pipeline (geometry, shadow, occlusion, post-process),
 * and a handle-based mesh/material/texture cache.
 *
 * Usage: construct, call initialize() with a platform Window, then call
 * draw_frame() once per frame with the current camera and scene contents.
 */
class Renderer
{
    public:
        static constexpr uint32_t kMaxFramesInFlight = 2; ///< Frames pipelined concurrently (double-buffering).
        static constexpr uint32_t kMaxLights = 4;          ///< Upper bound on simultaneously shaded lights.
        static constexpr uint32_t kShadowMapResolution = 2048; ///< Width/height of each shadow map, in texels.
        /**
         * Shadow maps are far more expensive than shading a light, so only
         * the first kMaxShadowCasters lights (in scene order) get one —
         * still "multiple lights casting shadows" per the subject, just
         * not unbounded. Extending this further means adding more shadow
         * maps, not a redesign.
         */
        static constexpr uint32_t kMaxShadowCasters = 2;
        /**
         * Occlusion-query slots per frame-in-flight. Generous headroom
         * over this demo's object count (tens, not hundreds); items beyond
         * this cap simply don't get occlusion-tested (documented scope
         * line in draw_frame()) rather than the renderer failing outright.
         */
        static constexpr uint32_t kMaxOcclusionQueries = 256;

        Renderer();
        ~Renderer();

        /**
         * @brief Brings up the entire Vulkan stack against the given window:
         * instance, surface, device, swapchain, render passes, pipelines,
         * and per-frame sync objects.
         * @param window Platform window to render into; must outlive the Renderer.
         * @return true on success; false if any setup step failed.
         */
        bool initialize(Window *window);
        /// Tears down every Vulkan object this Renderer owns, in reverse dependency order.
        void destroy();
        /// Blocks until the device has finished all submitted work — call before destroy().
        void wait_idle();

        /**
         * @brief Loads an .obj (and any .mtl/.tga it references) and uploads
         * it to GPU-resident buffers.
         *
         * Textures and materials are cached by path, so loading the same
         * texture from two different meshes only uploads it once. Safe to
         * call for several different files before the first draw_frame() —
         * that's the "multiple OBJs at once" case.
         * @param path Filesystem path to the .obj file.
         * @return Handle to the uploaded mesh, usable in a RenderItem.
         */
        MeshHandle load_mesh_from_obj(const char *path);

        /**
         * @brief Renders one frame: shadow passes for the active casters,
         * the geometry pass (with frustum + occlusion culling applied to
         * `items`), and the post-process composite to the swapchain.
         *
         * The first min(kMaxShadowCasters, lights.size()) lights (in scene
         * order) each get their own shadow map, rendered fresh every frame
         * from that light's point of view — so both static geometry and
         * moving objects cast correct, up-to-date shadows from every
         * shadow-casting light ("static and dynamic shadow rendering").
         * Remaining lights (up to kMaxLights) still shade the scene, just
         * without casting a shadow — a deliberate scope line (unbounded
         * shadow-casting lights would need unbounded shadow maps), not a
         * hard limitation of the mechanism itself.
         *
         * @param view Camera view matrix.
         * @param projection Camera projection matrix.
         * @param view_position Camera world-space position (used for specular/AO).
         * @param lights Active scene lights.
         * @param ambient_intensity Flat ambient term added before per-light shading.
         * @param items Candidate draw list for this frame, before culling.
         */
        void draw_frame(const mat4 &view, const mat4 &projection, const vec3 &view_position,
            const std::vector<Light> &lights, float ambient_intensity,
            const std::vector<RenderItem> &items);

        /**
         * @brief Culling counts from the most recently drawn frame.
         *
         * How many items draw_frame() was handed vs. how many survived
         * frustum culling vs. how many were actually drawn after occlusion
         * culling too. Exists so a caller (main.cpp's FPS report) can show
         * that culling is actually doing something, not just print a frame
         * rate and hope.
         */
        struct FrameStats
        {
            uint32_t total_items = 0;      ///< Size of the `items` list passed to draw_frame().
            uint32_t frustum_visible = 0;  ///< How many survived frustum culling.
            uint32_t drawn = 0;            ///< How many were actually drawn after occlusion culling.
        };
        /// @return Culling/draw counters from the most recently drawn frame.
        const FrameStats &get_last_frame_stats() const { return _last_frame_stats; }

        /**
         * @brief Debug/verification aid: writes the most recently presented
         * swapchain image out as a binary PPM file.
         *
         * Exists so the actual rendered output (materials, shadows,
         * lighting, culling — everything draw_frame() produces) can be
         * inspected directly from disk, on a machine or in an automated
         * context where there's no way to screenshot the on-screen window
         * itself. Does a full vkDeviceWaitIdle() first (this is a one-off
         * debug capture, not a per-frame operation, so a stall here is
         * fine) to guarantee the presented image is actually finished
         * presenting before reading it back.
         * @param path Output file path (should end in ".ppm").
         * @return true on success.
         */
        bool capture_screenshot(const char *path);

    private:
        /// A GPU-resident 2D texture and its view.
        struct GpuTexture
        {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
        };

        /// A GPU-resident material: a bound descriptor set plus the scalar
        /// parameters pushed per draw call (tint, roughness, metallic).
        struct GpuMaterial
        {
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
            float diffuse_tint[3] = {1.0f, 1.0f, 1.0f};
            float roughness = 0.8f;
            float metallic = 0.0f;
        };

        /// One contiguous index range of a GpuMesh sharing a single material.
        struct GpuSubMesh
        {
            uint32_t index_offset;
            uint32_t index_count;
            MaterialHandle material;
        };

        /// A GPU-resident mesh: device-local vertex/index buffers, its
        /// per-material submesh ranges, and a cached object-local bounding box.
        struct GpuMesh
        {
            VkBuffer vertex_buffer = VK_NULL_HANDLE;
            VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
            VkBuffer index_buffer = VK_NULL_HANDLE;
            VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
            std::vector<GpuSubMesh> submeshes;
            /**
             * Object-local bounding box (i.e. before a RenderItem's model
             * matrix is applied), computed once at load time from the raw
             * vertex positions — reused every frame for both frustum and
             * occlusion culling instead of re-scanning vertices.
             */
            AABB local_bounds;
        };

        /**
         * Mirrors the GlobalUbo block declared in shaders/mesh.vert /
         * mesh.frag — layout and field order must match exactly (std140).
         */
        struct GlobalUbo
        {
            mat4 view_proj;
            mat4 light_space_matrices[kMaxShadowCasters];
            float light_direction_or_position[kMaxLights][4]; ///< xyz + type (0=dir,1=point)
            float light_color_intensity[kMaxLights][4];       ///< rgb + intensity
            float light_count_ambient[4];                     ///< x = light count, y = ambient
            float view_position[4];
            float shadow_caster_count[4];                     ///< x = active shadow casters
        };

        Window *_window; ///< Non-owning pointer to the platform window passed to initialize().

        VkInstance _instance;
        VkDebugUtilsMessengerEXT _debug_messenger;
        VkSurfaceKHR _surface;

        VkPhysicalDevice _physical_device;
        VkDevice _device;
        uint32_t _graphics_queue_family;
        uint32_t _present_queue_family;
        VkQueue _graphics_queue;
        VkQueue _present_queue;

        VkSwapchainKHR _swapchain;
        VkFormat _swapchain_image_format;
        VkExtent2D _swapchain_extent;
        std::vector<VkImage> _swapchain_images;
        std::vector<VkImageView> _swapchain_image_views;

        VkFormat _depth_format;
        VkImage _depth_image;
        VkDeviceMemory _depth_image_memory;
        VkImageView _depth_image_view;

        /**
         * Intermediate HDR-capable color target the geometry pass renders
         * into, instead of the swapchain image directly — the separate
         * post-process pass below samples it (and the depth buffer) back
         * as regular textures (arbitrary-offset lookups, needed for SSAO's
         * multi-tap kernel — Vulkan input attachments only allow reading a
         * fragment's own pixel, so those don't work for this) to compute
         * and apply ambient occlusion before the final write to the
         * swapchain. Same lifetime as the depth buffer: swapchain-extent
         * sized, recreated together on resize.
         */
        VkFormat _scene_color_format;
        VkImage _scene_color_image;
        VkDeviceMemory _scene_color_image_memory;
        VkImageView _scene_color_image_view;

        VkRenderPass _render_pass; ///< Geometry pass: writes _scene_color_image + _depth_image.
        VkFramebuffer _scene_framebuffer; ///< Single instance (not one per swapchain image).
        VkDescriptorSetLayout _material_set_layout;  ///< set = 0: per-material diffuse sampler.
        VkDescriptorSetLayout _global_set_layout;     ///< set = 1: per-frame UBO + shadow maps.
        VkDescriptorPool _descriptor_pool;
        VkDescriptorPool _global_descriptor_pool;
        VkPipelineLayout _pipeline_layout;
        VkPipeline _graphics_pipeline;
        VkSampler _texture_sampler;

        /**
         * Post-process: a separate render pass (one framebuffer per
         * swapchain image, like the main pass used to be) whose
         * fullscreen-triangle fragment shader samples the geometry pass's
         * color+depth as regular textures, computes SSAO from depth, and
         * writes AO-modulated color to the swapchain image. One descriptor
         * set (not per-frame-in-flight — it points at single shared
         * images, not per-frame ones), rewritten whenever those images are
         * recreated on resize — see update_post_descriptor_set().
         */
        VkRenderPass _post_render_pass;
        std::vector<VkFramebuffer> _post_framebuffers;
        VkSampler _scene_color_sampler;
        VkSampler _scene_depth_sampler;
        VkDescriptorSetLayout _post_set_layout;
        VkDescriptorPool _post_descriptor_pool;
        VkDescriptorSet _post_descriptor_set;
        VkPipelineLayout _post_pipeline_layout;
        VkPipeline _post_pipeline;

        /**
         * Shadow maps: one depth-only render pass/pipeline shared by all
         * casters (same fixed resolution, independent of the swapchain —
         * no need to recreate on window resize), but a separate
         * image/view/framebuffer per shadow-casting light.
         */
        VkRenderPass _shadow_render_pass;
        VkPipelineLayout _shadow_pipeline_layout;
        VkPipeline _shadow_pipeline;
        VkImage _shadow_images[kMaxShadowCasters];
        VkDeviceMemory _shadow_image_memories[kMaxShadowCasters];
        VkImageView _shadow_image_views[kMaxShadowCasters];
        VkFramebuffer _shadow_framebuffers[kMaxShadowCasters];
        VkSampler _shadow_sampler;

        /**
         * Occlusion culling: a pipeline identical to _graphics_pipeline
         * except color writes and depth writes are disabled (it only ever
         * runs after the real geometry pass, purely to ask "would this
         * object's own geometry have produced any visible fragment against
         * what's already in the depth buffer" — see draw_frame()). One query
         * pool per frame-in-flight, mirroring how the global UBOs are
         * already double-buffered, so a pool can be reset and reused for
         * frame N+2 only once frame N's fence proves the GPU is done
         * reading it.
         */
        VkPipeline _occlusion_pipeline;
        VkQueryPool _occlusion_query_pools[kMaxFramesInFlight];
        /**
         * Which RenderItem::occlusion_id each query index corresponded to,
         * the *last* time this frame-in-flight slot's pool was recorded
         * into — read back (see update_occlusion_results()) right before
         * being overwritten with this frame's new set.
         */
        std::vector<uint32_t> _occlusion_query_ids[kMaxFramesInFlight];
        /**
         * Persisted visibility state by occlusion_id, carried across
         * frames: 1 = visible (or never yet queried — new objects default
         * to drawn rather than risk a pop-in-then-culled false negative),
         * 0 = occluded last time it was tested. Grows on demand.
         */
        std::vector<uint8_t> _occlusion_visible;

        FrameStats _last_frame_stats; ///< Backing storage for get_last_frame_stats().
        uint32_t _last_presented_image_index = 0; ///< Which swapchain image capture_screenshot() reads back.

        std::vector<VkBuffer> _global_ubo_buffers;
        std::vector<VkDeviceMemory> _global_ubo_memories;
        std::vector<void *> _global_ubo_mapped;
        std::vector<VkDescriptorSet> _global_descriptor_sets;

        VkCommandPool _command_pool;
        std::vector<VkCommandBuffer> _command_buffers;

        std::vector<VkSemaphore> _image_available_semaphores;
        std::vector<VkSemaphore> _render_finished_semaphores;
        std::vector<VkFence> _in_flight_fences;
        uint32_t _current_frame;

        bool _validation_enabled;

        std::vector<GpuTexture> _textures;
        std::vector<GpuMaterial> _materials;
        std::vector<GpuMesh> _meshes;
        std::map<std::string, TextureHandle> _texture_cache;
        std::map<std::string, MeshHandle> _mesh_cache;
        TextureHandle _default_white_texture;
        MaterialHandle _default_material;

        /// Creates the VkInstance, requesting the window's required extensions
        /// plus validation/debug extensions if available.
        void create_instance();
        /// Registers the debug messenger callback (only if validation layers are enabled).
        void setup_debug_messenger();
        /// Creates the VkSurfaceKHR for _window via its platform-specific backend.
        void create_surface();
        /// Selects a suitable VkPhysicalDevice.
        void pick_physical_device();
        /// Creates the logical VkDevice and retrieves its queues.
        void create_logical_device();

        /// Creates the swapchain for the window's current size.
        void create_swapchain();
        /// Creates an image view for every swapchain image.
        void create_image_views();
        void create_depth_resources();  ///< Also creates _scene_color_image (same lifetime).
        /// Creates the geometry render pass (_render_pass).
        void create_render_pass();
        /// Creates the material (set 0) and global (set 1) descriptor set layouts.
        void create_descriptor_set_layouts();
        /// Creates the main textured/lit graphics pipeline.
        void create_graphics_pipeline();
        void create_framebuffers(); ///< Single _scene_framebuffer (color + depth).
        /// Creates the command pool used for all command buffer allocation.
        void create_command_pool();
        /// Creates the descriptor pools backing per-material and per-frame descriptor sets.
        void create_descriptor_pool();
        /// Creates the shared texture sampler used by every material.
        void create_texture_sampler();
        /// Allocates the per-frame-in-flight primary command buffers.
        void create_command_buffers();
        /// Creates the per-frame-in-flight semaphores/fences.
        void create_sync_objects();

        void create_post_process_resources(); ///< Render pass/layout/pool/set/pipeline, once.
        void create_post_framebuffers();      ///< Per swapchain image; also called on resize.
        void update_post_descriptor_set();    ///< (Re)point it at the current color/depth views.

        /// Creates the shadow render pass and per-caster image/view/framebuffer resources.
        void create_shadow_resources();
        /// Creates the depth-only shadow pipeline.
        void create_shadow_pipeline();
        void create_occlusion_resources(); ///< Query pools + _occlusion_pipeline, created once.
        /**
         * Reads back the previous recording's query results for this
         * frame-in-flight slot into _occlusion_visible, before that slot's
         * pool is reset and reused for this frame's own queries. Safe to
         * call without a wait: draw_frame()'s fence wait, just above, already
         * proved the GPU finished the work that produced these results.
         */
        void update_occlusion_results();
        /// Creates the per-frame-in-flight global UBO buffers and descriptor sets.
        void create_global_ubo_resources();
        /**
         * @brief Computes a shadow caster's light-space (view*projection) matrix.
         *
         * Directional lights get an orthographic light-space matrix
         * covering the (hardcoded, demo-scene-sized) scene bounds; point
         * lights get a perspective one aimed from the light's position at
         * the scene center. A point light's shadow is a single frustum, not
         * a full omnidirectional cubemap — a real limitation for a point
         * light that needs to shadow objects outside that cone, but exact
         * for whatever the frustum does cover, and the demo scene sits
         * entirely inside it.
         * @param shadow_caster Light to compute the matrix for.
         * @return The light-space view-projection matrix.
         */
        mat4 compute_light_space_matrix(const Light &shadow_caster) const;
        /**
         * @brief Fills and uploads the GlobalUbo for one frame-in-flight slot.
         * @param frame_index Which frame-in-flight's UBO to update.
         * @param view Camera view matrix.
         * @param projection Camera projection matrix.
         * @param light_space_matrices Per-caster light-space matrices.
         * @param shadow_caster_count How many entries of light_space_matrices are active.
         * @param view_position Camera world-space position.
         * @param lights Active scene lights.
         * @param ambient_intensity Flat ambient term.
         */
        void update_global_ubo(uint32_t frame_index, const mat4 &view, const mat4 &projection,
            const mat4 light_space_matrices[kMaxShadowCasters], uint32_t shadow_caster_count,
            const vec3 &view_position, const std::vector<Light> &lights, float ambient_intensity);

        /**
         * @brief Records one shadow-caster's depth-only render pass.
         * @param command_buffer Command buffer to record into.
         * @param caster_index Which shadow map/framebuffer slot to render into.
         * @param light_space_matrix This caster's light-space view-projection matrix.
         * @param items Full, unculled render item list (a caster outside the
         * camera frustum can still need to cast a shadow into it).
         */
        void record_shadow_pass(VkCommandBuffer command_buffer, uint32_t caster_index,
            const mat4 &light_space_matrix, const std::vector<RenderItem> &items);
        /**
         * @brief Records the geometry pass and the occlusion-query pass, then
         * the post-process composite pass.
         * @param command_buffer Command buffer to record into.
         * @param image_index Swapchain image index to composite into.
         * @param projection Camera projection matrix (used by the post-process pass).
         * @param draw_items Frustum- and occlusion-culled — what actually gets
         * lit/shaded/composited this frame.
         * @param occlusion_test_items Every frustum-visible item that has a real
         * occlusion_id (see RenderItem); after draw_items is drawn for
         * real, each of these is redrawn once more with _occlusion_pipeline
         * (no color/depth writes) inside an occlusion query, to find out
         * whether it would contribute a visible pixel — the answer used to
         * populate/cull draw_items two frames from now.
         * @param out_query_ids Receives, in the same order queries were
         * issued, which occlusion_id each one belongs to (see
         * update_occlusion_results()).
         */
        void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index,
            const mat4 &projection, const std::vector<RenderItem> &draw_items,
            const std::vector<RenderItem> &occlusion_test_items,
            std::vector<uint32_t> *out_query_ids);

        /// Recreates the swapchain and everything sized from it (e.g. after a resize).
        void recreate_swapchain();
        /// Destroys the swapchain and everything sized from it.
        void cleanup_swapchain();

        /// Loads a precompiled SPIR-V shader module from disk.
        /// @param path Path to a `.spv` file.
        /// @return The created shader module.
        VkShaderModule load_shader_module(const char *path);
        /// @return The memory type index matching `type_filter` and `properties`.
        uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
        /// Allocates a VkBuffer + backing VkDeviceMemory with the given usage/properties.
        void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_memory);
        /// Uploads `data` to a new device-local buffer via a staging buffer.
        void upload_to_device_local_buffer(const void *data, VkDeviceSize size,
            VkBufferUsageFlags usage, VkBuffer *out_buffer, VkDeviceMemory *out_memory);
        /// @return The best supported depth format for this physical device.
        VkFormat find_depth_format();

        /// @return A command buffer allocated and begun for a one-off, immediately-submitted command.
        VkCommandBuffer begin_single_time_commands();
        /// Ends, submits, and waits on a command buffer from begin_single_time_commands().
        void end_single_time_commands(VkCommandBuffer command_buffer);
        /// Records an image layout transition barrier.
        void transition_image_layout(VkImage image, VkFormat format,
            VkImageLayout old_layout, VkImageLayout new_layout);
        /// Records a buffer-to-image copy for uploading pixel data.
        void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

        /// @return Handle to a lazily-created 1x1 white fallback texture.
        TextureHandle create_default_white_texture();
        /// Loads a texture from disk (TGA), caching by path.
        /// @return Handle to the loaded (or cached) texture.
        TextureHandle load_texture(const std::string &path);
        /// Uploads already-decoded image data as a texture.
        /// @param image Decoded pixel data.
        /// @param debug_name Name used for cache bookkeeping/diagnostics.
        /// @return Handle to the uploaded texture.
        TextureHandle load_texture_from_image_data(const ImageData &image, const std::string &debug_name);
        /// Creates a GPU-resident material (descriptor set + push-constant parameters) from parsed data.
        /// @return Handle to the created material.
        MaterialHandle create_material(const MaterialData &data);
};

} // namespace vre
