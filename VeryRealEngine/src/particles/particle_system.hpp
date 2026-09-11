/**
 * @file particle_system.hpp
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

#include "../math/vre_math.hpp"
#include "../renderer/renderer.hpp"

#include <vector>

namespace vre
{

/// Configuration for a ParticleSystem emitter.
struct ParticleSystemDesc
{
    vec3 emitter_position;
    MeshHandle mesh = 0;                  ///< Typically a small cube (see cube_steam.obj).
    float spawn_interval = 0.15f;         ///< Seconds between new particles.
    float lifetime = 2.2f;                ///< Seconds a particle lives before despawning.
    float start_scale = 0.14f;
    vec3 base_velocity{0.0f, 0.5f, 0.0f}; ///< Upward drift, steam-like.
    float velocity_jitter = 0.12f;        ///< Random horizontal spread.
    uint32_t random_seed = 1;
};

/// A simple emitter/lifetime/velocity CPU particle simulation, rendered through the normal mesh pipeline.
class ParticleSystem
{
    public:
        /// Constructs an emitter configured by `desc`.
        explicit ParticleSystem(const ParticleSystemDesc &desc);

        /// Spawns new particles (if due) and advances/despawns existing ones by `delta_seconds`.
        void update(float delta_seconds);
        /// Appends a RenderItem for every currently-alive particle to `out_items`.
        void collect_render_items(std::vector<RenderItem> *out_items) const;

    private:
        /// A single live particle's simulation state.
        struct Particle
        {
            vec3 position;
            vec3 velocity;
            float age = 0.0f;
        };

        ParticleSystemDesc _desc;
        std::vector<Particle> _particles;
        float _spawn_accumulator = 0.0f;
        uint32_t _rng_state;

        /// @return The next pseudo-random value in [0,1) — a tiny xorshift PRNG, no `<random>` needed.
        float next_random();
};

} // namespace vre
