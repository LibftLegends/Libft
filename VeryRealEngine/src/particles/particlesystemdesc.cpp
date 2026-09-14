#include "particlesystemdesc.hpp"

namespace vre
{
ParticleSystemDesc::ParticleSystemDesc() : _mesh(0), _spawn_interval(0.15f),
	_lifetime(2.2f), _start_scale(0.14f), _base_velocity(0.0f, 0.5f, 0.0f),
	_velocity_jitter(0.12f), _random_seed(1)
{
}

ParticleSystemDesc::ParticleSystemDesc(const ParticleSystemDesc &other) :
	_emitter_position(other._emitter_position), _mesh(other._mesh),
	_spawn_interval(other._spawn_interval), _lifetime(other._lifetime),
	_start_scale(other._start_scale), _base_velocity(other._base_velocity),
	_velocity_jitter(other._velocity_jitter),
	_random_seed(other._random_seed)
{
}

ParticleSystemDesc &ParticleSystemDesc::operator=(
	const ParticleSystemDesc &other)
{
	if (this != &other)
	{
		_emitter_position = other._emitter_position;
		_mesh = other._mesh;
		_spawn_interval = other._spawn_interval;
		_lifetime = other._lifetime;
		_start_scale = other._start_scale;
		_base_velocity = other._base_velocity;
		_velocity_jitter = other._velocity_jitter;
		_random_seed = other._random_seed;
	}
	return (*this);
}

ParticleSystemDesc::~ParticleSystemDesc()
{
}

const vec3 &ParticleSystemDesc::emitter_position() const
{
	return (_emitter_position);
}

void ParticleSystemDesc::set_emitter_position(const vec3 &value)
{
	_emitter_position = value;
}

MeshHandle ParticleSystemDesc::mesh() const
{
	return (_mesh);
}

void ParticleSystemDesc::set_mesh(MeshHandle value)
{
	_mesh = value;
}

float ParticleSystemDesc::spawn_interval() const
{
	return (_spawn_interval);
}

void ParticleSystemDesc::set_spawn_interval(float value)
{
	_spawn_interval = value;
}

float ParticleSystemDesc::lifetime() const
{
	return (_lifetime);
}

void ParticleSystemDesc::set_lifetime(float value)
{
	_lifetime = value;
}

float ParticleSystemDesc::start_scale() const
{
	return (_start_scale);
}

void ParticleSystemDesc::set_start_scale(float value)
{
	_start_scale = value;
}

const vec3 &ParticleSystemDesc::base_velocity() const
{
	return (_base_velocity);
}

void ParticleSystemDesc::set_base_velocity(const vec3 &value)
{
	_base_velocity = value;
}

float ParticleSystemDesc::velocity_jitter() const
{
	return (_velocity_jitter);
}

void ParticleSystemDesc::set_velocity_jitter(float value)
{
	_velocity_jitter = value;
}

uint32_t ParticleSystemDesc::random_seed() const
{
	return (_random_seed);
}

void ParticleSystemDesc::set_random_seed(uint32_t value)
{
	_random_seed = value;
}

} // namespace vre
