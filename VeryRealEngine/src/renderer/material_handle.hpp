/**
 * @file material_handle.hpp
 * @brief Opaque handle identifying a GPU-resident material inside a Renderer.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class MaterialHandle
{
  public:
	MaterialHandle();
	MaterialHandle(const MaterialHandle &other);
	MaterialHandle &operator=(const MaterialHandle &other);
	~MaterialHandle();

	explicit MaterialHandle(size_t value);

	size_t value() const;
	bool is_valid() const;
	bool operator==(const MaterialHandle &other) const;
	bool operator!=(const MaterialHandle &other) const;

	static MaterialHandle invalid();

  private:
	size_t _value;
};

} // namespace vre
