/**
 * @file vulkaninstance.hpp
 * @brief Owns the VkInstance, optional validation/debug messenger, and the
 * VkSurfaceKHR for the platform window.
 */
#pragma once

#include "../platform/window.hpp"
#include "../vre.hpp"
#include "vkcheck.hpp"

namespace vre
{
class VulkanInstance
{
  public:
	VulkanInstance();
	~VulkanInstance();

	/**
		* @brief Creates the VkInstance (requesting the window's required
		* extensions plus validation/debug extensions if available), sets
		* up the debug messenger if validation layers were found, and
		* creates the VkSurfaceKHR for `window`.
		* @param window Platform window to render into; must outlive this object.
		*/
	void create(Window *window);
	/// Tears down the surface, debug messenger, and instance, in that order.
	void destroy();

	VkInstance instance() const;
	VkSurfaceKHR surface() const;
	bool validation_enabled() const;

  private:
	// Owns VkInstance/VkSurfaceKHR/VkDebugUtilsMessengerEXT — the
	// pre-C++11 idiom of a private, never-defined copy constructor/
	// assignment operator (this project avoids `= delete`).
	VulkanInstance(const VulkanInstance &other);
	VulkanInstance &operator=(const VulkanInstance &other);

	static VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT severity,
		VkDebugUtilsMessageTypeFlagsEXT type,
		const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
		void *user_data);
	static bool layer_is_available(const char *layer_name);
#ifdef __APPLE__
	static bool instance_extension_is_available(const char *extension_name);
#endif

	void setup_debug_messenger();
	void create_surface(Window *window);

	static constexpr const char *kValidationLayer = "VK_LAYER_KHRONOS_validation";

	Window *_window;
	VkInstance _instance;
	VkDebugUtilsMessengerEXT _debug_messenger;
	VkSurfaceKHR _surface;
	bool _validation_enabled;
};

} // namespace vre
