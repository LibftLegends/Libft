/**
 * @file poolbase.hpp
 * @brief Type-erased base so a Registry can hold pools of unrelated
 * component types (Pool<Transform>, Pool<Mesh>, ...) in one homogeneous
 * map, keyed by std::type_index.
 */
#pragma once

#include "../vre.hpp"

namespace vre::ecs
{
class PoolBase
{
  public:
	PoolBase();
	PoolBase(const PoolBase &other);
	PoolBase &operator=(const PoolBase &other);
	virtual ~PoolBase();
};

} // namespace vre::ecs
