/**
 * @file swap_chain.hpp
 * @brief Owns the VkSwapchainKHR, its images/image views, and the chosen
 * surface format/present mode/extent. Recreated wholesale on resize.
 */
#pragma once

#include "../platform/window.hpp"
#include "../vre.hpp"

namespace vre
{

class SwapChain
{
  public:
	SwapChain();
	~SwapChain();

	/// Creates the swapchain for the window's current size.
	void create(VkPhysicalDevice physical_device, VkDevice device,
		VkSurfaceKHR surface, Window *window, uint32_t graphics_queue_family,
		uint32_t present_queue_family);
	/// Creates an image view for every swapchain image.
	void create_image_views(VkDevice device);
	/// Destroys the image views and the swapchain.
	void destroy(VkDevice device);

	VkSwapchainKHR swapchain() const;
	VkFormat image_format() const;
	VkExtent2D extent() const;
	size_t image_count() const;
	VkImage image(size_t index) const;
	VkImageView image_view(size_t index) const;

  private:
	// Owns VkSwapchainKHR/VkImageView[] — the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator
	// (this project avoids `= delete`).
	SwapChain(const SwapChain &other);
	SwapChain &operator=(const SwapChain &other);

	static VkSurfaceFormatKHR choose_surface_format(const std::vector<VkSurfaceFormatKHR> &formats);
	static VkPresentModeKHR choose_present_mode(const std::vector<VkPresentModeKHR> &modes);
	static VkExtent2D choose_extent(const VkSurfaceCapabilitiesKHR &capabilities,
		int32_t window_width, int32_t window_height);

	VkSwapchainKHR _swapchain;
	VkFormat _image_format;
	VkExtent2D _extent;
	std::vector<VkImage> _images;
	std::vector<VkImageView> _image_views;
};

} // namespace vre
