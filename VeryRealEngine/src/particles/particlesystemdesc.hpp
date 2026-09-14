/**
 * @file particlesystemdesc.hpp
 * @brief Configuration for a ParticleSystem emitter.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../rendererresources/meshhandle.hpp"
#include "../vre.hpp"

namespace vre
{
class ParticleSystemDesc
{
  public:
	ParticleSystemDesc();
	ParticleSystemDesc(const ParticleSystemDesc &other);
	ParticleSystemDesc &operator=(const ParticleSystemDesc &other);
	~ParticleSystemDesc();

	const vec3 &emitter_position() const;
	void set_emitter_position(const vec3 &value);

	/** Typically a small cube (see cube_steam.obj). */
	MeshHandle mesh() const;
	void set_mesh(MeshHandle value);

	/** Seconds between new particles. */
	float spawn_interval() const;
	void set_spawn_interval(float value);

	/** Seconds a particle lives before despawning. */
	float lifetime() const;
	void set_lifetime(float value);

	float start_scale() const;
	void set_start_scale(float value);

	/** Upward drift, steam-like. */
	const vec3 &base_velocity() const;
	void set_base_velocity(const vec3 &value);

	/** Random horizontal spread. */
	float velocity_jitter() const;
	void set_velocity_jitter(float value);

	uint32_t random_seed() const;
	void set_random_seed(uint32_t value);

  private:
	vec3 _emitter_position;
	MeshHandle _mesh;
	float _spawn_interval;
	float _lifetime;
	float _start_scale;
	vec3 _base_velocity;
	float _velocity_jitter;
	uint32_t _random_seed;
};

} // namespace vre
