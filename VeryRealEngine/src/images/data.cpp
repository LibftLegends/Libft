#include "data.hpp"

namespace vre
{
ImageData::ImageData() : _width(0), _height(0)
{
}

ImageData::ImageData(const ImageData &other) : _pixels(other._pixels),
	_width(other._width), _height(other._height)
{
}

ImageData &ImageData::operator=(const ImageData &other)
{
	if (this != &other)
	{
		_pixels = other._pixels;
		_width = other._width;
		_height = other._height;
	}
	return (*this);
}

ImageData::~ImageData()
{
}

std::vector<uint8_t> &ImageData::pixels()
{
	return (_pixels);
}

const std::vector<uint8_t> &ImageData::pixels() const
{
	return (_pixels);
}

uint32_t ImageData::width() const
{
	return (_width);
}

void ImageData::set_width(uint32_t value)
{
	_width = value;
}

uint32_t ImageData::height() const
{
	return (_height);
}

void ImageData::set_height(uint32_t value)
{
	_height = value;
}

} // namespace vre
