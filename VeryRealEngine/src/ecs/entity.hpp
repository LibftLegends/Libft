/**
 * @file entity.hpp
 * @brief Opaque handle identifying an entity in a Registry.
 *
 * A thin wrapper around an integer id — kept as its own class (rather than a
 * bare `uint32_t` typedef) so it can't be silently confused with any other
 * handle type or a plain array index at a call site.
 */
#pragma once

#include "../vre.hpp"

namespace vre::ecs
{
class Entity
{
  public:
	Entity();
	Entity(const Entity &other);
	Entity &operator=(const Entity &other);
	~Entity();

	explicit Entity(uint32_t value);

	uint32_t value() const;
	bool is_valid() const;
	bool operator==(const Entity &other) const;
	bool operator!=(const Entity &other) const;

	/// @return The sentinel Entity meaning "no entity" (e.g. an entity
	/// with no parent).
	static Entity invalid();

  private:
	uint32_t _value;
};

} // namespace vre::ecs

namespace std
{
/// Lets Entity be used as an unordered_map/unordered_set key (e.g.
/// Pool<T>'s component table).
template <> struct hash<vre::ecs::Entity>
{
	size_t operator()(const vre::ecs::Entity &entity) const
	{
		return (std::hash<uint32_t>()(entity.value()));
	}
};

} // namespace std
