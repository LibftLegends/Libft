#include "vulkaninstance.hpp"

namespace vre
{
VulkanInstance::VulkanInstance() : _window(nullptr), _instance(VK_NULL_HANDLE),
	_debug_messenger(VK_NULL_HANDLE), _surface(VK_NULL_HANDLE),
	_validation_enabled(false)
{
}

VulkanInstance::~VulkanInstance()
{
}

VkBool32 VulkanInstance::debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT severity,
	VkDebugUtilsMessageTypeFlagsEXT /*type*/,
	const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
	void * /*user_data*/)
{
	if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
		std::fprintf(stderr, "[validation] %s\n", callback_data->pMessage);
	return (VK_FALSE);
}

bool VulkanInstance::layer_is_available(const char *layer_name)
{
	uint32_t	layer_count;

	layer_count = 0;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
	std::vector<VkLayerProperties> layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, layers.data());
	for (const auto &layer : layers)
	{
		if (std::strcmp(layer.layerName, layer_name) == 0)
			return (true);
	}
	return (false);
}

#ifdef __APPLE__
bool VulkanInstance::instance_extension_is_available(const char *extension_name)
{
	uint32_t	extension_count;

	extension_count = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> extensions(extension_count);
	vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
		extensions.data());
	for (const auto &extension : extensions)
	{
		if (std::strcmp(extension.extensionName, extension_name) == 0)
			return (true);
	}
	return (false);
}
#endif

void VulkanInstance::create(Window *window)
{
	const char	*window_extensions[8];
	uint32_t	window_extension_count;

	_window = window;
	VkApplicationInfo app_info{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "VeryRealEngine Demo";
	app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.pEngineName = "VeryRealEngine";
	app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	app_info.apiVersion = VK_API_VERSION_1_2;
	window_extension_count = 0;
	_window->get_required_instance_extensions(window_extensions,
		&window_extension_count);
	std::vector<const char *> extensions(window_extensions, window_extensions
		+ window_extension_count);
	_validation_enabled = layer_is_available(kValidationLayer);
	if (_validation_enabled)
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	VkInstanceCreateInfo create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
#ifdef __APPLE__
	// MoltenVK is a "portability" (non-fully-conformant) Vulkan
	// implementation; the *standard Vulkan Loader* requires instances to
	// opt in explicitly via this extension + flag once it detects a
	// portability ICD, or vkCreateInstance returns
	// VK_ERROR_INCOMPATIBLE_DRIVER. This engine links directly against
	// MoltenVK's own dylib instead (see the Makefile), which doesn't
	// advertise or require this extension at all — requesting it
	// unconditionally fails instance creation with
	// VK_ERROR_EXTENSION_NOT_PRESENT. Checked dynamically so this does the
	// right thing either way (direct MoltenVK link today, a real loader +
	// ICD manifest if that ever changes).
	if (instance_extension_is_available(
			VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
	{
		extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
		create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
	}
#endif
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
	if (_validation_enabled)
		setup_debug_messenger();
	create_surface(window);
}

void VulkanInstance::setup_debug_messenger()
{
	PFN_vkCreateDebugUtilsMessengerEXT	create_fn;

	VkDebugUtilsMessengerCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
		| VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	create_info.pfnUserCallback = debug_callback;
	create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
			vkGetInstanceProcAddr(_instance,
				"vkCreateDebugUtilsMessengerEXT"));
	if (create_fn != nullptr)
		create_fn(_instance, &create_info, nullptr, &_debug_messenger);
}

void VulkanInstance::create_surface(Window *window)
{
	VK_CHECK(window->create_vulkan_surface(_instance, &_surface));
}

void VulkanInstance::destroy()
{
	PFN_vkDestroyDebugUtilsMessengerEXT	destroy_fn;

	if (_surface != VK_NULL_HANDLE)
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
	_surface = VK_NULL_HANDLE;
	if (_debug_messenger != VK_NULL_HANDLE)
	{
		destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
				vkGetInstanceProcAddr(_instance,
					"vkDestroyDebugUtilsMessengerEXT"));
		if (destroy_fn != nullptr)
			destroy_fn(_instance, _debug_messenger, nullptr);
	}
	_debug_messenger = VK_NULL_HANDLE;
	if (_instance != VK_NULL_HANDLE)
		vkDestroyInstance(_instance, nullptr);
	_instance = VK_NULL_HANDLE;
}

VkInstance VulkanInstance::instance() const
{
	return (_instance);
}

VkSurfaceKHR VulkanInstance::surface() const
{
	return (_surface);
}

bool VulkanInstance::validation_enabled() const
{
	return (_validation_enabled);
}

} // namespace vre
