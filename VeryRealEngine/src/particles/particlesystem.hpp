/**
 * @file particlesystem.hpp
 * @brief A minimal CPU-side particle system — step 7's "particle-emitting
 * object" requirement (e.g. a coffee machine emitting steam).
 *
 * Scope decision: particles are rendered as small shrinking cubes (reusing
 * the ordinary mesh/material/RenderItem pipeline every other object uses),
 * not soft camera-facing billboards or a GPU-instanced sprite system. That
 * keeps this a self-contained CPU simulation with zero new rendering
 * infrastructure (no billboard shader, no per-instance vertex buffers) —
 * real puffs of rising, fading geometry, just blocky ones. Billboards
 * would look better and are a natural follow-up, not attempted here.
 */
#pragma once

#include "../math/vec3.hpp"
#include "../renderer/renderitem.hpp"
#include "../vre.hpp"
#include "particlesystemdesc.hpp"

namespace vre
{
/// A simple emitter/lifetime/velocity CPU particle simulation, rendered
/// through the normal mesh pipeline.
class ParticleSystem
{
  public:
	ParticleSystem();
	ParticleSystem(const ParticleSystem &other);
	ParticleSystem &operator=(const ParticleSystem &other);
	~ParticleSystem();

	/// Constructs an emitter configured by `desc`.
	explicit ParticleSystem(const ParticleSystemDesc &desc);

	/// Spawns new particles (if due) and advances/despawns existing ones
	/// by `delta_seconds`.
	void update(float delta_seconds);
	/// Appends a RenderItem for every currently-alive particle to
	/// `out_items`.
	void collect_render_items(std::vector<RenderItem> *out_items) const;

  private:
	/// A single live particle's simulation state.
	class Particle
	{
		public:
		Particle();
		Particle(const Particle &other);
		Particle &operator=(const Particle &other);
		~Particle();

		vec3 position;
		vec3 velocity;
		float age;
	};

	/// @return The next pseudo-random value in [0,1) — a tiny xorshift
	/// PRNG, no `<random>` needed.
	float next_random();

	ParticleSystemDesc _desc;
	std::vector<Particle> _particles;
	float _spawn_accumulator;
	uint32_t _rng_state;
};

} // namespace vre
