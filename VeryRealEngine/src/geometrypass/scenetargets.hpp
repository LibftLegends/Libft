/**
 * @file scenetargets.hpp
 * @brief The geometry pass's intermediate HDR scene-color + depth render
 * targets and their framebuffer — factored out of GeometryPass so that
 * class stays focused on the pipeline/UBO and recording draws. These are
 * the resources a swapchain resize recreates.
 */
#pragma once

#include "../vre.hpp"
#include "../vulkan/swapchain.hpp"
#include "../vulkan/device.hpp"

namespace vre
{
class GeometrySceneTargets
{
  public:
	GeometrySceneTargets();
	~GeometrySceneTargets();

	/// Creates the depth/scene-color images+views and the framebuffer
	/// binding them to `render_pass`.
	void create(const VulkanDevice &device, const SwapChain &swap_chain,
		VkRenderPass render_pass, VkFormat scene_color_format);
	/// Destroys the depth/scene-color images+views and the framebuffer
	/// (e.g. before a resize).
	void destroy();
	/// Destroys and recreates every resource above, e.g. after a resize.
	void recreate(const VulkanDevice &device, const SwapChain &swap_chain,
		VkRenderPass render_pass, VkFormat scene_color_format);

	VkFormat depth_format() const;
	VkImageView scene_color_image_view() const;
	VkImageView depth_image_view() const;
	VkFramebuffer framebuffer() const;

  private:
	// Owns VkImage/VkImageView/VkFramebuffer — the pre-C++11 idiom of a
	// private, never-defined copy constructor/assignment operator (this
	// project avoids `= delete`).
	GeometrySceneTargets(const GeometrySceneTargets &other);
	GeometrySceneTargets &operator=(const GeometrySceneTargets &other);

	void create_framebuffer(VkDevice device, VkRenderPass render_pass,
		VkExtent2D extent);

	VkDevice _device; ///< Non-owning; cached by create() for destroy().
	VkFormat		_depth_format;

	VkImage			_depth_image;
	VkDeviceMemory	_depth_image_memory;
	VkImageView		_depth_image_view;

	VkImage			_scene_color_image;
	VkDeviceMemory	_scene_color_image_memory;
	VkImageView		_scene_color_image_view;

	VkFramebuffer	_framebuffer;
};

} // namespace vre
