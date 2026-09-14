/**
 * @file pool.hpp
 * @brief Concrete, typed component storage for component type `T`.
 *
 * A template's member functions must be visible at every instantiation
 * site, so — unlike every other class in this refactor — Pool<T>'s bodies
 * stay inline in the header rather than moving to a .cpp; there is no
 * separate compilation unit a template could be instantiated once in here.
 */
#pragma once

#include "entity.hpp"
#include "pool_base.hpp"

namespace vre::ecs
{

template <typename T> class Pool : public PoolBase
{
  public:
	Pool() : PoolBase()
	{
	}

	Pool(const Pool &other) : PoolBase(other), _data(other._data)
	{
	}

	Pool &operator=(const Pool &other)
	{
		if (this != &other)
		{
			PoolBase::operator=(other);
			_data = other._data;
		}
		return (*this);
	}

	~Pool() override
	{
	}

	/// @return The component instances in this pool, keyed by owning entity.
	std::unordered_map<Entity, T> &data()
	{
		return (_data);
	}

	/** @return A const reference to the component instances in this pool,
		keyed by owning entity. */
	const std::unordered_map<Entity, T> &data() const
	{
		return (_data);
	}

  private:
	std::unordered_map<Entity, T> _data;
};

} // namespace vre::ecs
