/**
 * @file registry.hpp
 * @brief The registry itself: entity allocation plus generic
 * add/remove/get/query access to per-type component pools.
 *
 * Entities are never recycled (a monotonically increasing counter) —
 * simpler and sufficient for scenes that are loaded once and never
 * destroy individual entities at runtime, which is the only way this
 * engine currently uses it; recycling would only matter for a game that
 * spawns/despawns large numbers of entities continuously.
 *
 * Like Pool<T>, every method here is a template (the whole point of a
 * generic component store), so — same reasoning as pool.hpp — the bodies
 * stay inline in this header rather than moving to a .cpp.
 */
#pragma once

#include "entity.hpp"
#include "pool.hpp"
#include "pool_base.hpp"

namespace vre::ecs
{

class Registry
{
  public:
	Registry();
	~Registry();

	/** @return A freshly allocated entity handle,
		unique for this registry's lifetime. */
	Entity create()
	{
		Entity entity(_next_entity);
		_next_entity++;
		return (entity);
	}

	/**
		* @brief Adds (or replaces) a component of type `T` on `entity`.
		* @param entity Entity to attach the component to.
		* @param value Component value (defaults to `T{}`).
		* @return A reference to the stored component.
		*/
	template <typename T> T &emplace(Entity entity, T value = T{})
	{
		auto &table = storage<T>();
		table[entity] = std::move(value);
		return (table[entity]);
	}

	/** Removes `entity`'s component of type `T`, if it has one. */
	template <typename T> void remove(Entity entity)
	{
		auto it = _pools.find(std::type_index(typeid(T)));
		if (it != _pools.end())
			static_cast<Pool<T> *>(it->second.get())->data().erase(entity);
	}

	/** @return true if `entity` currently has a component of type `T`. */
	template <typename T> bool has(Entity entity) const
	{
		auto it = _pools.find(std::type_index(typeid(T)));
		if (it == _pools.end())
			return (false);
		const auto &table = static_cast<const Pool<T> *>(it->second.get())->data();
		return (table.find(entity) != table.end());
	}

	/** @return A pointer to `entity`'s component of type `T`,
		or nullptr if it has none. */
	template <typename T> T *try_get(Entity entity)
	{
		auto it = _pools.find(std::type_index(typeid(T)));
		if (it == _pools.end())
			return (nullptr);
		auto &table = static_cast<Pool<T> *>(it->second.get())->data();
		auto entry = table.find(entity);
		return (entry == table.end() ? nullptr : &entry->second);
	}

	/** @return A const pointer to `entity`'s component of type `T`,
		or nullptr if it has none. */
	template <typename T> const T *try_get(Entity entity) const
	{
		return (const_cast<Registry *>(this)->try_get<T>(entity));
	}

	/**
		* @brief Direct access to a whole component table, for systems that
		* need to iterate every entity carrying a given component (e.g.
		* "every entity with a Transform").
		*
		* Creates the (empty) table on first use for a component type
		* nothing has been added to yet, exactly like emplace() would.
		* @return A mutable reference to the entity->component map for type `T`.
		*/
	template <typename T> std::unordered_map<Entity, T> &storage()
	{
		std::type_index key = std::type_index(typeid(T));
		auto it = _pools.find(key);
		if (it == _pools.end())
			it = _pools.emplace(key, std::make_unique<Pool<T>>()).first;
		return (static_cast<Pool<T> *>(it->second.get())->data());
	}

	/** @return A const reference to the entity->component map for type `T`. */
	template <typename T> const std::unordered_map<Entity, T> &storage() const
	{
		return (const_cast<Registry *>(this)->storage<T>());
	}

  private:
	// A Registry owns polymorphic component pools behind unique_ptr<PoolBase>;
	// a generic deep copy would need a virtual clone() on every Pool<T> and
	// nothing in this engine ever copies a Registry (each Scene owns exactly
	// one for its whole lifetime), so it is made non-copyable instead — the
	// pre-C++11 idiom of a private, never-defined copy constructor/assignment
	// operator (this project avoids `= delete`).
	Registry(const Registry &other);
	Registry &operator=(const Registry &other);

	std::unordered_map<std::type_index, std::unique_ptr<PoolBase>> _pools;
		///< One pool per component type.
	uint32_t _next_entity = 0;                                            
		///< Next handle create() will hand out.
};

} // namespace vre::ecs
