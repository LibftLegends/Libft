#include "contact.hpp"

namespace vre
{
Contact::Contact() : valid(false), penetration(0.0f)
{
}

Contact::Contact(const Contact &other) : valid(other.valid),
	normal(other.normal), penetration(other.penetration)
{
}

Contact &Contact::operator=(const Contact &other)
{
	if (this != &other)
	{
		valid = other.valid;
		normal = other.normal;
		penetration = other.penetration;
	}
	return (*this);
}

Contact::~Contact()
{
}

} // namespace vre
