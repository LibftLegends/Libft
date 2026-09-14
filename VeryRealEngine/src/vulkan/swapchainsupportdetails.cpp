#include "swapchainsupportdetails.hpp"

namespace vre
{
SwapchainSupportDetails::SwapchainSupportDetails() : _capabilities()
{
}

SwapchainSupportDetails::SwapchainSupportDetails(
	const SwapchainSupportDetails &other) :
	_capabilities(other._capabilities), _formats(other._formats),
	_present_modes(other._present_modes)
{
}

SwapchainSupportDetails &SwapchainSupportDetails::operator=(
	const SwapchainSupportDetails &other)
{
	if (this != &other)
	{
		_capabilities = other._capabilities;
		_formats = other._formats;
		_present_modes = other._present_modes;
	}
	return (*this);
}

SwapchainSupportDetails::~SwapchainSupportDetails()
{
}

SwapchainSupportDetails SwapchainSupportDetails::query(VkPhysicalDevice device,
	VkSurfaceKHR surface)
{
	SwapchainSupportDetails	support;
	uint32_t				format_count;
	uint32_t				present_mode_count;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
		&support._capabilities);
	format_count = 0;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
		nullptr);
	support._formats.resize(format_count);
	if (format_count > 0)
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &format_count,
			support._formats.data());
	present_mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
		&present_mode_count, nullptr);
	support._present_modes.resize(present_mode_count);
	if (present_mode_count > 0)
	{
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
			&present_mode_count, support._present_modes.data());
	}
	return (support);
}

const VkSurfaceCapabilitiesKHR &SwapchainSupportDetails::capabilities() const
{
	return (_capabilities);
}

const std::vector<VkSurfaceFormatKHR> &SwapchainSupportDetails::formats()
	const
{
	return (_formats);
}

const std::vector<VkPresentModeKHR> &SwapchainSupportDetails::present_modes()
	const
{
	return (_present_modes);
}

} // namespace vre
