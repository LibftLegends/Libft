/**
 * @file swapchain_support_details.hpp
 * @brief What a physical device supports for presenting to a given surface.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class SwapchainSupportDetails
{
  public:
	SwapchainSupportDetails();
	SwapchainSupportDetails(const SwapchainSupportDetails &other);
	SwapchainSupportDetails &operator=(const SwapchainSupportDetails &other);
	~SwapchainSupportDetails();

	/// @return The surface capabilities/formats/present modes `device` supports for `surface`.
	static SwapchainSupportDetails query(VkPhysicalDevice device,
		VkSurfaceKHR surface);

	const VkSurfaceCapabilitiesKHR &capabilities() const;
	const std::vector<VkSurfaceFormatKHR> &formats() const;
	const std::vector<VkPresentModeKHR> &present_modes() const;

  private:
	VkSurfaceCapabilitiesKHR _capabilities;
	std::vector<VkSurfaceFormatKHR> _formats;
	std::vector<VkPresentModeKHR> _present_modes;
};

} // namespace vre
