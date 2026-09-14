/**
 * @file physics_collision.cpp
 * @brief Collision-shape testing: PhysicsWorld's private Aabb/Contact/Body
 * nested types' canonical form, plus the broad+narrow-phase test methods.
 * Split out of physics_world.cpp purely to keep each file under this
 * project's 250-line cap.
 */
#include "physics_world.hpp"

namespace vre
{

PhysicsWorld::Aabb::Aabb()
{
}

PhysicsWorld::Aabb::Aabb(const Aabb &other) : min_corner(other.min_corner),
	max_corner(other.max_corner)
{
}

PhysicsWorld::Aabb &PhysicsWorld::Aabb::operator=(const Aabb &other)
{
	if (this != &other)
	{
		min_corner = other.min_corner;
		max_corner = other.max_corner;
	}
	return (*this);
}

PhysicsWorld::Aabb::~Aabb()
{
}

PhysicsWorld::Contact::Contact() : valid(false), penetration(0.0f)
{
}

PhysicsWorld::Contact::Contact(const Contact &other) : valid(other.valid),
	normal(other.normal), penetration(other.penetration)
{
}

PhysicsWorld::Contact &PhysicsWorld::Contact::operator=(const Contact &other)
{
	if (this != &other)
	{
		valid = other.valid;
		normal = other.normal;
		penetration = other.penetration;
	}
	return (*this);
}

PhysicsWorld::Contact::~Contact()
{
}

PhysicsWorld::Body::Body() : inverse_mass(0.0f), restitution(0.0f),
	friction(0.0f), gravity_scale(1.0f), is_static(false), is_trigger(false)
{
}

PhysicsWorld::Body::Body(const Body &other) : position(other.position),
	velocity(other.velocity), inverse_mass(other.inverse_mass),
	restitution(other.restitution), friction(other.friction),
	gravity_scale(other.gravity_scale), is_static(other.is_static),
	is_trigger(other.is_trigger), collider(other.collider),
	debug_name(other.debug_name)
{
}

PhysicsWorld::Body &PhysicsWorld::Body::operator=(const Body &other)
{
	if (this != &other)
	{
		position = other.position;
		velocity = other.velocity;
		inverse_mass = other.inverse_mass;
		restitution = other.restitution;
		friction = other.friction;
		gravity_scale = other.gravity_scale;
		is_static = other.is_static;
		is_trigger = other.is_trigger;
		collider = other.collider;
		debug_name = other.debug_name;
	}
	return (*this);
}

PhysicsWorld::Body::~Body()
{
}

PhysicsWorld::Aabb PhysicsWorld::compute_aabb(const Body &body)
{
	Aabb	box;

	if (body.collider.type() == Collider::Type::Box)
	{
		box.min_corner = body.position - body.collider.half_extents();
		box.max_corner = body.position + body.collider.half_extents();
	}
	else
	{
		vec3 r(body.collider.radius(), body.collider.radius(),
			body.collider.radius());
		box.min_corner = body.position - r;
		box.max_corner = body.position + r;
	}
	return (box);
}

bool PhysicsWorld::aabb_overlap(const Aabb &a, const Aabb &b)
{
	return (a.min_corner.x() <= b.max_corner.x()
		&& a.max_corner.x() >= b.min_corner.x())
		&& (a.min_corner.y() <= b.max_corner.y()
		&& a.max_corner.y() >= b.min_corner.y())
		&& (a.min_corner.z() <= b.max_corner.z()
		&& a.max_corner.z() >= b.min_corner.z());
}

PhysicsWorld::Contact PhysicsWorld::collide(const Body &a, const Body &b)
{
	Contact	contact;

	if (a.collider.type() == Collider::Type::Box
		&& b.collider.type() == Collider::Type::Box)
		return (collide_box_box(a.position, a.collider.half_extents(),
				b.position, b.collider.half_extents()));
	if (a.collider.type() == Collider::Type::Sphere
		&& b.collider.type() == Collider::Type::Sphere)
		return (collide_sphere_sphere(a.position, a.collider.radius(),
				b.position, b.collider.radius()));
	if (a.collider.type() == Collider::Type::Box
		&& b.collider.type() == Collider::Type::Sphere)
		return (collide_box_sphere(a.position, a.collider.half_extents(),
				b.position, b.collider.radius()));
	// Sphere vs box: reuse box-vs-sphere and flip the normal, since it's
	// defined to point from the box toward the sphere either way.
	contact = collide_box_sphere(b.position, b.collider.half_extents(),
			a.position, a.collider.radius());
	contact.normal = contact.normal * -1.0f;
	return (contact);
}

} // namespace vre
