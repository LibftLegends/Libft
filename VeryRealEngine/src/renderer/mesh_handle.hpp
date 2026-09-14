/**
 * @file mesh_handle.hpp
 * @brief Opaque handle identifying a GPU-resident mesh inside a Renderer.
 */
#pragma once

#include "../vre.hpp"

namespace vre
{

class MeshHandle
{
  public:
	MeshHandle();
	MeshHandle(const MeshHandle &other);
	MeshHandle &operator=(const MeshHandle &other);
	~MeshHandle();

	explicit MeshHandle(size_t value);

	size_t value() const;
	bool is_valid() const;
	bool operator==(const MeshHandle &other) const;
	bool operator!=(const MeshHandle &other) const;

	/// @return The sentinel meaning "no mesh" — an organizational or physics-only entity.
	static MeshHandle invalid();

  private:
	size_t _value;
};

} // namespace vre
