/**
 * @file texturehandle.hpp
 * @brief Opaque handle identifying a GPU-resident texture inside a Renderer.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class TextureHandle
{
  public:
	TextureHandle();
	TextureHandle(const TextureHandle &other);
	TextureHandle &operator=(const TextureHandle &other);
	~TextureHandle();

	explicit TextureHandle(size_t value);

	size_t value() const;
	bool is_valid() const;
	bool operator==(const TextureHandle &other) const;
	bool operator!=(const TextureHandle &other) const;

	static TextureHandle invalid();

  private:
	size_t _value;
};

} // namespace vre
