/**
 * @file namecomponent.hpp
 * @brief An entity's human-readable, lookup-by-string name (from the JSON
 * scene file's "name" field).
 */
#pragma once

#include "../vre.hpp"

namespace vre
{
class NameComponent
{
  public:
	NameComponent();
	NameComponent(const NameComponent &other);
	NameComponent &operator=(const NameComponent &other);
	~NameComponent();

	explicit NameComponent(const std::string &name);

	/** Unique name used for lookups (Scene::set_visible(),
		Scene::get_node_position(), ...). */
	const std::string &name() const;
	void set_name(const std::string &value);

  private:
	std::string _name;
};

} // namespace vre
