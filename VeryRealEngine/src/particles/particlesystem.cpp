#include "particlesystem.hpp"

namespace vre
{
ParticleSystem::Particle::Particle() : age(0.0f)
{
}

ParticleSystem::Particle::Particle(const Particle &other) :
	position(other.position), velocity(other.velocity), age(other.age)
{
}

ParticleSystem::Particle &ParticleSystem::Particle::operator=(
	const Particle &other)
{
	if (this != &other)
	{
		position = other.position;
		velocity = other.velocity;
		age = other.age;
	}
	return (*this);
}

ParticleSystem::Particle::~Particle()
{
}

ParticleSystem::ParticleSystem() : _spawn_accumulator(0.0f), _rng_state(1)
{
}

ParticleSystem::ParticleSystem(const ParticleSystem &other) :
	_desc(other._desc), _particles(other._particles),
	_spawn_accumulator(other._spawn_accumulator),
	_rng_state(other._rng_state)
{
}

ParticleSystem &ParticleSystem::operator=(const ParticleSystem &other)
{
	if (this != &other)
	{
		_desc = other._desc;
		_particles = other._particles;
		_spawn_accumulator = other._spawn_accumulator;
		_rng_state = other._rng_state;
	}
	return (*this);
}

ParticleSystem::~ParticleSystem()
{
}

ParticleSystem::ParticleSystem(const ParticleSystemDesc &desc) : _desc(desc),
	_spawn_accumulator(0.0f),
	_rng_state(desc.random_seed() == 0 ? 1 : desc.random_seed())
{
}

float ParticleSystem::next_random()
{
	// xorshift32 — a tiny, dependency-free PRNG (no <random> needed for
	// something this small); not cryptographic, just needs to look random
	// enough to spread particles out visually.
	_rng_state ^= _rng_state << 13;
	_rng_state ^= _rng_state >> 17;
	_rng_state ^= _rng_state << 5;
	return (static_cast<float>(_rng_state) / static_cast<float>(0xFFFFFFFFu));
}

void ParticleSystem::update(float delta_seconds)
{
	Particle	particle;
	float		jitter_x;
	float		jitter_z;

	_spawn_accumulator += delta_seconds;
	while (_spawn_accumulator >= _desc.spawn_interval())
	{
		_spawn_accumulator -= _desc.spawn_interval();
		particle.position = _desc.emitter_position();
		jitter_x = (next_random() * 2.0f - 1.0f) * _desc.velocity_jitter();
		jitter_z = (next_random() * 2.0f - 1.0f) * _desc.velocity_jitter();
		particle.velocity = _desc.base_velocity() + vec3(jitter_x, 0.0f,
				jitter_z);
		particle.age = 0.0f;
		_particles.push_back(particle);
	}
	for (auto &particle : _particles)
	{
		particle.age += delta_seconds;
		particle.position = particle.position + particle.velocity
			* delta_seconds;
		// Steam decelerates and drifts as it disperses, rather than
		// accelerating upward forever.
		particle.velocity = particle.velocity * (1.0f - 0.6f * delta_seconds);
	}
	// Erase-remove expired particles.
	std::vector<Particle> alive;
	alive.reserve(_particles.size());
	for (auto &particle : _particles)
	{
		if (particle.age < _desc.lifetime())
			alive.push_back(particle);
	}
	_particles = alive;
}

void ParticleSystem::collect_render_items(
	std::vector<RenderItem> *out_items) const
{
	for (const auto &particle : _particles)
	{
		float life_fraction = particle.age / _desc.lifetime();
			// 0 (born) .. 1 (expiring)
		float scale = _desc.start_scale() * (1.0f - life_fraction);
		if (scale <= 0.001f)
			continue ;

		mat4 model = mat4::multiply(mat4::translate(particle.position),
				mat4::scale(vec3(scale, scale, scale)));
		out_items->push_back(RenderItem{_desc.mesh(), model});
	}
}

} // namespace vre
