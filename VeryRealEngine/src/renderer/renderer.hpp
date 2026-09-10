// Vulkan renderer: instance/device setup, swapchain, a depth-tested
// textured+lit pipeline with shadow mapping, and a handle-based
// mesh/material/texture system.
//
// Step 1 (../../verdict.md roadmap) proved swapchain + pipeline + depth
// buffer. Step 2 added OBJ-loaded meshes and materials/textures. Step 5
// adds multiple light sources and a directional-light shadow map: a
// depth-only render pass from the light's point of view, sampled back in
// the main pass's fragment shader.
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

using TextureHandle = size_t;
using MaterialHandle = size_t;
using MeshHandle = size_t;

// One item to draw this frame: a loaded mesh placed in the world by a
// model matrix. The renderer combines this with the camera's view/projection
// (passed separately to draw_frame) to build each draw call's MVP.
struct RenderItem
{
    MeshHandle mesh;
    mat4 model;
};

enum class LightType
{
    Directional,
    Point,
};

struct Light
{
    LightType type = LightType::Directional;
    // Directional: unit direction the light travels (e.g. (0,-1,0) = straight down).
    // Point: world-space position.
    vec3 direction_or_position;
    vec3 color{1.0f, 1.0f, 1.0f};
    float intensity = 1.0f;
};

class Renderer
{
    public:
        static constexpr uint32_t kMaxFramesInFlight = 2;
        static constexpr uint32_t kMaxLights = 4;
        static constexpr uint32_t kShadowMapResolution = 2048;

        Renderer();
        ~Renderer();

        bool initialize(Window *window);
        void destroy();
        void wait_idle();

        // Loads an .obj (and any .mtl/.tga it references) and uploads it to
        // GPU-resident buffers. Textures and materials are cached by path,
        // so loading the same texture from two different meshes only
        // uploads it once. Safe to call for several different files before
        // the first draw_frame() — that's the "multiple OBJs at once" case.
        MeshHandle load_mesh_from_obj(const char *path);

        // `lights[0]` must be a Directional light — it's the sole shadow
        // caster (a single shadow map, rendered every frame from its
        // point of view, so both static geometry and moving objects cast
        // correct shadows — that's step 5's "static and dynamic shadow
        // rendering"). Additional lights (up to kMaxLights) contribute to
        // shading but do not cast shadows — a deliberate scope line for
        // this step, not a hard engine limitation.
        void draw_frame(const mat4 &view, const mat4 &projection, const vec3 &view_position,
            const std::vector<Light> &lights, float ambient_intensity,
            const std::vector<RenderItem> &items);

    private:
        struct GpuTexture
        {
            VkImage image = VK_NULL_HANDLE;
            VkDeviceMemory memory = VK_NULL_HANDLE;
            VkImageView view = VK_NULL_HANDLE;
        };

        struct GpuMaterial
        {
            VkDescriptorSet descriptor_set = VK_NULL_HANDLE;
            float diffuse_tint[3] = {1.0f, 1.0f, 1.0f};
        };

        struct GpuSubMesh
        {
            uint32_t index_offset;
            uint32_t index_count;
            MaterialHandle material;
        };

        struct GpuMesh
        {
            VkBuffer vertex_buffer = VK_NULL_HANDLE;
            VkDeviceMemory vertex_buffer_memory = VK_NULL_HANDLE;
            VkBuffer index_buffer = VK_NULL_HANDLE;
            VkDeviceMemory index_buffer_memory = VK_NULL_HANDLE;
            std::vector<GpuSubMesh> submeshes;
        };

        // Mirrors the GlobalUbo block declared in shaders/mesh.vert /
        // mesh.frag — layout and field order must match exactly (std140).
        struct GlobalUbo
        {
            mat4 view_proj;
            mat4 light_space_matrix;
            float light_direction_or_position[kMaxLights][4]; // xyz + type (0=dir,1=point)
            float light_color_intensity[kMaxLights][4];       // rgb + intensity
            float light_count_ambient[4];                     // x = count, y = ambient
            float view_position[4];
        };

        Window *_window;

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
        std::vector<VkFramebuffer> _swapchain_framebuffers;

        VkFormat _depth_format;
        VkImage _depth_image;
        VkDeviceMemory _depth_image_memory;
        VkImageView _depth_image_view;

        VkRenderPass _render_pass;
        VkDescriptorSetLayout _material_set_layout;  // set = 0: per-material diffuse sampler
        VkDescriptorSetLayout _global_set_layout;     // set = 1: per-frame UBO + shadow map
        VkDescriptorPool _descriptor_pool;
        VkDescriptorPool _global_descriptor_pool;
        VkPipelineLayout _pipeline_layout;
        VkPipeline _graphics_pipeline;
        VkSampler _texture_sampler;

        // Shadow map: a separate depth-only render pass/pipeline/framebuffer,
        // fixed resolution independent of the swapchain (no need to
        // recreate it on window resize).
        VkRenderPass _shadow_render_pass;
        VkPipelineLayout _shadow_pipeline_layout;
        VkPipeline _shadow_pipeline;
        VkImage _shadow_image;
        VkDeviceMemory _shadow_image_memory;
        VkImageView _shadow_image_view;
        VkFramebuffer _shadow_framebuffer;
        VkSampler _shadow_sampler;

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

        void create_instance();
        void setup_debug_messenger();
        void create_surface();
        void pick_physical_device();
        void create_logical_device();

        void create_swapchain();
        void create_image_views();
        void create_depth_resources();
        void create_render_pass();
        void create_descriptor_set_layouts();
        void create_graphics_pipeline();
        void create_framebuffers();
        void create_command_pool();
        void create_descriptor_pool();
        void create_texture_sampler();
        void create_command_buffers();
        void create_sync_objects();

        void create_shadow_resources();
        void create_shadow_pipeline();
        void create_global_ubo_resources();
        mat4 compute_light_space_matrix(const Light &shadow_caster) const;
        void update_global_ubo(uint32_t frame_index, const mat4 &view, const mat4 &projection,
            const mat4 &light_space_matrix, const vec3 &view_position,
            const std::vector<Light> &lights, float ambient_intensity);

        void record_shadow_pass(VkCommandBuffer command_buffer, const mat4 &light_space_matrix,
            const std::vector<RenderItem> &items);
        void record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index,
            const std::vector<RenderItem> &items);

        void recreate_swapchain();
        void cleanup_swapchain();

        VkShaderModule load_shader_module(const char *path);
        uint32_t find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties);
        void create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_memory);
        void upload_to_device_local_buffer(const void *data, VkDeviceSize size,
            VkBufferUsageFlags usage, VkBuffer *out_buffer, VkDeviceMemory *out_memory);
        VkFormat find_depth_format();

        VkCommandBuffer begin_single_time_commands();
        void end_single_time_commands(VkCommandBuffer command_buffer);
        void transition_image_layout(VkImage image, VkFormat format,
            VkImageLayout old_layout, VkImageLayout new_layout);
        void copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height);

        TextureHandle create_default_white_texture();
        TextureHandle load_texture(const std::string &path);
        TextureHandle load_texture_from_image_data(const ImageData &image, const std::string &debug_name);
        MaterialHandle create_material(const MaterialData &data);
};

} // namespace vre
