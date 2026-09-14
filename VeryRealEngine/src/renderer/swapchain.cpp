#include "swapchain.hpp"

namespace vre
{
SwapChain::SwapChain() : _swapchain(VK_NULL_HANDLE),
	_image_format(VK_FORMAT_UNDEFINED), _extent()
{
}

SwapChain::~SwapChain()
{
}

VkSurfaceFormatKHR SwapChain::choose_surface_format(
	const std::vector<VkSurfaceFormatKHR> &formats)
{
	for (const auto &format : formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB
			&& format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return (format);
	}
	return (formats[0]);
}

VkPresentModeKHR SwapChain::choose_present_mode(
	const std::vector<VkPresentModeKHR> &modes)
{
	// VRE_PRESENT_MODE=immediate: an uncapped-vsync override for FPS
	// benchmarking (Chapter IV.1's "must achieve at least 60 FPS" is about
	// real engine throughput, which MAILBOX/FIFO's display-refresh cap
	// can't measure — a display refreshing at 60Hz reads as "60 FPS"
	// whether the engine could actually push 60 or 6000). Not used by
	// default: normal interactive play has no reason to tear.
	if (const char *env = std::getenv("VRE_PRESENT_MODE"))
	{
		if (std::strcmp(env, "immediate") == 0)
		{
			for (const auto &mode : modes)
				if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
					return (mode);
		}
		else if (std::strcmp(env, "fifo") == 0)
		{
			return (VK_PRESENT_MODE_FIFO_KHR);
		}
	}
	for (const auto &mode : modes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
			return (mode);
	}
	return (VK_PRESENT_MODE_FIFO_KHR); // guaranteed available, vsync'd
}

VkExtent2D SwapChain::choose_extent(
	const VkSurfaceCapabilitiesKHR &capabilities, int32_t window_width,
	int32_t window_height)
{
	if (capabilities.currentExtent.width != UINT32_MAX)
		return (capabilities.currentExtent);
	VkExtent2D extent{static_cast<uint32_t>(window_width),
		static_cast<uint32_t>(window_height)};
	extent.width = std::clamp(extent.width, capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width);
	extent.height = std::clamp(extent.height,
			capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height);
	return (extent);
}

void SwapChain::create(VkPhysicalDevice physical_device, VkDevice device,
	VkSurfaceKHR surface, Window *window, uint32_t graphics_queue_family,
	uint32_t present_queue_family)
{
	SwapchainSupportDetails	support;
	VkSurfaceFormatKHR		surface_format;
	VkPresentModeKHR		present_mode;
	VkExtent2D				extent;
	uint32_t				image_count;
	uint32_t				queue_family_indices[] = {graphics_queue_family,
						present_queue_family};

	support = SwapchainSupportDetails::query(physical_device, surface);
	surface_format = choose_surface_format(support.formats());
	present_mode = choose_present_mode(support.present_modes());
	extent = choose_extent(support.capabilities(), window->get_width(),
			window->get_height());
	image_count = support.capabilities().minImageCount + 1;
	if (support.capabilities().maxImageCount > 0
		&& image_count > support.capabilities().maxImageCount)
		image_count = support.capabilities().maxImageCount;
	VkSwapchainCreateInfoKHR create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.imageArrayLayers = 1;
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	if (graphics_queue_family != present_queue_family)
	{
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices;
	}
	else
	{
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}
	create_info.preTransform = support.capabilities().currentTransform;
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	create_info.clipped = VK_TRUE;
	create_info.oldSwapchain = VK_NULL_HANDLE;
	VK_CHECK(vkCreateSwapchainKHR(device, &create_info, nullptr, &_swapchain));
	vkGetSwapchainImagesKHR(device, _swapchain, &image_count, nullptr);
	_images.resize(image_count);
	vkGetSwapchainImagesKHR(device, _swapchain, &image_count, _images.data());
	_image_format = surface_format.format;
	_extent = extent;
}

void SwapChain::create_image_views(VkDevice device)
{
	_image_views.resize(_images.size());
	for (size_t i = 0; i < _images.size(); i++)
	{
		_image_views[i] = VulkanDevice::create_image_view(device, _images[i],
				_image_format, VK_IMAGE_ASPECT_COLOR_BIT);
	}
}

void SwapChain::destroy(VkDevice device)
{
	for (VkImageView view : _image_views)
		vkDestroyImageView(device, view, nullptr);
	_image_views.clear();
	if (_swapchain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(device, _swapchain, nullptr);
	_swapchain = VK_NULL_HANDLE;
}

VkSwapchainKHR SwapChain::swapchain() const
{
	return (_swapchain);
}

VkFormat SwapChain::image_format() const
{
	return (_image_format);
}

VkExtent2D SwapChain::extent() const
{
	return (_extent);
}

size_t SwapChain::image_count() const
{
	return (_images.size());
}

VkImage SwapChain::image(size_t index) const
{
	return (_images[index]);
}

VkImageView SwapChain::image_view(size_t index) const
{
	return (_image_views[index]);
}

} // namespace vre
