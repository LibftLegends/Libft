#include "renderer.hpp"
#include "vk_check.hpp"
#include "../assets/obj_loader.hpp"
#include "../assets/tga_loader.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <vector>

namespace vre
{

static const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";

// Pushed per draw call. The model matrix (not a precomputed MVP) because
// view/projection now live in the per-frame GlobalUbo (set = 1) — see
// shaders/mesh.vert. shaders/shadow.vert instead pushes a single
// light-space MVP (computed on the CPU per object, since the shadow pass
// has no need for a separate model/view-proj split).
struct PushConstants
{
    mat4 model;
    float tint[4];
};

struct ShadowPushConstants
{
    mat4 light_mvp;
};

static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void * /*user_data*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        std::fprintf(stderr, "[validation] %s\n", callback_data->pMessage);
    return VK_FALSE;
}

static bool layer_is_available(const char *layer_name)
{
    uint32_t layer_count = 0;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    std::vector<VkLayerProperties> layers(layer_count);
    vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
    for (const auto &layer : layers)
    {
        if (std::strcmp(layer.layerName, layer_name) == 0)
            return true;
    }
    return false;
}

Renderer::Renderer()
    : _window(nullptr), _instance(VK_NULL_HANDLE), _debug_messenger(VK_NULL_HANDLE),
      _surface(VK_NULL_HANDLE), _physical_device(VK_NULL_HANDLE), _device(VK_NULL_HANDLE),
      _graphics_queue_family(0), _present_queue_family(0),
      _graphics_queue(VK_NULL_HANDLE), _present_queue(VK_NULL_HANDLE),
      _swapchain(VK_NULL_HANDLE), _swapchain_image_format(VK_FORMAT_UNDEFINED),
      _swapchain_extent{0, 0}, _depth_format(VK_FORMAT_UNDEFINED),
      _depth_image(VK_NULL_HANDLE), _depth_image_memory(VK_NULL_HANDLE),
      _depth_image_view(VK_NULL_HANDLE), _render_pass(VK_NULL_HANDLE),
      _material_set_layout(VK_NULL_HANDLE), _global_set_layout(VK_NULL_HANDLE),
      _descriptor_pool(VK_NULL_HANDLE), _global_descriptor_pool(VK_NULL_HANDLE),
      _pipeline_layout(VK_NULL_HANDLE), _graphics_pipeline(VK_NULL_HANDLE),
      _texture_sampler(VK_NULL_HANDLE),
      _shadow_render_pass(VK_NULL_HANDLE), _shadow_pipeline_layout(VK_NULL_HANDLE),
      _shadow_pipeline(VK_NULL_HANDLE), _shadow_image(VK_NULL_HANDLE),
      _shadow_image_memory(VK_NULL_HANDLE), _shadow_image_view(VK_NULL_HANDLE),
      _shadow_framebuffer(VK_NULL_HANDLE), _shadow_sampler(VK_NULL_HANDLE),
      _command_pool(VK_NULL_HANDLE), _current_frame(0),
      _validation_enabled(false), _default_white_texture(0), _default_material(0)
{
}

Renderer::~Renderer()
{
    destroy();
}

bool Renderer::initialize(Window *window)
{
    _window = window;

    create_instance();
    if (_validation_enabled)
        setup_debug_messenger();
    create_surface();
    pick_physical_device();
    create_logical_device();

    create_swapchain();
    create_image_views();
    create_depth_resources();
    create_render_pass();
    create_descriptor_set_layouts();

    create_shadow_resources();
    create_shadow_pipeline();
    create_graphics_pipeline();

    create_framebuffers();
    create_command_pool();
    create_descriptor_pool();
    create_texture_sampler();
    create_global_ubo_resources();
    create_command_buffers();
    create_sync_objects();

    _default_white_texture = create_default_white_texture();
    MaterialData default_material_data;
    default_material_data.name = "__default_white";
    _default_material = create_material(default_material_data);

    return true;
}

void Renderer::wait_idle()
{
    if (_device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(_device);
}

void Renderer::cleanup_swapchain()
{
    if (_depth_image_view != VK_NULL_HANDLE)
        vkDestroyImageView(_device, _depth_image_view, nullptr);
    if (_depth_image != VK_NULL_HANDLE)
        vkDestroyImage(_device, _depth_image, nullptr);
    if (_depth_image_memory != VK_NULL_HANDLE)
        vkFreeMemory(_device, _depth_image_memory, nullptr);

    for (auto framebuffer : _swapchain_framebuffers)
        vkDestroyFramebuffer(_device, framebuffer, nullptr);
    _swapchain_framebuffers.clear();

    for (auto image_view : _swapchain_image_views)
        vkDestroyImageView(_device, image_view, nullptr);
    _swapchain_image_views.clear();

    if (_swapchain != VK_NULL_HANDLE)
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

void Renderer::destroy()
{
    if (_device == VK_NULL_HANDLE)
        return;

    wait_idle();

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
    {
        vkDestroySemaphore(_device, _render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(_device, _image_available_semaphores[i], nullptr);
        vkDestroyFence(_device, _in_flight_fences[i], nullptr);
    }

    for (auto &mesh : _meshes)
    {
        vkDestroyBuffer(_device, mesh.index_buffer, nullptr);
        vkFreeMemory(_device, mesh.index_buffer_memory, nullptr);
        vkDestroyBuffer(_device, mesh.vertex_buffer, nullptr);
        vkFreeMemory(_device, mesh.vertex_buffer_memory, nullptr);
    }
    _meshes.clear();

    for (auto &texture : _textures)
    {
        vkDestroyImageView(_device, texture.view, nullptr);
        vkDestroyImage(_device, texture.image, nullptr);
        vkFreeMemory(_device, texture.memory, nullptr);
    }
    _textures.clear();

    for (size_t i = 0; i < _global_ubo_buffers.size(); i++)
    {
        vkUnmapMemory(_device, _global_ubo_memories[i]);
        vkDestroyBuffer(_device, _global_ubo_buffers[i], nullptr);
        vkFreeMemory(_device, _global_ubo_memories[i], nullptr);
    }
    _global_ubo_buffers.clear();
    _global_ubo_memories.clear();
    _global_ubo_mapped.clear();

    vkDestroySampler(_device, _texture_sampler, nullptr);
    vkDestroySampler(_device, _shadow_sampler, nullptr);
    vkDestroyDescriptorPool(_device, _descriptor_pool, nullptr);
    vkDestroyDescriptorPool(_device, _global_descriptor_pool, nullptr);

    vkDestroyCommandPool(_device, _command_pool, nullptr);

    vkDestroyPipeline(_device, _graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _pipeline_layout, nullptr);
    vkDestroyPipeline(_device, _shadow_pipeline, nullptr);
    vkDestroyPipelineLayout(_device, _shadow_pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(_device, _material_set_layout, nullptr);
    vkDestroyDescriptorSetLayout(_device, _global_set_layout, nullptr);
    vkDestroyRenderPass(_device, _render_pass, nullptr);

    vkDestroyFramebuffer(_device, _shadow_framebuffer, nullptr);
    vkDestroyImageView(_device, _shadow_image_view, nullptr);
    vkDestroyImage(_device, _shadow_image, nullptr);
    vkFreeMemory(_device, _shadow_image_memory, nullptr);
    vkDestroyRenderPass(_device, _shadow_render_pass, nullptr);

    cleanup_swapchain();

    vkDestroyDevice(_device, nullptr);
    _device = VK_NULL_HANDLE;

    if (_surface != VK_NULL_HANDLE)
        vkDestroySurfaceKHR(_instance, _surface, nullptr);

    if (_debug_messenger != VK_NULL_HANDLE)
    {
        auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroy_fn != nullptr)
            destroy_fn(_instance, _debug_messenger, nullptr);
    }

    if (_instance != VK_NULL_HANDLE)
        vkDestroyInstance(_instance, nullptr);
    _instance = VK_NULL_HANDLE;
}

void Renderer::create_instance()
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "VeryRealEngine Demo";
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = "VeryRealEngine";
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.apiVersion = VK_API_VERSION_1_2;

    const char *window_extensions[8];
    uint32_t window_extension_count = 0;
    _window->get_required_instance_extensions(window_extensions, &window_extension_count);

    std::vector<const char *> extensions(window_extensions,
        window_extensions + window_extension_count);

    _validation_enabled = layer_is_available(kValidationLayer);
    if (_validation_enabled)
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create_info.ppEnabledExtensionNames = extensions.data();

    if (_validation_enabled)
    {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &kValidationLayer;
    }
    else
    {
        create_info.enabledLayerCount = 0;
        std::fprintf(stderr,
            "Renderer: VK_LAYER_KHRONOS_validation not found, "
            "running without validation layers\n");
    }

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &_instance));
}

void Renderer::setup_debug_messenger()
{
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = debug_callback;

    auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(_instance, "vkCreateDebugUtilsMessengerEXT"));
    if (create_fn != nullptr)
        create_fn(_instance, &create_info, nullptr, &_debug_messenger);
}

void Renderer::create_surface()
{
    VK_CHECK(_window->create_vulkan_surface(_instance, &_surface));
}

struct SwapchainSupport
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> present_modes;
};

static SwapchainSupport query_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &support.capabilities);

    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, nullptr);
    support.formats.resize(format_count);
    if (format_count > 0)
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count, support.formats.data());

    uint32_t present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count, nullptr);
    support.present_modes.resize(present_mode_count);
    if (present_mode_count > 0)
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &present_mode_count,
            support.present_modes.data());

    return support;
}

static bool find_queue_families(VkPhysicalDevice device, VkSurfaceKHR surface,
    uint32_t *out_graphics_family, uint32_t *out_present_family)
{
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

    bool found_graphics = false;
    bool found_present = false;
    for (uint32_t i = 0; i < queue_family_count; i++)
    {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            *out_graphics_family = i;
            found_graphics = true;
        }

        VkBool32 present_support = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
        if (present_support == VK_TRUE)
        {
            *out_present_family = i;
            found_present = true;
        }

        if (found_graphics && found_present)
            return true;
    }
    return (found_graphics && found_present);
}

static bool device_supports_extensions(VkPhysicalDevice device)
{
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> available(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available.data());

    for (const auto &extension : available)
    {
        if (std::strcmp(extension.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            return true;
    }
    return false;
}

void Renderer::pick_physical_device()
{
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
    if (device_count == 0)
    {
        std::fprintf(stderr, "Renderer: no Vulkan-capable physical device found\n");
        std::abort();
    }
    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(_instance, &device_count, devices.data());

    for (auto device : devices)
    {
        if (!device_supports_extensions(device))
            continue;

        SwapchainSupport support = query_swapchain_support(device, _surface);
        if (support.formats.empty() || support.present_modes.empty())
            continue;

        VkPhysicalDeviceFeatures features;
        vkGetPhysicalDeviceFeatures(device, &features);
        if (!features.samplerAnisotropy)
            continue;

        uint32_t graphics_family = 0;
        uint32_t present_family = 0;
        if (!find_queue_families(device, _surface, &graphics_family, &present_family))
            continue;

        _physical_device = device;
        _graphics_queue_family = graphics_family;
        _present_queue_family = present_family;
        break;
    }

    if (_physical_device == VK_NULL_HANDLE)
    {
        std::fprintf(stderr, "Renderer: no suitable Vulkan physical device found\n");
        std::abort();
    }

    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(_physical_device, &properties);
    std::fprintf(stderr, "Renderer: using physical device \"%s\"\n", properties.deviceName);
}

void Renderer::create_logical_device()
{
    std::set<uint32_t> unique_queue_families = {_graphics_queue_family, _present_queue_family};
    std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
    float queue_priority = 1.0f;

    for (uint32_t family : unique_queue_families)
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = family;
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    VkPhysicalDeviceFeatures device_features{};
    device_features.samplerAnisotropy = VK_TRUE;

    const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
    create_info.pQueueCreateInfos = queue_create_infos.data();
    create_info.pEnabledFeatures = &device_features;
    create_info.enabledExtensionCount = 1;
    create_info.ppEnabledExtensionNames = device_extensions;

    if (_validation_enabled)
    {
        create_info.enabledLayerCount = 1;
        create_info.ppEnabledLayerNames = &kValidationLayer;
    }

    VK_CHECK(vkCreateDevice(_physical_device, &create_info, nullptr, &_device));

    vkGetDeviceQueue(_device, _graphics_queue_family, 0, &_graphics_queue);
    vkGetDeviceQueue(_device, _present_queue_family, 0, &_present_queue);
}

static VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR> &formats)
{
    for (const auto &format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return format;
    }
    return formats[0];
}

static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR> &modes)
{
    for (const auto &mode : modes)
    {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
            return mode;
    }
    return VK_PRESENT_MODE_FIFO_KHR; // guaranteed available, vsync'd
}

static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR &capabilities,
    int32_t window_width, int32_t window_height)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
        return capabilities.currentExtent;

    VkExtent2D extent{static_cast<uint32_t>(window_width), static_cast<uint32_t>(window_height)};
    extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height, capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}

void Renderer::create_swapchain()
{
    SwapchainSupport support = query_swapchain_support(_physical_device, _surface);

    VkSurfaceFormatKHR surface_format = choose_surface_format(support.formats);
    VkPresentModeKHR present_mode = choose_present_mode(support.present_modes);
    VkExtent2D extent = choose_extent(support.capabilities,
        _window->get_width(), _window->get_height());

    uint32_t image_count = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && image_count > support.capabilities.maxImageCount)
        image_count = support.capabilities.maxImageCount;

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = _surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    uint32_t queue_family_indices[] = {_graphics_queue_family, _present_queue_family};
    if (_graphics_queue_family != _present_queue_family)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    create_info.preTransform = support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(_device, &create_info, nullptr, &_swapchain));

    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, nullptr);
    _swapchain_images.resize(image_count);
    vkGetSwapchainImagesKHR(_device, _swapchain, &image_count, _swapchain_images.data());

    _swapchain_image_format = surface_format.format;
    _swapchain_extent = extent;
}

static VkImageView create_image_view(VkDevice device, VkImage image, VkFormat format,
    VkImageAspectFlags aspect_flags)
{
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = image;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.subresourceRange.aspectMask = aspect_flags;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = 1;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = 1;

    VkImageView view;
    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &view));
    return view;
}

void Renderer::create_image_views()
{
    _swapchain_image_views.resize(_swapchain_images.size());
    for (size_t i = 0; i < _swapchain_images.size(); i++)
    {
        _swapchain_image_views[i] = create_image_view(_device, _swapchain_images[i],
            _swapchain_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
}

VkFormat Renderer::find_depth_format()
{
    VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (VkFormat format : candidates)
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(_physical_device, format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    std::fprintf(stderr, "Renderer: no supported depth format found\n");
    std::abort();
}

uint32_t Renderer::find_memory_type(uint32_t type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(_physical_device, &memory_properties);

    for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
    {
        if ((type_filter & (1 << i))
            && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
            return i;
    }
    std::fprintf(stderr, "Renderer: failed to find suitable memory type\n");
    std::abort();
}

void Renderer::create_depth_resources()
{
    _depth_format = find_depth_format();

    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = _swapchain_extent.width;
    image_info.extent.height = _swapchain_extent.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = _depth_format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(_device, &image_info, nullptr, &_depth_image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(_device, _depth_image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(_device, &alloc_info, nullptr, &_depth_image_memory));
    vkBindImageMemory(_device, _depth_image, _depth_image_memory, 0);

    _depth_image_view = create_image_view(_device, _depth_image, _depth_format,
        VK_IMAGE_ASPECT_DEPTH_BIT);
}

void Renderer::create_render_pass()
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = _swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = _depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference color_ref{};
    color_ref.attachment = 0;
    color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 1;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_ref;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
        | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkAttachmentDescription attachments[] = {color_attachment, depth_attachment};

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 2;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_render_pass));
}

void Renderer::create_descriptor_set_layouts()
{
    // Set 0: per-material diffuse sampler (one descriptor set per material,
    // allocated in create_material()).
    VkDescriptorSetLayoutBinding material_sampler_binding{};
    material_sampler_binding.binding = 0;
    material_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    material_sampler_binding.descriptorCount = 1;
    material_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo material_layout_info{};
    material_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    material_layout_info.bindingCount = 1;
    material_layout_info.pBindings = &material_sampler_binding;

    VK_CHECK(vkCreateDescriptorSetLayout(_device, &material_layout_info, nullptr,
        &_material_set_layout));

    // Set 1: per-frame globals — the GlobalUbo (view/proj, lights, shadow
    // matrix) and the shadow map itself, shared by every draw call in the frame.
    VkDescriptorSetLayoutBinding ubo_binding{};
    ubo_binding.binding = 0;
    ubo_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_binding.descriptorCount = 1;
    ubo_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding shadow_sampler_binding{};
    shadow_sampler_binding.binding = 1;
    shadow_sampler_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    shadow_sampler_binding.descriptorCount = 1;
    shadow_sampler_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding global_bindings[] = {ubo_binding, shadow_sampler_binding};

    VkDescriptorSetLayoutCreateInfo global_layout_info{};
    global_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    global_layout_info.bindingCount = 2;
    global_layout_info.pBindings = global_bindings;

    VK_CHECK(vkCreateDescriptorSetLayout(_device, &global_layout_info, nullptr,
        &_global_set_layout));
}

VkShaderModule Renderer::load_shader_module(const char *path)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        std::fprintf(stderr, "Renderer: failed to open shader file \"%s\"\n", path);
        std::abort();
    }

    size_t file_size = static_cast<size_t>(file.tellg());
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(file_size));
    file.close();

    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = buffer.size();
    create_info.pCode = reinterpret_cast<const uint32_t *>(buffer.data());

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(_device, &create_info, nullptr, &module));
    return module;
}

void Renderer::create_shadow_resources()
{
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = kShadowMapResolution;
    image_info.extent.height = kShadowMapResolution;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    image_info.format = _depth_format;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(_device, &image_info, nullptr, &_shadow_image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(_device, _shadow_image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(_device, &alloc_info, nullptr, &_shadow_image_memory));
    vkBindImageMemory(_device, _shadow_image, _shadow_image_memory, 0);

    _shadow_image_view = create_image_view(_device, _shadow_image, _depth_format,
        VK_IMAGE_ASPECT_DEPTH_BIT);

    // Depth-only render pass: no color attachment. finalLayout leaves the
    // image ready to be sampled by the main pass's fragment shader —
    // re-rendered (and re-transitioned) fresh every frame, which is what
    // makes shadows track both static and moving geometry correctly.
    VkAttachmentDescription depth_attachment{};
    depth_attachment.format = _depth_format;
    depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depth_attachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkAttachmentReference depth_ref{};
    depth_ref.attachment = 0;
    depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.pDepthStencilAttachment = &depth_ref;

    VkSubpassDependency dependencies[2]{};
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &depth_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 2;
    render_pass_info.pDependencies = dependencies;

    VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_shadow_render_pass));

    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = _shadow_render_pass;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = &_shadow_image_view;
    framebuffer_info.width = kShadowMapResolution;
    framebuffer_info.height = kShadowMapResolution;
    framebuffer_info.layers = 1;

    VK_CHECK(vkCreateFramebuffer(_device, &framebuffer_info, nullptr, &_shadow_framebuffer));

    // NEAREST + CLAMP_TO_BORDER(white): outside the light's frustum reads
    // back the maximum depth (1.0), which compute_shadow() in mesh.frag
    // treats as "nothing there to occlude" rather than an artificial edge.
    // No PCF/soft-shadow filtering — hard-edged shadows are enough to prove
    // the mechanism; softening them is a natural follow-up, not this step.
    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_NEAREST;
    sampler_info.minFilter = VK_FILTER_NEAREST;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    sampler_info.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    sampler_info.compareEnable = VK_FALSE;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;

    VK_CHECK(vkCreateSampler(_device, &sampler_info, nullptr, &_shadow_sampler));
}

void Renderer::create_shadow_pipeline()
{
    VkShaderModule vertex_module = load_shader_module("shaders/shadow.vert.spv");

    VkPipelineShaderStageCreateInfo vertex_stage{};
    vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = vertex_module;
    vertex_stage.pName = "main";

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription position_attribute{};
    position_attribute.binding = 0;
    position_attribute.location = 0;
    position_attribute.format = VK_FORMAT_R32G32B32_SFLOAT;
    position_attribute.offset = offsetof(MeshVertex, position);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 1;
    vertex_input.pVertexAttributeDescriptions = &position_attribute;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(kShadowMapResolution);
    viewport.height = static_cast<float>(kShadowMapResolution);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{{0, 0}, {kShadowMapResolution, kShadowMapResolution}};

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    // Cull front faces (instead of the main pass's back faces) for the
    // shadow pass: a standard trick that biases surviving depth samples
    // toward the lit-side surface, reducing shadow acne without a large bias.
    rasterizer.cullMode = VK_CULL_MODE_FRONT_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_TRUE;
    rasterizer.depthBiasConstantFactor = 1.25f;
    rasterizer.depthBiasSlopeFactor = 1.75f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 0;

    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(ShadowPushConstants);

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant_range;

    VK_CHECK(vkCreatePipelineLayout(_device, &layout_info, nullptr, &_shadow_pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 1;
    pipeline_info.pStages = &vertex_stage;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.layout = _shadow_pipeline_layout;
    pipeline_info.renderPass = _shadow_render_pass;
    pipeline_info.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
        &_shadow_pipeline));

    vkDestroyShaderModule(_device, vertex_module, nullptr);
}

void Renderer::create_graphics_pipeline()
{
    VkShaderModule vertex_module = load_shader_module("shaders/mesh.vert.spv");
    VkShaderModule fragment_module = load_shader_module("shaders/mesh.frag.spv");

    VkPipelineShaderStageCreateInfo vertex_stage{};
    vertex_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertex_stage.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertex_stage.module = vertex_module;
    vertex_stage.pName = "main";

    VkPipelineShaderStageCreateInfo fragment_stage{};
    fragment_stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragment_stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragment_stage.module = fragment_module;
    fragment_stage.pName = "main";

    VkPipelineShaderStageCreateInfo stages[] = {vertex_stage, fragment_stage};

    VkVertexInputBindingDescription binding{};
    binding.binding = 0;
    binding.stride = sizeof(MeshVertex);
    binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription attributes[3]{};
    attributes[0].binding = 0;
    attributes[0].location = 0;
    attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[0].offset = offsetof(MeshVertex, position);
    attributes[1].binding = 0;
    attributes[1].location = 1;
    attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributes[1].offset = offsetof(MeshVertex, normal);
    attributes[2].binding = 0;
    attributes[2].location = 2;
    attributes[2].format = VK_FORMAT_R32G32_SFLOAT;
    attributes[2].offset = offsetof(MeshVertex, uv);

    VkPipelineVertexInputStateCreateInfo vertex_input{};
    vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input.vertexBindingDescriptionCount = 1;
    vertex_input.pVertexBindingDescriptions = &binding;
    vertex_input.vertexAttributeDescriptionCount = 3;
    vertex_input.pVertexAttributeDescriptions = attributes;

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo depth_stencil{};
    depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil.depthTestEnable = VK_TRUE;
    depth_stencil.depthWriteEnable = VK_TRUE;
    depth_stencil.depthCompareOp = VK_COMPARE_OP_LESS;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT
        | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    VkDynamicState dynamic_states[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state{};
    dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state.dynamicStateCount = 2;
    dynamic_state.pDynamicStates = dynamic_states;

    VkPushConstantRange push_constant_range{};
    push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PushConstants);

    VkDescriptorSetLayout set_layouts[] = {_material_set_layout, _global_set_layout};

    VkPipelineLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layout_info.setLayoutCount = 2;
    layout_info.pSetLayouts = set_layouts;
    layout_info.pushConstantRangeCount = 1;
    layout_info.pPushConstantRanges = &push_constant_range;

    VK_CHECK(vkCreatePipelineLayout(_device, &layout_info, nullptr, &_pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = stages;
    pipeline_info.pVertexInputState = &vertex_input;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = &depth_stencil;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;
    pipeline_info.layout = _pipeline_layout;
    pipeline_info.renderPass = _render_pass;
    pipeline_info.subpass = 0;

    VK_CHECK(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr,
        &_graphics_pipeline));

    vkDestroyShaderModule(_device, fragment_module, nullptr);
    vkDestroyShaderModule(_device, vertex_module, nullptr);
}

void Renderer::create_framebuffers()
{
    _swapchain_framebuffers.resize(_swapchain_image_views.size());
    for (size_t i = 0; i < _swapchain_image_views.size(); i++)
    {
        VkImageView attachments[] = {_swapchain_image_views[i], _depth_image_view};

        VkFramebufferCreateInfo framebuffer_info{};
        framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebuffer_info.renderPass = _render_pass;
        framebuffer_info.attachmentCount = 2;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = _swapchain_extent.width;
        framebuffer_info.height = _swapchain_extent.height;
        framebuffer_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(_device, &framebuffer_info, nullptr,
            &_swapchain_framebuffers[i]));
    }
}

void Renderer::create_command_pool()
{
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool_info.queueFamilyIndex = _graphics_queue_family;

    VK_CHECK(vkCreateCommandPool(_device, &pool_info, nullptr, &_command_pool));
}

void Renderer::create_descriptor_pool()
{
    // Modest fixed budget: plenty for a hand-authored demo scene. A scene
    // system with dynamic material counts would size (or grow) this instead.
    const uint32_t kMaxMaterials = 64;

    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_size.descriptorCount = kMaxMaterials;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = kMaxMaterials;

    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_descriptor_pool));
}

void Renderer::create_texture_sampler()
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(_physical_device, &properties);

    VkSamplerCreateInfo sampler_info{};
    sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sampler_info.magFilter = VK_FILTER_LINEAR;
    sampler_info.minFilter = VK_FILTER_LINEAR;
    sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampler_info.anisotropyEnable = VK_TRUE;
    sampler_info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    sampler_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    sampler_info.compareOp = VK_COMPARE_OP_ALWAYS;
    sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VK_CHECK(vkCreateSampler(_device, &sampler_info, nullptr, &_texture_sampler));
}

void Renderer::create_global_ubo_resources()
{
    VkDeviceSize buffer_size = sizeof(GlobalUbo);

    _global_ubo_buffers.resize(kMaxFramesInFlight);
    _global_ubo_memories.resize(kMaxFramesInFlight);
    _global_ubo_mapped.resize(kMaxFramesInFlight);

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
    {
        create_buffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &_global_ubo_buffers[i], &_global_ubo_memories[i]);
        vkMapMemory(_device, _global_ubo_memories[i], 0, buffer_size, 0, &_global_ubo_mapped[i]);
    }

    VkDescriptorPoolSize pool_sizes[2]{};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_sizes[0].descriptorCount = kMaxFramesInFlight;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[1].descriptorCount = kMaxFramesInFlight;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = kMaxFramesInFlight;

    VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &_global_descriptor_pool));

    std::vector<VkDescriptorSetLayout> layouts(kMaxFramesInFlight, _global_set_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = _global_descriptor_pool;
    alloc_info.descriptorSetCount = kMaxFramesInFlight;
    alloc_info.pSetLayouts = layouts.data();

    _global_descriptor_sets.resize(kMaxFramesInFlight);
    VK_CHECK(vkAllocateDescriptorSets(_device, &alloc_info, _global_descriptor_sets.data()));

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = _global_ubo_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(GlobalUbo);

        VkDescriptorImageInfo shadow_image_info{};
        shadow_image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        shadow_image_info.imageView = _shadow_image_view;
        shadow_image_info.sampler = _shadow_sampler;

        VkWriteDescriptorSet writes[2]{};
        writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet = _global_descriptor_sets[i];
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[0].pBufferInfo = &buffer_info;

        writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet = _global_descriptor_sets[i];
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1].pImageInfo = &shadow_image_info;

        vkUpdateDescriptorSets(_device, 2, writes, 0, nullptr);
    }
}

mat4 Renderer::compute_light_space_matrix(const Light &shadow_caster) const
{
    vec3 light_direction = vec3::normalize(shadow_caster.direction_or_position);

    // The engine doesn't compute a dynamic scene-bounds AABB yet (that's
    // future work alongside a general culling system) — this fixed box is
    // sized generously for the demo scene in assets/scenes/demo_scene.json.
    vec3 scene_center(0.0f, 0.5f, 0.0f);
    vec3 up(0.0f, 1.0f, 0.0f);
    if (std::fabs(vec3::dot(light_direction, up)) > 0.99f)
        up = vec3(0.0f, 0.0f, 1.0f); // avoid a degenerate look_at when the light is near-vertical

    vec3 light_position = scene_center - light_direction * 10.0f;
    mat4 light_view = mat4::look_at(light_position, scene_center, up);
    mat4 light_projection = mat4::orthographic(-6.0f, 6.0f, -6.0f, 6.0f, 0.1f, 20.0f);
    return mat4::multiply(light_projection, light_view);
}

void Renderer::update_global_ubo(uint32_t frame_index, const mat4 &view, const mat4 &projection,
    const mat4 &light_space_matrix, const vec3 &view_position,
    const std::vector<Light> &lights, float ambient_intensity)
{
    GlobalUbo ubo{};
    ubo.view_proj = mat4::multiply(projection, view);
    ubo.light_space_matrix = light_space_matrix;

    uint32_t light_count = std::min(static_cast<uint32_t>(lights.size()), kMaxLights);
    for (uint32_t i = 0; i < light_count; i++)
    {
        const Light &light = lights[i];
        ubo.light_direction_or_position[i][0] = light.direction_or_position.x;
        ubo.light_direction_or_position[i][1] = light.direction_or_position.y;
        ubo.light_direction_or_position[i][2] = light.direction_or_position.z;
        ubo.light_direction_or_position[i][3] = (light.type == LightType::Point) ? 1.0f : 0.0f;

        ubo.light_color_intensity[i][0] = light.color.x;
        ubo.light_color_intensity[i][1] = light.color.y;
        ubo.light_color_intensity[i][2] = light.color.z;
        ubo.light_color_intensity[i][3] = light.intensity;
    }

    ubo.light_count_ambient[0] = static_cast<float>(light_count);
    ubo.light_count_ambient[1] = ambient_intensity;
    ubo.view_position[0] = view_position.x;
    ubo.view_position[1] = view_position.y;
    ubo.view_position[2] = view_position.z;

    std::memcpy(_global_ubo_mapped[frame_index], &ubo, sizeof(GlobalUbo));
}

void Renderer::create_buffer(VkDeviceSize size, VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_memory)
{
    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(_device, &buffer_info, nullptr, out_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(_device, *out_buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(_device, &alloc_info, nullptr, out_memory));
    vkBindBufferMemory(_device, *out_buffer, *out_memory, 0);
}

VkCommandBuffer Renderer::begin_single_time_commands()
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = _command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    vkAllocateCommandBuffers(_device, &alloc_info, &command_buffer);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(command_buffer, &begin_info);

    return command_buffer;
}

void Renderer::end_single_time_commands(VkCommandBuffer command_buffer)
{
    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(_graphics_queue);

    vkFreeCommandBuffers(_device, _command_pool, 1, &command_buffer);
}

void Renderer::upload_to_device_local_buffer(const void *data, VkDeviceSize size,
    VkBufferUsageFlags usage, VkBuffer *out_buffer, VkDeviceMemory *out_memory)
{
    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    create_buffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory);

    void *mapped;
    vkMapMemory(_device, staging_memory, 0, size, 0, &mapped);
    std::memcpy(mapped, data, static_cast<size_t>(size));
    vkUnmapMemory(_device, staging_memory);

    create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, out_buffer, out_memory);

    VkCommandBuffer command_buffer = begin_single_time_commands();
    VkBufferCopy copy_region{};
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, staging_buffer, *out_buffer, 1, &copy_region);
    end_single_time_commands(command_buffer);

    vkDestroyBuffer(_device, staging_buffer, nullptr);
    vkFreeMemory(_device, staging_memory, nullptr);
}

void Renderer::transition_image_layout(VkImage image, VkFormat /*format*/,
    VkImageLayout old_layout, VkImageLayout new_layout)
{
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage;
    VkPipelineStageFlags destination_stage;

    if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED
        && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else
    {
        std::fprintf(stderr, "Renderer: unsupported image layout transition\n");
        std::abort();
    }

    vkCmdPipelineBarrier(command_buffer, source_stage, destination_stage, 0,
        0, nullptr, 0, nullptr, 1, &barrier);

    end_single_time_commands(command_buffer);
}

void Renderer::copy_buffer_to_image(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
{
    VkCommandBuffer command_buffer = begin_single_time_commands();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(command_buffer, buffer, image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    end_single_time_commands(command_buffer);
}

TextureHandle Renderer::create_default_white_texture()
{
    ImageData image;
    image.width = 1;
    image.height = 1;
    image.pixels = {255, 255, 255, 255};
    return load_texture_from_image_data(image, "__default_white");
}

TextureHandle Renderer::load_texture(const std::string &path)
{
    auto cached = _texture_cache.find(path);
    if (cached != _texture_cache.end())
        return cached->second;

    ImageData image;
    if (!load_tga(path.c_str(), &image))
    {
        std::fprintf(stderr, "Renderer: failed to load texture \"%s\", using default white\n",
            path.c_str());
        return _default_white_texture;
    }

    TextureHandle handle = load_texture_from_image_data(image, path);
    _texture_cache[path] = handle;
    return handle;
}

TextureHandle Renderer::load_texture_from_image_data(const ImageData &image, const std::string & /*debug_name*/)
{
    VkDeviceSize image_size = static_cast<VkDeviceSize>(image.pixels.size());

    VkBuffer staging_buffer;
    VkDeviceMemory staging_memory;
    create_buffer(image_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer, &staging_memory);

    void *mapped;
    vkMapMemory(_device, staging_memory, 0, image_size, 0, &mapped);
    std::memcpy(mapped, image.pixels.data(), static_cast<size_t>(image_size));
    vkUnmapMemory(_device, staging_memory);

    GpuTexture texture;
    VkImageCreateInfo image_info{};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.extent.width = image.width;
    image_info.extent.height = image.height;
    image_info.extent.depth = 1;
    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;
    // UNORM, not SRGB: this engine doesn't do gamma-correct lighting yet
    // (a follow-up to step 5's lighting pass), so textures are sampled and
    // shaded as plain linear color for now.
    image_info.format = VK_FORMAT_R8G8B8A8_UNORM;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateImage(_device, &image_info, nullptr, &texture.image));

    VkMemoryRequirements memory_requirements;
    vkGetImageMemoryRequirements(_device, texture.image, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = find_memory_type(memory_requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VK_CHECK(vkAllocateMemory(_device, &alloc_info, nullptr, &texture.memory));
    vkBindImageMemory(_device, texture.image, texture.memory, 0);

    transition_image_layout(texture.image, image_info.format,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copy_buffer_to_image(staging_buffer, texture.image, image.width, image.height);
    transition_image_layout(texture.image, image_info.format,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    vkDestroyBuffer(_device, staging_buffer, nullptr);
    vkFreeMemory(_device, staging_memory, nullptr);

    texture.view = create_image_view(_device, texture.image, image_info.format,
        VK_IMAGE_ASPECT_COLOR_BIT);

    TextureHandle handle = _textures.size();
    _textures.push_back(texture);
    return handle;
}

MaterialHandle Renderer::create_material(const MaterialData &data)
{
    TextureHandle texture = data.diffuse_texture_path.empty()
        ? _default_white_texture
        : load_texture(data.diffuse_texture_path);

    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = _descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &_material_set_layout;

    GpuMaterial material;
    VK_CHECK(vkAllocateDescriptorSets(_device, &alloc_info, &material.descriptor_set));

    VkDescriptorImageInfo image_info{};
    image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info.imageView = _textures[texture].view;
    image_info.sampler = _texture_sampler;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = material.descriptor_set;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.pImageInfo = &image_info;

    vkUpdateDescriptorSets(_device, 1, &write, 0, nullptr);

    material.diffuse_tint[0] = data.diffuse_color[0];
    material.diffuse_tint[1] = data.diffuse_color[1];
    material.diffuse_tint[2] = data.diffuse_color[2];

    MaterialHandle handle = _materials.size();
    _materials.push_back(material);
    return handle;
}

MeshHandle Renderer::load_mesh_from_obj(const char *path)
{
    auto cached = _mesh_cache.find(path);
    if (cached != _mesh_cache.end())
        return cached->second;

    MeshData mesh_data;
    std::vector<MaterialData> material_data;
    if (!load_obj(path, &mesh_data, &material_data))
    {
        std::fprintf(stderr, "Renderer: load_mesh_from_obj(\"%s\") failed\n", path);
        std::abort();
    }

    std::vector<MaterialHandle> local_to_global_material(material_data.size());
    for (size_t i = 0; i < material_data.size(); i++)
        local_to_global_material[i] = create_material(material_data[i]);

    GpuMesh mesh;
    upload_to_device_local_buffer(mesh_data.vertices.data(),
        sizeof(MeshVertex) * mesh_data.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        &mesh.vertex_buffer, &mesh.vertex_buffer_memory);
    upload_to_device_local_buffer(mesh_data.indices.data(),
        sizeof(uint32_t) * mesh_data.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        &mesh.index_buffer, &mesh.index_buffer_memory);

    for (const auto &submesh : mesh_data.submeshes)
    {
        GpuSubMesh gpu_submesh;
        gpu_submesh.index_offset = submesh.index_offset;
        gpu_submesh.index_count = submesh.index_count;
        gpu_submesh.material = (submesh.material_index >= 0)
            ? local_to_global_material[submesh.material_index]
            : _default_material;
        mesh.submeshes.push_back(gpu_submesh);
    }

    MeshHandle handle = _meshes.size();
    _meshes.push_back(mesh);
    _mesh_cache[path] = handle;
    std::fprintf(stderr, "Renderer: loaded \"%s\": %zu vertices, %zu indices, %zu submesh(es)\n",
        path, mesh_data.vertices.size(), mesh_data.indices.size(), mesh.submeshes.size());
    return handle;
}

void Renderer::create_command_buffers()
{
    _command_buffers.resize(kMaxFramesInFlight);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = _command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = kMaxFramesInFlight;

    VK_CHECK(vkAllocateCommandBuffers(_device, &alloc_info, _command_buffers.data()));
}

void Renderer::create_sync_objects()
{
    _image_available_semaphores.resize(kMaxFramesInFlight);
    _render_finished_semaphores.resize(kMaxFramesInFlight);
    _in_flight_fences.resize(kMaxFramesInFlight);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < kMaxFramesInFlight; i++)
    {
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr,
            &_image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_info, nullptr,
            &_render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence(_device, &fence_info, nullptr, &_in_flight_fences[i]));
    }
}

void Renderer::record_shadow_pass(VkCommandBuffer command_buffer, const mat4 &light_space_matrix,
    const std::vector<RenderItem> &items)
{
    VkClearValue clear_value{};
    clear_value.depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = _shadow_render_pass;
    render_pass_info.framebuffer = _shadow_framebuffer;
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = {kShadowMapResolution, kShadowMapResolution};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_value;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _shadow_pipeline);

    for (const auto &item : items)
    {
        const GpuMesh &mesh = _meshes[item.mesh];

        ShadowPushConstants push{};
        push.light_mvp = mat4::multiply(light_space_matrix, item.model);

        VkBuffer vertex_buffers[] = {mesh.vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdPushConstants(command_buffer, _shadow_pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(ShadowPushConstants), &push);

        for (const auto &submesh : mesh.submeshes)
            vkCmdDrawIndexed(command_buffer, submesh.index_count, 1, submesh.index_offset, 0, 0);
    }

    vkCmdEndRenderPass(command_buffer);
}

void Renderer::record_command_buffer(VkCommandBuffer command_buffer, uint32_t image_index,
    const std::vector<RenderItem> &items)
{
    VkClearValue clear_values[2];
    clear_values[0].color = {{0.02f, 0.02f, 0.05f, 1.0f}};
    clear_values[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = _render_pass;
    render_pass_info.framebuffer = _swapchain_framebuffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = _swapchain_extent;
    render_pass_info.clearValueCount = 2;
    render_pass_info.pClearValues = clear_values;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphics_pipeline);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(_swapchain_extent.width);
    viewport.height = static_cast<float>(_swapchain_extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, _swapchain_extent};
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline_layout,
        1, 1, &_global_descriptor_sets[_current_frame], 0, nullptr);

    for (const auto &item : items)
    {
        const GpuMesh &mesh = _meshes[item.mesh];

        PushConstants push{};
        push.model = item.model;

        VkBuffer vertex_buffers[] = {mesh.vertex_buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);
        vkCmdBindIndexBuffer(command_buffer, mesh.index_buffer, 0, VK_INDEX_TYPE_UINT32);

        for (const auto &submesh : mesh.submeshes)
        {
            const GpuMaterial &material = _materials[submesh.material];
            push.tint[0] = material.diffuse_tint[0];
            push.tint[1] = material.diffuse_tint[1];
            push.tint[2] = material.diffuse_tint[2];
            push.tint[3] = 1.0f;

            vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                _pipeline_layout, 0, 1, &material.descriptor_set, 0, nullptr);
            vkCmdPushConstants(command_buffer, _pipeline_layout,
                VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                0, sizeof(PushConstants), &push);

            vkCmdDrawIndexed(command_buffer, submesh.index_count, 1, submesh.index_offset, 0, 0);
        }
    }

    vkCmdEndRenderPass(command_buffer);
}

void Renderer::recreate_swapchain()
{
    // Extent 0 (minimized window) — wait until the window reports real size again.
    while (_window->get_width() == 0 || _window->get_height() == 0)
        _window->poll_events();

    vkDeviceWaitIdle(_device);

    cleanup_swapchain();

    create_swapchain();
    create_image_views();
    create_depth_resources();
    create_framebuffers();
}

void Renderer::draw_frame(const mat4 &view, const mat4 &projection, const vec3 &view_position,
    const std::vector<Light> &lights, float ambient_intensity,
    const std::vector<RenderItem> &items)
{
    vkWaitForFences(_device, 1, &_in_flight_fences[_current_frame], VK_TRUE, UINT64_MAX);

    uint32_t image_index = 0;
    VkResult result = vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX,
        _image_available_semaphores[_current_frame], VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        recreate_swapchain();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
    {
        std::fprintf(stderr, "Renderer: vkAcquireNextImageKHR failed (%d)\n", static_cast<int>(result));
        std::abort();
    }

    vkResetFences(_device, 1, &_in_flight_fences[_current_frame]);

    // lights[0] is the shadow caster and must be directional (see
    // draw_frame's doc comment in renderer.hpp); fall back to a sensible
    // default so a scene with no lights still shows *something* rather
    // than aborting or rendering fully black.
    static const Light kDefaultShadowCaster{
        LightType::Directional, vec3(-0.4f, -1.0f, -0.3f), vec3(1.0f, 1.0f, 1.0f), 1.0f};
    const Light &shadow_caster = (!lights.empty() && lights[0].type == LightType::Directional)
        ? lights[0] : kDefaultShadowCaster;
    mat4 light_space_matrix = compute_light_space_matrix(shadow_caster);

    update_global_ubo(_current_frame, view, projection, light_space_matrix, view_position,
        lights, ambient_intensity);

    VkCommandBuffer command_buffer = _command_buffers[_current_frame];
    vkResetCommandBuffer(command_buffer, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    record_shadow_pass(command_buffer, light_space_matrix, items);
    record_command_buffer(command_buffer, image_index, items);

    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkSemaphore wait_semaphores[] = {_image_available_semaphores[_current_frame]};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {_render_finished_semaphores[_current_frame]};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_CHECK(vkQueueSubmit(_graphics_queue, 1, &submit_info, _in_flight_fences[_current_frame]));

    VkSwapchainKHR swapchains[] = {_swapchain};
    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swapchains;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(_present_queue, &present_info);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR
        || _window->was_resized())
    {
        _window->clear_resized_flag();
        recreate_swapchain();
    }
    else if (result != VK_SUCCESS)
    {
        std::fprintf(stderr, "Renderer: vkQueuePresentKHR failed (%d)\n", static_cast<int>(result));
        std::abort();
    }

    _current_frame = (_current_frame + 1) % kMaxFramesInFlight;
}

} // namespace vre
