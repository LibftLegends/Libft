/**
 * @file contact.hpp
 * @brief The result of a narrow-phase collision test between two shapes.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../vre.hpp"

namespace vre
{
class Contact
{
  public:
	Contact();
	Contact(const Contact &other);
	Contact &operator=(const Contact &other);
	~Contact();

	bool valid;
	vec3 normal; ///< Points from shape A toward shape B.
	float penetration;
};

} // namespace vre
