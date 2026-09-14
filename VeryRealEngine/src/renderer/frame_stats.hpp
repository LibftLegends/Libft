/**
 * @file frame_stats.hpp
 * @brief Culling counts from the most recently drawn frame.
 *
 * How many items Renderer::draw_frame() was handed vs. how many survived
 * frustum culling vs. how many were actually drawn after occlusion
 * culling too. Exists so a caller (main.cpp's FPS report) can show that
 * culling is actually doing something, not just print a frame rate and hope.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class FrameStats
{
  public:
	FrameStats();
	FrameStats(const FrameStats &other);
	FrameStats &operator=(const FrameStats &other);
	~FrameStats();

	/** Size of the `items` list passed to draw_frame(). */
	uint32_t total_items() const;
	void set_total_items(uint32_t value);

	/** How many survived frustum culling. */
	uint32_t frustum_visible() const;
	void set_frustum_visible(uint32_t value);

	/** How many were actually drawn after occlusion culling. */
	uint32_t drawn() const;
	void set_drawn(uint32_t value);

  private:
	uint32_t _total_items;
	uint32_t _frustum_visible;
	uint32_t _drawn;
};

} // namespace vre
