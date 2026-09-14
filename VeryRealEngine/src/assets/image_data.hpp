/**
 * @file image_data.hpp
 * @brief Decoded image pixel data.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class ImageData
{
  public:
	ImageData();
	ImageData(const ImageData &other);
	ImageData &operator=(const ImageData &other);
	~ImageData();

	/// Tightly packed RGBA8, row 0 = top of image.
	std::vector<uint8_t> &pixels();
	const std::vector<uint8_t> &pixels() const;

	uint32_t width() const;
	void set_width(uint32_t value);

	uint32_t height() const;
	void set_height(uint32_t value);

  private:
	std::vector<uint8_t> _pixels;
	uint32_t _width;
	uint32_t _height;
};

} // namespace vre
